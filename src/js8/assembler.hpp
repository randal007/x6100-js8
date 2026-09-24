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
/// Frames are grouped by audio offset (±10 Hz). A frame flagged both first
/// and last is a complete message by itself. A buffer that never sees its
/// last frame is emitted as-is after 90 s, the same as the Android port.
class MessageAssembler {
public:
    using Emit = std::function<void(const RxFrame &)>;

    static constexpr std::int64_t BUFFER_TIMEOUT_MS = 90'000;
    static constexpr float        FREQ_TOLERANCE_HZ = 10.0f;

    explicit MessageAssembler(Emit emit) : emit_(std::move(emit)) {}

    void add(const RxFrame &frame);

    /// Emit and drop buffers older than BUFFER_TIMEOUT_MS.
    void flush_stale(std::int64_t now_ms);

    void clear() { buffers_.clear(); }

private:
    struct Buffer {
        std::vector<RxFrame> frames;
        std::int64_t         first_timestamp_ms;
    };

    RxFrame assemble(const Buffer &buffer) const;
    std::optional<int> find_key(float freq_hz) const;

    Emit                emit_;
    std::map<int, Buffer> buffers_;
};

/// Concatenate frame texts, adding a space only at a directed-header/payload
/// boundary where neither side carries one. Exposed for tests.
std::string assemble_multipart_text(const std::vector<RxFrame> &frames);

} // namespace x6100::js8
