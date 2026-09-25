/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 one-press messages, heartbeats and the
 *  station list; C interface for the dialog.
 */

#include "js8_ops.h"

#include "autoreply.hpp"
#include "classify.hpp"
#include "commands.hpp"
#include "stations.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <random>
#include <vector>

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

/* ---- T4 -------------------------------------------------------------- */

struct js8_auto {
    AutoPolicy policy;
};

namespace {
Incoming to_incoming(const js8_rx_msg_t *m) {
    Incoming in;
    in.from           = m->from;
    in.to             = m->to;
    in.text           = m->text;
    in.to_me          = m->to_me;
    in.to_group       = m->to_group;
    in.heartbeat      = m->heartbeat;
    in.low_confidence = m->low_confidence;
    in.snr            = m->snr;
    return in;
}
} // namespace

extern "C" js8_auto_t *js8_auto_create(void) {
    return new (std::nothrow) js8_auto;
}

extern "C" void js8_auto_destroy(js8_auto_t *a) {
    delete a;
}

extern "C" void js8_auto_consider(js8_auto_t *a, const js8_rx_msg_t *msg, const js8_auto_settings_t *s,
                                  const char *const *heard, unsigned n_heard, const char *last_tx, int64_t now_ms,
                                  js8_auto_result_t *out) {
    if (!out) return;
    *out = js8_auto_result_t{};
    if (!a || !msg || !s || msg->tx) return;

    AutoSettings settings;
    settings.autoreply = s->autoreply;
    settings.heartbeat = s->heartbeat;
    settings.hb_ack    = s->hb_ack;
    settings.my_call   = s->my_call ? s->my_call : "";
    settings.my_grid   = s->my_grid ? s->my_grid : "";
    settings.info      = s->info ? s->info : "";
    settings.status    = s->status ? s->status : "";

    std::vector<std::string> calls;
    for (unsigned i = 0; i < n_heard; i++) calls.emplace_back(heard[i]);

    auto r = build_reply(to_incoming(msg), settings, calls, last_tx ? last_tx : "");
    if (!r) return;
    auto act    = a->policy.decide(*r, settings, now_ms);
    out->action = act == AutoPolicy::Action::Send    ? JS8_AUTO_SEND
                  : act == AutoPolicy::Action::Offer ? JS8_AUTO_OFFER
                                                     : JS8_AUTO_IGNORE;
    out->hb_ack = r->kind == ReplyKind::HeartbeatAck;
    copy_str(out->text, sizeof(out->text), r->text);
    copy_str(out->to, sizeof(out->to), r->to);
    copy_str(out->command, sizeof(out->command), r->command);
}

extern "C" void js8_auto_sent(js8_auto_t *a, const js8_auto_result_t *r, int64_t now_ms) {
    if (!a || !r) return;
    AutoReply reply{r->text, r->to, r->command, r->hb_ack ? ReplyKind::HeartbeatAck : ReplyKind::Query};
    a->policy.sent(reply, now_ms);
}

extern "C" void js8_auto_user_activity(js8_auto_t *a, int64_t now_ms) {
    if (a) a->policy.user_activity(now_ms);
}

extern "C" bool js8_auto_idle(js8_auto_t *a, int64_t now_ms) {
    return a && a->policy.idle(now_ms);
}

extern "C" bool js8_starts_qso(const js8_rx_msg_t *msg) {
    return msg && !msg->tx && starts_qso(to_incoming(msg));
}

extern "C" int64_t js8_next_heartbeat_ms(int64_t now_ms, int interval_min) {
    static std::mt19937 rng{std::random_device{}()};
    return next_heartbeat_ms(now_ms, interval_min, rng);
}

extern "C" bool js8_clock_correction(const float *dt, unsigned n, float *correction_s) {
    if (!dt || !correction_s || n < JS8_SYNC_MIN_DECODES) return false;
    std::vector<float> v(dt, dt + n);
    std::sort(v.begin(), v.end());
    float median   = n % 2 ? v[n / 2] : 0.5f * (v[n / 2 - 1] + v[n / 2]);
    *correction_s  = -median;
    return true;
}

extern "C" bool js8_latlon_to_grid(double lat, double lon, int chars, char *out, unsigned size) {
    if (!out || chars < 2 || chars > 10 || chars % 2 || size < (unsigned)chars + 1) return false;
    if (!(lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)) return false;
    // Each pair subdivides the last: fields 18, squares 10, subsquares 24,
    // then 10 and 24 again. Work in fractions of the whole range.
    static const int base[5] = {18, 10, 24, 10, 24};
    double x = (lon + 180.0) / 360.0, y = (lat + 90.0) / 180.0;
    for (int i = 0; i < chars / 2; i++) {
        x *= base[i];
        y *= base[i];
        int xi = std::min((int)x, base[i] - 1), yi = std::min((int)y, base[i] - 1); // lat 90 / lon 180
        char first = (i % 2) ? '0' : 'A';
        out[2 * i]     = (char)(first + xi);
        out[2 * i + 1] = (char)(first + yi);
        x -= xi;
        y -= yi;
    }
    out[chars] = '\0';
    return true;
}
