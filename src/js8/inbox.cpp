/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 inbox
 */

#include "inbox.hpp"

#include "classify.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace x6100::js8 {

namespace {

// File format, one message per line (tabs never appear in JS8 text):
//   id <TAB> utc_ms <TAB> U|R <TAB> from <TAB> text
const char *const HEADER = "# X6100 JS8 inbox: id, UTC ms, U(nread)/R(ead), from, message\n";

std::string one_line(const std::string &s) {
    std::string out = s;
    for (char &c : out)
        if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return out;
}

std::vector<std::string> words(const std::string &s) {
    std::istringstream       in(s);
    std::vector<std::string> out;
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

} // namespace

bool Inbox::load(const std::string &path) {
    msgs_.clear();
    next_id_ = 1;
    std::ifstream f(path);
    if (!f) return access(path.c_str(), F_OK) != 0; // missing: empty inbox
    for (std::string line; std::getline(f, line);) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> field;
        std::size_t              start = 0;
        for (int i = 0; i < 4; i++) {
            auto tab = line.find('\t', start);
            if (tab == std::string::npos) break;
            field.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
        if (field.size() != 4) continue;
        InboxMessage m;
        try {
            m.id     = std::stoi(field[0]);
            m.utc_ms = std::stoll(field[1]);
        } catch (...) {
            continue;
        }
        m.read = field[2] == "R";
        m.from = field[3];
        m.text = line.substr(start);
        if (!m.text.empty() && m.text.back() == '\r') m.text.pop_back();
        next_id_ = std::max(next_id_, m.id + 1);
        msgs_.push_back(m);
    }
    return true;
}

bool Inbox::save(const std::string &path) const {
    std::string tmp = path + ".tmp";
    FILE       *f   = std::fopen(tmp.c_str(), "w");
    if (!f) return false;
    bool ok = std::fputs(HEADER, f) >= 0;
    for (auto &m : msgs_) {
        ok = ok && std::fprintf(f, "%d\t%lld\t%s\t%s\t%s\n", m.id, (long long)m.utc_ms, m.read ? "R" : "U",
                                one_line(m.from).c_str(), one_line(m.text).c_str()) >= 0;
    }
    ok = ok && std::fflush(f) == 0;
    fsync(fileno(f));
    ok = std::fclose(f) == 0 && ok;
    if (!ok || std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

int Inbox::add(const std::string &from, const std::string &text, std::int64_t utc_ms) {
    for (auto &m : msgs_)
        if (m.from == from && m.text == text && utc_ms - m.utc_ms < REPEAT_MS) return m.id;

    InboxMessage m;
    m.id     = next_id_++;
    m.utc_ms = utc_ms;
    m.from   = one_line(from);
    m.text   = one_line(text);
    msgs_.push_back(m);

    // Full: drop the oldest read message, else the oldest.
    while (msgs_.size() > MAX_MESSAGES) {
        auto it = std::find_if(msgs_.begin(), msgs_.end(), [](const InboxMessage &x) { return x.read; });
        msgs_.erase(it != msgs_.end() ? it : msgs_.begin());
    }
    return m.id;
}

std::vector<InboxMessage> Inbox::list() const {
    return std::vector<InboxMessage>(msgs_.rbegin(), msgs_.rend());
}

std::optional<InboxMessage> Inbox::get(int id) const {
    for (auto &m : msgs_)
        if (m.id == id) return m;
    return std::nullopt;
}

bool Inbox::mark_read(int id) {
    for (auto &m : msgs_)
        if (m.id == id) {
            bool changed = !m.read;
            m.read       = true;
            return changed;
        }
    return false;
}

bool Inbox::remove(int id) {
    auto it = std::find_if(msgs_.begin(), msgs_.end(), [id](const InboxMessage &m) { return m.id == id; });
    if (it == msgs_.end()) return false;
    msgs_.erase(it);
    return true;
}

int Inbox::unread() const {
    return (int)std::count_if(msgs_.begin(), msgs_.end(), [](const InboxMessage &m) { return !m.read; });
}

std::optional<std::string> msg_body(const std::string &text, const std::string &my_call) {
    if (my_call.empty()) return std::nullopt;
    auto colon = text.find(':');
    if (colon == std::string::npos) return std::nullopt;
    std::string rest = text.substr(colon + 1);
    auto        w    = words(rest);
    if (w.size() < 3 || base_callsign(w[0]) != base_callsign(my_call) || w[1] != "MSG") return std::nullopt;
    if (w[2].rfind("TO:", 0) == 0) return std::nullopt;

    // Everything after "MSG ", spacing kept.
    auto pos = rest.find(" MSG ");
    if (pos == std::string::npos) return std::nullopt;
    std::string body = rest.substr(pos + 5);
    while (!body.empty() && body.front() == ' ') body.erase(body.begin());
    while (!body.empty() && body.back() == ' ') body.pop_back();
    if (body.empty()) return std::nullopt;
    return body;
}

std::optional<int> msg_id_offered(const std::string &text) {
    auto w = words(text);
    for (std::size_t i = 0; i + 2 < w.size(); i++) {
        if (w[i] != "MSG" || w[i + 1] != "ID") continue;
        const auto &n = w[i + 2];
        if (n.empty() || n.size() > 6 || !std::all_of(n.begin(), n.end(), ::isdigit)) continue;
        return std::stoi(n);
    }
    return std::nullopt;
}

} // namespace x6100::js8
