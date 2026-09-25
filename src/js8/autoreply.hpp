/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 auto-reply, heartbeat acks and heartbeat timing
 *
 *  Behaviour follows desktop JS8Call's MainWindow::processCommandActivity()
 *  and scheduleHeartbeat(). The switches (AUTO, HB, HB ACK) are the user's,
 *  all off by default.
 */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace x6100::js8 {

/// A decoded message, as the auto-reply logic needs it.
struct Incoming {
    std::string  from, to, text;
    bool         to_me          = false;
    bool         to_group       = false;
    bool         heartbeat      = false; ///< "@HB HEARTBEAT ..." or an ack
    bool         low_confidence = false;
    int          snr            = 0;
    std::int64_t when_ms        = 0;
};

struct AutoSettings {
    bool        autoreply = false; ///< AUTO
    bool        heartbeat = false; ///< HB (periodic heartbeats)
    bool        hb_ack    = false; ///< HB ACK (needs AUTO and HB, as on desktop)
    std::string my_call, my_grid, info, status;
};

enum class ReplyKind { Query, HeartbeatAck };

struct AutoReply {
    std::string text; ///< e.g. "N0XYZ SNR -12"
    std::string to;
    std::string command; ///< e.g. "SNR?", or "HEARTBEAT" for an ack
    ReplyKind   kind = ReplyKind::Query;
};

/// What desktop JS8Call would answer to `in`, ignoring the switches: queries
/// to our call (SNR?, ?, GRID?, INFO?, STATUS?, HEARING?, AGN?) and others'
/// heartbeats (an ack with their SNR). Nothing for groups, our own traffic,
/// low-confidence decodes, or queries we can't answer (no INFO text, etc.).
/// `heard` is recently heard calls, most recent first (for HEARING?).
std::optional<AutoReply> build_reply(const Incoming &in, const AutoSettings &s, const std::vector<std::string> &heard,
                                     const std::string &last_tx);

/// Does `in` start a QSO with us? Any message to our call except a
/// heartbeat ack (those answer our own heartbeat).
bool starts_qso(const Incoming &in);

/// Rate limits and switches for automatic replies.
class AutoPolicy {
public:
    static constexpr std::int64_t QUERY_REPEAT_MS = 5 * 60 * 1000;  ///< same station and command
    static constexpr std::int64_t HB_ACK_REPEAT_MS = 15 * 60 * 1000; ///< desktop's @ALLCALL cache
    static constexpr std::int64_t IDLE_MS          = 60 * 60 * 1000; ///< desktop's idle watchdog default

    enum class Action { Ignore, Send, Offer };

    /// Send if the switches allow it and the rate limits pass; Offer a query
    /// reply when AUTO is off (desktop puts it in the outgoing box instead).
    Action decide(const AutoReply &r, const AutoSettings &s, std::int64_t now_ms);

    /// Record that `r` was queued, for the rate limits.
    void sent(const AutoReply &r, std::int64_t now_ms);

    /// Any key, button or knob. Automatic transmissions stop after
    /// IDLE_MS without one.
    void user_activity(std::int64_t now_ms) { last_user_ms_ = now_ms; }
    bool idle(std::int64_t now_ms) const { return now_ms - last_user_ms_ >= IDLE_MS; }

private:
    std::map<std::string, std::int64_t> last_sent_; // "CALL|CMD" -> when
    std::int64_t                        last_user_ms_ = 0;
};

/// When the next heartbeat is due, as desktop's scheduleHeartbeat() computes
/// it: the next 15 s boundary + 1 s + `interval_min`, and 25 % of the time
/// one slot later so stations don't stay in step.
std::int64_t next_heartbeat_ms(std::int64_t now_ms, int interval_min, std::mt19937 &rng);

constexpr int HB_MIN_INTERVAL     = 5;
constexpr int HB_MAX_INTERVAL     = 30;
constexpr int HB_DEFAULT_INTERVAL = 30;

} // namespace x6100::js8
