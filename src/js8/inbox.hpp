/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 inbox
 *
 *  Desktop JS8Call keeps "CALL MSG text" messages sent to you in an inbox
 *  and answers them with "CALL ACK". Here the inbox is a plain text file on
 *  the SD card's DATA partition, one message per line, readable on a PC.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace x6100::js8 {

struct InboxMessage {
    int          id     = 0;
    std::int64_t utc_ms = 0;
    std::string  from, text;
    bool         read = false;
};

class Inbox {
public:
    static constexpr std::size_t  MAX_MESSAGES = 200;          ///< oldest go first
    static constexpr std::int64_t REPEAT_MS    = 30 * 60 * 1000; ///< same message again: a resend

    /// A missing file is an empty inbox (true); an unreadable one is false.
    bool load(const std::string &path);
    /// Write via a temporary file and rename, so a power cut keeps the old one.
    bool save(const std::string &path) const;

    /// Returns the id. The same text from the same station within REPEAT_MS
    /// is a resend (they didn't get our ACK): the existing id, not a copy.
    int add(const std::string &from, const std::string &text, std::int64_t utc_ms);

    /// Newest first.
    std::vector<InboxMessage> list() const;
    std::optional<InboxMessage> get(int id) const;
    bool mark_read(int id);
    bool remove(int id);
    int  unread() const;
    std::size_t size() const { return msgs_.size(); }

private:
    std::vector<InboxMessage> msgs_; ///< oldest first
    int                       next_id_ = 1;
};

/// A message held here for another station, as desktop JS8Call stores
/// "MSG TO:" messages ("STORE") until that station asks with QUERY MSGS /
/// QUERY MSG n (then "DELIVERED"). One per line in a text file, like the inbox.
struct HeldMessage {
    int          id     = 0;
    std::int64_t utc_ms = 0;
    std::string  from, to, text; ///< `to` is a base call, as desktop stores it
    bool         delivered = false;
};

class HeldMessages {
public:
    static constexpr std::size_t MAX_MESSAGES = 100;

    bool load(const std::string &path);
    bool save(const std::string &path) const;

    /// Hold `text` from `from` for `to` (stored as its base call). A resend
    /// within Inbox::REPEAT_MS returns the first one's id.
    int add(const std::string &from, const std::string &to, const std::string &text, std::int64_t utc_ms);

    /// The oldest undelivered message for `call` (or its base call), as
    /// desktop's getNextMessageIdForCallsign().
    std::optional<int> next_for(const std::string &call) const;
    std::optional<HeldMessage> get(int id) const;
    /// Is message `id` for `call`? (desktop: TO equals the caller or its base)
    bool is_for(int id, const std::string &call) const;
    bool mark_delivered(int id);
    bool remove(int id);
    std::vector<HeldMessage> list() const; ///< newest first
    int  waiting() const;                  ///< not yet delivered
    std::size_t size() const { return msgs_.size(); }

private:
    std::vector<HeldMessage> msgs_; ///< oldest first
    int                      next_id_ = 1;
};

/// "FROM: MYCALL MSG TO:W1ABC HELLO" (or "MSG TO: W1ABC HELLO") to my_call:
/// {"W1ABC", "HELLO"}.
std::optional<std::pair<std::string, std::string>> msg_to_body(const std::string &text, const std::string &my_call);

/// "FROM: MYCALL QUERY MSG 3": 3.
std::optional<int> query_msg_id(const std::string &text);

/// The message in "FROM: MYCALL MSG HELLO THERE" when it's to my_call
/// (or its base call): "HELLO THERE". Not "MSG TO:" (stored for others).
std::optional<std::string> msg_body(const std::string &text, const std::string &my_call);

/// "N0XYZ: K2XYZ YES MSG ID 3" or a heartbeat ack ending "MSG ID 3": they
/// hold message 3 for us.
std::optional<int> msg_id_offered(const std::string &text);

} // namespace x6100::js8
