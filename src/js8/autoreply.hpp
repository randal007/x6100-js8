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

#include "inbox.hpp"

namespace x6100::js8 {

/// A decoded message, as the auto-reply logic needs it.
struct Incoming {
    std::string  from, to, text;
    bool         to_me          = false;
    bool         to_group       = false;
    bool         heartbeat      = false; ///< "@HB HEARTBEAT ..." or an ack
    bool         low_confidence = false;
    bool         checksum_ok    = false; ///< a buffered command (MSG) with a valid checksum
    int          snr            = 0;
    std::int64_t when_ms        = 0;
};

struct AutoSettings {
    bool        autoreply = false; ///< AUTO
    bool        heartbeat = false; ///< HB (periodic heartbeats)
    bool        hb_ack    = false; ///< HB ACK (needs AUTO and HB, as on desktop)
    bool        relay     = true;  ///< desktop's "Disable message relay (>)" unticked: relay and hold MSG TO:
    std::string my_call, my_grid, info, status;
    std::vector<std::string> groups; ///< @GROUPs we're in (desktop's My Groups), besides @ALLCALL
    const HeldMessages *held = nullptr; ///< messages held for others (QUERY MSGS / QUERY MSG n, HB acks)
};

/// Query: answer a question (AUTO sends, else offered). HeartbeatAck: needs
/// AUTO, HB and HB ACK. MsgAck: "CALL ACK" for a message kept here (inbox or
/// held for someone) or a relay that ends here, sent again for a resend (they
/// missed our ACK). Relay: a relay passed on to its next station. Stored: an
/// answer about held messages (YES MSG ID n / NO, the message itself,
/// RETRIEVE MSG) or QUERY CALL. Suggest: only ever offered on Reply, e.g.
/// "CALL QUERY MSG 3" when they say they hold a message.
enum class ReplyKind { Query, HeartbeatAck, MsgAck, Suggest, Relay, Stored };

struct AutoReply {
    std::string text; ///< e.g. "N0XYZ SNR -12"
    std::string to;   ///< whose message it answers (the station heard, even when relayed)
    std::string command; ///< e.g. "SNR?", or "HEARTBEAT" for an ack
    ReplyKind   kind = ReplyKind::Query;
    int         deliver_id = 0; ///< a held message this delivers: mark it delivered once sent
    std::string deliver_group_call; ///< a group message: who it's delivered to
    bool        auto_only = false;  ///< desktop answers only with AUTO on (QUERY MSGS, QUERY CALL)
    bool        allcall   = false;  ///< answers an @ALLCALL: AUTO only, once per station per 15 min
};

/// What desktop keeps from a message: "MSG" to us or our group goes to the
/// inbox (with the relay path it came by), "MSG TO:" is held for someone.
struct StoreAction {
    enum class Kind { None, Inbox, Held } kind = Kind::None;
    std::string from, to, path, text;
};

struct Processed {
    std::optional<AutoReply> reply;
    StoreAction              store;
};

/// Desktop JS8Call's processCommandActivity() for one decoded message: what
/// to keep and what to answer, ignoring the switches (AutoPolicy applies
/// those). Answers queries to our call (SNR?, ?, GRID?, INFO?, STATUS?,
/// HEARING?, AGN?), others' heartbeats (an ack with their SNR, plus "MSG ID
/// n [+k]" if we hold messages for them), MSG to us or our groups (inbox,
/// ACK), MSG TO: (held, ACK), QUERY MSGS ("YES MSG ID n [+k]" or "NO"),
/// QUERY MSG n (the message, "NEXT MSG ID" if more), QUERY CALL, and relays:
/// passed on while the text starts with a callsign, else answered with an
/// ACK back along the path, or with the answer to the command they carry.
/// Group messages ("MSG TO:@GROUP") for members of our groups. Suggests
/// QUERY MSG n when someone offers one ("MSG ID n", "RETRIEVE MSG n"), and
/// our SNR for HW CPY?. Nothing for our own traffic, low-confidence decodes,
/// others' traffic, or queries we can't answer (no INFO text, etc.).
/// `heard` is recently heard stations, most recent first.
Processed process(const Incoming &in, const AutoSettings &s, const std::vector<Heard> &heard,
                  const std::string &last_tx);

/// process()'s answer alone.
std::optional<AutoReply> build_reply(const Incoming &in, const AutoSettings &s, const std::vector<Heard> &heard,
                                     const std::string &last_tx);

/// Does `in` start a QSO with us? Any message to our call except a
/// heartbeat ack (those answer our own heartbeat).
bool starts_qso(const Incoming &in);

/// Rate limits and switches for automatic replies.
class AutoPolicy {
public:
    static constexpr std::int64_t QUERY_REPEAT_MS = 5 * 60 * 1000;  ///< same station and command
    static constexpr std::int64_t HB_ACK_REPEAT_MS = 15 * 60 * 1000; ///< desktop's @ALLCALL cache (HB acks too)
    static constexpr std::int64_t IDLE_MS          = 60 * 60 * 1000; ///< desktop's idle watchdog default

    enum class Action { Ignore, Send, Offer };

    /// Send if the switches allow it and the rate limits pass; Offer a query
    /// reply when AUTO is off (desktop puts it in the outgoing box instead),
    /// except what desktop only answers with AUTO on. Only simple queries
    /// wait QUERY_REPEAT_MS for the same station (our own limit); ACKs,
    /// relays and held messages go every time, as on desktop.
    Action decide(const AutoReply &r, const AutoSettings &s, std::int64_t now_ms);

    /// Record that `r` was queued, for the rate limits.
    void sent(const AutoReply &r, std::int64_t now_ms);

    /// Any key, button or knob. Automatic transmissions stop after
    /// IDLE_MS without one.
    void user_activity(std::int64_t now_ms) { last_user_ms_ = now_ms; }
    bool idle(std::int64_t now_ms) const { return now_ms - last_user_ms_ >= IDLE_MS; }

private:
    std::map<std::string, std::int64_t> last_sent_; // "CALL|CMD" or "@ALLCALL|CALL" -> when
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
