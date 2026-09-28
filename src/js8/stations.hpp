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
    std::string  via;            ///< only heard through this relay station (desktop's "through")
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

    /// `call` named in a relay that reached us through `via` (desktop puts
    /// a relay path's stations in its Call Activity). A station heard
    /// directly within `expire_ms` keeps what we heard of it.
    void add_via(const std::string &call, const std::string &via, float freq_hz, int mode, std::int64_t when_ms,
                 std::int64_t expire_ms = EXPIRE_MS);

    /// Stations heard within `expire_ms` (0: all): those that heard us
    /// first (most recent first), then the rest by most recently heard.
    std::vector<Station> sorted(std::int64_t now_ms, std::int64_t expire_ms = EXPIRE_MS) const;

    void clear() { stations_.clear(); }

private:
    std::map<std::string, Station> stations_;
};

} // namespace x6100::js8
