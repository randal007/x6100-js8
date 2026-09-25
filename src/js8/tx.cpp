/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 transmit
 */

#include "tx.hpp"

#include "assembler.hpp"
#include "classify.hpp"
#include "render.hpp"

#include "js8core/decoder.hpp"
#include "js8core/protocol/costas.hpp"
#include "js8core/protocol/varicode.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>

namespace x6100::js8 {

namespace {

std::string normalise(const std::string &text) {
    std::string out;
    bool        space = false;
    for (char c : text) {
        if (std::isspace((unsigned char)c)) {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        out += (char)std::toupper((unsigned char)c);
    }
    return out;
}

// Decode our own frames with the same code the receiver uses.
std::string decode_back(const std::vector<TxFrame> &frames) {
    FrameRenderer    renderer;
    std::string      out;
    MessageAssembler assembler([&](const RxFrame &m) { out = m.text; });
    for (auto &f : frames) {
        RxFrame rx;
        rx.type    = f.bits;
        rx.freq_hz = 1500;
        rx.text    = renderer.render(f.frame, &rx.type, rx.freq_hz);
        assembler.add(rx);
    }
    assembler.flush_stale(INT64_MAX);
    verify_command_checksum(out);
    return out;
}

class WallClock : public Transmitter::Clock {
public:
    std::int64_t now_ms() override {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }
    bool wait_until(std::int64_t ms, const std::atomic<bool> &cancelled) override {
        // Short sleeps so stop() is noticed quickly without extra plumbing.
        while (!cancelled) {
            std::int64_t left = ms - now_ms();
            if (left <= 0) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(std::min<std::int64_t>(left, 50)));
        }
        return false;
    }
};

} // namespace

bool is_sendable_char(char c) {
    static const char *const extra = " .-+?!\"/@:>";
    unsigned char            u     = (unsigned char)std::toupper((unsigned char)c);
    return (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || (c && std::strchr(extra, c));
}

TxPlan plan_message(const std::string &my_call, const std::string &my_grid, const std::string &text) {
    namespace vc = js8core::protocol::varicode;
    TxPlan plan;
    plan.text = normalise(text);

    if (my_call.empty()) {
        plan.error = "set your callsign first";
        return plan;
    }
    if (plan.text.empty()) {
        plan.error = "nothing to send";
        return plan;
    }
    for (char c : plan.text) {
        if (!is_sendable_char(c)) {
            plan.error = std::string("JS8 can't send '") + c + "'";
            return plan;
        }
    }

    // Free text that doesn't name anyone would otherwise go out anonymously.
    auto mc             = classify(plan.text, my_call);
    bool force_identify = mc.to.empty() && !mc.cq;

    std::vector<std::pair<std::string, int>> frames;
    try {
        frames = vc::build_message_frames(normalise(my_call), normalise(my_grid), "", plan.text, force_identify,
                                          false, 0);
    } catch (const std::exception &e) {
        plan.error = e.what();
        return plan;
    }
    if (frames.empty()) {
        plan.error = "JS8 can't encode this message";
        return plan;
    }

    const auto &costas = js8core::protocol::costas(js8core::protocol::CostasType::Original);
    for (auto &[frame, bits] : frames) {
        TxFrame f;
        f.frame = frame.substr(0, 12);
        f.bits  = bits;
        js8core::legacy_encode(bits, costas, f.frame.c_str(), f.tones.data());
        plan.frames.push_back(f);
    }
    plan.preview = decode_back(plan.frames);
    if ((int)plan.frames.size() > TX_MAX_FRAMES)
        plan.error = "too long: " + std::to_string(plan.frames.size()) + " frames (max " +
                     std::to_string(TX_MAX_FRAMES) + ")";
    return plan;
}

// Same pulse shaping as src/ft8/gfsk.c, generalised to any BT and returning
// float samples.
std::vector<float> synth_frame(const std::array<int, TX_SYMBOLS> &tones, double offset_hz, int rate, double bt) {
    constexpr double K        = 5.336446; // pi * sqrt(2 / ln 2)
    const int        nsps     = (int)std::lround(0.16 * rate);
    const std::size_t n_wave  = (std::size_t)TX_SYMBOLS * nsps;
    const double     dphi_pk  = 2.0 * M_PI / nsps; // one tone spacing (1 / T) in radians per sample

    std::vector<double> pulse(3 * nsps);
    for (int i = 0; i < 3 * nsps; i++) {
        double t = i / (double)nsps - 1.5;
        pulse[i] = (std::erf(K * bt * (t + 0.5)) - std::erf(K * bt * (t - 0.5))) / 2.0;
    }

    std::vector<double> dphi(n_wave + 2 * nsps, 2.0 * M_PI * offset_hz / rate);
    for (int s = 0; s < TX_SYMBOLS; s++)
        for (int j = 0; j < 3 * nsps; j++) dphi[s * nsps + j] += dphi_pk * tones[s] * pulse[j];
    // Extend the first and last symbols so the ends are shaped too.
    for (int j = 0; j < 2 * nsps; j++) {
        dphi[j] += dphi_pk * pulse[j + nsps] * tones[0];
        dphi[j + TX_SYMBOLS * nsps] += dphi_pk * pulse[j] * tones[TX_SYMBOLS - 1];
    }

    std::vector<float> out(n_wave);
    double             phi = 0.0;
    for (std::size_t k = 0; k < n_wave; k++) {
        out[k] = (float)std::sin(phi);
        phi    = std::fmod(phi + dphi[k + nsps], 2.0 * M_PI);
    }

    // Raised-cosine ramp over 1/8 symbol at each end, as the FT8 app does.
    const int ramp = nsps / 8;
    for (int i = 0; i < ramp; i++) {
        float env = (float)((1.0 - std::cos(M_PI * i / ramp)) / 2.0);
        out[i] *= env;
        out[n_wave - 1 - i] *= env;
    }
    return out;
}

std::int64_t next_tx_start_ms(std::int64_t now_ms) {
    std::int64_t slot_start = now_ms - now_ms % TX_SLOT_MS;
    if (now_ms - slot_start < TX_START_DELAY_MS) return slot_start + TX_START_DELAY_MS;
    return slot_start + TX_SLOT_MS + TX_START_DELAY_MS;
}

Transmitter::Transmitter(int rate, Callbacks callbacks, Clock *clock)
    : rate_(rate), cb_(std::move(callbacks)), clock_(clock) {
    if (!clock_) {
        own_clock_ = std::make_unique<WallClock>();
        clock_     = own_clock_.get();
    }
}

Transmitter::~Transmitter() {
    stop();
    if (thread_.joinable()) thread_.join();
}

bool Transmitter::send(const TxPlan &plan, double offset_hz, std::string *why) {
    auto reject = [&](const std::string &m) {
        if (why) *why = m;
        return false;
    };
    if (!plan.ok()) return reject(plan.error.empty() ? "nothing to send" : plan.error);
    if (offset_hz < TX_MIN_OFFSET_HZ || offset_hz > TX_MAX_OFFSET_HZ)
        return reject("offset must be " + std::to_string(TX_MIN_OFFSET_HZ) + "-" + std::to_string(TX_MAX_OFFSET_HZ) +
                      " Hz");
    if (busy_.exchange(true)) return reject("already sending");

    if (thread_.joinable()) thread_.join(); // previous message's thread has finished
    stop_   = false;
    thread_ = std::thread(&Transmitter::run, this, plan, offset_hz);
    return true;
}

// Only signals: safe from any thread, including the host's play callback.
// The thread is joined by the next send() or the destructor.
void Transmitter::stop() {
    stop_ = true;
}

Transmitter::Status Transmitter::status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void Transmitter::set_status(const Status &s) {
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_ = s;
    }
    if (cb_.on_status) cb_.on_status(s);
}

void Transmitter::run(TxPlan plan, double offset_hz) {
    const int count = (int)plan.frames.size();
    Status    st;
    st.frames    = count;
    st.text      = plan.text;
    st.offset_hz = offset_hz;

    bool         completed = false;
    std::int64_t start     = next_tx_start_ms(clock_->now_ms());

    for (int i = 0; i < count && !stop_; i++) {
        // Each frame takes the next free slot: consecutive slots, unless
        // playing the last one overran its slot.
        std::int64_t now = clock_->now_ms();
        if (start < now) start = next_tx_start_ms(now);

        // Synthesise before waiting so keying starts on time.
        auto audio = synth_frame(plan.frames[i].tones, offset_hz, rate_);

        st.state   = State::Waiting;
        st.frame   = i + 1;
        st.next_ms = start;
        set_status(st);
        if (!clock_->wait_until(start, stop_)) break;

        st.state = State::Keying;
        set_status(st);
        bool played = cb_.play ? cb_.play(audio, plan.frames[i], i, count) : true;
        if (!played || stop_) break;

        start += TX_SLOT_MS;
        completed = (i == count - 1);
    }

    set_status(Status{});
    busy_ = false;
    if (cb_.on_done) cb_.on_done(plan.text, completed);
}

} // namespace x6100::js8
