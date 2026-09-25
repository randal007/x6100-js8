/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 station list (desktop JS8Call's Call Activity)
 */

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace x6100::js8 {

struct Station {
    std::string  call;
    std::string  grid;           ///< latest heard (heartbeat, CQ or GRID)
    std::int64_t heard_ms = 0;   ///< last decoded, wall clock ms
    int          snr      = 0;   ///< how we hear them (last decode)
    float        freq_hz  = 0;   ///< their last audio offset
    int          mode     = 0;   ///< speed last heard at (varicode submode 0/1/2/4)
    bool         heard_me = false;       ///< they have sent us something
    std::int64_t heard_me_ms = 0;
    std::optional<int> reported_snr;     ///< how they hear us, if they said
};

/// One decoded message, as the station list needs it.
struct StationEvent {
    std::string  from, to, text;
    bool         to_me = false;
    int          snr   = 0;
    float        freq_hz = 0;
    int          mode    = 0;    ///< speed (varicode submode 0/1/2/4)
    std::int64_t when_ms = 0;
};

/// Stations heard, with those that heard us first, like desktop JS8Call's
/// Call Activity with its ★ ("Hearing Your Station").
class StationList {
public:
    static constexpr std::int64_t EXPIRE_MS = 60 * 60 * 1000;

    void add(const StationEvent &ev, const std::string &my_call);

    /// Stations heard within EXPIRE_MS: those that heard us first (most
    /// recent first), then the rest by most recently heard.
    std::vector<Station> sorted(std::int64_t now_ms) const;

    void clear() { stations_.clear(); }

private:
    std::map<std::string, Station> stations_;
};

} // namespace x6100::js8
