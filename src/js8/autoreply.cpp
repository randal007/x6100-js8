/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 auto-reply, heartbeat acks and heartbeat timing
 */

#include "autoreply.hpp"

#include "classify.hpp"
#include "commands.hpp"

#include <cctype>
#include <sstream>

namespace x6100::js8 {

namespace {

std::string upper(std::string s) {
    for (auto &c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}

std::string trim(const std::string &s) {
    auto b = s.find_first_not_of(' ');
    if (b == std::string::npos) return "";
    return s.substr(b, s.find_last_not_of(' ') - b + 1);
}

// The command after "FROM: TO", e.g. "SNR?" in "N0XYZ: K2XYZ SNR?".
std::string command_after_target(const std::string &text, const std::string &to) {
    auto colon = text.find(": ");
    if (colon == std::string::npos) return "";
    std::string rest = text.substr(colon + 2);
    if (rest.rfind(to, 0) != 0) return "";
    return upper(trim(rest.substr(to.size())));
}

} // namespace

bool starts_qso(const Incoming &in) {
    if (!in.to_me) return false;
    return in.text.find(" HEARTBEAT SNR") == std::string::npos;
}

std::optional<AutoReply> build_reply(const Incoming &in, const AutoSettings &s, const std::vector<std::string> &heard,
                                     const std::string &last_tx) {
    if (s.my_call.empty() || in.from.empty() || in.low_confidence) return std::nullopt;
    if (base_callsign(in.from) == base_callsign(s.my_call)) return std::nullopt;

    // Someone else's heartbeat: acknowledge with how we hear them.
    if (in.heartbeat && !in.to_me && in.text.find(" HEARTBEAT SNR") == std::string::npos &&
        in.text.find("@HB HEARTBEAT") != std::string::npos) {
        auto text = query_text(Query::SendSnr, in.from, in.snr, "");
        if (text.empty()) return std::nullopt;
        // Desktop: "%1 HEARTBEAT SNR %2", i.e. "CALL HEARTBEAT SNR -08".
        text.replace(text.find(" SNR "), 5, " HEARTBEAT SNR ");
        return AutoReply{text, in.from, "HEARTBEAT", ReplyKind::HeartbeatAck};
    }

    // Queries to our call only: never to @ALLCALL or other groups.
    if (!in.to_me || in.to_group) return std::nullopt;

    const std::string cmd = command_after_target(in.text, in.to);
    std::string       text;
    if (cmd == "SNR?" || cmd == "?") {
        text = query_text(Query::SendSnr, in.from, in.snr, "");
    } else if (cmd == "GRID?") {
        text = query_text(Query::MyGrid, in.from, 0, s.my_grid);
    } else if (cmd == "INFO?") {
        if (!trim(s.info).empty()) text = upper(in.from) + " INFO " + upper(trim(s.info));
    } else if (cmd == "STATUS?") {
        if (!trim(s.status).empty()) text = upper(in.from) + " STATUS " + upper(trim(s.status));
    } else if (cmd == "HEARING?") {
        // Up to 4 recently heard stations other than the one asking.
        std::string list;
        int         n = 0;
        for (auto &call : heard) {
            if (n >= 4) break;
            if (base_callsign(call) == base_callsign(in.from)) continue;
            if (base_callsign(call) == base_callsign(s.my_call)) continue;
            list += " " + call;
            n++;
        }
        if (n > 0) text = upper(in.from) + " HEARING" + list;
    } else if (cmd == "AGN?") {
        text = trim(last_tx);
    }
    if (text.empty()) return std::nullopt;
    return AutoReply{text, in.from, cmd == "?" ? "SNR?" : cmd, ReplyKind::Query};
}

AutoPolicy::Action AutoPolicy::decide(const AutoReply &r, const AutoSettings &s, std::int64_t now_ms) {
    const bool auto_ok = s.autoreply && !idle(now_ms);

    if (r.kind == ReplyKind::HeartbeatAck) {
        // Desktop: HB ACK only with AUTO and heartbeat networking both on.
        if (!(auto_ok && s.heartbeat && s.hb_ack)) return Action::Ignore;
    } else if (!auto_ok) {
        return Action::Offer;
    }

    auto key = r.to + "|" + r.command;
    auto it  = last_sent_.find(key);
    auto gap = r.kind == ReplyKind::HeartbeatAck ? HB_ACK_REPEAT_MS : QUERY_REPEAT_MS;
    if (it != last_sent_.end() && now_ms - it->second < gap) return Action::Ignore;
    return Action::Send;
}

void AutoPolicy::sent(const AutoReply &r, std::int64_t now_ms) {
    last_sent_[r.to + "|" + r.command] = now_ms;
}

std::int64_t next_heartbeat_ms(std::int64_t now_ms, int interval_min, std::mt19937 &rng) {
    if (interval_min < HB_MIN_INTERVAL) interval_min = HB_MIN_INTERVAL;
    if (interval_min > HB_MAX_INTERVAL) interval_min = HB_MAX_INTERVAL;

    std::int64_t secs = now_ms / 1000;
    std::int64_t up   = (secs + 14) / 15 * 15; // round up to a 15 s boundary
    std::int64_t next = up + 1 + interval_min * 60;
    if (std::uniform_real_distribution<float>(0, 1)(rng) < 0.25f) next += 15;
    return next * 1000;
}

} // namespace x6100::js8
