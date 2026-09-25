/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive
 *
 *  Multi-frame message assembly. Ported from DecodeViewModel.kt in
 *  JS8Call-improved/Android-port (android/app/.../ui/DecodeViewModel.kt).
 */

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace x6100::js8 {

/// One decoded, rendered frame.
struct RxFrame {
    int         utc     = 0; ///< HHMMSS
    int         snr     = 0;
    float       dt      = 0.0f;
    float       freq_hz = 0.0f;
    std::string text;
    int         type    = 0; ///< FrameBits
    float       quality = 0.0f;
    bool        low_confidence = false; ///< quality below LOW_CONFIDENCE_QUALITY
    int         checksum = 0; ///< buffered-command checksum: 0 none, 1 valid (stripped), -1 invalid
    int         mode    = 0; ///< varicode submode (0 = Normal)
    int         drift_ms = 0;
    std::int64_t timestamp_ms = 0; ///< local receive time

    bool is_first() const { return type & 0b001; }
    bool is_last() const { return type & 0b010; }
    bool is_data() const { return type & 0b100; }
};

/// Joins multi-frame JS8 transmissions back into one message.
///
/// Frames are grouped by speed and audio offset, within the speed's
/// rxThreshold (±10 Hz Normal and Slow, ±16 Fast, ±32 Turbo), as desktop
/// JS8Call matches its message buffers; frames of different speeds never
/// join. A frame flagged both first and last is a complete message by
/// itself. A buffer that never sees its last frame is emitted as-is once
/// 60 s pass without a new frame (desktop closes it then; the time runs from
/// the latest frame, so long messages and Slow's 30 s frames aren't cut).
class MessageAssembler {
public:
    using Emit = std::function<void(const RxFrame &)>;

    static constexpr std::int64_t IDLE_TIMEOUT_MS = 60'000;

    explicit MessageAssembler(Emit emit) : emit_(std::move(emit)) {}

    void add(const RxFrame &frame);

    /// Emit and drop buffers with no new frame for IDLE_TIMEOUT_MS.
    void flush_stale(std::int64_t now_ms);

    void clear() { buffers_.clear(); }

private:
    struct Buffer {
        std::vector<RxFrame> frames;
        std::int64_t         last_timestamp_ms;
    };
    using Key = std::pair<int, int>; ///< (speed's varicode submode, offset in Hz)

    RxFrame            assemble(const Buffer &buffer) const;
    std::optional<Key> find_key(const RxFrame &frame) const;
    static Key         key_for(const RxFrame &frame);

    Emit                  emit_;
    std::map<Key, Buffer> buffers_;
};

/// Drops a frame decoded again in the same slot. The engine retries Turbo
/// every second (as desktop does), so one Turbo frame can decode twice; a
/// repeat would become a stray message. Same speed, same 12-character frame,
/// within the speed's rxThreshold, less than one slot (minus 1 s for decode
/// timing jitter) apart: a duplicate.
class DuplicateFilter {
public:
    /// True if this frame was already seen (and should be dropped).
    bool seen(int mode, const std::string &frame, float freq_hz, std::int64_t now_ms);

private:
    struct Entry {
        int          mode;
        std::string  frame;
        float        freq_hz;
        std::int64_t at_ms;
    };
    std::vector<Entry> recent_;
};

/// Concatenate frame texts, adding a space only at a directed-header/payload
/// boundary where neither side carries one. Exposed for tests.
std::string assemble_multipart_text(const std::vector<RxFrame> &frames);

} // namespace x6100::js8
