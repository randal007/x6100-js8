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
#include "js8/js8_speed.h"
#include "qth/qth.h"
#include "qso_log.h"

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
#include <fcntl.h>
#include <linux/rtc.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

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
#ifndef JS8_INBOX_PATH
#define JS8_INBOX_PATH   "/mnt/js8_inbox.txt"   /* one message per line */
#endif
#ifndef JS8_HELD_PATH
#define JS8_HELD_PATH    "/mnt/js8_held.txt"    /* MSG TO: messages held for others */
#endif
#ifndef JS8_LOG_PATH
#define JS8_LOG_PATH     "/mnt/js8call_log.adi" /* desktop JS8Call's name */
#endif
#define SYNC_WINDOW_MS   120000 /* Time Sync uses decodes from the last 2 min */
#define SYNC_DTS         32
#define WF_ROWS_PER_SEC  10     /* waterfall rows per second of audio */
#define WF_ROW_SAMPLES   (SAMPLE_RATE / WF_ROWS_PER_SEC)
#define WF_QUEUE         16     /* rows waiting to be drawn (jitter buffer) */
#define WF_TICK_MS       10     /* how often the drawing timer looks at the clock */
#define HB_ADJUST_MS     8000   /* setting the HB interval ends after this idle */
#define ALERT_COLOR      0x6a2ca0 /* rows matching an alert word */
/* params.js8_alerts bits: what beeps (alert words always highlight). */
#define JS8_ALERT_BEEP     0x01 /* beeping at all */
#define JS8_ALERT_TO_ME    0x02 /* a message to your call (not a heartbeat ack) */
#define JS8_ALERT_INBOX    0x04 /* a MSG saved to the inbox */
#define JS8_ALERT_CQ       0x08 /* someone calling CQ */
#define JS8_ALERT_NEW      0x10 /* a station heard for the first time, not in the log */
/* The waterfall is drawn relative to the noise floor, so it works at any
 * audio level: WF_MIN_DB..WF_MAX_DB above the floor spans the palette. */
#define WF_MIN_DB        0
#define WF_MAX_DB        30

/* The receive filter while the app is open, and the range the decoder
 * searches (desktop searches its waterfall filter's edges). Yours comes
 * back when the app closes. */
#define JS8_FILTER_LOW   200
#define JS8_FILTER_HIGH  3000

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
static void        rotary_cb(int32_t diff);
static void        reply_cb(button_data_t *btn);
static void        send_cb(button_data_t *btn);
static void        stop_tx_cb(button_data_t *btn);
static void        hw_cpy_cb(button_data_t *btn);
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
static void        aprs_cb(button_data_t *btn);
static void        aprs_close(void);
static bool        popup_guard(void);
static void        js8_next_page_cb(button_data_t *btn);
static void        js8_prev_page_cb(button_data_t *btn);
static void        texts_close(void);
static bool        aprs_prepare(const char *in, char *out, size_t size);
static bool        any_popup(void);
static void        log_cb(button_data_t *btn);
static void        log_close(void);
static void        log_offer(const char *call);

static void        log_refresh(void);
static const char *inbox_label_getter(void);
static void        inbox_cb(button_data_t *btn);
static void        inbox_close(void);
static void        inbox_received(const js8_rx_msg_t *m);
static void        held_received(const js8_rx_msg_t *m);
static void        msg_compose(const char *call, const char *kind);
static void        alerts_cb(button_data_t *btn);
static void        alerts_close(void);
static void        alert_check(js8_rx_msg_t *m, bool new_station);
static void        alert_beep(int count);
static const char *speed_label_getter(void);
static void        speed_cb(button_data_t *btn);
static void        speed_hold_cb(button_data_t *btn);
static const char *decode_label_getter(void);
static void        decode_cb(button_data_t *btn);
static void        speed_warn(void);
static const char *act_label_getter(void);
static void        act_cb(button_data_t *btn);
static void        act_hold_cb(button_data_t *btn);
static const char *prompt_label_getter(void);
static void        prompt_cb(button_data_t *btn);

/* ---- State (UI thread unless noted) ------------------------------------ */

static js8_rx_t *rx;
static js8_tx_t *tx;

static lv_obj_t   *tx_bar;
static lv_timer_t *tx_timer;           /* refreshes the TX bar countdown */
static js8_tx_status_t tx_status;      /* UI-thread copy of the last status */
static char        tx_preview[JS8_RX_TEXT_LEN]; /* what we're sending, as others see it */
static float       base_gain_offset;
static float       sync_dt[SYNC_DTS];  /* recent decode DTs for Time Sync */
static int64_t     sync_ms[SYNC_DTS];
static unsigned    sync_head;
static float       qso_freq = -1;     /* the selected station's offset (the green line), -1 none */
static atomic_bool keyed;              /* a frame is on the air (TX thread) */
/* Held while an alert beep plays; tx_play takes it after setting keyed, so
 * a beep stops within one part and never goes out with our TX audio. */
static pthread_mutex_t speaker_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int  tx_offset_active;   /* offset of the message being sent */
static bool        composing;          /* compose window open */

static js8_stations_t *stations;       /* who we've heard, who heard us */
static bool           view_stations;   /* list shows stations, not messages */
static js8_station_t  st_rows[MAX_ROWS];
static int            st_count;
static lv_obj_t      *query_list;      /* Query popup, when open */
static lv_obj_t      *aprs_list;       /* APRS popup, when open */
static lv_obj_t      *log_list;        /* Log QSO popup, when open */
static lv_obj_t      *inbox_list;      /* Inbox popup (list or one message), when open */
static lv_obj_t      *alerts_list;     /* Alerts popup, when open */
static char           alert_words[128]; /* "VE7ABC @POTA SOTA", ALERTS= in JS8_TEXTS_PATH */
static bool           st_alert[MAX_ROWS]; /* st_rows matching an alert word */
static js8_inbox_t   *inbox;           /* MSGs to us, JS8_INBOX_PATH */
static js8_held_t    *held;            /* MSG TO: messages held for others, JS8_HELD_PATH */
static struct {
    int  id;                           /* offered on Reply: mark it delivered when that goes */
    char text[JS8_RX_TEXT_LEN];
} deliver_pending;
static bool           st_worked[MAX_ROWS]; /* in the log already (st_rows) */
static js8_qsos_t    *qsos;            /* QSOs, for the log */
static js8_log_entry_t log_entry;      /* the entry the Log popup shows */
static char           log_pending[JS8_RX_CALL_LEN]; /* a QSO that ended, not logged yet */
static bool           log_grid_typed;  /* the grid was typed: don't replace it */
static lv_obj_t      *log_reports;     /* the popup's Sent / Rcvd line */
static lv_obj_t      *log_grid_btn;    /* the popup's Grid item */

/* T4: auto-reply and heartbeats. The switches live in params (js8_auto,
 * js8_hb, js8_hb_ack, js8_hb_interval), all off by default. */
static js8_auto_t *autop;
static int64_t     hb_next_ms;       /* 0: send the first one at the next chance */
static bool        hb_adjusting;     /* main knob sets the HB interval */
static int64_t     hb_adjust_ms;     /* last knob turn while adjusting */
static char        info_text[TEXT_MAX + 1], status_text[TEXT_MAX + 1];
static char        last_pota[16], last_sota[24]; /* last park / summit spotted via APRS */
static char        last_tx_text[JS8_RX_TEXT_LEN]; /* for AGN? */
static struct {
    char    call[JS8_RX_CALL_LEN];
    char    text[JS8_RX_TEXT_LEN];
    int64_t ms;
    int     deliver_id; /* the held message it delivers, or 0 */
} offer; /* AUTO off: a reply waiting for Reply, like desktop's outgoing box */
static js8_auto_result_t pending_auto;       /* arrived while TX was busy */
static bool              pending_auto_valid;
static int64_t           pending_auto_ms;    /* when it was put off */
#define PENDING_AUTO_MS  (2 * 60 * 1000)     /* then it's too late to answer */
static int               edit_target;        /* 0 compose, else an edit_t */
static lv_obj_t         *texts_list;

static lv_obj_t *waterfall;
static lv_obj_t *finder;
static lv_obj_t *table;
static lv_obj_t *status;

static int32_t filter_low, filter_high;
static int32_t saved_filter_low, saved_filter_high; /* the user's, restored on close */
static bool    filter_saved;
static show_t  show = SHOW_NO_HB;

/* What the compose window is editing (edit_target). */
typedef enum {
    EDIT_INFO = 1,
    EDIT_STATUS,
    EDIT_LOG_GRID, /* the Log popup's fields, then back to it */
    EDIT_LOG_NAME,
    EDIT_LOG_NOTE,
    EDIT_POTA_REF, /* your park / summit for the log */
    EDIT_SOTA_REF,
    EDIT_ALERT_WORDS, /* then back to the Alerts popup */
    EDIT_COUNT,
} edit_t;

/* The Log popup's items. */
typedef enum { LOG_SAVE, LOG_GRID, LOG_NAME, LOG_NOTE, LOG_CANCEL } log_item_t;
static void log_list_open(log_item_t focus);

static void log_edit_done(const char *value);
static void alerts_show(void);

/* Message history, a ring, so the list can be rebuilt when the filter
 * changes. row_hist[] maps a table row to its history slot (-1 = info row). */
static js8_rx_msg_t history[HISTORY];
static uint16_t     hist_head;  /* next slot to write */
static uint16_t     hist_count;
static int16_t      row_hist[MAX_ROWS + 1];
static uint16_t     rows;

static unsigned cycles, cycle_decodes;

/* Waterfall PSD, touched only on the receiver's worker thread. */
static spgramf  sg;
static float   *psd;
static uint16_t nfft;
static unsigned wf_row_fill;   /* samples in the row being built (receiver thread) */

/* Rows are made per fixed amount of audio, but audio arrives in bursts; a
 * timer draws them at an even pace so the waterfall scrolls smoothly. The
 * pace comes from the system clock: an LVGL timer's period counts from when
 * it last ran (and LVGL's tick runs slow), so a 100 ms timer drew ~9.7 rows
 * a second, fell behind and caught up with a two-row jump every 2 s. */
static float     *wf_queue[WF_QUEUE];
static uint16_t   wf_queue_size[WF_QUEUE];
static unsigned   wf_q_head, wf_q_count;
static lv_timer_t *wf_timer;
static int64_t    wf_due_ms;   /* when the next row should be drawn (monotonic) */
static float    wf_floor_db;   /* smoothed noise floor (receiver thread) */
static bool     wf_floor_set;

/* ---- Buttons ---------------------------------------------------------- */

static buttons_page_t page_1;
static buttons_page_t page_2;
static buttons_page_t page_3;
static buttons_page_t page_4;
static buttons_page_t page_5;
static buttons_page_t page_6;

static button_data_t btn_p1      = {.type = BTN_TEXT, .label = "(JS8 1:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_2, .prev = &page_6};
static button_data_t btn_show    = {.type = BTN_TEXT_FN, .label_fn = show_label_getter, .press = show_cb};
static button_data_t btn_reply   = {.type = BTN_TEXT, .label = "Reply", .press = reply_cb};
static button_data_t btn_send    = {.type = BTN_TEXT, .label = "Send...", .press = send_cb};
static button_data_t btn_hw_cpy  = {.type = BTN_TEXT, .label = "HW CPY?", .press = hw_cpy_cb};

static button_data_t btn_p2    = {.type = BTN_TEXT, .label = "(JS8 2:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_3, .prev = &page_1};
static button_data_t btn_cq    = {.type = BTN_TEXT, .label = "CQ", .press = cq_cb};
static button_data_t btn_hb    = {.type = BTN_TEXT, .label = "Heart-\nbeat", .press = heartbeat_cb};
static button_data_t btn_query = {.type = BTN_TEXT, .label = "Query >", .press = query_cb};
static button_data_t btn_clear = {.type = BTN_TEXT, .label = "Clear", .press = clear_cb};

static button_data_t btn_p3        = {.type = BTN_TEXT, .label = "(JS8 3:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_4, .prev = &page_2};
static button_data_t btn_time_sync = {.type = BTN_TEXT, .label = "Time\nSync", .press = time_sync_cb};
static button_data_t btn_hold      = {.type = BTN_TEXT_FN, .label_fn = hold_label_getter, .press = hold_cb};
static button_data_t btn_stations  = {.type = BTN_TEXT_FN, .label_fn = stations_label_getter, .press = stations_cb};
static button_data_t btn_inbox     = {.type = BTN_TEXT_FN, .label_fn = inbox_label_getter, .press = inbox_cb};

static buttons_page_t page_1 = {{&btn_p1, &btn_show, &btn_reply, &btn_send, &btn_hw_cpy}};
static buttons_page_t page_2 = {{&btn_p2, &btn_cq, &btn_hb, &btn_query, &btn_clear}};
static buttons_page_t page_3 = {{&btn_p3, &btn_time_sync, &btn_hold, &btn_stations, &btn_inbox}};

static button_data_t btn_p4     = {.type = BTN_TEXT, .label = "(JS8 4:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_5, .prev = &page_3};
static button_data_t btn_auto   = {.type = BTN_TEXT_FN, .label_fn = auto_label_getter, .press = auto_cb};
static button_data_t btn_hbauto = {.type = BTN_TEXT_FN, .label_fn = hb_label_getter, .press = hb_cb, .hold = hb_hold_cb};
static button_data_t btn_hbackk = {.type = BTN_TEXT_FN, .label_fn = hb_ack_label_getter, .press = hb_ack_cb};
static button_data_t btn_texts  = {.type = BTN_TEXT, .label = "Texts...", .press = texts_cb};
static buttons_page_t page_4 = {{&btn_p4, &btn_auto, &btn_hbauto, &btn_hbackk, &btn_texts}};

static button_data_t  btn_p5        = {.type = BTN_TEXT, .label = "(JS8 5:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_6, .prev = &page_4};
static button_data_t  btn_aprs      = {.type = BTN_TEXT, .label = "APRS >", .press = aprs_cb};
static button_data_t  btn_log       = {.type = BTN_TEXT, .label = "Log QSO", .press = log_cb};
static button_data_t  btn_act       = {.type = BTN_TEXT_FN, .label_fn = act_label_getter, .press = act_cb, .hold = act_hold_cb};
static button_data_t  btn_prompt    = {.type = BTN_TEXT_FN, .label_fn = prompt_label_getter, .press = prompt_cb};
static buttons_page_t page_5        = {{&btn_p5, &btn_aprs, &btn_log, &btn_act, &btn_prompt}};

static button_data_t  btn_p6        = {.type = BTN_TEXT, .label = "(JS8 6:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_1, .prev = &page_5};
static button_data_t  btn_alerts    = {.type = BTN_TEXT, .label = "Alerts >", .press = alerts_cb};
static button_data_t  btn_speed     = {.type = BTN_TEXT_FN, .label_fn = speed_label_getter, .press = speed_cb, .hold = speed_hold_cb};
static button_data_t  btn_decode    = {.type = BTN_TEXT_FN, .label_fn = decode_label_getter, .press = decode_cb};
static buttons_page_t page_6        = {{&btn_p6, &btn_alerts, &btn_speed, &btn_decode}};

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
    case SHOW_DIRECTED:
        /* Also everything on the selected station's frequency: in a long
         * QSO they often stop putting your call in. */
        /* 'On their frequency': within the decode's speed's rxThreshold, as desktop. */
        if (qso_freq >= 0 &&
            fabsf(m->freq_hz - qso_freq) <= js8_speed_rx_threshold_hz(js8_speed_from_submode(m->submode)))
            return true;
        return m->to_me || (m->to_group && !m->heartbeat && !m->cq);
    default:            return true;
    }
}

static void format_row(const js8_rx_msg_t *m, char *buf, size_t size) {
    int hh = m->utc / 10000, mm = (m->utc / 100) % 100, ss = m->utc % 100;

    /* Speed letter outside Normal: " F", " T", " S" (desktop's speed column). */
    js8_speed_t sp       = js8_speed_from_submode(m->submode);
    char        speed[4] = "";
    if (sp != JS8_SPEED_NORMAL) snprintf(speed, sizeof(speed), " %c", js8_speed_letter(sp));

    if (m->tx) {
        snprintf(buf, size, "%02d:%02d:%02d  TX %4.0f%s  %s", hh, mm, ss, m->freq_hz, speed, m->text);
        return;
    }

    snprintf(buf, size, "%02d:%02d:%02d %+3d %4.0f%s  %s%s%s%s",
             hh, mm, ss, m->snr, m->freq_hz, speed,
             m->low_confidence ? "[" : "",
             m->text,
             m->low_confidence ? "]" : "",
             m->partial ? " ..." : m->checksum < 0 ? "  (bad checksum)" : "");
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

/* The speed we transmit at (page 6). */
static js8_speed_t cur_speed(void) {
    return params.js8_speed.x < JS8_SPEED_COUNT ? (js8_speed_t)params.js8_speed.x : JS8_SPEED_NORMAL;
}

/* What the receiver decodes: every speed (desktop's multi-decoder, the
 * default) or only the one we transmit at. */
static int rx_speed_mask(void) {
    if (!params.js8_rx_all.x) return js8_speed_rx_mask(cur_speed());
    int mask = 0;
    for (int s = 0; s < JS8_SPEED_COUNT; s++) mask |= js8_speed_rx_mask((js8_speed_t)s);
    return mask;
}

static int64_t now_wall_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void handle_incoming(const js8_rx_msg_t *m);

static bool find_station(const char *call, js8_station_t *out);

/* The history slot of a message still arriving (desktop grows the line
 * each decode cycle), or -1. Only recent slots can hold one. */
static int find_partial(uint32_t msg_id) {
    if (!msg_id) return -1;
    for (int age = 0; age < hist_count && age < 100; age++) {
        int slot = (hist_head - 1 - age + HISTORY) % HISTORY;
        if (!history[slot].tx && history[slot].partial && history[slot].msg_id == msg_id) return slot;
    }
    return -1;
}

/* Everything that acts on a message, once it's complete: never on the text
 * so far of one still arriving. */
static void process_message(js8_rx_msg_t *m) {
    js8_station_t seen;
    bool          new_station = !m->tx && m->from[0] && !find_station(m->from, &seen);
    js8_stations_add(stations, m, params.callsign.x, now_wall_ms());
    if (m->tx) return;
    alert_check(m, new_station);
    if (!m->low_confidence) {
        sync_dt[sync_head] = m->dt;
        sync_ms[sync_head] = now_wall_ms();
        sync_head          = (sync_head + 1) % SYNC_DTS;
    }
    inbox_received(m);
    held_received(m);
    handle_incoming(m);
    char ended[JS8_RX_CALL_LEN];
    if (js8_qsos_received(qsos, m, params.callsign.x, now_wall_ms(), ended, sizeof(ended))) log_offer(ended);
    if (m->to_me && log_list) log_refresh();
}

/* A message that was showing as it arrived: new text in its slot, and in
 * its row if it has one (else a row now, if it passes the filter). */
static void update_slot(int slot, const js8_rx_msg_t *m) {
    history[slot] = *m;
    if (view_stations) {
        if (!m->partial) rebuild_station_rows();
        return;
    }
    char buf[JS8_RX_TEXT_LEN + 48];
    format_row(m, buf, sizeof(buf));
    for (uint16_t r = 0; r < rows; r++) {
        if (row_hist[r] == slot) {
            lv_table_set_cell_value(table, r, 0, buf);
            return;
        }
    }
    if (!passes_filter(m) || rows >= MAX_ROWS) return;
    bool scroll = at_bottom();
    append_row(buf, (int16_t)slot);
    if (scroll) follow();
}

static void add_message(const js8_rx_msg_t *msg) {
    js8_rx_msg_t  copy = *msg;
    js8_rx_msg_t *m    = &copy;

    int existing = m->tx ? -1 : find_partial(m->msg_id);
    if (!m->partial) process_message(m);
    if (existing >= 0) {
        update_slot(existing, m);
        return;
    }

    int slot = hist_head;
    history[slot] = *m;
    hist_head     = (hist_head + 1) % HISTORY;
    if (hist_count < HISTORY) hist_count++;

    /* Rows pointing at the slot we just overwrote would now show the wrong
     * message; the ring is larger than MAX_ROWS so this only happens after a
     * long session, and rebuilding fixes it. */
    if (view_stations) {
        if (!m->partial) rebuild_station_rows();
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
    char star[2], call[JS8_RX_CALL_LEN], speed[2], age[8], snr[8], heard[40], grid[8], dist[16];
} station_fields_t;

static void station_fields(const js8_station_t *st, int64_t now, station_fields_t *f) {
    memset(f, 0, sizeof(*f));
    f->star[0] = st->heard_me ? '*' : ' ';
    snprintf(f->call, sizeof(f->call), "%s", st->call);
    js8_speed_t sp = js8_speed_from_submode(st->submode);
    if (sp != JS8_SPEED_NORMAL) f->speed[0] = js8_speed_letter(sp); /* F, T, S */
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
        char hit[16];
        st_alert[i]  = js8_alert_hit("", st_rows[i].call, alert_words, hit, sizeof(hit));
        st_worked[i] = qso_log_search_worked(st_rows[i].call, MODE_JS8, qso_log_freq_to_band(cparam_i_get(cfg_fg_freq))) > 0;
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
        dsc->rect_dsc->bg_color = st_alert[h]           ? lv_color_hex(ALERT_COLOR)
                                  : st_rows[h].heard_me ? lv_color_hex(0x5a4400)
                                                        : lv_color_black();
    } else {
        const js8_rx_msg_t *m = &history[h];
        if (m->tx) {
            dsc->rect_dsc->bg_color = lv_color_hex(0x1830a0);
        } else if (m->alert) {
            dsc->rect_dsc->bg_color = lv_color_hex(ALERT_COLOR);
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
        {140, offsetof(station_fields_t, speed)}, {162, offsetof(station_fields_t, age)},
        {218, offsetof(station_fields_t, snr)},   {276, offsetof(station_fields_t, heard)},
        {520, offsetof(station_fields_t, grid)},  {610, offsetof(station_fields_t, dist)},
    };

    lv_area_t area = *dsc->draw_area;
    area.y1 += lv_obj_get_style_pad_top(obj, LV_PART_ITEMS);
    area.y2 -= lv_obj_get_style_pad_bottom(obj, LV_PART_ITEMS);
    lv_coord_t x0 = dsc->draw_area->x1 + lv_obj_get_style_pad_left(obj, LV_PART_ITEMS);

    /* Calls already in the log in green, like desktop's worked-before tick. */
    lv_draw_label_dsc_t worked = *dsc->label_dsc;
    worked.color               = lv_color_hex(0x80ff80);

    for (size_t i = 0; i < sizeof(cols) / sizeof(cols[0]); i++) {
        area.x1 = x0 + cols[i].x;
        area.x2 = (i + 1 < sizeof(cols) / sizeof(cols[0])) ? x0 + cols[i + 1].x - 6 : dsc->draw_area->x2;
        bool green = cols[i].field == offsetof(station_fields_t, call) && st_worked[h];
        lv_draw_label(dsc->draw_ctx, green ? &worked : dsc->label_dsc, &area, (const char *)&f + cols[i].field, NULL);
    }
}

/* Mark the selected message's offset on the waterfall; `announce` also
 * pops up who it is (on a tap, not on every MFK step). */
static void mark_selected(bool announce) {
    uint16_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row >= rows || row_hist[row] < 0) {
        qso_freq = -1;
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
    qso_freq = (!view_stations && history[row_hist[row]].tx) ? -1 : freq; /* not our own rows */
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
            if (!js8_speed_heartbeats(cur_speed())) {
                snprintf(hb, sizeof(hb), "HB paused (Turbo)  ");
            } else if (hb_next_ms > now_ms) {
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
    int unread = js8_inbox_unread(inbox);
    if (unread) {
        char m[24];
        snprintf(m, sizeof(m), "MSG %d NEW  ", unread);
        strcat(flags, m);
    }
    lv_label_set_text_fmt(status, "%s%s%s  %02d:%02d:%02dZ  total %u", flags, testing ? "TEST WAV  " : "",
                          cfg_digital_label_get(), tm.tm_hour, tm.tm_min, tm.tm_sec, hist_count);
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

static int64_t now_mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* At most one row per tick, each when it's due by the clock. Rows come a
 * little faster than 10 a second (1102 samples at 11025 Hz, and the audio
 * clock isn't the CPU's), so a queue that builds up is drained by drawing
 * slightly faster, never by a jump. */
static void wf_timer_cb(lv_timer_t *t) {
    (void)t;
    if (!wf_q_count) return;
    int64_t now    = now_mono_ms();
    int     period = 1000 / WF_ROWS_PER_SEC;
    if (wf_q_count > 6) period = period * 3 / 4;
    else if (wf_q_count > 3) period = period * 19 / 20;
    /* After a pause (no audio while transmitting) start again now. */
    if (now - wf_due_ms > period) wf_due_ms = now;
    if (now < wf_due_ms) return;

    lv_waterfall_add_data(waterfall, wf_queue[wf_q_head], wf_queue_size[wf_q_head]);
    free(wf_queue[wf_q_head]);
    wf_q_head = (wf_q_head + 1) % WF_QUEUE;
    wf_q_count--;
    wf_due_ms += period;
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
    rx = js8_rx_create(SAMPLE_RATE, rx_speed_mask(), params.callsign.x, &cb);
    if (!rx) msg_schedule_text_fmt("JS8: cannot start decoder");
    js8_rx_set_decode_range(rx, filter_low, filter_high);
    js8_rx_set_qso_offset(rx, params.js8_tx_freq.x);
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
    struct timespec until; /* a beep stops within one part; never hang TX on it */
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 1;
    if (pthread_mutex_timedlock(&speaker_lock, &until) == 0) pthread_mutex_unlock(&speaker_lock);
    /* Per frame, so a power change made while the app is open counts. */
    base_gain_offset = tx_player_base_gain_offset();
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
        m.submode      = (uint8_t)js8_speed_submode(st->speed);
        snprintf(m.text, sizeof(m.text), "%s", tx_preview[0] ? tx_preview : st->text);
        add_message(&m);
        char ended[JS8_RX_CALL_LEN];
        if (js8_qsos_sent(qsos, m.text, params.callsign.x, now_wall_ms(), ended, sizeof(ended))) log_offer(ended);
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
        snprintf(line, sizeof(line), "TX %4.0f Hz %s   %s   %s %d s  (%d/%d)", tx_status.offset_hz,
                 js8_speed_name(tx_status.speed), tx_status.text,
                 tx_status.frame == 1 ? "starts in" : "next frame in", secs, tx_status.frame, tx_status.frames);
        lv_obj_set_style_bg_color(tx_bar, lv_color_hex(0x5a4a00), 0);
        break;
    }
    case JS8_TX_KEYING:
        snprintf(line, sizeof(line), "TX %4.0f Hz %s   %s   sending %d/%d", tx_status.offset_hz,
                 js8_speed_name(tx_status.speed), tx_status.text, tx_status.frame, tx_status.frames);
        lv_obj_set_style_bg_color(tx_bar, lv_color_hex(0xa00000), 0);
        break;
    default:
        snprintf(line, sizeof(line), "TX %4u Hz %s   ready%s", offset, js8_speed_name(cur_speed()),
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
static void hb_pause(const char *why);

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
    js8_tx_preview(params.callsign.x, params.qth.x, text, cur_speed(), &pv);
    if (!pv.ok) {
        msg_update_text_fmt("JS8: %s", pv.error);
        return false;
    }

    char err[JS8_TX_ERR_LEN];
    atomic_store(&tx_offset_active, offset_hz);
    snprintf(tx_preview, sizeof(tx_preview), "%s", pv.preview);
    if (!js8_tx_send(tx, params.callsign.x, params.qth.x, text, (float)offset_hz, cur_speed(), err, sizeof(err))) {
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
    if (f > js8_speed_max_offset_hz(cur_speed())) f = js8_speed_max_offset_hz(cur_speed());
    params_uint16_set(&params.js8_tx_freq, (uint16_t)f);
    js8_rx_set_qso_offset(rx, f);
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
    if (f > js8_speed_max_offset_hz(cur_speed())) f = js8_speed_max_offset_hz(cur_speed());
    params_uint16_set(&params.js8_tx_freq, (uint16_t)f);
    js8_rx_set_qso_offset(rx, f);

    lv_finder_set_value(finder, (int16_t)f);
    lv_obj_invalidate(finder);
    update_tx_bar();
}

/* ---- Compose window --------------------------------------------------- */

static void compose_changed_cb(lv_event_t *e) {
    (void)e;
    js8_tx_preview_t pv;
    js8_tx_preview(params.callsign.x, params.qth.x, textarea_window_get(), cur_speed(), &pv);
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
    if (edit_target >= EDIT_LOG_GRID) {
        log_edit_done(textarea_window_get());
        return true;
    }
    if (edit_target) {
        char *dst = edit_target == 1 ? info_text : status_text;
        snprintf(dst, TEXT_MAX + 1, "%s", textarea_window_get());
        save_texts();
        msg_update_text_fmt("%s saved", edit_target == 1 ? "INFO" : "STATUS");
        edit_target = 0;
        compose_close();
        return true;
    }
    char text[TX_TEXT_MAX + 8];
    if (!aprs_prepare(textarea_window_get(), text, sizeof(text))) return false;
    if (!tx_queue(text)) return false; /* keep the window open */
    /* The held message offered on Reply went as offered: it's delivered. */
    if (deliver_pending.id && strcasecmp(text, deliver_pending.text) == 0) {
        js8_held_delivered(held, deliver_pending.id);
        add_info_row("Held message %d delivered", deliver_pending.id);
    }
    deliver_pending.id = 0;
    compose_close();
    return true;
}

static bool compose_cancel_cb(void) {
    deliver_pending.id = 0;
    if (edit_target >= EDIT_LOG_GRID) {
        log_edit_done(NULL);
        return true;
    }
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
    lv_textarea_set_accepted_chars(text, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-+?!\"/@:>{}_#&'(),=;");
    lv_textarea_set_max_length(text, TX_TEXT_MAX);
    lv_obj_add_event_cb(text, compose_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (edit_target) {
        lv_textarea_set_max_length(text, edit_target == EDIT_LOG_GRID   ? 6
                                         : edit_target == EDIT_POTA_REF ? sizeof(last_pota) - 1
                                         : edit_target == EDIT_SOTA_REF ? sizeof(last_sota) - 1
                                         : edit_target == EDIT_ALERT_WORDS ? sizeof(alert_words) - 1
                                                                        : TEXT_MAX);
        lv_obj_remove_event_cb(text, compose_changed_cb);
    }
    if (prefill && prefill[0]) {
        textarea_window_set(prefill);
    } else {
        static const char *const placeholders[EDIT_COUNT] = {
            [0]             = " CALL MESSAGE / @ALLCALL ...",
            [EDIT_INFO]     = " INFO, e.g. X6100 5W EFHW",
            [EDIT_STATUS]   = " STATUS, e.g. PORTABLE QRV",
            [EDIT_LOG_GRID] = " Their grid, e.g. DN17",
            [EDIT_LOG_NAME] = " Their name",
            [EDIT_LOG_NOTE] = " Comment for the log",
            [EDIT_POTA_REF] = " Your park, e.g. CA-1234",
            [EDIT_SOTA_REF] = " Your summit, e.g. VE7/LM-001",
            [EDIT_ALERT_WORDS] = " Calls or words, e.g. VE7ABC @POTA SOTA",
        };
        lv_textarea_set_placeholder_text(text, placeholders[edit_target]);
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
    qso_freq = -1;
    if (view_stations) rebuild_rows();
    add_info_row("%s", cfg_digital_label_get());
    update_status();
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    user_touch();

    switch (key) {
    case LV_KEY_ESC:
        LV_LOG_USER("JS8 ESC on the list: tick %u composing %d popup %d tx %d", (unsigned)lv_tick_get(), composing,
                    any_popup(), js8_tx_busy(tx));
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

    /* Nothing transmits on its own when the app opens: AUTO, HB and HB ACK
     * start off every time (the HB interval is remembered). */
    params_bool_set(&params.js8_auto, false);
    params_bool_set(&params.js8_hb, false);
    params_bool_set(&params.js8_hb_ack, false);

    /* Full-screen app with its own waterfall: skip main-screen DSP. */
    dsp_set_waterfall_enabled(false);
    dsp_set_spectrum_enabled(false);

    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_UP, NULL);
    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_DOWN, NULL);

    mem_save(MEM_BACKUP_ID);
    load_band(0);

    /* 200-3000 Hz while JS8 is open. High first: each edge is validated
     * against the other. */
    saved_filter_low  = cparam_i_get(cfg_cur_filter_low);
    saved_filter_high = cparam_i_get(cfg_cur_filter_high);
    filter_saved      = true;
    cparam_i_set(cfg_cur_filter_high, JS8_FILTER_HIGH);
    cparam_i_set(cfg_cur_filter_low, JS8_FILTER_LOW);

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
    wf_due_ms = 0;
    wf_timer  = lv_timer_create(wf_timer_cb, WF_TICK_MS, NULL);
    lv_obj_set_pos(waterfall, 13, 13);

    /* Finder marks the offset of the selected message. */

    finder = lv_finder_create(waterfall);
    lv_finder_set_range(finder, filter_low, filter_high);
    /* A stored offset outside the usable range (an old or damaged setting)
     * would make every send fail; start from 1500 Hz instead. */
    if (params.js8_tx_freq.x < JS8_TX_MIN_OFFSET || params.js8_tx_freq.x > js8_speed_max_offset_hz(cur_speed())) {
        params_uint16_set(&params.js8_tx_freq, 1500);
    }

    /* The finder's band is our TX offset; its cursor line marks the
     * selected message. */
    lv_finder_set_width(finder, js8_speed_bandwidth_hz(cur_speed()));
    lv_finder_set_value(finder, params.js8_tx_freq.x);
    lv_finder_clear_cursor(finder);
    qso_freq = -1;
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
    lv_obj_set_style_bg_opa(table, 150, LV_PART_MAIN); /* waterfall shows through */
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
    if (!qsos) qsos = js8_qsos_create();
    if (!inbox) inbox = js8_inbox_open(JS8_INBOX_PATH);
    if (!held) held = js8_held_open(JS8_HELD_PATH);
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
    /* Delete popups now, not with query_close()/aprs_close()'s delayed
     * delete: dialog_destruct() frees dialog.obj (their parent) right after
     * this, and the delayed delete would then touch freed memory - GEN or
     * APP with the Query list open crashed the app. */
    if (query_list) {
        lv_obj_del(query_list);
        query_list = NULL;
    }
    if (aprs_list) {
        lv_obj_del(aprs_list);
        aprs_list = NULL;
    }
    if (texts_list) {
        lv_obj_del(texts_list);
        texts_list = NULL;
    }
    if (log_list) {
        lv_obj_del(log_list);
        log_list = NULL;
    }
    if (inbox_list) {
        lv_obj_del(inbox_list);
        inbox_list = NULL;
    }
    if (alerts_list) {
        lv_obj_del(alerts_list);
        alerts_list = NULL;
    }
    hb_adjusting = false;
    radio_set_pwr(param_f_get(cfg_pwr));

    rx_stop();
    wf_queue_clear();

    dsp_set_waterfall_enabled(true);
    dsp_set_spectrum_enabled(true);

    /* Your filter back, before the saved band and mode return. */
    if (filter_saved) {
        cparam_i_set(cfg_cur_filter_high, saved_filter_high);
        cparam_i_set(cfg_cur_filter_low, saved_filter_low);
        filter_saved = false;
    }
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
    if (popup_guard()) return;
    show = (show + 1) % SHOW_COUNT;
    buttons_refresh(btn);
    rebuild_rows();
}

static void clear_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    hist_head = hist_count = 0;
    js8_stations_clear(stations);
    js8_rx_clear(rx);
    lv_waterfall_clear_data(waterfall);
    wf_queue_clear();
    lv_finder_clear_cursor(finder);
    qso_freq = -1;
    rebuild_rows();
    update_status();
}

/* Time Sync, like desktop JS8Call's drift tool: a decode's DT is how late
 * the signal started by our clock, so the median DT of recent decodes is
 * how fast our clock runs. Shift the clock by that and save it to the
 * battery-backed RTC, as the settings screen does. Needs the clock within
 * a couple of seconds already (or nothing decodes): set it roughly in
 * SETTINGS first. (Snapping to the nearest 15 s, as the FT8 app does, moves
 * the clock up to 7 s the wrong way unless pressed exactly as a signal
 * starts.) */
static void time_sync_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    int64_t  now = now_wall_ms();
    float    dts[SYNC_DTS];
    unsigned n = 0;
    for (unsigned i = 0; i < SYNC_DTS; i++)
        if (sync_ms[i] && now - sync_ms[i] <= SYNC_WINDOW_MS) dts[n++] = sync_dt[i];

    float corr;
    if (!js8_clock_correction(dts, n, &corr)) {
        msg_update_text_fmt("Time Sync needs %d decodes in the last 2 min (have %u). "
                            "Clock far off? Set it in SETTINGS first",
                            JS8_SYNC_MIN_DECODES, n);
        return;
    }
    if (fabsf(corr) < 0.05f) {
        msg_update_text_fmt("Clock is on time (within 0.05 s of %u decodes)", n);
        return;
    }

    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    int64_t ns = (int64_t)tp.tv_sec * 1000000000LL + tp.tv_nsec + (int64_t)(corr * 1e9f);
    tp.tv_sec  = ns / 1000000000LL;
    tp.tv_nsec = ns % 1000000000LL;
    if (clock_settime(CLOCK_REALTIME, &tp) != 0) {
        msg_update_text_fmt("Can't set the clock: %s", strerror(errno));
        return;
    }
    memset(sync_ms, 0, sizeof(sync_ms)); /* those DTs are stale now */

    /* Keep it across power-off (the kernel reads rtc1 at boot). */
    struct tm tm;
    gmtime_r(&tp.tv_sec, &tm);
    struct rtc_time rt = {.tm_sec = tm.tm_sec, .tm_min = tm.tm_min, .tm_hour = tm.tm_hour,
                          .tm_mday = tm.tm_mday, .tm_mon = tm.tm_mon, .tm_year = tm.tm_year};
    int fd = open("/dev/rtc1", O_WRONLY);
    if (fd >= 0) {
        if (ioctl(fd, RTC_SET_TIME, &rt) != 0) LV_LOG_ERROR("Can't set RTC: %s", strerror(errno));
        close(fd);
    }
    msg_update_text_fmt("Clock moved %+.2f s (median of %u decodes)", corr, n);
    add_info_row("Time Sync: clock moved %+.2f s", corr);
}

static void reply_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
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
        deliver_pending.id = offer.deliver_id;
        snprintf(deliver_pending.text, sizeof(deliver_pending.text), "%s", offer.text);
        offer.text[0] = '\0';
        apply_hold(freq);
        compose_open(text);
        speed_warn();
        return;
    }
    char prefill[JS8_RX_CALL_LEN + 2];
    snprintf(prefill, sizeof(prefill), "%s ", call);
    apply_hold(freq);
    compose_open(prefill);
    speed_warn();
}

static void send_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    compose_open(NULL);
}

static void stop_tx_cb(button_data_t *btn) {
    user_touch();
    popup_guard(); /* close any list, but always stop */
    (void)btn;
    if (!js8_tx_busy(tx)) {
        msg_update_text_fmt("Not sending");
        return;
    }
    js8_tx_stop(tx);
    msg_update_text_fmt("Stopping TX");
}

/* "CALL HW CPY?" to the selected station: desktop's usual first answer to a
 * CQ ("how do you copy?"). Stopping TX is ESC or the top knob's press. */
static void hw_cpy_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard()) return;
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    if (!selected_station(call, sizeof(call), &freq, &snr)) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }
    char text[JS8_RX_CALL_LEN + 12];
    snprintf(text, sizeof(text), "%s HW CPY?", call);
    apply_hold(freq);
    if (tx_queue(text)) speed_warn();
}

/* CQ with the 4-character grid, as desktop JS8Call sends it. */
static void cq_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    char text[32];
    snprintf(text, sizeof(text), "CQ CQ CQ %.4s", params.qth.x);
    if (tx_queue(text)) hb_pause("CQ");
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
    if (!js8_speed_heartbeats(cur_speed())) {
        if (!automatic) msg_update_text_fmt("No heartbeats in Turbo, as in desktop JS8Call");
        return false;
    }
    char text[48];
    js8_heartbeat_text(params.callsign.x, params.qth.x, text, sizeof(text));
    return tx_queue_at(text, free_hb_offset(), automatic);
}

static void heartbeat_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    send_heartbeat(false);
}

static const char *hold_label_getter(void) {
    return params.js8_hold_offset.x ? "Hold:\nOn" : "Hold:\nOff";
}

static void hold_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    params_bool_set(&params.js8_hold_offset, !params.js8_hold_offset.x);
    buttons_refresh(btn);
    msg_update_text_fmt(params.js8_hold_offset.x ? "Replies stay on your offset" : "Replies move to their offset");
}

static const char *stations_label_getter(void) {
    return view_stations ? "Show\nMessages" : "Show\nStations";
}

static void stations_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
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

/* Every list popup that's open. */
static void close_popups(void) {
    if (!any_popup()) return;
    query_close();
    texts_close();
    aprs_close();
    log_close();
    inbox_close();
    alerts_close();
}

/* One-press messages for the selected station: MFK to move, press or tap
 * to send, ESC to close. */
/* A list popup (Query, Texts..., APRS) must close before anything else
 * happens, or it's left behind with its buttons still holding the knob.
 * Any other bottom button just closes it; press again to do the thing. */
static bool popup_guard(void) {
    if (!any_popup()) return false;
    close_popups();
    msg_update_text_fmt("List closed");
    return true;
}

/* Changing page closes a list too (then changes page). */
static void js8_next_page_cb(button_data_t *btn) {
    close_popups();
    button_next_page_cb(btn);
}

/* Holding the page button goes back one, as on the main screen. */
static void js8_prev_page_cb(button_data_t *btn) {
    close_popups();
    button_prev_page_cb(btn);
}

/* Scroll a list to the focused item in one step: the default animated
 * scroll redraws the list for every animation frame, which tears on the
 * radio's display. */
static void list_item_focused_cb(lv_event_t *e) {
    lv_obj_scroll_to_view(lv_event_get_target(e), LV_ANIM_OFF);
}

static lv_obj_t *list_add_item(lv_obj_t *list, const char *label) {
    lv_obj_t *b = lv_list_add_btn(list, NULL, label);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x303030), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1830a0), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(b, lv_color_white(), 0);
    lv_obj_set_style_pad_ver(b, 6, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_event_cb(b, list_item_focused_cb, LV_EVENT_FOCUSED, NULL);
    return b;
}

/* The Query list's message items: 0 MSG..., 1 MSG TO:..., 2 QUERY MSGS. */
static void query_msg_cb(lv_event_t *e) {
    int   which = (int)(intptr_t)lv_event_get_user_data(e);
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    bool  have = selected_station(call, sizeof(call), &freq, &snr);
    if (which == 2 || !have) {
        query_close();
        if (!have) return;
        apply_hold(freq);
        char text[JS8_RX_CALL_LEN + 16];
        snprintf(text, sizeof(text), "%s QUERY MSGS", call);
        tx_queue(text);
        return;
    }
    /* Into the keyboard: see texts_item_cb. */
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(query_list); i++) lv_group_remove_obj(lv_obj_get_child(query_list, i));
    lv_obj_del_async(query_list);
    query_list = NULL;
    apply_hold(freq);
    msg_compose(call, which == 0 ? "MSG " : "MSG TO:");
}

static void query_close_cb(lv_event_t *e) {
    (void)e;
    query_close();
}

static void query_cb(button_data_t *btn) {
    user_touch();
    (void)btn;
    if (query_list) { /* Query > again closes it */
        query_close();
        return;
    }
    if (popup_guard()) return;
    if (composing || aprs_list || texts_list) return;
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
        lv_obj_t *b = list_add_item(query_list, js8_query_label((js8_query_t)q));
        lv_obj_add_event_cb(b, query_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)q);
        lv_obj_add_event_cb(b, query_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        if (!first) first = b;
    }
    static const char *const msg_items[] = {"Message...", "Message via them...", "Any messages?"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = list_add_item(query_list, msg_items[i]);
        lv_obj_set_style_text_color(b, lv_color_hex(0x80ff80), 0);
        lv_obj_add_event_cb(b, query_msg_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, query_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
    }
    /* Last, so one step back from the first item (the group wraps). */
    lv_obj_t *close = list_add_item(query_list, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_obj_add_event_cb(close, query_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(close, query_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, close);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
    speed_warn();
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

    if (js8_tx_busy(tx) || composing || any_popup()) {
        if (!pending_auto_valid || strcmp(pending_auto.text, r->text) != 0) pending_auto_ms = now;
        pending_auto       = *r; /* newest wins */
        pending_auto_valid = true;
        return;
    }
    if (r->hb_ack && !js8_speed_heartbeats(cur_speed())) return; /* desktop: no HB ACKs in Turbo */
    int offset = r->hb_ack ? free_hb_offset() : params.js8_tx_freq.x;
    LV_LOG_USER("JS8 auto: '%s' at %d Hz", r->text, offset);
    if (tx_queue_at(r->text, offset, true)) {
        js8_auto_sent(autop, r, now);
        if (r->deliver_id) js8_held_delivered(held, r->deliver_id);
        add_info_row("Auto: %s", r->text);
    }
}

/* Desktop pauses heartbeats during a QSO; here they switch off until you
 * turn them back on (the user's choice). A CQ does the same: its answers
 * start a QSO. */
static void hb_pause(const char *why) {
    if (!params.js8_hb.x && !params.js8_hb_ack.x) return;
    params_bool_set(&params.js8_hb, false);
    params_bool_set(&params.js8_hb_ack, false);
    hb_next_ms   = 0;
    hb_adjusting = false;
    if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    msg_update_text_fmt("Heartbeats off: %s", why);
    add_info_row("HB and HB ACK off: %s", why);
    update_status();
}

static void qso_started(const char *call) {
    char why[JS8_RX_CALL_LEN + 12];
    snprintf(why, sizeof(why), "QSO with %s", call);
    hb_pause(why);
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
        .held      = held,
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
        offer.ms         = now_wall_ms();
        offer.deliver_id = r.deliver_id;
        if (strcmp(r.command, "MSG") == 0) {
            msg_update_text_fmt("Message from %s - select it and press Reply to send ACK", r.to);
            add_info_row("%s: Reply sends \"%s\" (AUTO sends it by itself)", r.to, r.text);
        } else if (r.deliver_id) {
            msg_update_text_fmt("%s asks for the message held for them - select it and press Reply to send it", r.to);
            add_info_row("%s asks for held message %d: Reply sends it", r.to, r.deliver_id);
        } else if (strcmp(r.command, "HW CPY?") == 0) {
            msg_update_text_fmt("%s asks how you copy - select it and press Reply", r.to);
            add_info_row("%s asked HW CPY?: Reply has \"%s\"", r.to, r.text);
        } else if (strcmp(r.command, "MSG ID") == 0) {
            msg_update_text_fmt("%s holds a message for you - select it and press Reply to fetch it", r.to);
            add_info_row("%s holds a message for you: Reply sends \"%s\"", r.to, r.text);
        } else {
            msg_update_text_fmt("%s asked %s - select it and press Reply to answer", r.to, r.command);
            add_info_row("%s asked %s: Reply sends \"%s\"", r.to, r.command, r.text);
        }
        break;
    default:
        break;
    }
}

/* Once a second: send a heartbeat when one is due. */
static void hb_tick(void) {
    if (hb_adjusting && now_wall_ms() - hb_adjust_ms > HB_ADJUST_MS) hb_adjust_end();
    /* An automatic reply put off by a list or the keyboard: send it once
     * they close (after a transmission, ui_tx_done does the same). */
    if (pending_auto_valid) {
        if (now_wall_ms() - pending_auto_ms > PENDING_AUTO_MS) pending_auto_valid = false;
        else if (!js8_tx_busy(tx) && !composing && !any_popup()) {
            pending_auto_valid = false;
            auto_send(&pending_auto);
        }
    }
    if (!params.js8_hb.x) {
        hb_next_ms = 0;
        return;
    }
    int64_t now = now_wall_ms();
    if (js8_auto_idle(autop, now) || !js8_speed_heartbeats(cur_speed())) return;
    /* As on desktop, the first one comes an interval after switching on;
     * page 2's Heartbeat sends one now. */
    if (hb_next_ms == 0) {
        hb_next_ms = js8_next_heartbeat_ms(now, params.js8_hb_interval.x);
        update_status();
    }
    if (now < hb_next_ms - 5000) return;   /* desktop prepares it 5 s early */
    if (js8_tx_busy(tx) || composing || any_popup() || !params.callsign.x[0]) return;
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
    if (popup_guard()) return;
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
    if (popup_guard()) return;
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
    if (popup_guard()) return;
    hb_adjust_start(btn);
}

static const char *hb_ack_label_getter(void) {
    if (!params.js8_hb_ack.x) return "HB ACK:\nOff";
    return (params.js8_auto.x && params.js8_hb.x) ? "HB ACK:\nOn" : "HB ACK:\nOn (idle)";
}

static void hb_ack_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    params_bool_set(&params.js8_hb_ack, !params.js8_hb_ack.x);
    buttons_refresh(btn);
    if (params.js8_hb_ack.x && !(params.js8_auto.x && params.js8_hb.x)) {
        msg_update_text_fmt("HB ACK acts only while AUTO and HB are on");
    }
    update_status();
}

/* ---- INFO / STATUS texts -------------------------------------------- */

static void load_texts(void) {
    info_text[0] = status_text[0] = last_pota[0] = last_sota[0] = alert_words[0] = '\0';
    FILE *f = fopen(JS8_TEXTS_PATH, "r");
    if (!f) return;
    char line[sizeof(alert_words) + 16];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "INFO=", 5) == 0) snprintf(info_text, sizeof(info_text), "%s", line + 5);
        if (strncmp(line, "STATUS=", 7) == 0) snprintf(status_text, sizeof(status_text), "%s", line + 7);
        if (strncmp(line, "POTA=", 5) == 0) snprintf(last_pota, sizeof(last_pota), "%s", line + 5);
        if (strncmp(line, "SOTA=", 5) == 0) snprintf(last_sota, sizeof(last_sota), "%s", line + 5);
        if (strncmp(line, "ALERTS=", 7) == 0) js8_alert_words_normalise(line + 7, alert_words, sizeof(alert_words));
    }
    fclose(f);
}

static void save_texts(void) {
    FILE *f = fopen(JS8_TEXTS_PATH, "w");
    if (!f) {
        msg_update_text_fmt("Can't write %s", JS8_TEXTS_PATH);
        return;
    }
    fprintf(f, "INFO=%s\nSTATUS=%s\nPOTA=%s\nSOTA=%s\nALERTS=%s\n", info_text, status_text, last_pota, last_sota,
            alert_words);
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

static void texts_close_cb(lv_event_t *e) {
    (void)e;
    texts_close();
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
    if (texts_list) { /* Texts... again closes it */
        texts_close();
        return;
    }
    if (popup_guard()) return;
    if (query_list || aprs_list || composing) return;
    lv_group_remove_obj(table);
    texts_list = lv_list_create(dialog.obj);
    lv_obj_set_size(texts_list, 520, 200);
    lv_obj_align(texts_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(texts_list, &sony_24, 0);
    lv_obj_set_style_bg_color(texts_list, lv_color_hex(0x202020), 0);
    lv_list_add_text(texts_list, "Sent by AUTO for INFO? / STATUS?");

    for (int i = 1; i <= 2; i++) {
        char        label[TEXT_MAX + 16];
        const char *v = i == 1 ? info_text : status_text;
        snprintf(label, sizeof(label), "%s: %s", i == 1 ? "INFO" : "STATUS", v[0] ? v : "(not set)");
        lv_obj_t *b = list_add_item(texts_list, label);
        lv_obj_add_event_cb(b, texts_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, texts_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        if (i == 1) lv_group_focus_obj(b);
    }
    lv_obj_t *close = list_add_item(texts_list, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_obj_add_event_cb(close, texts_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(close, texts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, close);
    lv_group_set_editing(keyboard_group, false);
}

/* ---- APRS via @APRSIS -------------------------------------------------- */

/* JS8Call stations with "spot to APRS" on forward these to APRS-IS; formats
 * as desktop JS8Call and KF7MIX's JS8Spotter send them. Raw packets are
 * "@APRSIS CMD :<addressee padded to 9>:<text>"; APRS allows 67 characters
 * of text. The gateway sends you as your plain callsign (no SSID). */
#define APRS_CMD      "@APRSIS CMD :"
#define APRS_TEXT_MAX 67

typedef enum {
    APRS_GRID,
    APRS_GPS,
    APRS_POTA,
    APRS_SOTA,
    APRS_SMS,
    APRS_EMAIL,
    APRS_WL_START,
    APRS_WL_TEXT,
    APRS_WL_SEND,
    APRS_COUNT
} aprs_item_t;

static const char *const aprs_labels[APRS_COUNT] = {
    "Spot my grid", "Spot GPS position", "POTA spot", "SOTA spot", "SMS text", "Email",
    "Winlink: start", "Winlink: text", "Winlink: send",
};

static unsigned aprs_msg_id;

/* Before sending anything typed: APRS CMDs get checked, SMS/email/Winlink a
 * message ID like JS8Spotter's "{01}", and POTA/SOTA refs are remembered.
 * Anything else passes through unchanged. */
static bool aprs_prepare(const char *in, char *out, size_t size) {
    snprintf(out, size, "%s", in);
    size_t pre = strlen(APRS_CMD);
    if (strncmp(in, APRS_CMD, pre) != 0) return true;
    if (strlen(in) < pre + 10 || in[pre + 9] != ':') {
        msg_update_text_fmt("APRS: the addressee is 9 characters, then ':'");
        return false;
    }
    char to[10];
    memcpy(to, in + pre, 9);
    to[9] = '\0';
    for (int i = 8; i >= 0 && to[i] == ' '; i--) to[i] = '\0';
    const char *body = in + pre + 10;
    while (*body == ' ') body++;
    if (!*body) {
        msg_update_text_fmt("APRS: nothing to send to %s", to);
        return false;
    }

    bool want_id = !strcmp(to, "SMS") || !strcmp(to, "EMAIL-2") || !strcmp(to, "WLNK-1");
    if (want_id && !strchr(body, '{')) {
        if (!aprs_msg_id) aprs_msg_id = (unsigned)(time(NULL) % 90);
        aprs_msg_id = aprs_msg_id % 99 + 1;
        snprintf(out, size, "%s{%02u}", in, aprs_msg_id);
    }
    size_t text_len = strlen(out) - pre - 10;
    if (text_len > APRS_TEXT_MAX) {
        msg_update_text_fmt("APRS allows %d characters after the addressee, this is %u: shorten it",
                            APRS_TEXT_MAX, (unsigned)text_len);
        return false;
    }

    /* "CALL PARK FREQ MODE ..." / "SUMMIT FREQ MODE ..." */
    char w1[24] = "", w2[24] = "";
    sscanf(body, "%23s %23s", w1, w2);
    if (!strcmp(to, "POTAGW") && w2[0]) {
        snprintf(last_pota, sizeof(last_pota), "%s", w2);
        save_texts();
    } else if (!strcmp(to, "APRS2SOTA") && w1[0]) {
        snprintf(last_sota, sizeof(last_sota), "%s", w1);
        save_texts();
    }
    return true;
}

/* The keyboard with `head` + `tail`, the cursor between them. */
static void aprs_compose(const char *head, const char *tail) {
    char text[TX_TEXT_MAX + 1];
    snprintf(text, sizeof(text), "%s%s", head, tail);
    compose_open(text);
    if (composing) lv_textarea_set_cursor_pos(textarea_window_text(), (int32_t)strlen(head));
}

static void aprs_send_grid(void) {
    char grid[8];
    snprintf(grid, sizeof(grid), "%.6s", params.qth.x);
    if (strlen(grid) < 4) {
        msg_update_text_fmt("Set your grid first: APP > QTH");
        return;
    }
    char text[32];
    snprintf(text, sizeof(text), "@APRSIS GRID %s", grid);
    if (tx_queue(text)) add_info_row("APRS: spotting %s at %s", params.callsign.x, grid);
}

/* From the firmware's gps.c (gpsd): the latest fix and its age. */
bool gps_last_fix(double *lat, double *lon, int *age_s);

/* A GPS on the radio: spot a 10-character grid (about 20 x 35 m); APRS
 * gateways turn grids of any length into a position. */
static void aprs_send_gps(void) {
    double lat, lon;
    int    age;
    if (!gps_last_fix(&lat, &lon, &age)) {
        msg_update_text_fmt("No GPS fix: plug in a GPS and wait for a fix (APP > GPS shows it)");
        return;
    }
    if (age > 120) {
        msg_update_text_fmt("No current GPS fix (last one %d min ago)", age / 60);
        return;
    }
    char grid[12];
    if (!js8_latlon_to_grid(lat, lon, 10, grid, sizeof(grid))) {
        msg_update_text_fmt("GPS position out of range");
        return;
    }
    char text[40];
    snprintf(text, sizeof(text), "@APRSIS GRID %s", grid);
    if (tx_queue(text)) add_info_row("APRS: spotting %s at %s (GPS %.5f, %.5f)", params.callsign.x, grid, lat, lon);
}

static void aprs_close(void) {
    if (!aprs_list) return;
    lv_obj_del_async(aprs_list); /* often called from one of its buttons */
    aprs_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void aprs_item_cb(lv_event_t *e) {
    aprs_item_t item = (aprs_item_t)(intptr_t)lv_event_get_user_data(e);
    /* Straight into the keyboard for most items: take the buttons out of
     * the group first, as Texts... does, or the keyboard opens unfocused. */
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(aprs_list); i++)
        lv_group_remove_obj(lv_obj_get_child(aprs_list, i));
    lv_obj_del_async(aprs_list);
    aprs_list = NULL;

    uint64_t dial = (uint64_t)cparam_i_get(cfg_fg_freq);
    char     head[TX_TEXT_MAX + 1], tail[48];
    switch (item) {
    case APRS_GRID:
        aprs_send_grid();
        break;
    case APRS_GPS:
        aprs_send_gps();
        break;
    case APRS_POTA:
        snprintf(head, sizeof(head), APRS_CMD "POTAGW   :%s %s", params.callsign.x, last_pota);
        snprintf(tail, sizeof(tail), " %llu JS8", (unsigned long long)(dial / 1000));
        aprs_compose(head, tail);
        msg_update_text_fmt("POTA: type the park, e.g. VE-1234");
        break;
    case APRS_SOTA:
        snprintf(head, sizeof(head), APRS_CMD "APRS2SOTA:%s", last_sota);
        snprintf(tail, sizeof(tail), " %.3f DATA %s", dial / 1e6, params.callsign.x);
        aprs_compose(head, tail);
        msg_update_text_fmt("SOTA: type the summit, e.g. VE7/LM-001 (APRS2SOTA registration needed)");
        break;
    case APRS_SMS:
        aprs_compose(APRS_CMD "SMS      :@", "");
        msg_update_text_fmt("SMS (NA7Q gateway): 10-digit number, space, message");
        break;
    case APRS_EMAIL:
        aprs_compose(APRS_CMD "EMAIL-2  :", "");
        msg_update_text_fmt("Email: address, space, message");
        break;
    case APRS_WL_START:
        aprs_compose(APRS_CMD "WLNK-1   :SP ", "");
        msg_update_text_fmt("Winlink 1/3: address (or call), space, subject. Spot your grid first");
        break;
    case APRS_WL_TEXT:
        aprs_compose(APRS_CMD "WLNK-1   :", "");
        msg_update_text_fmt("Winlink 2/3: one line of the message; repeat for more lines");
        break;
    case APRS_WL_SEND:
        if (tx_queue(APRS_CMD "WLNK-1   :/EX")) msg_update_text_fmt("Winlink 3/3: sending /EX");
        break;
    default:
        break;
    }
    if (composing) lv_group_set_editing(keyboard_group, true);
    else if (table) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void aprs_close_cb(lv_event_t *e) {
    (void)e;
    aprs_close();
}

static void aprs_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    if (key == LV_KEY_ESC) aprs_close();
    else if (key == LV_KEY_LEFT || key == LV_KEY_UP) lv_group_focus_prev(keyboard_group);
    else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) lv_group_focus_next(keyboard_group);
}

static void aprs_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (aprs_list) { /* APRS > again closes it */
        aprs_close();
        return;
    }
    if (popup_guard()) return;
    if (query_list || texts_list || composing) return;
    if (!params.callsign.x[0]) {
        msg_update_text_fmt("Set your callsign first: APP > Callsign");
        return;
    }
    lv_group_remove_obj(table);
    aprs_list = lv_list_create(dialog.obj);
    lv_obj_set_size(aprs_list, 300, WF_HEIGHT - 10);
    lv_obj_align(aprs_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(aprs_list, &sony_24, 0);
    lv_obj_set_style_bg_color(aprs_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(aprs_list, lv_color_white(), 0);
    lv_obj_t *t = lv_list_add_text(aprs_list, "APRS via @APRSIS");
    lv_obj_set_style_text_font(t, &sony_22, 0);

    lv_obj_t *first = NULL;
    for (int i = 0; i < APRS_COUNT; i++) {
        lv_obj_t *b = list_add_item(aprs_list, aprs_labels[i]);
        lv_obj_add_event_cb(b, aprs_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, aprs_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        if (!first) first = b;
    }
    lv_obj_t *close = list_add_item(aprs_list, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_obj_add_event_cb(close, aprs_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(close, aprs_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, close);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

/* ---- Log QSO ----------------------------------------------------------- */

/* Like desktop JS8Call's Log QSO: the entry is filled in from the QSO and
 * goes to JS8_LOG_PATH (ADIF, on the SD card's DATA partition) and to the
 * radio's QSO database, which marks worked stations. A two-way QSO that
 * ends with 73 or SK opens it by itself (Log prompt), and nothing is
 * logged without Save. */

static bool any_popup(void) {
    return query_list || texts_list || aprs_list || log_list || inbox_list || alerts_list;
}

static bool find_station(const char *call, js8_station_t *out) {
    static js8_station_t list[MAX_ROWS];
    int                  n = js8_stations_list(stations, now_wall_ms(), list, MAX_ROWS);
    for (int i = 0; i < n; i++) {
        if (strcmp(list[i].call, call) == 0) {
            *out = list[i];
            return true;
        }
    }
    return false;
}

/* 6-character grid from a current GPS fix (portable), else the QTH setting. */
static void my_log_grid(char *out, size_t size) {
    double lat, lon;
    int    age;
    if (gps_last_fix(&lat, &lon, &age) && age <= 120 && js8_latlon_to_grid(lat, lon, 6, out, size)) return;
    snprintf(out, size, "%s", params.qth.x);
}

/* Sent: the report we gave them, else how we heard them. Rcvd: the report
 * they gave us (a message or a heartbeat ack). */
static void log_reports_fill(const js8_qso_t *q, const js8_station_t *st) {
    char *sent = log_entry.rst_sent, *rcvd = log_entry.rst_rcvd;
    if (q && q->has_sent_snr) snprintf(sent, sizeof(log_entry.rst_sent), "%+03d", q->sent_snr);
    else if (q && q->has_heard_snr) snprintf(sent, sizeof(log_entry.rst_sent), "%+03d", q->heard_snr);
    else if (st) snprintf(sent, sizeof(log_entry.rst_sent), "%+03d", st->snr);
    if (q && q->has_rcvd_snr) snprintf(rcvd, sizeof(log_entry.rst_rcvd), "%+03d", q->rcvd_snr);
    else if (st && st->has_reported_snr) snprintf(rcvd, sizeof(log_entry.rst_rcvd), "%+03d", st->reported_snr);
}

static void log_prepare(const char *call) {
    int64_t       now = now_wall_ms();
    js8_qso_t     q;
    js8_station_t st;
    bool          have_q  = js8_qsos_get(qsos, call, now, &q);
    bool          have_st = find_station(call, &st);

    memset(&log_entry, 0, sizeof(log_entry));
    log_grid_typed = false;
    snprintf(log_entry.call, sizeof(log_entry.call), "%s", have_q ? q.call : call);
    snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", have_q && q.grid[0] ? q.grid : have_st ? st.grid : "");

    log_reports_fill(have_q ? &q : NULL, have_st ? &st : NULL);

    log_entry.on_ms   = have_q ? q.start_ms : now;
    log_entry.off_ms  = now;
    log_entry.freq_hz = (uint64_t)cparam_i_get(cfg_fg_freq) + params.js8_tx_freq.x;
    snprintf(log_entry.my_call, sizeof(log_entry.my_call), "%s", params.callsign.x);
    my_log_grid(log_entry.my_grid, sizeof(log_entry.my_grid));
    float pwr          = param_f_get(cfg_pwr);
    log_entry.tx_pwr_w = pwr > TX_PLAYER_MAX_PWR_W ? TX_PLAYER_MAX_PWR_W : pwr;
    if (params.js8_log_activation.x == 1) snprintf(log_entry.pota_ref, sizeof(log_entry.pota_ref), "%s", last_pota);
    if (params.js8_log_activation.x == 2) snprintf(log_entry.sota_ref, sizeof(log_entry.sota_ref), "%s", last_sota);
}

static void log_close(void) {
    if (!log_list) return;
    lv_obj_del_async(log_list); /* often called from one of its buttons */
    log_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void log_save(void) {
    if (!log_entry.call[0] || !log_entry.my_call[0]) {
        msg_update_text_fmt("Nothing to log");
        return;
    }
    log_entry.off_ms = now_wall_ms();
    char err[96];
    if (!js8_log_append(JS8_LOG_PATH, &log_entry, err, sizeof(err))) {
        msg_update_text_fmt("Not logged: %s", err);
        return;
    }
    /* The radio's QSO database too: worked-before marks here and in FT8. */
    char            *canon = util_canonize_callsign(log_entry.call, false);
    qso_log_record_t rec   = qso_log_record_create(
        log_entry.my_call, canon ? canon : log_entry.call, (time_t)(log_entry.on_ms / 1000), MODE_JS8,
        atoi(log_entry.rst_sent), atoi(log_entry.rst_rcvd), log_entry.freq_hz, log_entry.name[0] ? log_entry.name : NULL,
        NULL, log_entry.my_grid, log_entry.grid);
    free(canon);
    qso_log_record_save(rec);

    js8_qsos_logged(qsos, log_entry.call);
    if (strcmp(log_pending, log_entry.call) == 0) log_pending[0] = '\0';
    const char *band = js8_log_band(log_entry.freq_hz);
    msg_update_text_fmt("Logged %s%s%s", log_entry.call, band[0] ? " on " : "", band);
    add_info_row("Logged %s %s %s/%s%s%s", log_entry.call, band, log_entry.rst_sent[0] ? log_entry.rst_sent : "-",
                 log_entry.rst_rcvd[0] ? log_entry.rst_rcvd : "-", log_entry.pota_ref[0] ? " POTA " : "",
                 log_entry.pota_ref[0] ? log_entry.pota_ref : log_entry.sota_ref);
    if (view_stations) rebuild_station_rows();
}

static void log_item_cb(lv_event_t *e) {
    log_item_t item = (log_item_t)(intptr_t)lv_event_get_user_data(e);
    if (item == LOG_SAVE || item == LOG_CANCEL) {
        if (item == LOG_SAVE) log_save();
        else log_pending[0] = '\0';
        log_close();
        return;
    }
    /* A field: into the keyboard, then back here (see texts_item_cb). */
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(log_list); i++) lv_group_remove_obj(lv_obj_get_child(log_list, i));
    lv_obj_del_async(log_list);
    log_list    = NULL;
    edit_target = item == LOG_GRID ? EDIT_LOG_GRID : item == LOG_NAME ? EDIT_LOG_NAME : EDIT_LOG_NOTE;
    compose_open(item == LOG_GRID ? log_entry.grid : item == LOG_NAME ? log_entry.name : log_entry.comment);
    lv_group_set_editing(keyboard_group, true);
}

static void log_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    if (key == LV_KEY_ESC) log_close();
    else if (key == LV_KEY_LEFT || key == LV_KEY_UP) lv_group_focus_prev(keyboard_group);
    else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) lv_group_focus_next(keyboard_group);
}

static lv_obj_t *log_add(log_item_t item, const char *label) {
    lv_obj_t *b = list_add_item(log_list, label);
    lv_obj_add_event_cb(b, log_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    lv_obj_add_event_cb(b, log_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    return b;
}

static void log_reports_line(char *line, size_t size) {
    snprintf(line, size, "Sent %s  Rcvd %s  %.3f MHz  %.0f W", log_entry.rst_sent[0] ? log_entry.rst_sent : "-",
             log_entry.rst_rcvd[0] ? log_entry.rst_rcvd : "-", log_entry.freq_hz / 1e6, log_entry.tx_pwr_w);
}

/* They sent something while the popup is open (often their 73 after ours
 * opened it): take in a new report or grid, in place, focus unchanged. */
static void log_refresh(void) {
    js8_qso_t q;
    if (!log_list || !js8_qsos_get(qsos, log_entry.call, now_wall_ms(), &q)) return;
    log_reports_fill(&q, NULL);
    char line[160];
    log_reports_line(line, sizeof(line));
    lv_label_set_text(log_reports, line);
    if (!log_grid_typed && q.grid[0] && strcmp(q.grid, log_entry.grid) != 0) {
        snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", q.grid);
        snprintf(line, sizeof(line), "Grid: %s", log_entry.grid);
        lv_label_set_text(lv_obj_get_child(log_grid_btn, 0), line);
    }
}

/* `focus`: the item to start on - Save, or the field just edited. */
static void log_list_open(log_item_t focus) {
    lv_group_remove_obj(table);
    log_list = lv_list_create(dialog.obj);
    lv_obj_set_size(log_list, 560, WF_HEIGHT - 10);
    lv_obj_align(log_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(log_list, &sony_24, 0);
    lv_obj_set_style_bg_color(log_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(log_list, lv_color_white(), 0);

    char      line[160], on[8], off[8];
    time_t    t_on = (time_t)(log_entry.on_ms / 1000), t_off = (time_t)(log_entry.off_ms / 1000);
    struct tm tm;
    strftime(on, sizeof(on), "%H:%M", gmtime_r(&t_on, &tm));
    strftime(off, sizeof(off), "%H:%M", gmtime_r(&t_off, &tm));
    snprintf(line, sizeof(line), "Log %s  %s  %s-%s UTC", log_entry.call, js8_log_band(log_entry.freq_hz), on, off);
    lv_obj_t *t = lv_list_add_text(log_list, line);
    lv_obj_set_style_text_font(t, &sony_22, 0);
    log_reports_line(line, sizeof(line));
    log_reports = lv_list_add_text(log_list, line);
    lv_obj_set_style_text_font(log_reports, &sony_22, 0);

    lv_obj_t *items[LOG_CANCEL + 1];
    items[LOG_SAVE] = log_add(LOG_SAVE, "Save to log");
    lv_obj_set_style_text_color(items[LOG_SAVE], lv_color_hex(0x80ff80), 0);
    snprintf(line, sizeof(line), "Grid: %s", log_entry.grid[0] ? log_entry.grid : "(none)");
    log_grid_btn = items[LOG_GRID] = log_add(LOG_GRID, line);
    snprintf(line, sizeof(line), "Name: %s", log_entry.name[0] ? log_entry.name : "(none)");
    items[LOG_NAME] = log_add(LOG_NAME, line);
    snprintf(line, sizeof(line), "Comment: %s", log_entry.comment[0] ? log_entry.comment : "(none)");
    items[LOG_NOTE] = log_add(LOG_NOTE, line);
    if (log_entry.pota_ref[0] || log_entry.sota_ref[0]) {
        snprintf(line, sizeof(line), "Activating %s %s", log_entry.pota_ref[0] ? "POTA" : "SOTA",
                 log_entry.pota_ref[0] ? log_entry.pota_ref : log_entry.sota_ref);
        t = lv_list_add_text(log_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
    }
    items[LOG_CANCEL] = log_add(LOG_CANCEL, "Cancel");
    lv_obj_set_style_text_color(items[LOG_CANCEL], lv_color_hex(0xffc040), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(items[focus]);
}

/* Back from the keyboard: keep the value (NULL: cancelled), reopen. */
static void log_edit_done(const char *text) {
    /* Copy first: closing the window frees the text. */
    char  buf[sizeof(alert_words)];
    char *value = NULL;
    if (text) value = strncpy(buf, text, sizeof(buf) - 1), buf[sizeof(buf) - 1] = '\0', buf;
    int target  = edit_target;
    edit_target = 0;
    compose_close();
    if (value) {
        switch (target) {
        case EDIT_LOG_GRID:
            snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", value);
            log_grid_typed = true;
            break;
        case EDIT_LOG_NAME:
            snprintf(log_entry.name, sizeof(log_entry.name), "%s", value);
            break;
        case EDIT_LOG_NOTE:
            snprintf(log_entry.comment, sizeof(log_entry.comment), "%s", value);
            break;
        case EDIT_POTA_REF:
            snprintf(last_pota, sizeof(last_pota), "%s", value);
            save_texts();
            break;
        case EDIT_SOTA_REF:
            snprintf(last_sota, sizeof(last_sota), "%s", value);
            save_texts();
            break;
        case EDIT_ALERT_WORDS:
            js8_alert_words_normalise(value, alert_words, sizeof(alert_words));
            save_texts();
            if (view_stations) rebuild_station_rows();
            break;
        }
    }
    if (target == EDIT_ALERT_WORDS) {
        alerts_show();
        return;
    }
    if (target == EDIT_POTA_REF || target == EDIT_SOTA_REF) {
        if (btn_act.disp_btn) buttons_refresh(&btn_act);
        return;
    }
    /* Back on the field just edited: Save is a separate, deliberate step. */
    log_list_open(target == EDIT_LOG_GRID ? LOG_GRID : target == EDIT_LOG_NAME ? LOG_NAME : LOG_NOTE);
}

static void log_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (log_list) { /* Log QSO again closes it */
        log_close();
        return;
    }
    if (popup_guard()) return;
    if (composing) return;
    if (!params.callsign.x[0]) {
        msg_update_text_fmt("Set your callsign first: APP > Callsign");
        return;
    }
    /* A QSO that just ended, else the selected station. */
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    if (log_pending[0]) snprintf(call, sizeof(call), "%s", log_pending);
    else if (!selected_station(call, sizeof(call), &freq, &snr)) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }
    log_prepare(call);
    log_list_open(LOG_SAVE);
}

/* A two-way QSO ended with 73 / SK. */
static void log_offer(const char *call) {
    snprintf(log_pending, sizeof(log_pending), "%s", call);
    if (!params.js8_log_prompt.x) {
        add_info_row("QSO with %s ended: Log QSO (page 5) to log it", call);
        return;
    }
    if (composing || any_popup()) {
        msg_update_text_fmt("QSO with %s ended: Log QSO (page 5) to log it", call);
        add_info_row("QSO with %s ended: Log QSO (page 5) to log it", call);
        return;
    }
    log_prepare(call);
    log_list_open(LOG_SAVE);
    msg_update_text_fmt("QSO with %s ended - Save to log?", call);
}

static const char *prompt_label_getter(void) {
    return params.js8_log_prompt.x ? "Log\nprompt: On" : "Log\nprompt: Off";
}

static void prompt_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    params_bool_set(&params.js8_log_prompt, !params.js8_log_prompt.x);
    buttons_refresh(btn);
    msg_update_text_fmt(params.js8_log_prompt.x ? "Offer to log when a QSO ends with 73 or SK"
                                                : "No log prompt: use Log QSO");
}

/* Activating a park or summit: MY_SIG / MY_SOTA_REF in the log. The
 * reference is the one last spotted via APRS, or set by holding this. */
static const char *act_label_getter(void) {
    static char label[40];
    switch (params.js8_log_activation.x) {
    case 1:
        snprintf(label, sizeof(label), "POTA\n%s", last_pota[0] ? last_pota : "(hold: set)");
        break;
    case 2:
        snprintf(label, sizeof(label), "SOTA\n%s", last_sota[0] ? last_sota : "(hold: set)");
        break;
    default:
        snprintf(label, sizeof(label), "Activ.:\nOff");
        break;
    }
    return label;
}

static void act_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    uint8_t v = (params.js8_log_activation.x + 1) % 3;
    params_uint8_set(&params.js8_log_activation, v);
    buttons_refresh(btn);
    const char *ref = v == 1 ? last_pota : last_sota;
    if (v == 0) msg_update_text_fmt("Not activating: no park or summit in the log");
    else if (!ref[0]) msg_update_text_fmt("Hold this button to set your %s", v == 1 ? "park" : "summit");
    else msg_update_text_fmt("Logging as %s %s (hold to change)", v == 1 ? "POTA" : "SOTA", ref);
}

static void act_hold_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard() || composing) return;
    uint8_t v = params.js8_log_activation.x;
    if (v == 0) {
        msg_update_text_fmt("Press to choose POTA or SOTA first");
        return;
    }
    edit_target = v == 1 ? EDIT_POTA_REF : EDIT_SOTA_REF;
    compose_open(v == 1 ? last_pota : last_sota);
    lv_group_set_editing(keyboard_group, true);
}

/* ---- Inbox and messages -------------------------------------------------- */

/* Like desktop JS8Call: "CALL MSG text" to us (checksum good) goes to the
 * inbox, answered with "CALL ACK" (sent by AUTO, else offered on Reply).
 * The Query list sends messages: MSG for their inbox, MSG TO: to leave one
 * at their station for someone else, QUERY MSGS to ask what they hold. */

#define INBOX_ROWS 50

static int inbox_view_id; /* the message shown, 0: the list */

static void inbox_refresh_button(void) {
    if (btn_inbox.disp_btn) buttons_refresh(&btn_inbox);
}

static const char *inbox_label_getter(void) {
    static char label[24];
    int         unread = js8_inbox_unread(inbox);
    if (unread) snprintf(label, sizeof(label), "Inbox\n%d new", unread);
    else snprintf(label, sizeof(label), "Inbox");
    return label;
}

static void inbox_received(const js8_rx_msg_t *m) {
    char text[JS8_RX_TEXT_LEN];
    if (!inbox || !js8_msg_for_me(m, params.callsign.x, text, sizeof(text))) return;
    int  before = js8_inbox_count(inbox);
    int  id     = js8_inbox_add(inbox, m->from, text, now_wall_ms());
    bool resend = js8_inbox_count(inbox) == before;
    if (id < 0) msg_update_text_fmt("Message from %s - can't save %s", m->from, JS8_INBOX_PATH);
    else if (!resend) msg_update_text_fmt("New message from %s - Inbox on page 3", m->from);
    if (!resend) add_info_row("Message from %s in the Inbox: %s", m->from, text);
    if (!resend && (params.js8_alerts.x & JS8_ALERT_INBOX)) alert_beep(2);
    inbox_refresh_button();
    update_status();
}

/* "MSG TO:W1ABC ..." sent to us: held here until W1ABC asks (QUERY MSGS,
 * QUERY MSG n, or our HB ack tells them "MSG ID n"), as desktop does. */
static void held_received(const js8_rx_msg_t *m) {
    char to[JS8_RX_CALL_LEN], text[JS8_RX_TEXT_LEN];
    if (!held || !js8_msg_to_for_me(m, params.callsign.x, to, sizeof(to), text, sizeof(text))) return;
    int before = js8_held_count(held);
    int id     = js8_held_add(held, m->from, to, text, now_wall_ms());
    if (js8_held_count(held) == before) return; /* a resend */
    if (id < 0) msg_update_text_fmt("Can't save %s", JS8_HELD_PATH);
    else msg_update_text_fmt("Holding a message from %s for %s (Inbox)", m->from, to);
    add_info_row("Holding message %d from %s for %s: %s", id, m->from, to, text);
}

static void inbox_close(void) {
    if (!inbox_list) return;
    lv_obj_del_async(inbox_list); /* often called from one of its buttons */
    inbox_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* Leave the popup from one of its own buttons, going somewhere else (the
 * keyboard, or another view): buttons out of the group now, list later. */
static void inbox_leave(void) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(inbox_list); i++) lv_group_remove_obj(lv_obj_get_child(inbox_list, i));
    lv_obj_del_async(inbox_list);
    inbox_list = NULL;
}

/* "CALL MSG " / "CALL MSG TO:" in the keyboard; kind is "MSG " or "MSG TO:". */
static void msg_compose(const char *call, const char *kind) {
    char prefill[JS8_RX_CALL_LEN + 12];
    snprintf(prefill, sizeof(prefill), "%s %s", call, kind);
    compose_open(prefill);
    lv_group_set_editing(keyboard_group, true);
    if (strcmp(kind, "MSG TO:") == 0)
        msg_update_text_fmt("Left at %s for someone: type their call, a space, the message", call);
    else msg_update_text_fmt("Message for %s's inbox: type it and press Enter", call);
}

static void inbox_show(int id);

static void inbox_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    if (key == LV_KEY_ESC) {
        if (inbox_view_id) { /* ESC in a message: back to the list */
            inbox_leave();
            inbox_show(0);
        } else {
            inbox_close();
        }
    } else if (key == LV_KEY_LEFT || key == LV_KEY_UP) lv_group_focus_prev(keyboard_group);
    else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) lv_group_focus_next(keyboard_group);
}

typedef enum { INBOX_NEW = -1, INBOX_CLOSE = -2, INBOX_BACK = -3, INBOX_REPLY = -4, INBOX_DELETE = -5 } inbox_action_t;
#define INBOX_HELD 100000 /* list/view ids from here on are held messages (id - INBOX_HELD) */

static void inbox_item_cb(lv_event_t *e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    int viewed = inbox_view_id;
    switch (action) {
    case INBOX_CLOSE:
        inbox_close();
        return;
    case INBOX_BACK:
        inbox_leave();
        inbox_show(0);
        return;
    case INBOX_DELETE:
        if (viewed >= INBOX_HELD) js8_held_delete(held, viewed - INBOX_HELD);
        else js8_inbox_delete(inbox, viewed);
        msg_update_text_fmt("Message deleted");
        inbox_leave();
        inbox_show(0);
        inbox_refresh_button();
        return;
    case INBOX_REPLY: {
        js8_inbox_msg_t m;
        if (!js8_inbox_get(inbox, viewed, &m)) return;
        inbox_leave();
        msg_compose(m.from, "MSG ");
        return;
    }
    case INBOX_NEW: {
        char  call[JS8_RX_CALL_LEN];
        float freq;
        int   snr;
        inbox_leave();
        if (selected_station(call, sizeof(call), &freq, &snr)) {
            apply_hold(freq);
            msg_compose(call, "MSG ");
        } else {
            compose_open(NULL);
            lv_group_set_editing(keyboard_group, true);
            msg_update_text_fmt("Type their call, then MSG and the message");
        }
        return;
    }
    default: /* a message */
        inbox_leave();
        inbox_show(action);
        return;
    }
}

static lv_obj_t *inbox_add(const char *label, int action) {
    lv_obj_t *b = list_add_item(inbox_list, label);
    lv_obj_add_event_cb(b, inbox_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)action);
    lv_obj_add_event_cb(b, inbox_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    /* Long lines end in "..." rather than scrolling, which redraws the list. */
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

static void utc_label(int64_t ms, char *buf, size_t size, const char *fmt) {
    time_t    t = (time_t)(ms / 1000);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, size, fmt, &tm);
}

/* The list (id 0) or one message. */
static void inbox_show(int id) {
    inbox_view_id = id;
    lv_group_remove_obj(table);
    inbox_list = lv_list_create(dialog.obj);
    lv_obj_set_size(inbox_list, 600, WF_HEIGHT - 10);
    lv_obj_align(inbox_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(inbox_list, &sony_24, 0);
    lv_obj_set_style_bg_color(inbox_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(inbox_list, lv_color_white(), 0);

    char      line[JS8_RX_TEXT_LEN + 48];
    lv_obj_t *first = NULL;
    if (id >= INBOX_HELD) {
        /* A message held here for another station. */
        js8_held_msg_t m;
        if (!js8_held_get(held, id - INBOX_HELD, &m)) {
            inbox_view_id = 0;
            lv_obj_del(inbox_list);
            inbox_show(0);
            return;
        }
        char when[32];
        utc_label(m.utc_ms, when, sizeof(when), "%d %b %H:%M UTC");
        snprintf(line, sizeof(line), "For %s from %s  %s", m.to, m.from, when);
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        lv_obj_t *body = lv_list_add_text(inbox_list, m.text);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_color(body, lv_color_white(), 0);
        lv_obj_set_style_pad_ver(body, 8, 0);
        snprintf(line, sizeof(line), m.delivered ? "Delivered to %s" : "Waiting for %s to ask (QUERY MSGS)", m.to);
        t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        first = inbox_add("Delete", INBOX_DELETE);
        lv_obj_t *back = inbox_add("Back", INBOX_BACK);
        lv_obj_set_style_text_color(back, lv_color_hex(0xffc040), 0);
    } else if (id) {
        js8_inbox_msg_t m;
        if (!js8_inbox_get(inbox, id, &m)) {
            inbox_view_id = 0;
            lv_obj_del(inbox_list);
            inbox_show(0);
            return;
        }
        js8_inbox_mark_read(inbox, id);
        inbox_refresh_button();
        update_status();
        char when[32];
        utc_label(m.utc_ms, when, sizeof(when), "%d %b %H:%M UTC");
        snprintf(line, sizeof(line), "From %s  %s", m.from, when);
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        lv_obj_t *body = lv_list_add_text(inbox_list, m.text);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_color(body, lv_color_white(), 0);
        lv_obj_set_style_pad_ver(body, 8, 0);
        snprintf(line, sizeof(line), "Reply: MSG to %s", m.from);
        first = inbox_add(line, INBOX_REPLY);
        inbox_add("Delete", INBOX_DELETE);
        lv_obj_t *back = inbox_add("Back", INBOX_BACK);
        lv_obj_set_style_text_color(back, lv_color_hex(0xffc040), 0);
    } else {
        static js8_inbox_msg_t rows[INBOX_ROWS];
        int                    n = js8_inbox_list(inbox, rows, INBOX_ROWS);
        snprintf(line, sizeof(line), "Inbox: %d message%s, %d new", js8_inbox_count(inbox),
                 js8_inbox_count(inbox) == 1 ? "" : "s", js8_inbox_unread(inbox));
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);

        char  call[JS8_RX_CALL_LEN];
        float freq;
        int   snr;
        if (selected_station(call, sizeof(call), &freq, &snr)) snprintf(line, sizeof(line), "New message to %s...", call);
        else snprintf(line, sizeof(line), "New message...");
        first = inbox_add(line, INBOX_NEW);
        lv_obj_set_style_text_color(first, lv_color_hex(0x80ff80), 0);

        lv_obj_t *oldest_new = NULL;
        for (int i = 0; i < n; i++) {
            char when[16];
            utc_label(rows[i].utc_ms, when, sizeof(when), "%d %b %H:%M");
            snprintf(line, sizeof(line), "%s%s  %s  %s", rows[i].read ? "" : "* ", when, rows[i].from, rows[i].text);
            lv_obj_t *b = inbox_add(line, rows[i].id);
            if (!rows[i].read) {
                lv_obj_set_style_text_color(b, lv_color_hex(0xffe080), 0);
                oldest_new = b;
            }
        }
        if (n == 0) {
            t = lv_list_add_text(inbox_list, "No messages yet");
            lv_obj_set_style_text_font(t, &sony_22, 0);
        }
        /* Messages held here for others (MSG TO:), until they ask. */
        static js8_held_msg_t held_rows[INBOX_ROWS];
        int                   nh = js8_held_list(held, held_rows, INBOX_ROWS);
        if (nh > 0) {
            snprintf(line, sizeof(line), "Held for others: %d waiting", js8_held_waiting(held));
            t = lv_list_add_text(inbox_list, line);
            lv_obj_set_style_text_font(t, &sony_22, 0);
        }
        for (int i = 0; i < nh; i++) {
            snprintf(line, sizeof(line), "%sfor %s from %s  %s", held_rows[i].delivered ? "(sent) " : "",
                     held_rows[i].to, held_rows[i].from, held_rows[i].text);
            lv_obj_t *b = inbox_add(line, INBOX_HELD + held_rows[i].id);
            if (held_rows[i].delivered) lv_obj_set_style_text_color(b, lv_color_hex(0x909090), 0);
        }
        lv_obj_t *close = inbox_add("Close", INBOX_CLOSE);
        lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
        if (oldest_new) first = oldest_new; /* straight to what's unread */
    }
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

static void inbox_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (inbox_list) { /* Inbox again closes it */
        inbox_close();
        return;
    }
    if (popup_guard()) return;
    if (composing || !inbox) return;
    inbox_show(0);
}

/* ---- Alerts ---------------------------------------------------------------- */

/* Like desktop JS8Call's notifications and highlight words: a beep through
 * the speaker for what's switched on in the Alerts list, and rows matching
 * an alert word (calls or words you type) in purple. Never while
 * transmitting: the speaker path is the TX audio path then. */

#define BEEP_HZ       1000
#define BEEP_MS       120
#define BEEP_GAP_MS   100
#define BEEP_EVERY_MS 3000 /* at most one alert sound per 3 s */

static int64_t beep_last_ms;

enum { BEEP_TONE = AUDIO_PLAY_RATE * BEEP_MS / 1000, BEEP_GAP = AUDIO_PLAY_RATE * BEEP_GAP_MS / 1000 };
static int16_t         beep_buf[BEEP_TONE + BEEP_GAP];
static int             beep_count;
static atomic_bool     beeping;

/* Beep thread: audio_play() waits for room in the stream, so it can't run
 * on the LVGL thread, and it only ever gets small parts (like tx_player):
 * one call with the whole beep never finds room and hangs forever. */
static void *beep_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&speaker_lock);
    for (int i = 0; i < beep_count && !atomic_load(&keyed); i++) {
        for (size_t at = 0; at < BEEP_TONE + BEEP_GAP && !atomic_load(&keyed);) {
            size_t part = LV_MIN(1024 * 2, BEEP_TONE + BEEP_GAP - at);
            audio_play(beep_buf + at, part);
            at += part;
        }
    }
    audio_play_wait();
    pthread_mutex_unlock(&speaker_lock);
    atomic_store(&beeping, false);
    return NULL;
}

static void alert_beep(int count) {
    if (!(params.js8_alerts.x & JS8_ALERT_BEEP)) return;
    if (atomic_load(&keyed) || js8_tx_busy(tx)) return;
    int64_t now = now_wall_ms();
    if (now - beep_last_ms < BEEP_EVERY_MS) return;
    if (atomic_exchange(&beeping, true)) return;
    beep_last_ms = now;

    static bool made;
    if (!made) {
        int ramp = AUDIO_PLAY_RATE / 200; /* 5 ms fades: no clicks */
        for (int i = 0; i < BEEP_TONE; i++) {
            float env = 1.0f;
            if (i < ramp) env = (float)i / ramp;
            if (i > BEEP_TONE - ramp) env = (float)(BEEP_TONE - i) / ramp;
            beep_buf[i] = (int16_t)(8000.0f * env * sinf(2.0f * (float)M_PI * BEEP_HZ * i / AUDIO_PLAY_RATE));
        }
        made = true;
    }
    beep_count = count;
    pthread_t th;
    if (pthread_create(&th, NULL, beep_thread, NULL) == 0) pthread_detach(th);
    else atomic_store(&beeping, false);
}

/* Every decode but our own: alert words, then what beeps. */
static void alert_check(js8_rx_msg_t *m, bool new_station) {
    if (m->low_confidence) return;
    char hit[16];
    if (js8_alert_hit(m->text, m->from, alert_words, hit, sizeof(hit))) {
        m->alert = true;
        msg_update_text_fmt("Alert %s: %s", hit, m->text);
        alert_beep(2);
        return;
    }
    uint8_t a      = params.js8_alerts.x;
    bool    hb_ack = strstr(m->text, " HEARTBEAT SNR") != NULL;
    if ((a & JS8_ALERT_TO_ME) && m->to_me && !hb_ack) alert_beep(1);
    else if ((a & JS8_ALERT_CQ) && m->cq) alert_beep(1);
    else if ((a & JS8_ALERT_NEW) && new_station && m->from[0] &&
             qso_log_search_worked(m->from, MODE_JS8, qso_log_freq_to_band(cparam_i_get(cfg_fg_freq))) <= 0) {
        msg_update_text_fmt("New station: %s", m->from);
        alert_beep(1);
    }
}

typedef enum {
    AL_BEEP,
    AL_TO_ME,
    AL_INBOX,
    AL_CQ,
    AL_NEW,
    AL_WORDS,
    AL_TEST,
    AL_CLOSE,
} alerts_item_t;

static const struct {
    uint8_t     bit;
    const char *label;
} alert_switches[] = {
    [AL_BEEP]  = {JS8_ALERT_BEEP, "Beep"},
    [AL_TO_ME] = {JS8_ALERT_TO_ME, "Message to me"},
    [AL_INBOX] = {JS8_ALERT_INBOX, "Inbox message"},
    [AL_CQ]    = {JS8_ALERT_CQ, "Someone calls CQ"},
    [AL_NEW]   = {JS8_ALERT_NEW, "New station (not in log)"},
};

static void alerts_switch_label(alerts_item_t item, char *buf, size_t size) {
    snprintf(buf, size, "%s: %s", alert_switches[item].label,
             (params.js8_alerts.x & alert_switches[item].bit) ? "On" : "Off");
}

static void alerts_close(void) {
    if (!alerts_list) return;
    lv_obj_del_async(alerts_list); /* often called from one of its buttons */
    alerts_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void alerts_item_cb(lv_event_t *e) {
    alerts_item_t item = (alerts_item_t)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t     *btn  = lv_event_get_target(e);
    char          line[sizeof(alert_words) + 24];
    switch (item) {
    case AL_CLOSE:
        alerts_close();
        return;
    case AL_TEST: {
        int64_t last = beep_last_ms;
        beep_last_ms = 0;
        if (!(params.js8_alerts.x & JS8_ALERT_BEEP)) msg_update_text_fmt("Beep is off");
        else if (atomic_load(&keyed) || js8_tx_busy(tx)) msg_update_text_fmt("Not while transmitting");
        alert_beep(2);
        if (!beep_last_ms) beep_last_ms = last;
        return;
    }
    case AL_WORDS:
        /* Into the keyboard, then back here (see texts_item_cb). */
        for (uint32_t i = 0; i < lv_obj_get_child_cnt(alerts_list); i++)
            lv_group_remove_obj(lv_obj_get_child(alerts_list, i));
        lv_obj_del_async(alerts_list);
        alerts_list = NULL;
        edit_target = EDIT_ALERT_WORDS;
        compose_open(alert_words[0] ? alert_words : NULL);
        lv_group_set_editing(keyboard_group, true);
        msg_update_text_fmt("Calls or words to watch for, separated by spaces");
        return;
    default: /* a switch: flip it, relabel in place */
        params_uint8_set(&params.js8_alerts, params.js8_alerts.x ^ alert_switches[item].bit);
        alerts_switch_label(item, line, sizeof(line));
        lv_label_set_text(lv_obj_get_child(btn, 0), line);
        return;
    }
}

static void alerts_key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    if (key == LV_KEY_ESC) alerts_close();
    else if (key == LV_KEY_LEFT || key == LV_KEY_UP) lv_group_focus_prev(keyboard_group);
    else if (key == LV_KEY_RIGHT || key == LV_KEY_DOWN) lv_group_focus_next(keyboard_group);
}

static lv_obj_t *alerts_add(alerts_item_t item, const char *label) {
    lv_obj_t *b = list_add_item(alerts_list, label);
    lv_obj_add_event_cb(b, alerts_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    lv_obj_add_event_cb(b, alerts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

static void alerts_show(void) {
    lv_group_remove_obj(table);
    alerts_list = lv_list_create(dialog.obj);
    lv_obj_set_size(alerts_list, 560, WF_HEIGHT - 10);
    lv_obj_align(alerts_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(alerts_list, &sony_24, 0);
    lv_obj_set_style_bg_color(alerts_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(alerts_list, lv_color_white(), 0);
    lv_obj_t *t = lv_list_add_text(alerts_list, "Alerts: beep, and alert words in purple");
    lv_obj_set_style_text_font(t, &sony_22, 0);

    char      line[sizeof(alert_words) + 24];
    lv_obj_t *first = NULL;
    for (int i = AL_BEEP; i <= AL_NEW; i++) {
        alerts_switch_label((alerts_item_t)i, line, sizeof(line));
        lv_obj_t *b = alerts_add((alerts_item_t)i, line);
        if (!first) first = b;
    }
    snprintf(line, sizeof(line), "Alert words: %s", alert_words[0] ? alert_words : "(none)");
    lv_obj_t *words = alerts_add(AL_WORDS, line);
    lv_obj_set_style_text_color(words, lv_color_hex(0xd0a0ff), 0);
    alerts_add(AL_TEST, "Test beep");
    lv_obj_t *close = alerts_add(AL_CLOSE, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

static void alerts_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (alerts_list) { /* Alerts > again closes it */
        alerts_close();
        return;
    }
    if (popup_guard()) return;
    if (composing) return;
    alerts_show();
}

/* ---- Speeds (T6) ----------------------------------------------------------- */

/* As desktop JS8Call: everything we send goes at one speed (Normal, Fast,
 * Turbo, Slow), and the receiver decodes every speed at once unless
 * Decode is set to My speed. Turbo sends no heartbeats or HB acks. */

/* The selected row's speed (station or message). */
static bool selected_speed(js8_speed_t *out) {
    uint16_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row >= rows || row_hist[row] < 0) return false;
    uint8_t submode = view_stations ? st_rows[row_hist[row]].submode : history[row_hist[row]].submode;
    if (!view_stations && history[row_hist[row]].tx) return false;
    *out = js8_speed_from_submode(submode);
    return true;
}

/* Replying to someone heard on another speed: say so (desktop offers "Jump
 * to Fast speed"; here, hold Speed). */
static void speed_warn(void) {
    js8_speed_t their;
    char        call[JS8_RX_CALL_LEN];
    float       freq;
    int         snr;
    if (!selected_speed(&their) || their == cur_speed() || !selected_station(call, sizeof(call), &freq, &snr)) return;
    msg_update_text_fmt("%s was heard on %s, you send %s - hold Speed (page 6) to match", call, js8_speed_name(their),
                        js8_speed_name(cur_speed()));
}

static void set_speed(js8_speed_t s) {
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    params_uint8_set(&params.js8_speed, (uint8_t)s);
    /* The offset must leave room for the wider signal below 2500 Hz. */
    int max = js8_speed_max_offset_hz(s);
    if (params.js8_tx_freq.x > max) params_uint16_set(&params.js8_tx_freq, (uint16_t)max);
    js8_rx_set_qso_offset(rx, params.js8_tx_freq.x);
    lv_finder_set_width(finder, js8_speed_bandwidth_hz(s));
    lv_finder_set_value(finder, (int16_t)params.js8_tx_freq.x);
    lv_obj_invalidate(finder);
    if (!params.js8_rx_all.x) js8_rx_set_submodes(rx, rx_speed_mask());
    /* A heartbeat that fell due while in Turbo mustn't go out the moment we
     * leave it: start the interval again. */
    hb_next_ms = 0;
    if (btn_speed.disp_btn) buttons_refresh(&btn_speed);
    update_tx_bar();
    msg_update_text_fmt("Sending %s: %d s slots, %d Hz wide%s", js8_speed_name(s), js8_speed_period_s(s),
                        js8_speed_bandwidth_hz(s), js8_speed_heartbeats(s) ? "" : ", no heartbeats (as desktop)");
    add_info_row("Sending at %s speed", js8_speed_name(s));
}

static const char *speed_label_getter(void) {
    static char label[24];
    snprintf(label, sizeof(label), "Speed:\n%s", js8_speed_name(cur_speed()));
    return label;
}

static void speed_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard()) return;
    set_speed((js8_speed_t)((cur_speed() + 1) % JS8_SPEED_COUNT));
}

/* Hold: the selected station's speed. */
static void speed_hold_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard()) return;
    js8_speed_t their;
    if (!selected_speed(&their)) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }
    if (their == cur_speed()) {
        msg_update_text_fmt("Already sending %s", js8_speed_name(their));
        return;
    }
    set_speed(their);
}

static const char *decode_label_getter(void) {
    return params.js8_rx_all.x ? "Decode:\nAll speeds" : "Decode:\nMy speed";
}

static void decode_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    params_bool_set(&params.js8_rx_all, !params.js8_rx_all.x);
    js8_rx_set_submodes(rx, rx_speed_mask());
    buttons_refresh(btn);
    if (params.js8_rx_all.x) msg_update_text_fmt("Decoding every speed (Normal, Fast, Turbo, Slow)");
    else msg_update_text_fmt("Decoding %s only", js8_speed_name(cur_speed()));
}
