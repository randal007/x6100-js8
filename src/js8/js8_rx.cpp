/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive, C interface for the dialog
 */

#include "js8_rx.h"

#include "classify.hpp"
#include "receiver.hpp"
#include "resampler.hpp"
#include "wav.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <new>
#include <numeric>
#include <string>
#include <thread>

using namespace x6100::js8;

struct js8_rx {
    std::string               my_call;
    int                       input_rate = 0;
    js8_rx_cb_t               cb{};
    std::unique_ptr<Receiver> receiver;

    // WAV test mode
    std::thread       wav_thread;
    std::atomic<bool> wav_active{false};
    std::atomic<bool> wav_stop{false};
};

namespace {

void copy_str(char *dst, std::size_t cap, const std::string &src) {
    std::size_t n = std::min(cap - 1, src.size());
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

js8_rx_msg_t to_c(const RxFrame &f, const std::string &my_call) {
    js8_rx_msg_t m{};
    m.utc            = f.utc;
    m.snr            = (int16_t)f.snr;
    m.dt             = f.dt;
    m.freq_hz        = f.freq_hz;
    m.type           = (uint8_t)f.type;
    m.submode        = (uint8_t)f.mode;
    m.low_confidence = f.low_confidence;
    m.checksum       = (int8_t)f.checksum;
    m.partial        = f.partial;
    m.msg_id         = f.msg_id;

    auto mc    = classify(f.text, my_call);
    m.heartbeat = mc.heartbeat;
    m.snr_report = mc.snr_report;
    m.cq        = mc.cq;
    m.to_me     = mc.to_me;
    m.to_group  = mc.to_group;
    copy_str(m.from, sizeof(m.from), mc.from);
    copy_str(m.to, sizeof(m.to), mc.to);
    copy_str(m.text, sizeof(m.text), f.text);
    return m;
}

} // namespace

extern "C" js8_rx_t *js8_rx_create(int input_rate, int submodes, const char *my_call, const js8_rx_cb_t *cb) {
    auto *rx = new (std::nothrow) js8_rx;
    if (!rx) return nullptr;
    if (my_call) rx->my_call = my_call;
    rx->input_rate = input_rate;
    if (cb) rx->cb = *cb;

    Receiver::Config config;
    config.input_rate = input_rate;
    config.submodes   = submodes;

    Receiver::Callbacks callbacks;
    callbacks.on_frame = [rx](const RxFrame &f) {
        if (rx->cb.on_frame) {
            auto m = to_c(f, rx->my_call);
            rx->cb.on_frame(&m, rx->cb.ctx);
        }
    };
    callbacks.on_message = [rx](const RxFrame &f) {
        if (rx->cb.on_message) {
            auto m = to_c(f, rx->my_call);
            rx->cb.on_message(&m, rx->cb.ctx);
        }
    };
    callbacks.on_cycle_done = [rx](std::size_t n) {
        if (rx->cb.on_cycle_done) rx->cb.on_cycle_done((unsigned)n, rx->cb.ctx);
    };
    if (rx->cb.on_audio) {
        callbacks.on_audio = [rx](const float *samples, std::size_t n) {
            rx->cb.on_audio(samples, (unsigned)n, rx->cb.ctx);
        };
    }

    try {
        rx->receiver = std::make_unique<Receiver>(config, std::move(callbacks));
    } catch (...) {
        delete rx;
        return nullptr;
    }
    return rx;
}

extern "C" void js8_rx_feed(js8_rx_t *rx, const float *samples, unsigned n) {
    if (rx && samples && n && !rx->wav_active) rx->receiver->feed(samples, n);
}

namespace {

constexpr int SLOT_MS = 30000; // a slot start for every speed (6, 10, 15 and 30 s)

// Real-time feeder: silence until the next slot boundary, then the file.
// Feeding silence (rather than nothing) keeps the decoder's ring continuous.
void play(js8_rx_t *rx, std::vector<float> audio, std::int64_t start_wall_ms) {
    using namespace std::chrono;
    const std::size_t        piece = (std::size_t)rx->input_rate / 50; // 20 ms
    const std::vector<float> silence(piece, 0.0f);
    const auto               t0    = steady_clock::now();
    const std::int64_t       lead  = start_wall_ms - wall_ms();
    std::size_t              fed   = 0; // samples since t0, silence included
    std::size_t              pos   = 0;

    const std::size_t lead_samples = lead > 0 ? (std::size_t)(lead * rx->input_rate / 1000) : 0;

    while (!rx->wav_stop && pos < audio.size()) {
        const float *p;
        std::size_t  n;
        if (fed < lead_samples) {
            p = silence.data();
            n = std::min(piece, lead_samples - fed);
        } else {
            p = &audio[pos];
            n = std::min(piece, audio.size() - pos);
            pos += n;
        }
        rx->receiver->feed(p, n);
        fed += n;
        std::this_thread::sleep_until(t0 + microseconds((long long)(fed * 1e6 / rx->input_rate)));
    }
    rx->wav_active = false;
}

} // namespace

extern "C" float js8_rx_play_wav(js8_rx_t *rx, const char *path, char *err, unsigned err_len) {
    auto fail = [&](const std::string &m) {
        if (err && err_len) {
            std::strncpy(err, m.c_str(), err_len - 1);
            err[err_len - 1] = '\0';
        }
        return -1.0f;
    };
    if (!rx || !path) return fail("no receiver");

    js8_rx_stop_wav(rx);

    WavAudio    wav;
    std::string error;
    if (!read_wav(path, wav, error)) return fail(error);
    if (wav.samples.empty()) return fail("empty WAV");

    std::vector<float> audio;
    if (wav.rate == rx->input_rate) {
        audio = std::move(wav.samples);
    } else {
        int g = std::gcd(wav.rate, rx->input_rate);
        int l = rx->input_rate / g, m = wav.rate / g;
        if (l > 2000 || m > 2000) return fail("unsupported sample rate " + std::to_string(wav.rate));
        RationalResampler r(l, m);
        r.process(wav.samples.data(), wav.samples.size(), audio);
    }

    // Start at the next slot boundary, at least half a second away.
    std::int64_t now   = wall_ms();
    std::int64_t start = (now / SLOT_MS + 1) * SLOT_MS;
    if (start - now < 500) start += SLOT_MS;

    rx->wav_stop   = false;
    rx->wav_active = true;
    rx->receiver->clear_messages();
    rx->wav_thread = std::thread(play, rx, std::move(audio), start);
    return (float)(start - now) / 1000.0f;
}

extern "C" void js8_rx_stop_wav(js8_rx_t *rx) {
    if (!rx) return;
    rx->wav_stop = true;
    if (rx->wav_thread.joinable()) rx->wav_thread.join();
    rx->wav_active = false;
}

extern "C" bool js8_rx_wav_active(js8_rx_t *rx) {
    return rx && rx->wav_active;
}

extern "C" void js8_rx_clear(js8_rx_t *rx) {
    if (rx) rx->receiver->clear_messages();
}

extern "C" void js8_rx_set_submodes(js8_rx_t *rx, int submodes) {
    if (rx) rx->receiver->set_submodes(submodes);
}

extern "C" void js8_rx_set_decode_range(js8_rx_t *rx, int low_hz, int high_hz) {
    if (rx) rx->receiver->set_decode_range(low_hz, high_hz);
}

extern "C" void js8_rx_set_qso_offset(js8_rx_t *rx, int offset_hz) {
    if (rx) rx->receiver->set_qso_offset(offset_hz);
}

extern "C" void js8_rx_destroy(js8_rx_t *rx) {
    if (!rx) return;
    js8_rx_stop_wav(rx);
    rx->receiver.reset();
    delete rx;
}
