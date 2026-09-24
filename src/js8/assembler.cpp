/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive
 *
 *  Ported from DecodeViewModel.kt in JS8Call-improved/Android-port.
 */

#include "assembler.hpp"

#include "render.hpp"

#include <cctype>
#include <cmath>
#include <regex>
#include <sstream>

namespace x6100::js8 {

namespace {

std::string trim(const std::string &s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> split_ws(const std::string &s) {
    std::istringstream       in(s);
    std::vector<std::string> out;
    for (std::string t; in >> t;) out.push_back(t);
    return out;
}

std::string upper(std::string s) {
    for (auto &c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

// Data frames split mid-word and carry their own spaces. A directed header
// can meet its payload flush: a buffered command strips the separator before
// packing, so that boundary needs the space put back.
bool needs_space_before(const RxFrame &prev, const RxFrame &next) {
    return !prev.is_data() && !std::isspace((unsigned char)prev.text.back()) &&
           !std::isspace((unsigned char)next.text.front());
}

const std::regex &grid_re() {
    static const std::regex re("^[A-R]{2}[0-9]{2}([A-X]{2})?$");
    return re;
}

const std::regex &directed_command_tail_re() {
    static const std::regex re(
        "^(?:AGN\\?|QSL\\?|HW CPY\\?|MSG TO:|SNR\\?|INFO\\?|GRID\\?|STATUS\\?|QUERY MSGS\\?|HEARING\\?|STATUS|HEARING|"
        "QUERY CALL|QUERY MSGS|QUERY|CMD|MSG|NACK|ACK|73|YES|NO|HEARTBEAT SNR|SNR|QSL|RR|SK|FB|INFO|GRID|DIT DIT|>|\\?)"
        "(?:\\s+[+-]?\\d{1,3})?$");
    return re;
}

const std::regex &directed_placeholder_re() {
    static const std::regex re("^<\\.{4}>:\\s*(.+)$");
    return re;
}

// Kotlin isCallsignLike(): 3-12 chars of [A-Z0-9/] with a letter and a digit.
bool callsign_like_strict(const std::string &token) {
    auto u = upper(trim(token));
    if (u.size() < 3 || u.size() > 12) return false;
    bool letter = false, digit = false;
    for (char c : u) {
        if (std::isalpha((unsigned char)c)) letter = true;
        else if (std::isdigit((unsigned char)c)) digit = true;
        else if (c != '/') return false;
    }
    return letter && digit;
}

bool is_compound_de_helper_frame(const std::string &text) {
    auto parts = split_ws(text);
    if (parts.size() != 2) return false;
    return callsign_like_strict(parts[0]) && std::regex_match(upper(parts[1]), grid_re());
}

bool is_directed_compound_header(const std::string &text) {
    auto tokens = split_ws(text);
    if (tokens.empty() || !callsign_like_strict(tokens[0])) return false;
    if (tokens.size() == 1) return true;
    std::string tail;
    for (std::size_t i = 1; i < tokens.size(); ++i) tail += (i > 1 ? " " : "") + tokens[i];
    return std::regex_match(upper(tail), directed_command_tail_re());
}

// A compound sender's directed message arrives as a "CALL GRID" helper frame
// followed by a "<....>: ..." placeholder frame; fold them into "CALL: ...".
void normalize_compound_directed_helpers(std::vector<RxFrame> &frames) {
    if (frames.size() < 2) return;
    auto &f0 = frames[0];
    auto &f1 = frames[1];
    if (f0.is_data() || f1.is_data()) return;
    if (!f0.is_first() || f0.is_last()) return;
    if (f1.is_first()) return;

    auto first  = trim(f0.text);
    auto second = trim(f1.text);
    if (!is_compound_de_helper_frame(first)) return;

    auto from_call = trim(first.substr(0, first.find(' ')));
    if (from_call.empty()) return;

    std::smatch m;
    if (std::regex_match(second, m, directed_placeholder_re())) {
        auto tail = trim(m[1].str());
        if (!tail.empty()) {
            f0.text = "";
            f1.text = from_call + ": " + tail;
            return;
        }
    }
    if (is_directed_compound_header(second)) {
        f0.text = "";
        f1.text = from_call + ": " + second;
        return;
    }
    // Our renderer already attributes a compound-directed frame to the
    // compound call it heard at this offset ("EA8/K1ABC: G4XYZ"); the helper
    // frame is then redundant. (Not in the Android port, whose output here
    // repeats the sender.)
    if (second.rfind(from_call + ":", 0) == 0) f0.text = "";
}

} // namespace

std::string assemble_multipart_text(const std::vector<RxFrame> &frames) {
    std::vector<const RxFrame *> parts;
    for (auto &f : frames)
        if (!f.text.empty()) parts.push_back(&f);

    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0 && needs_space_before(*parts[i - 1], *parts[i])) out += ' ';
        out += parts[i]->text;
    }
    return out;
}

RxFrame MessageAssembler::assemble(const Buffer &buffer) const {
    auto frames = buffer.frames;
    normalize_compound_directed_helpers(frames);

    // Metadata comes from the most recent frame.
    RxFrame msg = buffer.frames.back();
    msg.text    = assemble_multipart_text(frames);
    return msg;
}

std::optional<int> MessageAssembler::find_key(float freq_hz) const {
    for (auto &[key, buf] : buffers_) {
        if (std::fabs(freq_hz - (float)key) <= FREQ_TOLERANCE_HZ) return key;
    }
    return std::nullopt;
}

void MessageAssembler::add(const RxFrame &frame) {
    flush_stale(frame.timestamp_ms);

    auto match = find_key(frame.freq_hz);

    // Complete single-frame message.
    if (frame.is_first() && frame.is_last()) {
        if (match) buffers_.erase(*match);
        emit_(frame);
        return;
    }

    // First frame: start a fresh buffer at this offset.
    if (frame.is_first()) {
        if (match) buffers_.erase(*match);
        int key       = (int)std::lround(frame.freq_hz);
        buffers_[key] = Buffer{{frame}, frame.timestamp_ms};
        return;
    }

    // Middle or last frame of a buffer we're tracking.
    if (match) {
        int   key = *match;
        auto &buf = buffers_[key];
        buf.frames.push_back(frame);
        if (frame.is_last()) {
            emit_(assemble(buf));
            buffers_.erase(key);
        }
        return;
    }

    // No buffer: probably a missed first frame. Start one anyway.
    int key = (int)std::lround(frame.freq_hz);
    Buffer buf{{frame}, frame.timestamp_ms};
    if (frame.is_last()) {
        emit_(assemble(buf));
        return;
    }
    buffers_[key] = std::move(buf);
}

void MessageAssembler::flush_stale(std::int64_t now_ms) {
    for (auto it = buffers_.begin(); it != buffers_.end();) {
        if (now_ms - it->second.first_timestamp_ms > BUFFER_TIMEOUT_MS) {
            emit_(assemble(it->second));
            it = buffers_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace x6100::js8
