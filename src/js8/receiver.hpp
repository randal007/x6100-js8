/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive
 */

#pragma once

#include "assembler.hpp"
#include "render.hpp"
#include "resampler.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace js8core {
class Js8Engine;
}

namespace x6100::js8 {

/// Submode bits for Receiver::Config::submodes (js8core SubmodeId order).
enum SubmodeMask : int {
    SUBMODE_NORMAL = 1 << 0,
    SUBMODE_FAST   = 1 << 1,
    SUBMODE_TURBO  = 1 << 2,
    SUBMODE_SLOW   = 1 << 3,
};

/// JS8 receive pipeline for the X6100.
///
/// feed() may be called from the audio thread; it only appends to a buffer.
/// A worker thread resamples to 12 kHz and hands audio to the js8core engine,
/// whose own thread decodes. Callbacks run on those worker threads, never on
/// the caller's thread; UI code must marshal them to its own thread.
class Receiver {
public:
    struct Config {
        int input_rate = 11025;
        int submodes   = SUBMODE_NORMAL;
        /// Re-snap the decoder's 60 s ring to the wall clock when the sample
        /// count and real time disagree by more than this. 0 disables it
        /// (tests feed audio faster than real time).
        int realign_threshold_ms = 150;
    };

    struct Callbacks {
        /// Every decoded frame, rendered (band activity).
        std::function<void(const RxFrame &)> on_frame;
        /// Complete messages after multi-frame assembly.
        std::function<void(const RxFrame &)> on_message;
        /// A decode pass finished with this many unique decodes.
        std::function<void(std::size_t)> on_cycle_done;
        /// Input-rate audio as the worker takes it, for a waterfall.
        std::function<void(const float *, std::size_t)> on_audio;
        /// Engine diagnostics. Very chatty; leave empty in production.
        std::function<void(const std::string &)> on_log;
    };

    Receiver(const Config &config, Callbacks callbacks);
    ~Receiver();

    Receiver(const Receiver &)            = delete;
    Receiver &operator=(const Receiver &) = delete;

    /// Queue audio at Config::input_rate, float in [-1, 1]. Thread-safe.
    void feed(const float *samples, std::size_t n);

    /// Drop partially received multi-frame messages (e.g. after a band change).
    void clear_messages();

    /// How often the ring has been re-snapped to the clock since start.
    unsigned realign_count() const { return realigns_.load(); }

private:
    void worker_loop();
    void submit(const std::vector<float> &audio_12k);
    void check_clock(std::size_t new_samples);

    Config    config_;
    Callbacks cb_;

    std::unique_ptr<js8core::Js8Engine> engine_;
    RationalResampler                   resampler_;

    // feed() -> worker
    std::mutex              in_mutex_;
    std::condition_variable in_cv_;
    std::vector<float>      pending_;
    bool                    stop_ = false;
    std::thread             worker_;

    // Clock tracking (worker thread only)
    bool                  aligned_ = false;
    std::int64_t          align_wall_ms_ = 0;
    std::uint64_t         samples_since_align_ = 0;
    std::atomic<unsigned> realigns_{0};

    // Decode-thread state; assembler also touched by the worker's flush.
    FrameRenderer    renderer_;
    std::mutex       assembler_mutex_;
    MessageAssembler assembler_;
};

std::int64_t wall_ms();

} // namespace x6100::js8
