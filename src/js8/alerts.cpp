/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 alerts
 */

#include "alerts.hpp"

#include "classify.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace x6100::js8 {

namespace {

constexpr std::size_t MAX_WORDS = 20;

std::vector<std::string> split(const std::string &s, const char *seps) {
    std::vector<std::string> out;
    std::string              cur;
    for (char c : s) {
        if (std::strchr(seps, c)) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += (char)std::toupper((unsigned char)c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

} // namespace

std::vector<std::string> parse_alert_words(const std::string &typed) {
    std::vector<std::string> out;
    for (auto &w : split(typed, ", \t")) {
        if (std::find(out.begin(), out.end(), w) != out.end()) continue;
        out.push_back(w);
        if (out.size() == MAX_WORDS) break;
    }
    return out;
}

std::string format_alert_words(const std::vector<std::string> &words) {
    std::string out;
    for (auto &w : words) {
        if (!out.empty()) out += ' ';
        out += w;
    }
    return out;
}

std::string alert_word_hit(const std::string &text, const std::string &from, const std::vector<std::string> &words) {
    if (words.empty()) return "";
    auto tokens = split(text, ":> \t");
    for (auto &w : words) {
        if (std::find(tokens.begin(), tokens.end(), w) != tokens.end()) return w;
        bool call = std::any_of(w.begin(), w.end(), ::isdigit) && w[0] != '@';
        if (call && !from.empty() && base_callsign(from) == base_callsign(w)) return w;
    }
    return "";
}

} // namespace x6100::js8
