/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive
 */

#include "receiver.hpp"

#include "classify.hpp"
#include "js8core/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <span>
#include <variant>

namespace x6100::js8 {

namespace {

constexpr int JS8_RATE = 12000;

// Longest backlog the worker will accept before discarding it: the decoder
// needs contiguous audio, so a stalled worker is better restarted cleanly.
constexpr double MAX_PENDING_SEC = 5.0;

// How often the sample count is compared with the wall clock.
constexpr std::int64_t CLOCK_CHECK_MS = 2000;

// Audio this far behind the clock went missing (a stall, or the caller
// stopped feeding it during TX). The gap is filled with silence: a realign
// alone would leave minute-old audio in the ring, which decodes again as new.
constexpr std::int64_t GAP_FILL_MS = 1000;
constexpr std::int64_t RING_MS     = 60000;

int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }

} // namespace

std::int64_t wall_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

Receiver::Receiver(const Config &config, Callbacks callbacks)
    : config_(config),
      cb_(std::move(callbacks)),
      resampler_(JS8_RATE / gcd(JS8_RATE, config.input_rate), config.input_rate / gcd(JS8_RATE, config.input_rate)),
      assembler_([this](const RxFrame &assembled) {
          if (!cb_.on_message) return;
          RxFrame msg = assembled;
          switch (verify_command_checksum(msg.text)) {
          case Checksum::None: msg.checksum = 0; break;
          case Checksum::Valid: msg.checksum = 1; break;
          case Checksum::Invalid: msg.checksum = -1; break;
          }
          cb_.on_message(msg);
      }) {
    js8core::EngineConfig ec;
    ec.sample_rate_hz   = JS8_RATE;
    // Schedules for every speed; set_submodes() below picks what's decoded.
    ec.submodes         = SUBMODE_NORMAL | SUBMODE_FAST | SUBMODE_TURBO | SUBMODE_SLOW;
    ec.spectrum_enabled = false;

    js8core::EngineCallbacks ecb;
    ecb.on_event = [this](js8core::events::Variant const &ev) {
        if (auto d = std::get_if<js8core::events::Decoded>(&ev)) {
            {
                std::lock_guard<std::mutex> lock(assembler_mutex_);
                if (duplicates_.seen(d->mode, d->data, d->frequency, wall_ms())) return;
            }
            RxFrame f;
            f.type           = d->type;
            f.text           = renderer_.render(d->data, &f.type, d->frequency);
            f.utc            = d->utc;
            f.snr            = d->snr;
            f.dt             = d->xdt;
            f.freq_hz        = d->frequency;
            f.quality        = d->quality;
            f.low_confidence = d->quality < LOW_CONFIDENCE_QUALITY;
            f.mode           = d->mode;
            f.drift_ms       = d->drift_ms;
            f.timestamp_ms   = wall_ms();

            if (cb_.on_frame) cb_.on_frame(f);
            std::lock_guard<std::mutex> lock(assembler_mutex_);
            assembler_.add(f);
        } else if (auto fin = std::get_if<js8core::events::DecodeFinished>(&ev)) {
            if (cb_.on_cycle_done) cb_.on_cycle_done(fin->decoded);
        }
    };
    if (cb_.on_log) {
        ecb.on_log   = [this](js8core::LogLevel, std::string_view m) { cb_.on_log(std::string(m)); };
        ecb.on_error = [this](std::string_view m) { cb_.on_log("error: " + std::string(m)); };
    }

    engine_ = js8core::make_engine(ec, std::move(ecb), {});
    engine_->set_submodes(config_.submodes);
    engine_->start();

    worker_ = std::thread([this] { worker_loop(); });
}

Receiver::~Receiver() {
    {
        std::lock_guard<std::mutex> lock(in_mutex_);
        stop_ = true;
    }
    in_cv_.notify_one();
    if (worker_.joinable()) worker_.join();

    // Joins the engine's decode thread; no callbacks fire after this.
    engine_->stop();
    engine_.reset();
}

void Receiver::set_submodes(int submodes) {
    engine_->set_submodes(submodes);
}

void Receiver::feed(const float *samples, std::size_t n) {
    {
        std::lock_guard<std::mutex> lock(in_mutex_);
        pending_.insert(pending_.end(), samples, samples + n);
    }
    in_cv_.notify_one();
}

void Receiver::clear_messages() {
    std::lock_guard<std::mutex> lock(assembler_mutex_);
    assembler_.clear();
}

void Receiver::worker_loop() {
    std::vector<float> in;
    std::vector<float> out;

    for (;;) {
        {
            std::unique_lock<std::mutex> lock(in_mutex_);
            in_cv_.wait_for(lock, std::chrono::milliseconds(250), [this] { return stop_ || !pending_.empty(); });
            if (stop_) return;
            in.swap(pending_);
            pending_.clear();
        }

        if (in.size() > MAX_PENDING_SEC * config_.input_rate) {
            // We fell far behind; this audio is stale. Start over in sync.
            in.clear();
            resampler_.reset();
            aligned_ = false;
        }

        if (!in.empty()) {
            if (cb_.on_audio) cb_.on_audio(in.data(), in.size());
            out.clear();
            resampler_.process(in.data(), in.size(), out);
            in.clear();
            submit(out);
        }

        std::lock_guard<std::mutex> lock(assembler_mutex_);
        assembler_.flush_stale(wall_ms());
    }
}

void Receiver::submit(const std::vector<float> &audio_12k) {
    if (audio_12k.empty()) return;

    check_clock(audio_12k.size());

    std::vector<std::int16_t> pcm(audio_12k.size());
    for (std::size_t i = 0; i < pcm.size(); ++i) {
        float v = std::clamp(audio_12k[i], -1.0f, 1.0f);
        pcm[i]  = (std::int16_t)std::lrintf(v * 32767.0f);
    }
    push_pcm(pcm.data(), pcm.size());
}

void Receiver::push_pcm(const std::int16_t *pcm, std::size_t count) {
    // The engine's slot scheduler steps through its ring one capture buffer
    // at a time; a multi-second burst (after a stall) would jump over decode
    // windows. Hand it audio in sound-card-sized pieces.
    constexpr std::size_t CHUNK = 4096;
    for (std::size_t off = 0; off < count; off += CHUNK) {
        std::size_t n = std::min(CHUNK, count - off);

        js8core::AudioInputBuffer buf;
        buf.data        = std::as_bytes(std::span<const std::int16_t>(pcm + off, n));
        buf.format      = {JS8_RATE, 1, js8core::SampleType::Int16};
        buf.captured_at = std::chrono::steady_clock::now();
        engine_->submit_capture(buf);
    }
}

void Receiver::check_clock(std::size_t new_samples) {
    const std::int64_t now = wall_ms();

    if (!aligned_) {
        // First audio (or recovery): align on the buffer we're about to submit.
        engine_->request_realign();
        aligned_             = true;
        align_wall_ms_       = now;
        samples_since_align_ = new_samples;
        return;
    }

    samples_since_align_ += new_samples;

    if (config_.realign_threshold_ms <= 0) return;

    const std::int64_t elapsed_wall = now - align_wall_ms_;
    if (elapsed_wall < CLOCK_CHECK_MS) return;

    // Audio arrives in bursts, so compare against the end of this buffer.
    const std::int64_t elapsed_audio = (std::int64_t)(samples_since_align_ * 1000 / JS8_RATE);
    const std::int64_t error_ms      = elapsed_wall - elapsed_audio;

    if (error_ms >= GAP_FILL_MS) {
        // Silence for the missing audio (at most one ring's worth), placed
        // before the buffer about to be submitted.
        const std::int64_t fill_ms = std::min(error_ms, RING_MS);
        std::vector<std::int16_t> silence((std::size_t)(fill_ms * JS8_RATE / 1000), 0);
        push_pcm(silence.data(), silence.size());
        if (cb_.on_log) cb_.on_log("gap: " + std::to_string(error_ms) + " ms of audio missing, filled with silence");
        if (error_ms <= RING_MS) {
            samples_since_align_ += silence.size();
            return;
        }
        // Longer than the ring: the silence cleared it; now realign.
    }

    if (std::llabs(error_ms) > config_.realign_threshold_ms) {
        engine_->request_realign();
        realigns_++;
        if (cb_.on_log) cb_.on_log("realign: audio/clock error " + std::to_string(error_ms) + " ms");
        align_wall_ms_       = now;
        samples_since_align_ = new_samples;
    }
}

} // namespace x6100::js8
