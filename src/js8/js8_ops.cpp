/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 one-press messages, heartbeats and the
 *  station list; C interface for the dialog.
 */

#include "js8_ops.h"

#include "commands.hpp"
#include "stations.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <random>

using namespace x6100::js8;

namespace {

void copy_str(char *dst, std::size_t cap, const std::string &src) {
    if (!dst || cap == 0) return;
    std::size_t n = std::min(cap - 1, src.size());
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

const char *const LABELS[JS8_Q_COUNT] = {
    "SNR?", "Send SNR", "GRID?", "My grid", "INFO?", "STATUS?", "HEARING?", "AGN?", "RR", "73",
};

} // namespace

struct js8_stations {
    StationList list;
};

extern "C" const char *js8_query_label(js8_query_t q) {
    return (unsigned)q < JS8_Q_COUNT ? LABELS[q] : "";
}

extern "C" bool js8_query_text(js8_query_t q, const char *to_call, int their_snr, const char *my_grid, char *out,
                               unsigned out_len) {
    if ((unsigned)q >= JS8_Q_COUNT) return false;
    auto text = query_text((Query)q, to_call ? to_call : "", their_snr, my_grid ? my_grid : "");
    copy_str(out, out_len, text);
    return !text.empty();
}

extern "C" void js8_heartbeat_text(const char *my_call, const char *my_grid, char *out, unsigned out_len) {
    copy_str(out, out_len, heartbeat_text(my_call ? my_call : "", my_grid ? my_grid : ""));
}

extern "C" int js8_heartbeat_offset(const float *offsets_hz, const int64_t *heard_ms, unsigned n, int64_t now_ms) {
    static std::mt19937          rng{std::random_device{}()};
    std::vector<OffsetActivity> activity;
    for (unsigned i = 0; i < n; i++) activity.push_back({offsets_hz[i], heard_ms[i]});
    return find_free_offset(activity, now_ms, rng);
}

extern "C" js8_stations_t *js8_stations_create(void) {
    return new (std::nothrow) js8_stations;
}

extern "C" void js8_stations_destroy(js8_stations_t *s) {
    delete s;
}

extern "C" void js8_stations_add(js8_stations_t *s, const js8_rx_msg_t *m, const char *my_call, int64_t now_ms) {
    if (!s || !m || m->tx) return;
    StationEvent ev;
    ev.from    = m->from;
    ev.to      = m->to;
    ev.text    = m->text;
    ev.to_me   = m->to_me;
    ev.snr     = m->snr;
    ev.freq_hz = m->freq_hz;
    ev.when_ms = now_ms;
    s->list.add(ev, my_call ? my_call : "");
}

extern "C" int js8_stations_list(js8_stations_t *s, int64_t now_ms, js8_station_t *out, int max) {
    if (!s || !out || max <= 0) return 0;
    auto list = s->list.sorted(now_ms);
    int  n    = std::min<int>(max, (int)list.size());
    for (int i = 0; i < n; i++) {
        const auto &st = list[i];
        js8_station_t &o = out[i];
        o = js8_station_t{};
        copy_str(o.call, sizeof(o.call), st.call);
        copy_str(o.grid, sizeof(o.grid), st.grid);
        o.heard_ms         = st.heard_ms;
        o.snr              = (int16_t)st.snr;
        o.freq_hz          = st.freq_hz;
        o.heard_me         = st.heard_me;
        o.heard_me_ms      = st.heard_me_ms;
        o.has_reported_snr = st.reported_snr.has_value();
        o.reported_snr     = (int16_t)st.reported_snr.value_or(0);
    }
    return n;
}

extern "C" void js8_stations_clear(js8_stations_t *s) {
    if (s) s->list.clear();
}
