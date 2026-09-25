/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 (receive)
 *
 *  Layout follows the FT8 app: a waterfall strip across the top and a
 *  message list over the rest of it. Decoding lives in src/js8 (no LVGL);
 *  its callbacks arrive on worker threads and are marshalled here with
 *  scheduler_put().
 */

#include "dialog_js8.h"

#include "js8/js8_rx.h"
#include "js8/js8_tx.h"
#include "js8/js8_ops.h"
#include "qth/qth.h"

#include "audio.h"
#include "buttons.h"
#include "cfg/cfg_api.h"
#include "cfg/digital_modes.h"
#include "dialog.h"
#include "dsp.h"
#include "events.h"
#include "keyboard.h"
#include "main_screen.h"
#include "msg.h"
#include "params/params.h"
#include "radio.h"
#include "scheduler.h"
#include "styles.h"
#include "textarea_window.h"
#include "tx_player.h"
#include "util.h"
#include "widgets/lv_finder.h"
#include "widgets/lv_waterfall.h"

#include <liquid/liquid.h>

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SAMPLE_RATE      (AUDIO_CAPTURE_RATE / AUDIO_DECIM)
#define WIDTH            771
#define WF_HEIGHT        325
#define WF_VISIBLE       55     /* waterfall rows left uncovered by the list */
#define TX_BAR_H         30     /* TX status line between waterfall and list */
#define TX_TEXT_MAX      160    /* longest message the compose window takes */
#define OFFER_MS         (5 * 60 * 1000) /* how long an offered reply stays on Reply */
#define TEXT_MAX         64     /* INFO / STATUS */
#ifndef JS8_TEXTS_PATH
#define JS8_TEXTS_PATH   "/mnt/js8_texts.txt" /* DATA partition; editable on a PC */
#endif
#define JS8_WIDTH_HZ     50     /* 8 tones x 6.25 Hz, JS8 Normal */
#define JS8_SLOT_SEC     15.0f
#define WF_ROWS_PER_SEC  10     /* waterfall rows per second of audio */
#define WF_ROW_SAMPLES   (SAMPLE_RATE / WF_ROWS_PER_SEC)
#define WF_QUEUE         16     /* rows waiting to be drawn (jitter buffer) */
#define HB_ADJUST_MS     8000   /* setting the HB interval ends after this idle */
/* The waterfall is drawn relative to the noise floor, so it works at any
 * audio level: WF_MIN_DB..WF_MAX_DB above the floor spans the palette. */
#define WF_MIN_DB        0
#define WF_MAX_DB        30
#define TEST_WAV         "/mnt/js8_test.wav"  /* DATA partition, as NavTex's test file */

#define HISTORY          300    /* messages kept for re-filtering */
#define MAX_ROWS         200    /* rows shown before trimming to KEEP_ROWS */
#define KEEP_ROWS        150

typedef enum {
    SHOW_ALL,       /* everything, heartbeats included */
    SHOW_NO_HB,     /* everything except heartbeats */
    SHOW_DIRECTED,  /* to me, or to a group (not HB/CQ) */
    SHOW_COUNT,
} show_t;

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void audio_cb(unsigned int n, float *samples);
static void key_cb(lv_event_t *e);

static const char *show_label_getter(void);
static void        show_cb(button_data_t *btn);
static void        clear_cb(button_data_t *btn);
static void        time_sync_cb(button_data_t *btn);
static const char *test_wav_label_getter(void);
static void        test_wav_cb(button_data_t *btn);
static void        rotary_cb(int32_t diff);
static void        reply_cb(button_data_t *btn);
static void        send_cb(button_data_t *btn);
static void        stop_tx_cb(button_data_t *btn);
static void        cq_cb(button_data_t *btn);
static void        heartbeat_cb(button_data_t *btn);
static void        query_cb(button_data_t *btn);
static const char *hold_label_getter(void);
static void        hold_cb(button_data_t *btn);
static const char *stations_label_getter(void);
static void        stations_cb(button_data_t *btn);
static void        query_close(void);
static const char *auto_label_getter(void);
static void        auto_cb(button_data_t *btn);
static const char *hb_label_getter(void);
static void        hb_cb(button_data_t *btn);
static void        hb_hold_cb(button_data_t *btn);
static const char *hb_ack_label_getter(void);
static void        hb_ack_cb(button_data_t *btn);
static void        texts_cb(button_data_t *btn);
static bool        tx_queue_at(const char *text, int offset_hz, bool automatic);
static void        user_touch(void);
static void        auto_send(const js8_auto_result_t *r);
static void        hb_tick(void);
static void        load_texts(void);
static void        save_texts(void);
static void        compose_open(const char *prefill);
static void        update_tx_bar(void);
static void        tx_start(void);
static void        tx_stop_all(void);
static void        tx_timer_cb(lv_timer_t *t);
static void        compose_close(void);
static void        hb_adjust_end(void);

/* ---- State (UI thread unless noted) ------------------------------------ */

static js8_rx_t *rx;
static js8_tx_t *tx;

static lv_obj_t   *tx_bar;
static lv_timer_t *tx_timer;           /* refreshes the TX bar countdown */
static js8_tx_status_t tx_status;      /* UI-thread copy of the last status */
static char        tx_preview[JS8_RX_TEXT_LEN]; /* what we're sending, as others see it */
static float       base_gain_offset;
static atomic_bool keyed;              /* a frame is on the air (TX thread) */
static atomic_int  tx_offset_active;   /* offset of the message being sent */
static bool        composing;          /* compose window open */

static js8_stations_t *stations;       /* who we've heard, who heard us */
static bool           view_stations;   /* list shows stations, not messages */
static js8_station_t  st_rows[MAX_ROWS];
static int            st_count;
static lv_obj_t      *query_list;      /* Query popup, when open */

/* T4: auto-reply and heartbeats. The switches live in params (js8_auto,
 * js8_hb, js8_hb_ack, js8_hb_interval), all off by default. */
static js8_auto_t *autop;
static int64_t     hb_next_ms;       /* 0: send the first one at the next chance */
static bool        hb_adjusting;     /* main knob sets the HB interval */
static int64_t     hb_adjust_ms;     /* last knob turn while adjusting */
static char        info_text[TEXT_MAX + 1], status_text[TEXT_MAX + 1];
static char        last_tx_text[JS8_RX_TEXT_LEN]; /* for AGN? */
static struct {
    char    call[JS8_RX_CALL_LEN];
    char    text[JS8_RX_TEXT_LEN];
    int64_t ms;
} offer; /* AUTO off: a reply waiting for Reply, like desktop's outgoing box */
static js8_auto_result_t pending_auto;       /* arrived while TX was busy */
static bool              pending_auto_valid;
static int               edit_target;        /* 0 compose, 1 INFO, 2 STATUS */
static lv_obj_t         *texts_list;

static lv_obj_t *waterfall;
static lv_obj_t *finder;
static lv_obj_t *table;
static lv_obj_t *status;

static int32_t filter_low, filter_high;
static show_t  show = SHOW_NO_HB;

/* Message history, a ring, so the list can be rebuilt when the filter
 * changes. row_hist[] maps a table row to its history slot (-1 = info row). */
static js8_rx_msg_t history[HISTORY];
static uint16_t     hist_head;  /* next slot to write */
static uint16_t     hist_count;
static int16_t      row_hist[MAX_ROWS + 1];
static uint16_t     rows;

static unsigned cycles, cycle_decodes;
static bool     test_wav_shown; /* label state of btn_test_wav */

/* Waterfall PSD, touched only on the receiver's worker thread. */
static spgramf  sg;
static float   *psd;
static uint16_t nfft;
static unsigned wf_row_fill;   /* samples in the row being built (receiver thread) */

/* Rows are made per fixed amount of audio, but audio arrives in bursts; a
 * timer draws them at an even pace so the waterfall scrolls smoothly. */
static float     *wf_queue[WF_QUEUE];
static uint16_t   wf_queue_size[WF_QUEUE];
static unsigned   wf_q_head, wf_q_count;
static lv_timer_t *wf_timer;
static float    wf_floor_db;   /* smoothed noise floor (receiver thread) */
static bool     wf_floor_set;

/* ---- Buttons ---------------------------------------------------------- */

static buttons_page_t page_1;
static buttons_page_t page_2;
static buttons_page_t page_3;
static buttons_page_t page_4;

static button_data_t btn_p1      = {.type = BTN_TEXT, .label = "(JS8 1:4)", .press = button_next_page_cb, .next = &page_2};
static button_data_t btn_show    = {.type = BTN_TEXT_FN, .label_fn = show_label_getter, .press = show_cb};
static button_data_t btn_reply   = {.type = BTN_TEXT, .label = "Reply", .press = reply_cb};
static button_data_t btn_send    = {.type = BTN_TEXT, .label = "Send...", .press = send_cb};
static button_data_t btn_stop_tx = {.type = BTN_TEXT, .label = "Stop TX", .press = stop_tx_cb};

static button_data_t btn_p2    = {.type = BTN_TEXT, .label = "(JS8 2:4)", .press = button_next_page_cb, .next = &page_3};
static button_data_t btn_cq    = {.type = BTN_TEXT, .label = "CQ", .press = cq_cb};
static button_data_t btn_hb    = {.type = BTN_TEXT, .label = "Heart-\nbeat", .press = heartbeat_cb};
static button_data_t btn_query = {.type = BTN_TEXT, .label = "Query >", .press = query_cb};
static button_data_t btn_clear = {.type = BTN_TEXT, .label = "Clear", .press = clear_cb};

static button_data_t btn_p3        = {.type = BTN_TEXT, .label = "(JS8 3:4)", .press = button_next_page_cb, .next = &page_4};
static button_data_t btn_time_sync = {.type = BTN_TEXT, .label = "Time\nSync", .press = time_sync_cb};
static button_data_t btn_test_wav  = {.type = BTN_TEXT_FN, .label_fn = test_wav_label_getter, .press = test_wav_cb};
static button_data_t btn_hold      = {.type = BTN_TEXT_FN, .label_fn = hold_label_getter, .press = hold_cb};
static button_data_t btn_stations  = {.type = BTN_TEXT_FN, .label_fn = stations_label_getter, .press = stations_cb};

static buttons_page_t page_1 = {{&btn_p1, &btn_show, &btn_reply, &btn_send, &btn_stop_tx}};
static buttons_page_t page_2 = {{&btn_p2, &btn_cq, &btn_hb, &btn_query, &btn_clear}};
static buttons_page_t page_3 = {{&btn_p3, &btn_time_sync, &btn_test_wav, &btn_hold, &btn_stations}};

static button_data_t btn_p4     = {.type = BTN_TEXT, .label = "(JS8 4:4)", .press = button_next_page_cb, .next = &page_1};
static button_data_t btn_auto   = {.type = BTN_TEXT_FN, .label_fn = auto_label_getter, .press = auto_cb};
static button_data_t btn_hbauto = {.type = BTN_TEXT_FN, .label_fn = hb_label_getter, .press = hb_cb, .hold = hb_hold_cb};
static button_data_t btn_hbackk = {.type = BTN_TEXT_FN, .label_fn = hb_ack_label_getter, .press = hb_ack_cb};
static button_data_t btn_texts  = {.type = BTN_TEXT, .label = "Texts...", .press = texts_cb};
static buttons_page_t page_4 = {{&btn_p4, &btn_auto, &btn_hbauto, &btn_hbackk, &btn_texts}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = audio_cb,
    .key_cb       = key_cb,
    .rotary_cb    = rotary_cb,
    .btn_page     = &page_1,
};

dialog_t *dialog_js8 = &dialog;

/* ---- Message list ----------------------------------------------------- */

static bool passes_filter(const js8_rx_msg_t *m) {
    if (m->tx) return true;
    switch (show) {
    case SHOW_ALL:      return true;
    case SHOW_NO_HB:    return !m->heartbeat || m->to_me; /* e.g. HB acks to us */
    case SHOW_DIRECTED: return m->to_me || (m->to_group && !m->heartbeat && !m->cq);
    default:            return true;
    }
}

static void format_row(const js8_rx_msg_t *m, char *buf, size_t size) {
    int hh = m->utc / 10000, mm = (m->utc / 100) % 100, ss = m->utc % 100;

    if (m->tx) {
        snprintf(buf, size, "%02d:%02d:%02d  TX %4.0f  %s", hh, mm, ss, m->freq_hz, m->text);
        return;
    }

    snprintf(buf, size, "%02d:%02d:%02d %+3d %4.0f  %s%s%s%s",
             hh, mm, ss, m->snr, m->freq_hz,
             m->low_confidence ? "[" : "",
             m->text,
             m->low_confidence ? "]" : "",
             m->checksum < 0 ? "  (bad checksum)" : "");
}

static bool auto_selecting; /* follow() is moving the selection, not the user */

/* Keep following new rows if the selection is on the last one. */
static bool at_bottom(void) {
    uint16_t sel_row = 0, sel_col = 0;
    lv_table_get_selected_cell(table, &sel_row, &sel_col);
    return rows == 0 || sel_row == LV_TABLE_CELL_NONE || sel_row + 1 >= rows;
}

/* Select the last row and scroll it into view. lv_table 8.3 has no setter
 * for the selection, so put it one row above and let the table's own key
 * handler step down: that also runs its scroll-to-selected logic. */
static void select_row(uint16_t r) {
    if (rows == 0) return;
    if (r >= rows) r = rows - 1;
    lv_table_t *t  = (lv_table_t *)table;
    auto_selecting = true;
    if (r == 0) {
        t->row_act = 0;
        t->col_act = 0;
        lv_obj_scroll_to_y(table, 0, LV_ANIM_OFF);
        lv_obj_invalidate(table);
    } else {
        static uint32_t key = LV_KEY_DOWN;
        t->row_act          = r - 1;
        t->col_act          = 0;
        lv_event_send(table, LV_EVENT_KEY, &key);
    }
    auto_selecting = false;
}

static void follow(void) {
    if (rows > 0) select_row(rows - 1);
}

static void append_row(const char *text, int16_t hist) {
    lv_table_set_cell_value(table, rows, 0, text);
    row_hist[rows] = hist;
    rows++;
}

static void add_info_row(const char *fmt, ...) {
    char    buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (view_stations) return;
    bool scroll = at_bottom();
    if (rows >= MAX_ROWS) return;
    append_row(buf, -1);
    if (scroll) follow();
}

/* Rebuild the list from history, newest KEEP_ROWS matching messages. */
static void rebuild_station_rows(void);

static void rebuild_rows(void) {
    if (view_stations) {
        rebuild_station_rows();
        return;
    }
    int16_t idx[KEEP_ROWS]; /* newest first */
    int     n = 0;

    for (int age = 0; age < hist_count && n < KEEP_ROWS; age++) {
        int slot = (hist_head - 1 - age + HISTORY) % HISTORY;
        if (passes_filter(&history[slot])) idx[n++] = (int16_t)slot;
    }

    lv_table_set_row_cnt(table, 1);
    lv_table_set_cell_value(table, 0, 0, "");
    rows = 0;

    char buf[JS8_RX_TEXT_LEN + 48];
    if (n == 0) {
        append_row("Listening for JS8...", -1);
    }
    for (int k = n - 1; k >= 0; k--) {
        format_row(&history[idx[k]], buf, sizeof(buf));
        append_row(buf, idx[k]);
    }
    follow();
}

static int64_t now_wall_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void handle_incoming(const js8_rx_msg_t *m);

static void add_message(const js8_rx_msg_t *m) {
    js8_stations_add(stations, m, params.callsign.x, now_wall_ms());
    if (!m->tx) handle_incoming(m);

    int slot = hist_head;
    history[slot] = *m;
    hist_head     = (hist_head + 1) % HISTORY;
    if (hist_count < HISTORY) hist_count++;

    /* Rows pointing at the slot we just overwrote would now show the wrong
     * message; the ring is larger than MAX_ROWS so this only happens after a
     * long session, and rebuilding fixes it. */
    if (view_stations) {
        rebuild_station_rows();
        return;
    }
    for (uint16_t r = 0; r < rows; r++) {
        if (row_hist[r] == slot) {
            rebuild_rows();
            return;
        }
    }

    if (!passes_filter(m)) return;

    if (rows >= MAX_ROWS) {
        rebuild_rows();
        return;
    }

    bool scroll = at_bottom();
    char buf[JS8_RX_TEXT_LEN + 48];
    format_row(m, buf, sizeof(buf));
    append_row(buf, (int16_t)slot);
    if (scroll) follow();
}

/* "4m" style age; "now" under a minute. */
static void format_age(int64_t ms, char *buf, size_t size) {
    int64_t min = ms / 60000;
    if (min <= 0) snprintf(buf, size, "now");
    else if (min < 60) snprintf(buf, size, "%dm", (int)min);
    else snprintf(buf, size, "%dh", (int)(min / 60));
}

/* Station-view fields, drawn in fixed columns by table_draw_end_cb() since
 * the radio's font is proportional and spaces can't line text up. */
typedef struct {
    char star[2], call[JS8_RX_CALL_LEN], age[8], snr[8], heard[40], grid[8], dist[16];
} station_fields_t;

static void station_fields(const js8_station_t *st, int64_t now, station_fields_t *f) {
    memset(f, 0, sizeof(*f));
    f->star[0] = st->heard_me ? '*' : ' ';
    snprintf(f->call, sizeof(f->call), "%s", st->call);
    snprintf(f->snr, sizeof(f->snr), "%+d", st->snr);
    snprintf(f->grid, sizeof(f->grid), "%s", st->grid);
    char *age = f->age, *heard = f->heard, *dist = f->dist;
    format_age(now - st->heard_ms, age, sizeof(f->age));
    if (st->heard_me) {
        char hage[8];
        format_age(now - st->heard_me_ms, hage, sizeof(hage));
        if (st->has_reported_snr) {
            snprintf(heard, sizeof(f->heard), "heard you %+03d (%s)", st->reported_snr, hage);
        } else {
            snprintf(heard, sizeof(f->heard), "heard you (%s)", hage);
        }
    }
    if (st->grid[0] && params.qth.x[0]) {
        double lat, lon, my_lat, my_lon;
        qth_str_to_pos(st->grid, &lat, &lon);
        qth_str_to_pos(params.qth.x, &my_lat, &my_lon);
        snprintf(dist, sizeof(f->dist), "%.0f km", qth_pos_dist(lat, lon, my_lat, my_lon));
    }
}

/* The station the list's selection points at, in either view. */
static bool selected_station(char *call, size_t call_len, float *freq, int *snr) {
    uint16_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row >= rows || row_hist[row] < 0) return false;
    if (view_stations) {
        const js8_station_t *st = &st_rows[row_hist[row]];
        snprintf(call, call_len, "%s", st->call);
        *freq = st->freq_hz;
        *snr  = st->snr;
    } else {
        const js8_rx_msg_t *m = &history[row_hist[row]];
        if (m->tx || !m->from[0]) return false;
        snprintf(call, call_len, "%s", m->from);
        *freq = m->freq_hz;
        *snr  = m->snr;
    }
    return true;
}

static void rebuild_station_rows(void) {
    /* Keep the cursor on the same station while the list re-sorts. */
    char  keep[JS8_RX_CALL_LEN] = "";
    float f;
    int   n;
    if (!selected_station(keep, sizeof(keep), &f, &n)) keep[0] = '\0';

    int64_t now = now_wall_ms();
    st_count    = js8_stations_list(stations, now, st_rows, MAX_ROWS);

    lv_table_set_row_cnt(table, 1);
    lv_table_set_cell_value(table, 0, 0, "");
    rows = 0;
    if (st_count == 0) append_row("No stations heard yet", -1);

    int keep_row = 0;
    for (int i = 0; i < st_count; i++) {
        if (keep[0] && strcmp(st_rows[i].call, keep) == 0) keep_row = rows;
        append_row(" ", (int16_t)i); /* drawn by table_draw_end_cb() */
    }
    select_row(keep_row);
}

static void table_draw_cb(lv_event_t *e) {
    lv_obj_t               *obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);

    if (dsc->part != LV_PART_ITEMS) return;

    uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
    int16_t  h   = row < rows ? row_hist[row] : -1;

    dsc->rect_dsc->bg_opa = LV_OPA_50;

    if (h < 0) {
        dsc->label_dsc->align   = LV_TEXT_ALIGN_CENTER;
        dsc->rect_dsc->bg_color = lv_color_hex(0x303030);
    } else if (view_stations) {
        /* Stations that heard us stand out, like desktop's star. */
        dsc->rect_dsc->bg_color = st_rows[h].heard_me ? lv_color_hex(0x5a4400) : lv_color_black();
    } else {
        const js8_rx_msg_t *m = &history[h];
        if (m->tx) {
            dsc->rect_dsc->bg_color = lv_color_hex(0x1830a0);
        } else if (m->to_me) {
            dsc->rect_dsc->bg_color = lv_color_hex(0xB00000);
        } else if (m->cq) {
            dsc->rect_dsc->bg_color = lv_color_hex(0x006000);
        } else if (m->heartbeat) {
            dsc->rect_dsc->bg_color = lv_color_black();
            dsc->label_dsc->color   = lv_color_hex(0x808080);
        } else if (m->to_group) {
            dsc->rect_dsc->bg_color = lv_color_hex(0x102a60);
        } else {
            dsc->rect_dsc->bg_color = lv_color_black();
        }
        if (m->low_confidence) dsc->label_dsc->opa = LV_OPA_60;
    }

    uint16_t sel_row, sel_col;
    lv_table_get_selected_cell(obj, &sel_row, &sel_col);
    if (sel_row == row) dsc->rect_dsc->bg_color = lv_color_lighten(dsc->rect_dsc->bg_color, 30);
}

/* Station view: draw the fields at fixed x positions within the cell. */
static void table_draw_end_cb(lv_event_t *e) {
    if (!view_stations) return;
    lv_obj_t               *obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS) return;

    uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
    int16_t  h   = row < rows ? row_hist[row] : -1;
    if (h < 0) return;

    station_fields_t f;
    station_fields(&st_rows[h], now_wall_ms(), &f);

    static const struct {
        lv_coord_t x;
        size_t     field;
    } cols[] = {
        {0, offsetof(station_fields_t, star)},    {18, offsetof(station_fields_t, call)},
        {150, offsetof(station_fields_t, age)},   {210, offsetof(station_fields_t, snr)},
        {270, offsetof(station_fields_t, heard)}, {520, offsetof(station_fields_t, grid)},
        {610, offsetof(station_fields_t, dist)},
    };

    lv_area_t area = *dsc->draw_area;
    area.y1 += lv_obj_get_style_pad_top(obj, LV_PART_ITEMS);
    area.y2 -= lv_obj_get_style_pad_bottom(obj, LV_PART_ITEMS);
    lv_coord_t x0 = dsc->draw_area->x1 + lv_obj_get_style_pad_left(obj, LV_PART_ITEMS);

    for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++) {
        area.x1 = x0 + cols[i].x;
        area.x2 = (i + 1 < sizeof(cols) / sizeof(cols[0])) ? x0 + cols[i + 1].x - 6 : dsc->draw_area->x2;
        lv_draw_label(dsc->draw_ctx, dsc->label_dsc, &area, (const char *)&f + cols[i].field, NULL);
    }
}

/* Mark the selected message's offset on the waterfall; `announce` also
 * pops up who it is (on a tap, not on every MFK step). */
static void mark_selected(bool announce) {
    uint16_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row >= rows || row_hist[row] < 0) {
        lv_finder_clear_cursor(finder);
        lv_obj_invalidate(finder);
        return;
    }
    const char *from;
    float       freq;
    int         snr;
    if (view_stations) {
        from = st_rows[row_hist[row]].call;
        freq = st_rows[row_hist[row]].freq_hz;
        snr  = st_rows[row_hist[row]].snr;
    } else {
        from = history[row_hist[row]].from;
        freq = history[row_hist[row]].freq_hz;
        snr  = history[row_hist[row]].snr;
    }
    lv_finder_set_cursor(finder, (int16_t)(freq + 0.5f));
    lv_obj_invalidate(finder);
    if (announce && from[0]) {
        msg_update_text_fmt("%s at %.0f Hz, %+d dB", from, freq, snr);
    }
}

static void table_press_cb(lv_event_t *e) {
    (void)e;
    mark_selected(true);
}

static void table_select_cb(lv_event_t *e) {
    (void)e;
    if (!auto_selecting) mark_selected(false);
}

/* ---- Receiver callbacks (worker threads) ------------------------------- */

static void ui_add_message(void *arg) {
    if (!dialog.run || !table) return;
    add_message((const js8_rx_msg_t *)arg);
}

static void on_message(const js8_rx_msg_t *m, void *ctx) {
    (void)ctx;
    scheduler_put(ui_add_message, (void *)m, sizeof(*m));
}

static void update_status(void) {
    time_t    now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    bool testing = js8_rx_wav_active(rx);

    /* Always show what may transmit by itself. */
    char    flags[80] = "";
    int64_t now_ms    = now_wall_ms();
    bool    any_auto  = params.js8_auto.x || params.js8_hb.x;
    if (any_auto && js8_auto_idle(autop, now_ms)) {
        snprintf(flags, sizeof(flags), "AUTO/HB PAUSED (idle)  ");
    } else {
        if (params.js8_auto.x) strcat(flags, "AUTO  ");
        if (params.js8_hb.x) {
            char hb[40];
            if (hb_next_ms > now_ms) {
                time_t    t = (time_t)(hb_next_ms / 1000);
                struct tm nt;
                gmtime_r(&t, &nt);
                snprintf(hb, sizeof(hb), "HB %um next %02d:%02d  ", params.js8_hb_interval.x, nt.tm_hour, nt.tm_min);
            } else {
                snprintf(hb, sizeof(hb), "HB %um  ", params.js8_hb_interval.x);
            }
            strcat(flags, hb);
        }
        if (params.js8_hb_ack.x && params.js8_auto.x && params.js8_hb.x) strcat(flags, "ACK  ");
    }
    lv_label_set_text_fmt(status, "%s%s%s  %02d:%02d:%02dZ  total %u", flags, testing ? "TEST WAV  " : "",
                          cfg_digital_label_get(), tm.tm_hour, tm.tm_min, tm.tm_sec, hist_count);

    /* Playback ends on its own; bring the button label back in step. */
    if (testing != test_wav_shown) {
        test_wav_shown = testing;
        if (btn_test_wav.disp_btn) buttons_refresh(&btn_test_wav); /* only when page 2 is shown */
    }
}

static void ui_cycle_done(void *arg) {
    if (!dialog.run || !status) return;
    cycles++;
    cycle_decodes = *(unsigned *)arg;
    update_status();
}

static void on_cycle_done(unsigned decodes, void *ctx) {
    (void)ctx;
    scheduler_put(ui_cycle_done, &decodes, sizeof(decodes));
}

typedef struct {
    float   *psd;
    uint16_t size;
} wf_data_t;

static void wf_queue_clear(void) {
    while (wf_q_count) {
        free(wf_queue[wf_q_head]);
        wf_q_head = (wf_q_head + 1) % WF_QUEUE;
        wf_q_count--;
    }
}

static void ui_waterfall_add(void *arg) {
    wf_data_t *d = (wf_data_t *)arg;
    if (!dialog.run || !waterfall) {
        free(d->psd);
        return;
    }
    if (wf_q_count == WF_QUEUE) { /* far behind: drop the oldest */
        free(wf_queue[wf_q_head]);
        wf_q_head = (wf_q_head + 1) % WF_QUEUE;
        wf_q_count--;
    }
    unsigned tail       = (wf_q_head + wf_q_count) % WF_QUEUE;
    wf_queue[tail]      = d->psd;
    wf_queue_size[tail] = d->size;
    wf_q_count++;
}

/* One row per tick; two when the queue builds up, so the delay stays short. */
static void wf_timer_cb(lv_timer_t *t) {
    (void)t;
    unsigned n = wf_q_count > 3 ? 2 : (wf_q_count ? 1 : 0);
    while (n--) {
        lv_waterfall_add_data(waterfall, wf_queue[wf_q_head], wf_queue_size[wf_q_head]);
        free(wf_queue[wf_q_head]);
        wf_q_head = (wf_q_head + 1) % WF_QUEUE;
        wf_q_count--;
    }
}

static int cmp_float(const void *a, const void *b) {
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

static void wf_emit_row(void) {
    spgramf_get_psd(sg, psd);
    liquid_vectorf_addscalar(psd, nfft, -10.f * log10f(sqrtf(nfft)), psd);
    spgramf_reset(sg);

    /* spgram output is FFT-shifted: bin nfft/2 is 0 Hz. */
    uint32_t low_bin  = nfft / 2u + (uint32_t)nfft * filter_low / SAMPLE_RATE;
    uint32_t high_bin = nfft / 2u + (uint32_t)nfft * filter_high / SAMPLE_RATE;
    if (high_bin > nfft) high_bin = nfft;
    if (low_bin >= high_bin) return;

    wf_data_t d = {.size = (uint16_t)(high_bin - low_bin)};
    d.psd       = malloc(d.size * sizeof(float));
    if (!d.psd) return;
    memcpy(d.psd, &psd[low_bin], d.size * sizeof(float));

    /* Noise floor: the 30th percentile of the row, smoothed over ~2 s. */
    float *sorted = malloc(d.size * sizeof(float));
    if (sorted) {
        memcpy(sorted, d.psd, d.size * sizeof(float));
        qsort(sorted, d.size, sizeof(float), cmp_float);
        float floor_now = sorted[d.size * 3 / 10];
        free(sorted);
        wf_floor_db  = wf_floor_set ? wf_floor_db + 0.05f * (floor_now - wf_floor_db) : floor_now;
        wf_floor_set = true;
    }
    for (uint16_t i = 0; i < d.size; i++) d.psd[i] -= wf_floor_db;

    scheduler_put(ui_waterfall_add, &d, sizeof(d));
}

static void on_audio(const float *samples, unsigned n, void *ctx) {
    (void)ctx;
    if (!sg) return;

    /* One row per WF_ROW_SAMPLES of audio, however the audio is chunked. */
    while (n) {
        unsigned take = WF_ROW_SAMPLES - wf_row_fill;
        if (take > n) take = n;
        spgramf_write(sg, (float *)samples, take);
        samples += take;
        n -= take;
        wf_row_fill += take;
        if (wf_row_fill >= WF_ROW_SAMPLES) {
            wf_row_fill = 0;
            wf_emit_row();
        }
    }
}

/* ---- Receiver lifecycle ----------------------------------------------- */

static void rx_start(void) {
    int span = filter_high - filter_low;
    nfft     = (uint16_t)(span > 0 ? WIDTH * SAMPLE_RATE / span : 4096);
    /* Step at most half a row, so every row gets at least one transform. */
    unsigned step = nfft / 2 < WF_ROW_SAMPLES / 2 ? nfft / 2 : WF_ROW_SAMPLES / 2;
    sg           = spgramf_create(nfft, LIQUID_WINDOW_HANN, nfft, step);
    wf_floor_set = false;
    wf_row_fill  = 0;
    psd      = malloc(nfft * sizeof(float));

    js8_rx_cb_t cb = {
        .on_message    = on_message,
        .on_cycle_done = on_cycle_done,
        .on_audio      = on_audio,
    };
    rx = js8_rx_create(SAMPLE_RATE, JS8_SUBMODE_NORMAL, params.callsign.x, &cb);
    if (!rx) msg_schedule_text_fmt("JS8: cannot start decoder");
}

static void rx_stop(void) {
    /* Joins the receiver's threads: no callback runs after this, so the
     * spgram below is no longer in use. */
    js8_rx_destroy(rx);
    rx = NULL;

    if (sg) {
        spgramf_destroy(sg);
        sg = NULL;
    }
    free(psd);
    psd = NULL;
}

static void audio_cb(unsigned int n, float *samples) {
    if (atomic_load(&keyed)) return; /* our own TX, or nothing useful */
    js8_rx_feed(rx, samples, n);
}

/* ---- Transmit ---------------------------------------------------------- */

static bool tx_abort_check(void *ctx) {
    (void)ctx;
    return js8_tx_stopping(tx);
}

/* TX thread: key the radio for one frame. tx_player shifts the VFO so the
 * 1325 Hz synthesis tone lands at our offset, drops PTT and restores the VFO
 * afterwards. */
static bool tx_play(int16_t *samples, unsigned n, int index, int count, void *ctx) {
    (void)index;
    (void)count;
    (void)ctx;
    atomic_store(&keyed, true);
    bool done = tx_player_play(samples, n, atomic_load(&tx_offset_active), base_gain_offset, tx_abort_check, NULL);
    atomic_store(&keyed, false);
    return done;
}

static int current_utc_hhmmss(void) {
    time_t    now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm);
    return tm.tm_hour * 10000 + tm.tm_min * 100 + tm.tm_sec;
}

static void ui_tx_status(void *arg) {
    if (!dialog.run || !tx_bar) return;
    const js8_tx_status_t *st = (const js8_tx_status_t *)arg;

    /* When the first frame goes out, add the message to the list. */
    if (st->state == JS8_TX_KEYING && st->frame == 1 && tx_status.state != JS8_TX_KEYING) {
        js8_rx_msg_t m = {0};
        m.tx           = true;
        m.utc          = current_utc_hhmmss();
        m.freq_hz      = st->offset_hz;
        snprintf(m.text, sizeof(m.text), "%s", tx_preview[0] ? tx_preview : st->text);
        add_message(&m);
    }
    tx_status = *st;
    update_tx_bar();
}

static void on_tx_status(const js8_tx_status_t *st, void *ctx) {
    (void)ctx;
    scheduler_put(ui_tx_status, (void *)st, sizeof(*st));
}

static void ui_tx_done(void *arg) {
    if (!dialog.run || !tx_bar) return;
    bool completed = *(bool *)arg;
    if (!completed) add_info_row("TX stopped");
    memset(&tx_status, 0, sizeof(tx_status));
    update_tx_bar();

    /* An auto-reply that arrived while we were sending. */
    if (pending_auto_valid) {
        pending_auto_valid = false;
        auto_send(&pending_auto);
    }
}

static void on_tx_done(const char *text, bool completed, void *ctx) {
    (void)text;
    (void)ctx;
    scheduler_put(ui_tx_done, &completed, sizeof(completed));
}

static void tx_start(void) {
    js8_tx_cb_t cb = {
        .play      = tx_play,
        .on_status = on_tx_status,
        .on_done   = on_tx_done,
    };
    memset(&tx_status, 0, sizeof(tx_status));
    atomic_store(&keyed, false);
    tx = js8_tx_create(AUDIO_PLAY_RATE, TX_PLAYER_AUDIO_HZ, &cb);
    if (!tx) msg_schedule_text_fmt("JS8: cannot start transmitter");
}

static void tx_stop_all(void) {
    js8_tx_destroy(tx); /* stops and waits for the current frame to end */
    tx = NULL;
    atomic_store(&keyed, false);
}

static void tx_timer_cb(lv_timer_t *t) {
    (void)t;
    static unsigned ticks;
    update_tx_bar();
    /* Keep the status clock moving; decode cycles, which also refresh it,
     * pause while we transmit. */
    if (++ticks % 4 == 0) {
        hb_tick();
        update_status();
    }
    if (view_stations && ticks % 20 == 0) rebuild_station_rows(); /* ages */
}

/* "TX 1500 Hz  ready" / "... K2XYZ SNR?  starts in 9 s" / "... sending 2/3" */
static void update_tx_bar(void) {
    if (!tx_bar) return;
    char     line[JS8_RX_TEXT_LEN + 64];
    uint16_t offset = params.js8_tx_freq.x;

    if (hb_adjusting && tx_status.state == JS8_TX_IDLE) {
        snprintf(line, sizeof(line), "HB every %u min: turn the knob (5-30), press HB when done",
                 params.js8_hb_interval.x);
        lv_obj_set_style_bg_color(tx_bar, lv_color_hex(0x5a4a00), 0);
        lv_label_set_text(tx_bar, line);
        return;
    }

    switch (tx_status.state) {
    case JS8_TX_WAITING: {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        int64_t now_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        int     secs   = (int)((tx_status.next_ms - now_ms + 999) / 1000);
        if (secs < 0) secs = 0;
        snprintf(line, sizeof(line), "TX %4.0f Hz   %s   %s %d s  (%d/%d)", tx_status.offset_hz, tx_status.text,
                 tx_status.frame == 1 ? "starts in" : "next frame in", secs, tx_status.frame, tx_status.frames);
        lv_obj_set_style_bg_color(tx_bar, lv_color_hex(0x5a4a00), 0);
        break;
    }
    case JS8_TX_KEYING:
        snprintf(line, sizeof(line), "TX %4.0f Hz   %s   sending %d/%d", tx_status.offset_hz, tx_status.text,
                 tx_status.frame, tx_status.frames);
        lv_obj_set_style_bg_color(tx_bar, lv_color_hex(0xa00000), 0);
        break;
    default:
        snprintf(line, sizeof(line), "TX %4u Hz   ready%s", offset,
                 params.callsign.x[0] ? "" : "  (set your callsign: APP > Callsign)");
        lv_obj_set_style_bg_color(tx_bar, lv_color_hex(0x202020), 0);
        break;
    }
    lv_label_set_text(tx_bar, line);

    /* A red frame round the waterfall while keyed. */
    bool on = tx_status.state == JS8_TX_KEYING;
    lv_obj_set_style_border_width(waterfall, on ? 3 : 0, 0);
    lv_obj_set_style_border_color(waterfall, lv_color_hex(0xff2020), 0);
}

/* Queue `text` at our offset. Returns false (with a message shown) if it
 * can't be sent, e.g. a bad character or something already sending. */
static void qso_started(const char *call);

/* "N0XYZ ..." with a real-looking call first: a directed message. */
static bool starts_with_call(const char *text, char *call, size_t len) {
    size_t n = strcspn(text, " ");
    if (n < 3 || n >= len || text[0] == '@' || strncmp(text, "CQ", 2) == 0) return false;
    bool letter = false, digit = false;
    for (size_t i = 0; i < n; i++) {
        if (text[i] >= 'A' && text[i] <= 'Z') letter = true;
        else if (text[i] >= '0' && text[i] <= '9') digit = true;
        else if (text[i] != '/') return false;
    }
    if (!letter || !digit) return false;
    memcpy(call, text, n);
    call[n] = '\0';
    return true;
}

static bool tx_queue_at(const char *text, int offset_hz, bool automatic) {
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Already sending - Stop TX first");
        return false;
    }

    js8_tx_preview_t pv;
    js8_tx_preview(params.callsign.x, params.qth.x, text, &pv);
    if (!pv.ok) {
        msg_update_text_fmt("JS8: %s", pv.error);
        return false;
    }

    char err[JS8_TX_ERR_LEN];
    atomic_store(&tx_offset_active, offset_hz);
    snprintf(tx_preview, sizeof(tx_preview), "%s", pv.preview);
    if (!js8_tx_send(tx, params.callsign.x, params.qth.x, text, (float)offset_hz, err, sizeof(err))) {
        msg_update_text_fmt("JS8: %s", err);
        return false;
    }
    msg_update_text_fmt("%sQueued: %d frame%s, %.0f s", automatic ? "Auto: " : "", pv.frames,
                        pv.frames == 1 ? "" : "s", pv.seconds);
    snprintf(last_tx_text, sizeof(last_tx_text), "%s", text);

    /* A directed message you sent yourself starts a QSO. */
    char call[JS8_RX_CALL_LEN];
    if (!automatic && starts_with_call(text, call, sizeof(call))) qso_started(call);
    return true;
}

/* At our TX offset (the red band), sent by you. */
static bool tx_queue(const char *text) {
    return tx_queue_at(text, params.js8_tx_freq.x, false);
}

/* Hold off: answer on the other station's offset. Hold on (default): stay
 * on ours, which is JS8 etiquette. */
static void apply_hold(float their_freq) {
    if (params.js8_hold_offset.x) return;
    int f = (int)(their_freq + 0.5f);
    if (f < JS8_TX_MIN_OFFSET) f = JS8_TX_MIN_OFFSET;
    if (f > JS8_TX_MAX_OFFSET) f = JS8_TX_MAX_OFFSET;
    params_uint16_set(&params.js8_tx_freq, (uint16_t)f);
    lv_finder_set_value(finder, (int16_t)f);
    lv_obj_invalidate(finder);
    update_tx_bar();
}

/* Main tuning knob: move the TX offset, as in the FT8 app. The dial
 * frequency stays locked. */
static void rotary_cb(int32_t diff) {
    user_touch();
    if (hb_adjusting) {
        int v = (int)params.js8_hb_interval.x + (diff > 0 ? 1 : -1);
        if (v < JS8_HB_MIN_INTERVAL) v = JS8_HB_MIN_INTERVAL;
        if (v > JS8_HB_MAX_INTERVAL) v = JS8_HB_MAX_INTERVAL;
        params_uint16_set(&params.js8_hb_interval, (uint16_t)v);
        hb_adjust_ms = now_wall_ms();
        if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
        if (params.js8_hb.x && hb_next_ms) hb_next_ms = js8_next_heartbeat_ms(now_wall_ms(), v);
        update_tx_bar();
        update_status();
        return;
    }
    int32_t abs_diff = abs(diff);
    if (abs_diff > 3) diff *= (abs_diff < 6) ? 5 : 10;

    int32_t f = (int32_t)params.js8_tx_freq.x + diff;
    if (f < JS8_TX_MIN_OFFSET) f = JS8_TX_MIN_OFFSET;
    if (f > JS8_TX_MAX_OFFSET) f = JS8_TX_MAX_OFFSET;
    params_uint16_set(&params.js8_tx_freq, (uint16_t)f);

    lv_finder_set_value(finder, (int16_t)f);
    lv_obj_invalidate(finder);
    update_tx_bar();
}

/* ---- Compose window --------------------------------------------------- */

static void compose_changed_cb(lv_event_t *e) {
    (void)e;
    js8_tx_preview_t pv;
    js8_tx_preview(params.callsign.x, params.qth.x, textarea_window_get(), &pv);
    if (pv.ok) {
        msg_update_text_fmt("%d frame%s, %.0f s", pv.frames, pv.frames == 1 ? "" : "s", pv.seconds);
    }
}

static void compose_close(void) {
    if (!composing) return;
    textarea_window_close();
    composing = false;
    if (table) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* Same pattern as the FT8 app's keyboard: close here and return true;
 * textarea_window's own close is then a no-op. */
static bool compose_ok_cb(void) {
    if (edit_target) {
        char *dst = edit_target == 1 ? info_text : status_text;
        snprintf(dst, TEXT_MAX + 1, "%s", textarea_window_get());
        save_texts();
        msg_update_text_fmt("%s saved", edit_target == 1 ? "INFO" : "STATUS");
        edit_target = 0;
        compose_close();
        return true;
    }
    if (!tx_queue(textarea_window_get())) return false; /* keep the window open */
    compose_close();
    return true;
}

static bool compose_cancel_cb(void) {
    edit_target = 0;
    compose_close();
    return true;
}

static void compose_open(const char *prefill) {
    if (composing) return;
    if (!params.callsign.x[0]) {
        msg_update_text_fmt("Set your callsign first: APP > Callsign");
        return;
    }
    composing = true;
    lv_group_remove_obj(table);
    textarea_window_open(compose_ok_cb, compose_cancel_cb);

    lv_obj_t *text = textarea_window_text();
    lv_textarea_set_accepted_chars(text, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-+?!\"/@:>");
    lv_textarea_set_max_length(text, TX_TEXT_MAX);
    lv_obj_add_event_cb(text, compose_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (edit_target) {
        lv_textarea_set_max_length(text, TEXT_MAX);
        lv_obj_remove_event_cb(text, compose_changed_cb);
    }
    if (prefill && prefill[0]) {
        textarea_window_set(prefill);
    } else {
        lv_textarea_set_placeholder_text(text, edit_target == 1   ? " INFO, e.g. X6100 5W EFHW"
                                               : edit_target == 2 ? " STATUS, e.g. PORTABLE QRV"
                                                                  : " CALL MESSAGE / @ALLCALL ...");
    }
}

/* ---- Band and screen -------------------------------------------------- */

static void load_band(int8_t dir) {
    if (cfg_digital_load(dir, CFG_DIG_TYPE_JS8)) {
        msg_update_text_fmt("%s", cfg_digital_label_get());
    }
}

static void band_cb(lv_event_t *e) {
    load_band(lv_event_get_code(e) == EVENT_BAND_UP ? 1 : -1);

    js8_rx_clear(rx);
    js8_stations_clear(stations); /* a different band, different stations */
    lv_waterfall_clear_data(waterfall);
    wf_queue_clear();
    lv_finder_clear_cursor(finder);
    if (view_stations) rebuild_rows();
    add_info_row("%s", cfg_digital_label_get());
    update_status();
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    user_touch();

    switch (key) {
    case LV_KEY_ESC:
        if (hb_adjusting) {
            hb_adjusting = false;
            update_tx_bar();
        } else if (js8_tx_busy(tx)) {
            stop_tx_cb(NULL); /* first ESC stops TX; the next one closes */
        } else {
            dialog_destruct();
        }
        break;
    case KEY_VOL_LEFT_EDIT:
    case KEY_VOL_LEFT_SELECT:
        radio_change_vol(-1);
        break;
    case KEY_VOL_RIGHT_EDIT:
    case KEY_VOL_RIGHT_SELECT:
        radio_change_vol(1);
        break;
    }
}

static void construct_cb(lv_obj_t *parent) {
    dialog.obj = dialog_init(parent);

    /* Full-screen app with its own waterfall: skip main-screen DSP. */
    dsp_set_waterfall_enabled(false);
    dsp_set_spectrum_enabled(false);

    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_UP, NULL);
    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_DOWN, NULL);

    mem_save(MEM_BACKUP_ID);
    load_band(0);

    filter_low  = cparam_i_get(cfg_cur_filter_low);
    filter_high = cparam_i_get(cfg_cur_filter_high);

    /* Waterfall */

    waterfall = lv_waterfall_create(dialog.obj);
    lv_obj_add_style(waterfall, &waterfall_style, 0);
    lv_obj_clear_flag(waterfall, LV_OBJ_FLAG_SCROLLABLE);
    lv_waterfall_set_palette(waterfall, (lv_color_t *)wf_palette, 256);
    lv_waterfall_set_size(waterfall, WIDTH, WF_HEIGHT);
    lv_waterfall_set_min(waterfall, WF_MIN_DB);
    lv_waterfall_set_max(waterfall, WF_MAX_DB);
    wf_timer = lv_timer_create(wf_timer_cb, 1000 / WF_ROWS_PER_SEC, NULL);
    lv_obj_set_pos(waterfall, 13, 13);

    /* Finder marks the offset of the selected message. */

    finder = lv_finder_create(waterfall);
    lv_finder_set_range(finder, filter_low, filter_high);
    /* A stored offset outside the usable range (an old or damaged setting)
     * would make every send fail; start from 1500 Hz instead. */
    if (params.js8_tx_freq.x < JS8_TX_MIN_OFFSET || params.js8_tx_freq.x > JS8_TX_MAX_OFFSET) {
        params_uint16_set(&params.js8_tx_freq, 1500);
    }

    /* The finder's band is our TX offset; its cursor line marks the
     * selected message. */
    lv_finder_set_width(finder, JS8_WIDTH_HZ);
    lv_finder_set_value(finder, params.js8_tx_freq.x);
    lv_finder_clear_cursor(finder);
    lv_obj_set_size(finder, WIDTH, WF_HEIGHT);
    lv_obj_set_pos(finder, 0, 0);
    lv_obj_set_style_radius(finder, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(finder, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(finder, LV_OPA_0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(finder, bg_color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(finder, LV_OPA_50, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(finder, 1, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(finder, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_border_opa(finder, LV_OPA_50, LV_PART_INDICATOR);

    status = lv_label_create(waterfall);
    lv_obj_set_style_text_font(status, &sony_18, 0);
    lv_obj_set_style_text_color(status, lv_color_white(), 0);
    lv_obj_set_style_bg_color(status, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(status, LV_OPA_50, 0);
    lv_obj_align(status, LV_ALIGN_TOP_RIGHT, -4, 4);

    /* TX status line */

    tx_bar = lv_label_create(dialog.obj);
    lv_obj_set_size(tx_bar, WIDTH, TX_BAR_H);
    lv_obj_set_pos(tx_bar, 13, 13 + WF_VISIBLE);
    lv_obj_set_style_text_font(tx_bar, &sony_22, 0);
    lv_obj_set_style_pad_left(tx_bar, 6, 0);
    lv_obj_set_style_pad_top(tx_bar, 3, 0);
    lv_obj_set_style_bg_opa(tx_bar, LV_OPA_COVER, 0);
    lv_label_set_long_mode(tx_bar, LV_LABEL_LONG_DOT);

    /* Message list */

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_add_event_cb(table, table_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(table, table_select_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(table, key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(table, table_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_add_event_cb(table, table_draw_end_cb, LV_EVENT_DRAW_PART_END, NULL);
    lv_obj_set_size(table, WIDTH, WF_HEIGHT - WF_VISIBLE - TX_BAR_H);
    lv_obj_set_pos(table, 13, 13 + WF_VISIBLE + TX_BAR_H);
    lv_table_set_col_cnt(table, 1);
    lv_table_set_col_width(table, 0, WIDTH - 2);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, 192, LV_PART_MAIN);
    lv_obj_set_style_bg_color(table, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(table, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(table, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_opa(table, 128, LV_PART_MAIN);
    lv_obj_set_style_text_color(table, lv_color_hex(0xC0C0C0), LV_PART_ITEMS);
    /* JS8 messages are long; the dialog's 36 px font fits only two rows. */
    lv_obj_set_style_text_font(table, &sony_24, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(table, 0, LV_PART_ITEMS);

    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);

    rebuild_rows();
    add_info_row("%s", cfg_digital_label_get());

    main_screen_lock_ab(true);
    main_screen_lock_mode(true);
    main_screen_lock_freq(true);
    main_screen_lock_band(true);

    cycles = cycle_decodes = 0;
    test_wav_shown          = false;
    rx_start();
    update_status();

    /* Transmit: same 5 W cap and gain start as the FT8 app. */
    if (param_f_get(cfg_pwr) > TX_PLAYER_MAX_PWR_W) {
        radio_set_pwr(TX_PLAYER_MAX_PWR_W);
        msg_schedule_text_fmt("Power was limited to %0.0fW", TX_PLAYER_MAX_PWR_W);
    }
    base_gain_offset = tx_player_base_gain_offset();
    tx_start();
    if (!stations) stations = js8_stations_create();
    if (!autop) autop = js8_auto_create();
    user_touch();
    load_texts();
    hb_next_ms         = 0;
    hb_adjusting       = false;
    pending_auto_valid = false;
    memset(&offer, 0, sizeof(offer));
    tx_timer = lv_timer_create(tx_timer_cb, 250, NULL);
    update_tx_bar();
}

static void destruct_cb(void) {
    /* Unkey and join the TX thread first: tx_player restores the VFO and
     * drops PTT before we restore the memory slot below. */
    tx_stop_all();
    if (tx_timer) {
        lv_timer_del(tx_timer);
        tx_timer = NULL;
    }
    if (wf_timer) {
        lv_timer_del(wf_timer);
        wf_timer = NULL;
    }
    compose_close();
    query_close();
    if (texts_list) {
        lv_obj_del(texts_list);
        texts_list = NULL;
    }
    hb_adjusting = false;
    radio_set_pwr(param_f_get(cfg_pwr));

    rx_stop();
    wf_queue_clear();

    dsp_set_waterfall_enabled(true);
    dsp_set_spectrum_enabled(true);

    mem_load(MEM_BACKUP_ID);

    main_screen_lock_mode(false);
    main_screen_lock_ab(false);
    main_screen_lock_freq(false);
    main_screen_lock_band(false);

    /* LVGL objects are children of dialog.obj, deleted by dialog_destruct()
     * right after this returns. */
    waterfall = finder = table = status = tx_bar = NULL;
}

/* ---- Buttons ---------------------------------------------------------- */

static const char *show_label_getter(void) {
    static const char *const labels[SHOW_COUNT] = {"Show:\nAll", "Show:\nNo HB", "Show:\nDirected"};
    return labels[show];
}

static void show_cb(button_data_t *btn) {
    user_touch();
    show = (show + 1) % SHOW_COUNT;
    buttons_refresh(btn);
    rebuild_rows();
}

static void clear_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    hist_head = hist_count = 0;
    js8_stations_clear(stations);
    js8_rx_clear(rx);
    lv_waterfall_clear_data(waterfall);
    wf_queue_clear();
    lv_finder_clear_cursor(finder);
    rebuild_rows();
    update_status();
}

/* Snap the system clock to the nearest 15 s boundary, for use when you know
 * a transmission just started. Same approach as the FT8 app. */
static void time_sync_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    time_t now   = time(NULL);
    float  drift = fmodf((now % 60) + JS8_SLOT_SEC / 2, JS8_SLOT_SEC) - JS8_SLOT_SEC / 2;

    struct timespec tp = {.tv_sec = now - (int)drift, .tv_nsec = 0};
    if (clock_settime(CLOCK_REALTIME, &tp) != 0) {
        LV_LOG_ERROR("Can't set system time: %s", strerror(errno));
        return;
    }
    msg_update_text_fmt("Clock moved %+d s", -(int)drift);
}

static const char *test_wav_label_getter(void) {
    return js8_rx_wav_active(rx) ? "Stop\nTest" : "Test\nWAV";
}

/* Play TEST_WAV through the decoder instead of the receiver audio, starting
 * at the next slot boundary. tools/js8_wavgen makes suitable files. */
static void test_wav_cb(button_data_t *btn) {
    user_touch();
    if (js8_rx_wav_active(rx)) {
        js8_rx_stop_wav(rx);
        add_info_row("Test stopped");
    } else {
        char  err[96];
        float starts = js8_rx_play_wav(rx, TEST_WAV, err, sizeof(err));
        if (starts < 0) {
            msg_update_text_fmt("JS8 test: %s", err);
            return;
        }
        add_info_row("Test: %s from next slot", TEST_WAV);
        msg_update_text_fmt("Test WAV starts in %.0f s", starts);
    }
    update_status();
    buttons_refresh(btn);
}

static void reply_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    if (!selected_station(call, sizeof(call), &freq, &snr)) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }
    /* AUTO off and this station asked us something: offer the answer. */
    if (offer.text[0] && now_wall_ms() - offer.ms < OFFER_MS && strcmp(offer.call, call) == 0) {
        char text[JS8_RX_TEXT_LEN];
        snprintf(text, sizeof(text), "%s", offer.text);
        offer.text[0] = '\0';
        apply_hold(freq);
        compose_open(text);
        return;
    }
    char prefill[JS8_RX_CALL_LEN + 2];
    snprintf(prefill, sizeof(prefill), "%s ", call);
    apply_hold(freq);
    compose_open(prefill);
}

static void send_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    compose_open(NULL);
}

static void stop_tx_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    if (!js8_tx_busy(tx)) {
        msg_update_text_fmt("Not sending");
        return;
    }
    js8_tx_stop(tx);
    msg_update_text_fmt("Stopping TX");
}

/* CQ with the 4-character grid, as desktop JS8Call sends it. */
static void cq_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    char text[32];
    snprintf(text, sizeof(text), "CQ CQ CQ %.4s", params.qth.x);
    tx_queue(text);
}

/* One heartbeat now, at a free spot in the 500-1000 Hz heartbeat sub-band
 * (desktop's rule: clear of anything heard in the last 30 s). Our chat
 * offset (the red band) doesn't move. */
static int free_hb_offset(void) {
    static js8_station_t heard[MAX_ROWS];
    float                offsets[MAX_ROWS];
    int64_t              times[MAX_ROWS];
    int64_t              now = now_wall_ms();
    int                  n   = js8_stations_list(stations, now, heard, MAX_ROWS);
    for (int i = 0; i < n; i++) {
        offsets[i] = heard[i].freq_hz;
        times[i]   = heard[i].heard_ms;
    }
    return js8_heartbeat_offset(offsets, times, (unsigned)n, now);
}

static bool send_heartbeat(bool automatic) {
    char text[48];
    js8_heartbeat_text(params.callsign.x, params.qth.x, text, sizeof(text));
    return tx_queue_at(text, free_hb_offset(), automatic);
}

static void heartbeat_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    send_heartbeat(false);
}

static const char *hold_label_getter(void) {
    return params.js8_hold_offset.x ? "Hold:\nOn" : "Hold:\nOff";
}

static void hold_cb(button_data_t *btn) {
    user_touch();
    params_bool_set(&params.js8_hold_offset, !params.js8_hold_offset.x);
    buttons_refresh(btn);
    msg_update_text_fmt(params.js8_hold_offset.x ? "Replies stay on your offset" : "Replies move to their offset");
}

static const char *stations_label_getter(void) {
    return view_stations ? "Show\nMessages" : "Show\nStations";
}

static void stations_cb(button_data_t *btn) {
    user_touch();
    view_stations = !view_stations;
    buttons_refresh(btn);
    rebuild_rows();
}

/* ---- Query popup ------------------------------------------------------ */

static void query_close(void) {
    if (!query_list) return;
    /* Often called from one of the list's own buttons: delete later. */
    lv_obj_del_async(query_list);
    query_list = NULL;
    if (table) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void query_item_cb(lv_event_t *e) {
    js8_query_t q = (js8_query_t)(intptr_t)lv_event_get_user_data(e);
    char        call[JS8_RX_CALL_LEN];
    float       freq;
    int         snr;
    bool        have = selected_station(call, sizeof(call), &freq, &snr);
    query_close();
    if (!have) return;

    char text[64];
    if (!js8_query_text(q, call, snr, params.qth.x, text, sizeof(text))) {
        msg_update_text_fmt(q == JS8_Q_MY_GRID ? "Set your grid first: APP > QTH" : "Nothing to send");
        return;
    }
    apply_hold(freq);
    tx_queue(text);
}

static void query_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    switch (key) {
    case LV_KEY_ESC:
        query_close();
        break;
    case LV_KEY_LEFT:
    case LV_KEY_UP:
        lv_group_focus_prev(keyboard_group);
        break;
    case LV_KEY_RIGHT:
    case LV_KEY_DOWN:
        lv_group_focus_next(keyboard_group);
        break;
    case KEY_VOL_LEFT_EDIT:
    case KEY_VOL_LEFT_SELECT:
        radio_change_vol(-1);
        break;
    case KEY_VOL_RIGHT_EDIT:
    case KEY_VOL_RIGHT_SELECT:
        radio_change_vol(1);
        break;
    }
}

/* One-press messages for the selected station: MFK to move, press or tap
 * to send, ESC to close. */
static void query_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    if (query_list || composing) return;
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    if (!selected_station(call, sizeof(call), &freq, &snr)) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }

    lv_group_remove_obj(table);
    query_list = lv_list_create(dialog.obj);
    lv_obj_set_size(query_list, 300, WF_HEIGHT - 10);
    lv_obj_align(query_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(query_list, &sony_24, 0);
    lv_obj_set_style_bg_color(query_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(query_list, lv_color_white(), 0);

    char title[40];
    snprintf(title, sizeof(title), "To %s (%+d dB)", call, snr);
    lv_obj_t *t = lv_list_add_text(query_list, title);
    lv_obj_set_style_text_font(t, &sony_22, 0);

    lv_obj_t *first = NULL;
    for (int q = 0; q < JS8_Q_COUNT; q++) {
        lv_obj_t *b = lv_list_add_btn(query_list, NULL, js8_query_label((js8_query_t)q));
        lv_obj_set_style_bg_color(b, lv_color_hex(0x303030), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x1830a0), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(b, lv_color_white(), 0);
        lv_obj_add_event_cb(b, query_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)q);
        lv_obj_add_event_cb(b, query_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        if (!first) first = b;
    }
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

/* For tools/js8_ui_harness: the station the selection points at. */
bool dialog_js8_selected_call(char *call, unsigned len) {
    float freq;
    int   snr;
    return table && selected_station(call, len, &freq, &snr);
}

/* ---- T4: auto-reply, heartbeats ---------------------------------------- */

/* Any key, button or knob: resets the idle watchdog. */
static void user_touch(void) {
    js8_auto_user_activity(autop, now_wall_ms());
}

/* Recently heard calls, most recent first, for HEARING?. */
static unsigned heard_calls(const char **out, unsigned max) {
    static js8_station_t list[MAX_ROWS];
    int                  n = js8_stations_list(stations, now_wall_ms(), list, MAX_ROWS);
    /* js8_stations_list puts stations that heard us first; re-sort by time. */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && list[j].heard_ms > list[j - 1].heard_ms; j--) {
            js8_station_t t = list[j];
            list[j]         = list[j - 1];
            list[j - 1]     = t;
        }
    unsigned k = 0;
    for (int i = 0; i < n && k < max; i++) out[k++] = list[i].call;
    return k;
}

static void auto_send(const js8_auto_result_t *r) {
    int64_t now = now_wall_ms();
    /* The switches may have changed while it waited. */
    bool allowed = r->hb_ack ? (params.js8_auto.x && params.js8_hb.x && params.js8_hb_ack.x) : params.js8_auto.x;
    if (!allowed || js8_auto_idle(autop, now)) return;

    if (js8_tx_busy(tx) || composing || query_list || texts_list) {
        pending_auto       = *r; /* newest wins */
        pending_auto_valid = true;
        return;
    }
    int offset = r->hb_ack ? free_hb_offset() : params.js8_tx_freq.x;
    LV_LOG_USER("JS8 auto: '%s' at %d Hz", r->text, offset);
    if (tx_queue_at(r->text, offset, true)) {
        js8_auto_sent(autop, r, now);
        add_info_row("Auto: %s", r->text);
    }
}

/* Desktop pauses heartbeats during a QSO; here they switch off until you
 * turn them back on (the user's choice). */
static void qso_started(const char *call) {
    if (!params.js8_hb.x && !params.js8_hb_ack.x) return;
    params_bool_set(&params.js8_hb, false);
    params_bool_set(&params.js8_hb_ack, false);
    hb_next_ms   = 0;
    hb_adjusting = false;
    if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    msg_update_text_fmt("Heartbeats off: QSO with %s", call);
    add_info_row("HB and HB ACK off: QSO with %s", call);
    update_status();
}

static void handle_incoming(const js8_rx_msg_t *m) {
    if (js8_starts_qso(m)) qso_started(m->from);

    const char *heard[16];
    unsigned    n = heard_calls(heard, 16);

    js8_auto_settings_t st = {
        .autoreply = params.js8_auto.x,
        .heartbeat = params.js8_hb.x,
        .hb_ack    = params.js8_hb_ack.x,
        .my_call   = params.callsign.x,
        .my_grid   = params.qth.x,
        .info      = info_text,
        .status    = status_text,
    };
    js8_auto_result_t r;
    js8_auto_consider(autop, m, &st, heard, n, last_tx_text, now_wall_ms(), &r);

    switch (r.action) {
    case JS8_AUTO_SEND:
        auto_send(&r);
        break;
    case JS8_AUTO_OFFER:
        snprintf(offer.call, sizeof(offer.call), "%s", r.to);
        snprintf(offer.text, sizeof(offer.text), "%s", r.text);
        offer.ms = now_wall_ms();
        msg_update_text_fmt("%s asked %s - select it and press Reply to answer", r.to, r.command);
        add_info_row("%s asked %s: Reply sends \"%s\"", r.to, r.command, r.text);
        break;
    default:
        break;
    }
}

/* Once a second: send a heartbeat when one is due. */
static void hb_tick(void) {
    if (hb_adjusting && now_wall_ms() - hb_adjust_ms > HB_ADJUST_MS) hb_adjust_end();
    if (!params.js8_hb.x) {
        hb_next_ms = 0;
        return;
    }
    int64_t now = now_wall_ms();
    if (js8_auto_idle(autop, now)) return;
    if (hb_next_ms == 0) hb_next_ms = now; /* first one at the next chance */
    if (now < hb_next_ms - 5000) return;   /* desktop prepares it 5 s early */
    if (js8_tx_busy(tx) || composing || query_list || texts_list || !params.callsign.x[0]) return;
    LV_LOG_USER("JS8 auto: heartbeat (due %lld)", (long long)hb_next_ms);
    if (send_heartbeat(true)) {
        hb_next_ms = js8_next_heartbeat_ms(now, params.js8_hb_interval.x);
        update_status();
    }
}

static const char *auto_label_getter(void) {
    return params.js8_auto.x ? "AUTO:\nOn" : "AUTO:\nOff";
}

static void auto_cb(button_data_t *btn) {
    user_touch();
    params_bool_set(&params.js8_auto, !params.js8_auto.x);
    buttons_refresh(btn);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    msg_update_text_fmt(params.js8_auto.x ? "AUTO on: answers SNR? GRID? INFO? STATUS? HEARING? AGN?"
                                          : "AUTO off: answers are offered on Reply");
    update_status();
}

static const char *hb_label_getter(void) {
    static char buf[24];
    if (!params.js8_hb.x) return "HB:\nOff";
    snprintf(buf, sizeof(buf), hb_adjusting ? "HB: knob\n< %u min >" : "HB:\n%u min", params.js8_hb_interval.x);
    return buf;
}

/* Off -> On, straight into setting the interval with the knob; press
 * again (or wait) to finish; press once more to turn heartbeats off. */
static void hb_adjust_start(button_data_t *btn) {
    hb_adjusting    = true;
    hb_adjust_ms    = now_wall_ms();
    buttons_refresh(btn);
    update_tx_bar();
}

static void hb_adjust_end(void) {
    if (!hb_adjusting) return;
    hb_adjusting = false;
    if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
    update_tx_bar();
    if (params.js8_hb.x) msg_update_text_fmt("HB every %u min", params.js8_hb_interval.x);
}

static void hb_cb(button_data_t *btn) {
    user_touch();
    if (hb_adjusting) {
        hb_adjust_end();
        return;
    }
    params_bool_set(&params.js8_hb, !params.js8_hb.x);
    hb_next_ms = 0;
    buttons_refresh(btn);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    if (params.js8_hb.x) {
        hb_adjust_start(btn);
    } else {
        msg_update_text_fmt("HB off");
    }
    update_status();
}

static void hb_hold_cb(button_data_t *btn) {
    user_touch();
    hb_adjust_start(btn);
}

static const char *hb_ack_label_getter(void) {
    if (!params.js8_hb_ack.x) return "HB ACK:\nOff";
    return (params.js8_auto.x && params.js8_hb.x) ? "HB ACK:\nOn" : "HB ACK:\nOn (idle)";
}

static void hb_ack_cb(button_data_t *btn) {
    user_touch();
    params_bool_set(&params.js8_hb_ack, !params.js8_hb_ack.x);
    buttons_refresh(btn);
    if (params.js8_hb_ack.x && !(params.js8_auto.x && params.js8_hb.x)) {
        msg_update_text_fmt("HB ACK acts only while AUTO and HB are on");
    }
    update_status();
}

/* ---- INFO / STATUS texts -------------------------------------------- */

static void load_texts(void) {
    info_text[0] = status_text[0] = '\0';
    FILE *f = fopen(JS8_TEXTS_PATH, "r");
    if (!f) return;
    char line[TEXT_MAX + 16];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "INFO=", 5) == 0) snprintf(info_text, sizeof(info_text), "%s", line + 5);
        if (strncmp(line, "STATUS=", 7) == 0) snprintf(status_text, sizeof(status_text), "%s", line + 7);
    }
    fclose(f);
}

static void save_texts(void) {
    FILE *f = fopen(JS8_TEXTS_PATH, "w");
    if (!f) {
        msg_update_text_fmt("Can't write %s", JS8_TEXTS_PATH);
        return;
    }
    fprintf(f, "INFO=%s\nSTATUS=%s\n", info_text, status_text);
    fclose(f);
}

static void texts_close(void) {
    if (!texts_list) return;
    lv_obj_del_async(texts_list);
    texts_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void texts_item_cb(lv_event_t *e) {
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    /* Straight into the keyboard: take the list's buttons out of the group
     * now (the list itself goes later, it's running this callback) and
     * don't hand the focus back to the table, or the keyboard opens
     * without it and can't be used. */
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(texts_list); i++)
        lv_group_remove_obj(lv_obj_get_child(texts_list, i));
    lv_obj_del_async(texts_list);
    texts_list  = NULL;
    edit_target = which;
    compose_open(which == 1 ? info_text : status_text);
    lv_group_set_editing(keyboard_group, true); /* as after Reply / Send... */
}

static void texts_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    if (key == LV_KEY_ESC) texts_close();
    else if (key == LV_KEY_LEFT || key == LV_KEY_UP) lv_group_focus_prev(keyboard_group);
    else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) lv_group_focus_next(keyboard_group);
}

/* INFO and STATUS: what AUTO sends for INFO? and STATUS?. */
static void texts_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (texts_list || query_list || composing) return;
    lv_group_remove_obj(table);
    texts_list = lv_list_create(dialog.obj);
    lv_obj_set_size(texts_list, 520, 170);
    lv_obj_align(texts_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(texts_list, &sony_24, 0);
    lv_obj_set_style_bg_color(texts_list, lv_color_hex(0x202020), 0);
    lv_list_add_text(texts_list, "Sent by AUTO for INFO? / STATUS?");

    for (int i = 1; i <= 2; i++) {
        char        label[TEXT_MAX + 16];
        const char *v = i == 1 ? info_text : status_text;
        snprintf(label, sizeof(label), "%s: %s", i == 1 ? "INFO" : "STATUS", v[0] ? v : "(not set)");
        lv_obj_t *b = lv_list_add_btn(texts_list, NULL, label);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x303030), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x1830a0), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(b, lv_color_white(), 0);
        lv_obj_add_event_cb(b, texts_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, texts_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        if (i == 1) lv_group_focus_obj(b);
    }
    lv_group_set_editing(keyboard_group, false);
}
