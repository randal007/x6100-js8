/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 (receive)
 *
 *  Layout follows the FT8 app: a waterfall strip across the top and a
 *  message list over the rest of it. Decoding lives in src/js8 (no LVGL);
 *  its callbacks arrive on worker threads and reach the GUI thread through
 *  JS8's own queues (ev_push, the waterfall ring), not scheduler_put().
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
#include "keypad.h"
#include "scheduler.h"
#include "styles.h"
#include "textarea_window.h"
#include "tx_player.h"
#include "util.h"
#include "widgets/lv_finder.h"
#include "widgets/lv_waterfall.h"

#include <liquid/liquid.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#define SYNC_SAMPLES     64     /* decodes kept for Time Sync (a busy band's 2 min) */
#define WF_ROWS_PER_SEC  15     /* waterfall rows per second of audio, as the main screen's */
#define WF_ROW_SAMPLES   (SAMPLE_RATE / WF_ROWS_PER_SEC)
#define WF_QUEUE         16     /* rows waiting to be drawn (jitter buffer) */
#define WF_TICK_MS       5      /* how often the drawing timer looks at the clock */
#define HB_ADJUST_MS     8000   /* setting the HB interval ends after this idle */
#define HB_PAUSE_MS      (10 * 60 * 1000) /* heartbeats wait this long after you send */
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
/* The TX filter while the app is open: the radio's own default, so it
 * only changes anything if yours was set narrower (for voice). JS8 never
 * sends below 500 Hz; the top must pass a signal up to 3000 Hz. */
#define JS8_TX_FILTER_LOW  160
#define JS8_TX_FILTER_HIGH 3000

#define HISTORY          300    /* messages kept for re-filtering */
#define MAX_ROWS         200    /* rows shown before trimming to KEEP_ROWS */
#define KEEP_ROWS        150
#define CQ_MIN_INTERVAL  1      /* auto CQ: minutes after our last TX ends (desktop's shortest: 1) */
#define CQ_MAX_INTERVAL  30
#define EOT_MARK         "\xE2\x99\xA2" /* desktop JS8Call's end-of-transmission mark, U+2662 */
#define READ_PAUSE_MS    30000 /* list follows new rows again this long after the last MFK move */
#define HOLD_MS          500      /* a button held this long is a hold (1 s elsewhere) */
#define CUSTOM_MIN_HZ    1800000  /* custom dial frequency: 160m ... */
#define CUSTOM_MAX_HZ    54000000 /* ... to the top of 6m */

typedef enum {
    SHOW_ALL,       /* everything, heartbeats included */
    SHOW_NO_HB,     /* everything except heartbeats and SNR reports */
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
static void        time_sync_hold_cb(button_data_t *btn);
static void        rotary_cb(int32_t diff);
static void        reply_cb(button_data_t *btn);
static void        send_cb(button_data_t *btn);
static void        stop_tx_cb(button_data_t *btn);
static void        hw_cpy_cb(button_data_t *btn);
static void        cq_cb(button_data_t *btn);
static const char *cq_label_getter(void);
static int64_t     cq_interval_ms(void);
static void        cq_adjust_end(void);
static void        cq_adjust_turn(int32_t diff);
static void        cq_hold_cb(button_data_t *btn);
static void        auto_cq_stop(const char *why);
static void        auto_cq_tick(void);
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
static void        auto_queue(const js8_auto_result_t *r);
static void        auto_try_send(void);
static void        auto_clear(void);
static int         offer_find(const char *call);
static void        hb_tick(void);
static int64_t     hb_first_ms(void);
static void        load_texts(void);
static void        data_file_notice(const char *notice);
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
static void        stored_received(const js8_stored_t *k);
static void        deliver_start(int id, const char *group_call, const char *text);
static void        deliver_end(const char *text, bool completed);
static void        inbox_refresh_button(void);
static void        msg_compose(const char *call, const char *kind);
static void        alerts_cb(button_data_t *btn);
static void        alerts_close(void);
static const char *freq_label_getter(void);
static void        freq_cb(button_data_t *btn);
static void        freq_close(void);
static void        freq_show(void);
static void        tune_custom(const char *khz);
static bool        parse_custom(const char *text, int32_t *out);
typedef enum { SP_SEND, SP_REF, SP_FREQ, SP_MODE, SP_NOTE, SP_CLOSE, SP_COUNT } spot_item_t;
static bool        spot_sota; /* the spot form is for SOTA, else POTA */
static bool        spot_parse_freq(const char *text, int32_t *out);
static bool        spot_freq_empty(const char *text);
static void        spot_show(bool sota, spot_item_t focus);
static void        spot_close(void);
static void        aprs_beacon(bool gps, const char *message);
static void        beacon_changed_cb(lv_event_t *e);
#define APRS_COMMENT_MAX 43 /* an APRS position report's comment */
static const char *where_label(void);
static void        alert_check(js8_rx_msg_t *m, bool new_station);
static void        alert_beep(int count);
static void        beep_log_level(void);
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
static js8_sync_sample_t sync_samples[SYNC_SAMPLES]; /* recent decodes for Time Sync */
static unsigned          sync_head;
static float       qso_freq = -1;     /* the selected station's offset (the green line), -1 none */
/* The selected station: set by MFK or a tap on its row, never by new rows
 * arriving, so Reply still answers it after the list moves on. */
static char        sel_call[JS8_RX_CALL_LEN];
static int         sel_snr;
/* Hold MFK on a station to lock it: turning then only scrolls, and the
 * selection stays until you press another station (or hold it again). */
static bool        sel_locked;
/* The cursor is on a row without a callsign (a QSO's later lines often
 * have none): its offset, whose rows get the green bar; -1 otherwise. It
 * selects nobody: Reply still goes to the selected station. */
static float       cursor_freq = -1;
static bool        press_on_locked; /* this press began on the locked station */
static bool        press_held;      /* this press has already been a hold */
static int64_t     cursor_user_ms;    /* last MFK move on the list */
static atomic_bool keyed;              /* a frame is on the air (TX thread) */
/* Held while an alert beep plays; tx_play takes it after setting keyed, so
 * a beep stops within one part and never goes out with our TX audio. */
static pthread_mutex_t speaker_lock = PTHREAD_MUTEX_INITIALIZER;
/* While the speaker plays a beep the base doesn't send us the receiver:
 * the decoder gets silence instead (same length, so its clock stays in
 * step) and the waterfall pauses. On from speaker-on until beep_guard_end
 * (monotonic ms), a little after speaker-off, for the capture latency. */
static atomic_bool     beep_guard;
static _Atomic int64_t beep_guard_end;
static _Atomic int64_t beep_guard_sq, beep_guard_n; /* capture level during it (log) */
static atomic_int  tx_offset_active;   /* offset of the message being sent */
static bool        composing;          /* compose window open */

static js8_stations_t *stations;       /* who we've heard, who heard us: this band's */

/* One Stations list per dial frequency (7.078 and GhostNet's 7.107 are
 * different nets): going back brings its list back. They last while the
 * radio is on, JS8 closed and reopened included; each station drops off an
 * hour after it was last heard. */
#define BAND_LISTS 16
static struct {
    int32_t         dial_khz;
    js8_stations_t *list;
} band_lists[BAND_LISTS];
static int band_lists_next; /* slot reused when all are taken */
static bool           view_stations;   /* list shows stations, not messages */
static js8_station_t  st_rows[MAX_ROWS];
static int            st_count;
static lv_obj_t      *query_list;      /* Query popup, when open */
static lv_obj_t      *aprs_list;       /* APRS popup, when open */
static lv_obj_t      *log_list;        /* Log QSO popup, when open */
static lv_obj_t      *inbox_list;      /* Inbox popup (list or one message), when open */
static lv_obj_t      *alerts_list;     /* Alerts popup, when open */
static lv_obj_t      *freq_list;       /* Freq popup, when open */
static lv_obj_t      *spot_list;       /* POTA / SOTA spot form, when open */
static char           alert_words[128]; /* "VE7ABC @POTA SOTA", ALERTS= in JS8_TEXTS_PATH */
static char           groups_text[160]; /* "@NET @GROUP2": groups we're in (10 at most), GROUPS= in JS8_TEXTS_PATH */
static char           operator_call[JS8_RX_CALL_LEN]; /* logged as OPERATOR ("": the station call), OPERATOR= */
static bool           st_alert[MAX_ROWS]; /* st_rows matching an alert word */
static js8_inbox_t   *inbox;           /* MSGs to us, JS8_INBOX_PATH */
static js8_held_t    *held;            /* MSG TO: messages held for others, JS8_HELD_PATH */
static struct {
    int  id;                           /* offered on Reply: queued as offered, it's on its way */
    char group_call[JS8_RX_CALL_LEN];  /* a group message: who it's for */
    char text[JS8_RX_TEXT_LEN];
} deliver_pending;
/* Held messages on their way: each is delivered once it has gone out in
 * full. Desktop marks it as soon as it starts sending; stopped halfway
 * here, it stays held and is offered again when they next ask. */
#define DELIVERIES 4
static struct {
    int  id; /* 0: a free entry */
    char group_call[JS8_RX_CALL_LEN];
    char text[JS8_RX_TEXT_LEN];
} deliveries[DELIVERIES];
/* How a message ended, from the transmitter's thread to ours. */
typedef struct {
    bool completed;
    char text[JS8_RX_TEXT_LEN]; /* as sent: upper case, trimmed */
} tx_done_t;
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
static int64_t     hb_paused_until;  /* HB and HB ACK wait until then; 0: not paused */
static bool        auto_cq;          /* hold CQ: a CQ every js8_cq_interval min until answered */
static int64_t     auto_cq_next_ms;
static int64_t     auto_cq_from_ms;  /* the interval counts from here: our last TX's end */
static bool        hb_adjusting;     /* main knob sets the HB interval */
static int64_t     hb_adjust_ms;     /* last knob turn while adjusting */
static bool        cq_adjusting;     /* main knob sets the auto CQ interval */
static int64_t     cq_adjust_ms;
static char        info_text[TEXT_MAX + 1], status_text[TEXT_MAX + 1];
static char        last_pota[16], last_sota[24]; /* last park / summit spotted via APRS */
/* The spot form (APRS > POTA/SOTA spot), remembered in JS8_TEXTS_PATH. */
static char        spot_mode[8] = "DATA";
static int32_t     spot_typed_hz;  /* last frequency typed in the form, 0 = none */
static bool        spot_use_typed; /* spot that one, not the JS8 dial */
static char        spot_note[32];
static char        last_tx_text[JS8_RX_TEXT_LEN]; /* for AGN?: set when its first frame keys */
/* Our message on the air, from queueing it until ui_tx_done hears it
 * ended (the transmitter says "not busy" a moment before that). */
static bool        tx_active;
static bool        tx_auto;        /* it's automatic: not our side of a QSO */
static bool        tx_cq;          /* it's a CQ: auto CQ counts from its end */
static int64_t     tx_quiet_ms;    /* tx_active but the transmitter idle since then */
#define TX_DONE_LOST_MS  5000      /* then the "done" was lost: not active */
/* AUTO off: answers waiting for Reply, one per station (desktop has one
 * outgoing box). */
#define OFFERS 8
static struct {
    char    call[JS8_RX_CALL_LEN]; /* "": a free entry */
    char    text[JS8_RX_TEXT_LEN];
    int64_t ms;
    int     deliver_id; /* the held message it delivers, or 0 */
    char    deliver_group_call[JS8_RX_CALL_LEN];
} offers[OFFERS];
/* Automatic replies waiting for their turn (the end of their decode
 * cycle, our TX, the keyboard, a message to us still arriving): oldest
 * first, the same text once. */
#define REPLIES          8
#define REPLY_WAIT_MS    (2 * 60 * 1000) /* then it's too late to answer */
#define CYCLE_WAIT_MS    5000            /* no end of cycle heard by then: go on */
static struct {
    js8_auto_result_t r;
    int64_t           ms;
    unsigned          cycle;   /* `cycles` when it was decided */
    bool              checked; /* its cycle ended and it wasn't dropped */
} replies[REPLIES];
static int     n_replies;
static int64_t push_next_ms; /* next look for RETRIEVE MSG notices, 0: 15 min from now */
/* Multi-frame messages still arriving, as desktop's open message buffers:
 * while one to us is open nothing automatic answers anyone, and while any
 * is open no HB ACK goes out, so we don't key over its next frames. */
#define OPEN_MSGS        8
#define OPEN_MSG_MS      60000 /* no frame for this long: it ended */
static struct {
    uint32_t id; /* 0: a free entry */
    bool     to_me;
    int64_t  ms; /* its last frame */
} open_msgs[OPEN_MSGS];
/* Band activity for heartbeat offsets (desktop's isFreqOffsetFree): every
 * decode's offset and time; the last 30 s counts. */
#define ACTIVITY         128
static struct {
    float   hz;
    int64_t ms;
} activity[ACTIVITY];
static int activity_head;
static int               edit_target;        /* 0 compose, else an edit_t */
static lv_obj_t         *texts_list;

/* Every list popup: whether it's open and how it closes. any_popup(),
 * close_popups() and destruct_cb() all go through this table, so a popup
 * can't be missing from one of them (GEN with the Query list open once
 * crashed the app that way). */
static const struct {
    lv_obj_t **list;
    void (*close)(void);
} popups[] = {
    {&query_list, query_close}, {&texts_list, texts_close},   {&aprs_list, aprs_close}, {&log_list, log_close},
    {&inbox_list, inbox_close}, {&alerts_list, alerts_close}, {&freq_list, freq_close}, {&spot_list, spot_close},
};
#define POPUPS (sizeof(popups) / sizeof(popups[0]))

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
    EDIT_GROUPS,   /* Settings: the groups we're in */
    EDIT_OPERATOR, /* Settings: the operator, if not the station call */
    EDIT_LOG_GRID, /* the Log popup's fields, then back to it */
    EDIT_LOG_NAME,
    EDIT_LOG_NOTE,
    EDIT_POTA_REF, /* your park / summit for the log */
    EDIT_SOTA_REF,
    EDIT_ALERT_WORDS, /* then back to the Alerts popup */
    EDIT_FREQ,        /* custom dial frequency, kHz */
    EDIT_SPOT_REF,    /* the spot form's fields, then back to it */
    EDIT_SPOT_FREQ,
    EDIT_SPOT_NOTE,
    EDIT_BEACON_GRID, /* optional message, then Spot my grid */
    EDIT_BEACON_GPS,  /* optional message, then Spot GPS position */
    EDIT_COUNT,
} edit_t;

/* The Log popup's items. */
typedef enum { LOG_SAVE, LOG_GRID, LOG_NAME, LOG_NOTE, LOG_CANCEL } log_item_t;
static void log_list_open(log_item_t focus);

static void log_edit_done(const char *value);
static void alerts_show(void);

/* Message history, a ring, so the list can be rebuilt when the filter
 * changes. row_hist[] maps a table row to its history slot (-1 = info row). */
LV_FONT_DECLARE(js8_marks_24);
static lv_font_t    table_font; /* the list's font: sony_24 with js8_marks_24 as fallback */
static js8_rx_msg_t history[HISTORY];
static int64_t      hist_ms[HISTORY]; /* when each arrived (JS8 time), for Messages kept */
static uint32_t     hist_seq[HISTORY]; /* arrival order, shared with info rows */
static uint16_t     hist_head;  /* next slot to write */
static uint16_t     hist_count;
/* Info rows ("Auto: N0XYZ SNR -05", "Held message 3 delivered"), kept
 * apart from the messages but in arrival order with them (seq), so a
 * rebuild (the Show filter, leaving Stations, a full list, Messages kept)
 * shows them again instead of dropping them (bug hunt 18). */
#define INFO_ROWS 64
static struct {
    char     text[128];
    int64_t  ms;
    uint32_t seq;
} info_rows[INFO_ROWS];
static unsigned info_head, info_count;
static uint32_t row_seq; /* the last message or info row's */
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
static pthread_mutex_t wf_lock = PTHREAD_MUTEX_INITIALIZER;
static float      wf_rows[WF_QUEUE][WIDTH]; /* rows made, not drawn yet (wf_lock) */
static unsigned   wf_q_head, wf_q_count;    /* (wf_lock) */
static float      wf_row[WIDTH], wf_sel[WIDTH]; /* the row being made (receiver thread) */
static lv_timer_t *wf_timer;
static int64_t    wf_due_us;   /* when the next row should be drawn (monotonic) */
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
static button_data_t btn_cq    = {.type = BTN_TEXT_FN, .label_fn = cq_label_getter, .press = cq_cb, .hold = cq_hold_cb};
static button_data_t btn_hb    = {.type = BTN_TEXT, .label = "Heart-\nbeat", .press = heartbeat_cb};
static button_data_t btn_query = {.type = BTN_TEXT, .label = "Query >", .press = query_cb};
static button_data_t btn_clear = {.type = BTN_TEXT, .label = "Clear", .press = clear_cb};

static button_data_t btn_p3        = {.type = BTN_TEXT, .label = "(JS8 3:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_4, .prev = &page_2};
static button_data_t btn_time_sync = {.type = BTN_TEXT, .label = "Time\nSync", .press = time_sync_cb, .hold = time_sync_hold_cb};
static button_data_t btn_hold      = {.type = BTN_TEXT_FN, .label_fn = hold_label_getter, .press = hold_cb};
static button_data_t btn_stations  = {.type = BTN_TEXT_FN, .label_fn = stations_label_getter, .press = stations_cb};
static button_data_t btn_inbox     = {.type = BTN_TEXT_FN, .label_fn = inbox_label_getter, .press = inbox_cb};

static buttons_page_t page_1 = {{&btn_p1, &btn_cq, &btn_hb, &btn_query, &btn_hw_cpy}};
static buttons_page_t page_2 = {{&btn_p2, &btn_show, &btn_reply, &btn_send, &btn_clear}};
static buttons_page_t page_3 = {{&btn_p3, &btn_time_sync, &btn_hold, &btn_stations, &btn_inbox}};

static button_data_t btn_p4     = {.type = BTN_TEXT, .label = "(JS8 4:6)", .press = js8_next_page_cb, .hold = js8_prev_page_cb, .next = &page_5, .prev = &page_3};
static button_data_t btn_auto   = {.type = BTN_TEXT_FN, .label_fn = auto_label_getter, .press = auto_cb};
static button_data_t btn_hbauto = {.type = BTN_TEXT_FN, .label_fn = hb_label_getter, .press = hb_cb, .hold = hb_hold_cb};
static button_data_t btn_hbackk = {.type = BTN_TEXT_FN, .label_fn = hb_ack_label_getter, .press = hb_ack_cb};
static button_data_t btn_texts  = {.type = BTN_TEXT, .label = "Settings...", .press = texts_cb};
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
static button_data_t  btn_freq      = {.type = BTN_TEXT_FN, .label_fn = freq_label_getter, .press = freq_cb};
static buttons_page_t page_6        = {{&btn_p6, &btn_alerts, &btn_speed, &btn_decode, &btn_freq}};

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
    /* SNR reports go with heartbeats: mostly answers to them. */
    case SHOW_NO_HB:    return !(m->heartbeat || m->snr_report) || m->to_me; /* e.g. HB acks to us */
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

    snprintf(buf, size, "%02d:%02d:%02d %+3d %4.0f%s  %s%s%s%s%s",
             hh, mm, ss, m->snr, m->freq_hz, speed,
             m->low_confidence ? "[" : "",
             m->text,
             m->low_confidence ? "]" : "",
             m->partial ? " ..." : m->checksum < 0 ? "  (bad checksum)" : "",
             /* Desktop's end-of-transmission mark: the message's last frame arrived. */
             !m->partial && (m->type & JS8_FRAME_LAST) ? " " EOT_MARK : "");
}

static bool auto_selecting; /* follow() is moving the selection, not the user */

static int64_t now_wall_ms(void);
static bool    hb_paused(void);
static void    show_selection(void);

/* Keep following new rows if the cursor is on the last one, or once the
 * knob has been left alone for a while after scrolling up to read. The
 * cursor only scrolls: the selected station (sel_call) stays put. */
static bool at_bottom(void) {
    uint16_t sel_row = 0, sel_col = 0;
    lv_table_get_selected_cell(table, &sel_row, &sel_col);
    return rows == 0 || sel_row == LV_TABLE_CELL_NONE || sel_row + 1 >= rows ||
           now_wall_ms() - cursor_user_ms > READ_PAUSE_MS;
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

/* Scroll to the very end: a long last row can be taller than its step. */
static void list_scroll_end(void) {
    lv_obj_update_layout(table);
    lv_coord_t below = lv_obj_get_scroll_bottom(table);
    if (below > 0) lv_obj_scroll_by(table, 0, -below, LV_ANIM_OFF);
}

static void follow(void) {
    if (rows == 0) return;
    select_row(rows - 1);
    list_scroll_end();
}

static void append_row(const char *text, int16_t hist) {
    lv_table_set_cell_value(table, rows, 0, text);
    row_hist[rows] = hist;
    rows++;
}

static void rebuild_rows(void);

static void add_info_row(const char *fmt, ...) {
    char   *buf = info_rows[info_head].text;
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(info_rows[0].text), fmt, args);
    va_end(args);
    info_rows[info_head].ms  = now_wall_ms();
    info_rows[info_head].seq = ++row_seq;
    info_head                = (info_head + 1) % INFO_ROWS;
    if (info_count < INFO_ROWS) info_count++;

    if (view_stations || !table) return;
    if (rows >= MAX_ROWS) { /* full: trimmed, this one included (it was dropped) */
        rebuild_rows();
        return;
    }
    bool scroll = at_bottom();
    append_row(buf, -1);
    if (scroll) follow();
}

/* Rebuild the list from history, newest KEEP_ROWS matching messages. */
static void rebuild_station_rows(void);

/* Settings: how long stations and messages stay listed (desktop's callsign
 * and activity aging), 0 minutes = always. */
static const struct {
    const char *label;
    int         min;
} st_keep_opts[] = {{"15 min", 15}, {"30 min", 30}, {"1 hour", 60}, {"2 hours", 120}, {"6 hours", 360}, {"always", 0}},
  msg_keep_opts[] = {{"all", 0}, {"15 min", 15}, {"30 min", 30}, {"1 hour", 60}, {"2 hours", 120}};
#define ST_KEEP_N  (int)(sizeof(st_keep_opts) / sizeof(st_keep_opts[0]))
#define MSG_KEEP_N (int)(sizeof(msg_keep_opts) / sizeof(msg_keep_opts[0]))

static int64_t msg_keep_ms(void) {
    return (int64_t)msg_keep_opts[params.js8_msg_keep.x < MSG_KEEP_N ? params.js8_msg_keep.x : 0].min * 60000;
}

static void apply_station_keep(void) {
    js8_stations_set_expire_ms((int64_t)st_keep_opts[params.js8_st_keep.x < ST_KEEP_N ? params.js8_st_keep.x : 2].min *
                               60000);
}

static void rebuild_rows(void) {
    if (view_stations) {
        rebuild_station_rows();
        return;
    }
    int16_t idx[KEEP_ROWS]; /* newest first */
    int     n = 0;

    int64_t keep = msg_keep_ms(), now = now_wall_ms();
    for (int age = 0; age < hist_count && n < KEEP_ROWS; age++) {
        int slot = (hist_head - 1 - age + HISTORY) % HISTORY;
        if (keep && now - hist_ms[slot] > keep) break; /* older ones too */
        if (passes_filter(&history[slot])) idx[n++] = (int16_t)slot;
    }
    /* Info rows in between, by arrival, newest first; they age out with
     * Messages kept as well. */
    int info[INFO_ROWS], ni = 0;
    for (unsigned a = 0; a < info_count; a++) {
        unsigned i = (info_head - 1 - a + INFO_ROWS) % INFO_ROWS;
        if (keep && now - info_rows[i].ms > keep) break;
        info[ni++] = (int)i;
    }
    /* The newest KEEP_ROWS of both: pick[k] >= 0 a history slot, else
     * -1 - an info row. */
    int pick[KEEP_ROWS], np = 0, a = 0, b = 0;
    while (np < KEEP_ROWS && (a < n || b < ni)) {
        if (b < ni && (a >= n || info_rows[info[b]].seq > hist_seq[idx[a]])) pick[np++] = -1 - info[b++];
        else pick[np++] = idx[a++];
    }

    lv_table_set_row_cnt(table, 1);
    lv_table_set_cell_value(table, 0, 0, "");
    rows = 0;

    char buf[JS8_RX_TEXT_LEN + 48];
    if (n == 0) append_row("Listening for JS8...", -1); /* no messages (yet, or through the filter) */
    for (int k = np - 1; k >= 0; k--) {
        if (pick[k] < 0) {
            append_row(info_rows[-1 - pick[k]].text, -1);
            continue;
        }
        format_row(&history[pick[k]], buf, sizeof(buf));
        append_row(buf, (int16_t)pick[k]);
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

/* JS8's time: the radio's clock plus the Time Sync drift (js8_rx.h). */
static int64_t now_wall_ms(void) {
    return js8_wall_ms();
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

/* Messages still arriving when the receiver stops listening (a retune, JS8
 * closing) never will: their rows stop showing " ...". A new message can't
 * take their rows over either: ids are unique for the whole run now
 * (B-12). True if there were any. */
static bool partials_end(void) {
    bool any = false;
    for (int age = 0; age < hist_count; age++) {
        int slot = (hist_head - 1 - age + HISTORY) % HISTORY;
        if (history[slot].partial && !history[slot].tx) {
            history[slot].partial = false;
            any                   = true;
        }
    }
    return any;
}

/* Everything that acts on a message, once it's complete: never on the text
 * so far of one still arriving. */
static void process_message(js8_rx_msg_t *m) {
    /* New: not heard on this frequency since the radio was switched on
     * (your choice), not just "not listed now": a regular who dropped off
     * the list an hour after they were last heard beeped again (B-20). */
    bool new_station = !m->tx && m->from[0] && !js8_stations_heard_before(stations, m->from);
    js8_stations_add(stations, m, params.callsign.x, now_wall_ms());
    if (m->tx) return;
    if (params.js8_relay.x) { /* stations a relay to us came through, as desktop lists them */
        char via_calls[4][JS8_RX_CALL_LEN], via[JS8_RX_CALL_LEN];
        int  n = js8_relay_stations(m, params.callsign.x, via_calls, 4, via, sizeof(via));
        for (int i = 0; i < n; i++)
            js8_stations_add_via(stations, via_calls[i], via, m->freq_hz, m->submode, now_wall_ms());
    }
    if (sel_call[0] && strcasecmp(m->from, sel_call) == 0) { /* they moved: the green line follows */
        qso_freq = m->freq_hz;
        sel_snr  = m->snr;
        show_selection();
    }
    alert_check(m, new_station);
    if (!m->low_confidence && m->from[0]) {
        js8_sync_sample_t *x = &sync_samples[sync_head];
        snprintf(x->call, sizeof(x->call), "%s", m->from);
        x->when_ms   = now_wall_ms();
        x->drift_ms  = m->drift_ms;
        x->period_ms = js8_speed_period_s(js8_speed_from_submode(m->submode)) * 1000;
        sync_head    = (sync_head + 1) % SYNC_SAMPLES;
    }
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
            /* A message growing as it arrives wraps onto new lines: keep
             * its newest words in view. */
            bool scroll = at_bottom();
            lv_table_set_cell_value(table, r, 0, buf);
            if (scroll) follow();
            return;
        }
    }
    if (!passes_filter(m) || rows >= MAX_ROWS) return;
    bool scroll = at_bottom();
    append_row(buf, (int16_t)slot);
    if (scroll) follow();
}

/* After every frame heard (a message, or the text so far of one still
 * arriving): the band activity, and which messages are still open. Before
 * the message is acted on, so a message to us that has just ended no
 * longer holds up its own answer. */
static void track_incoming(const js8_rx_msg_t *m) {
    int64_t now = now_wall_ms();
    activity[activity_head].hz = m->freq_hz;
    activity[activity_head].ms = now;
    activity_head              = (activity_head + 1) % ACTIVITY;

    if (!m->msg_id) return;
    int slot = -1, spare = 0; /* spare: a free entry, else the oldest */
    for (int i = 0; i < OPEN_MSGS; i++) {
        if (open_msgs[i].id == m->msg_id) slot = i;
        if (!open_msgs[spare].id) continue;
        if (!open_msgs[i].id || open_msgs[i].ms < open_msgs[spare].ms) spare = i;
    }
    if (!m->partial) { /* it ended */
        if (slot >= 0) open_msgs[slot].id = 0;
        return;
    }
    if (!m->to[0]) return; /* desktop opens a buffer only for a directed command */
    if (slot < 0) slot = spare;
    open_msgs[slot].id    = m->msg_id;
    open_msgs[slot].to_me = m->to_me;
    open_msgs[slot].ms    = now;
}

/* Is a message still arriving (to us only, or any)? */
static bool message_open(bool to_me) {
    int64_t now = now_wall_ms();
    for (int i = 0; i < OPEN_MSGS; i++) {
        if (!open_msgs[i].id) continue;
        if (now - open_msgs[i].ms > OPEN_MSG_MS) open_msgs[i].id = 0;
        else if (open_msgs[i].to_me || !to_me) return true;
    }
    return false;
}

static void add_message(const js8_rx_msg_t *msg) {
    js8_rx_msg_t  copy = *msg;
    js8_rx_msg_t *m    = &copy;

    if (!m->tx) track_incoming(m);
    int existing = m->tx ? -1 : find_partial(m->msg_id);
    if (!m->partial) process_message(m);
    if (existing >= 0) {
        update_slot(existing, m);
        return;
    }

    int slot = hist_head;
    history[slot]  = *m;
    hist_ms[slot]  = now_wall_ms();
    hist_seq[slot] = ++row_seq;
    hist_head      = (hist_head + 1) % HISTORY;
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
    char star[2], call[JS8_RX_CALL_LEN], speed[2], age[8], snr[8], heard[40], grid[8], dist[16], az[8];
} station_fields_t;

/* Initial great-circle bearing from 1 to 2, degrees 0-359. */
static double bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double p1 = lat1 * M_PI / 180, p2 = lat2 * M_PI / 180, dl = (lon2 - lon1) * M_PI / 180;
    double b  = atan2(sin(dl) * cos(p2), cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl)) * 180 / M_PI;
    return fmod(b + 360, 360);
}

static void station_fields(const js8_station_t *st, int64_t now, station_fields_t *f) {
    memset(f, 0, sizeof(*f));
    f->star[0] = st->heard_me ? '*' : ' ';
    snprintf(f->call, sizeof(f->call), "%s", st->call);
    js8_speed_t sp = js8_speed_from_submode(st->submode);
    if (sp != JS8_SPEED_NORMAL) f->speed[0] = js8_speed_letter(sp); /* F, T, S */
    if (!st->via[0]) snprintf(f->snr, sizeof(f->snr), "%+d", st->snr);
    snprintf(f->grid, sizeof(f->grid), "%s", st->grid);
    char *age = f->age, *heard = f->heard, *dist = f->dist;
    format_age(now - st->heard_ms, age, sizeof(f->age));
    if (st->via[0]) {
        snprintf(heard, sizeof(f->heard), "via %s", st->via); /* only heard through a relay */
    } else if (st->heard_me) {
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
        double km = qth_pos_dist(lat, lon, my_lat, my_lon);
        if (params.js8_miles.x) snprintf(dist, sizeof(f->dist), "%.0f mi", km * 0.621371);
        else snprintf(dist, sizeof(f->dist), "%.0f km", km);
        snprintf(f->az, sizeof(f->az), "%.0f\xC2\xB0", bearing_deg(my_lat, my_lon, lat, lon)); /* ° */
    }
}

/* The station on the list's cursor row, in either view (not our own rows
 * or info rows). */
static bool row_station(char *call, size_t call_len, float *freq, int *snr) {
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

/* The selected station, with its latest offset and SNR. */
static bool selected_station(char *call, size_t call_len, float *freq, int *snr) {
    if (!sel_call[0]) return false;
    snprintf(call, call_len, "%s", sel_call);
    *freq = qso_freq;
    *snr  = sel_snr;
    return true;
}

/* Worked before (the radio's QSO database), per call and band. The
 * Stations view asked the database for every listed station every 5 s and
 * after each decode (bug hunt 17). Forgotten when a QSO is logged here and
 * each time JS8 opens (the FT8 app may have logged meanwhile). */
#define WORKED_CACHE 512
static struct {
    char            call[JS8_RX_CALL_LEN]; /* "": empty */
    qso_log_band_t  band;
    bool            worked;
} worked_cache[WORKED_CACHE];

static bool worked_before(const char *call) {
    qso_log_band_t band = qso_log_freq_to_band(cparam_i_get(cfg_fg_freq));
    unsigned       h    = 5381;
    for (const char *p = call; *p; p++) h = h * 33 + (unsigned char)*p;
    h = (h * 33 + (unsigned)band) % WORKED_CACHE;
    if (worked_cache[h].call[0] && worked_cache[h].band == band && strcmp(worked_cache[h].call, call) == 0)
        return worked_cache[h].worked;
    bool worked = qso_log_search_worked(call, MODE_JS8, band) > 0;
    snprintf(worked_cache[h].call, sizeof(worked_cache[h].call), "%s", call);
    worked_cache[h].band   = band;
    worked_cache[h].worked = worked;
    return worked;
}

static void worked_forget(void) {
    memset(worked_cache, 0, sizeof(worked_cache));
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
        st_worked[i] = worked_before(st_rows[i].call);
        if (keep[0] && strcmp(st_rows[i].call, keep) == 0) keep_row = rows;
        append_row(" ", (int16_t)i); /* drawn by table_draw_end_cb() */
    }
    select_row(keep_row);
}

/* Commands in colour (desktop draws them as "pills"): the table draws such
 * a row's text transparent, and table_draw_end_cb() draws it again with
 * the command recoloured. Recolour codes take no width, so the lines break
 * exactly as the table measured them. */
#define CMD_COLOR "ffd24a"
static struct {
    bool                on;
    uint32_t            id;          /* the cell */
    unsigned            start, len;  /* the command in the cell's text */
    lv_draw_label_dsc_t label;       /* how the table would have drawn it */
} cmd_draw;

/* The command in message row `row`'s text: false for rows without one. */
static bool row_command(uint32_t row, const js8_rx_msg_t *m, unsigned *start, unsigned *len) {
    const char *cell = lv_table_get_cell_value(table, row, 0);
    const char *at   = cell ? strstr(cell, m->text) : NULL;
    if (!at || !js8_command_span(m->text, start, len)) return false;
    *start += (unsigned)(at - cell);
    return true;
}

static void draw_recoloured(lv_obj_t *obj, lv_obj_draw_part_dsc_t *dsc, uint32_t row) {
    const char *cell = lv_table_get_cell_value(obj, row, 0);
    if (!cell) return;
    char   buf[2 * (JS8_RX_TEXT_LEN + 48) + 16];
    size_t o = 0, n = strlen(cell);
    for (size_t i = 0; i < n && o + 12 < sizeof(buf); i++) {
        if (i == cmd_draw.start) o += (size_t)snprintf(buf + o, sizeof(buf) - o, "#" CMD_COLOR " ");
        if (cell[i] == '#' && o + 2 < sizeof(buf)) buf[o++] = '#'; /* "##" is a plain '#' */
        buf[o++] = cell[i];
        if (i + 1 == cmd_draw.start + cmd_draw.len) buf[o++] = '#';
    }
    buf[o] = '\0';

    /* Where lv_table puts a cell's text: inside the padding, centred. */
    lv_draw_label_dsc_t l    = cmd_draw.label;
    lv_area_t           cell_a = *dsc->draw_area, a;
    a.x1 = cell_a.x1 + lv_obj_get_style_pad_left(obj, LV_PART_ITEMS);
    a.x2 = cell_a.x2 - lv_obj_get_style_pad_right(obj, LV_PART_ITEMS);
    lv_point_t size;
    lv_txt_get_size(&size, cell, l.font, l.letter_space, l.line_space, lv_area_get_width(&a), LV_TEXT_FLAG_NONE);
    lv_coord_t h = lv_area_get_height(&cell_a);
    a.y1         = cell_a.y1 + h / 2 - size.y / 2;
    a.y2         = cell_a.y1 + h / 2 + size.y / 2;
    l.flag |= LV_TEXT_FLAG_RECOLOR;

    const lv_area_t *clip_ori = dsc->draw_ctx->clip_area;
    lv_area_t        clip;
    if (!_lv_area_intersect(&clip, clip_ori, &cell_a)) return;
    dsc->draw_ctx->clip_area = &clip;
    lv_draw_label(dsc->draw_ctx, &l, &a, buf, NULL);
    dsc->draw_ctx->clip_area = clip_ori;
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
        if (m->tx) { /* red: transmitting, as the TX bar */
            dsc->rect_dsc->bg_color = lv_color_hex(0xB00000);
        } else if (m->alert) {
            dsc->rect_dsc->bg_color = lv_color_hex(ALERT_COLOR);
        } else if (m->to_me) {
            dsc->rect_dsc->bg_color = lv_color_hex(0x1830a0);
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

    /* A command to colour (not in the dimmed heartbeat rows): drawn later. */
    cmd_draw.on = false;
    if (h >= 0 && !view_stations && !history[h].heartbeat &&
        row_command(row, &history[h], &cmd_draw.start, &cmd_draw.len)) {
        cmd_draw.on          = true;
        cmd_draw.id          = dsc->id;
        cmd_draw.label       = *dsc->label_dsc;
        dsc->label_dsc->opa = LV_OPA_TRANSP;
    }
}

/* On offset `f`: within desktop's "same station" window for the row's speed. */
static bool row_on_freq(const js8_rx_msg_t *m, float f) {
    return f >= 0 && fabsf(m->freq_hz - f) <= js8_speed_rx_threshold_hz(js8_speed_from_submode(m->submode));
}

/* Rows with the green bar: with the cursor on a row without a callsign,
 * every row on its frequency; else the selected station's rows, ours to
 * them, and the rows without a callsign on their frequency. */
static bool row_is_selected_station(int16_t h) {
    if (h < 0) return false;
    if (view_stations) return sel_call[0] && strcasecmp(st_rows[h].call, sel_call) == 0;
    const js8_rx_msg_t *m = &history[h];
    if (cursor_freq >= 0) return row_on_freq(m, cursor_freq);
    if (!sel_call[0]) return false;
    if (!m->tx) return strcasecmp(m->from, sel_call) == 0 || (!m->from[0] && row_on_freq(m, qso_freq));
    size_t n = strlen(sel_call);
    return strncasecmp(m->text, sel_call, n) == 0 && (m->text[n] == ' ' || m->text[n] == '\0');
}

/* A green bar marks the selected station's rows; station view: draw the
 * fields at fixed x positions within the cell. */
static void table_draw_end_cb(lv_event_t *e) {
    lv_obj_t               *obj = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part != LV_PART_ITEMS) return;

    uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
    int16_t  h   = row < rows ? row_hist[row] : -1;
    if (cmd_draw.on && cmd_draw.id == dsc->id) {
        cmd_draw.on = false;
        draw_recoloured(obj, dsc, row);
    }
    if (h < 0) return;

    if (row_is_selected_station(h)) {
        lv_draw_rect_dsc_t bar;
        lv_draw_rect_dsc_init(&bar);
        bar.bg_color   = lv_color_hex(0x40ff40);
        lv_area_t area = *dsc->draw_area;
        area.x2        = area.x1 + 3;
        lv_draw_rect(dsc->draw_ctx, &bar, &area);
    }
    if (!view_stations) return;

    station_fields_t f;
    station_fields(&st_rows[h], now_wall_ms(), &f);

    static const struct {
        lv_coord_t x;
        size_t     field;
    } cols[] = {
        {0, offsetof(station_fields_t, star)},    {18, offsetof(station_fields_t, call)},
        {140, offsetof(station_fields_t, speed)}, {162, offsetof(station_fields_t, age)},
        {218, offsetof(station_fields_t, snr)},   {276, offsetof(station_fields_t, heard)},
        {505, offsetof(station_fields_t, grid)},  {588, offsetof(station_fields_t, dist)},
        {692, offsetof(station_fields_t, az)},
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

/* Show the selected station's offset on the waterfall (the green line). */
static void show_selection(void) {
    if (qso_freq >= 0) lv_finder_set_cursor(finder, (int16_t)(qso_freq + 0.5f));
    else lv_finder_clear_cursor(finder);
    lv_obj_invalidate(finder);
    lv_obj_invalidate(table);
    update_tx_bar();
}

/* The station on the cursor's row becomes the selection (the user moved
 * or tapped); rows without one (ours, info) leave it as it was. `announce`
 * also pops up who it is (on a tap, not on every MFK step). */
static void select_at_cursor(bool announce) {
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    if (!row_station(call, sizeof(call), &freq, &snr)) return;
    snprintf(sel_call, sizeof(sel_call), "%s", call);
    qso_freq = freq;
    sel_snr  = snr;
    show_selection();
    if (announce) msg_update_text_fmt("%s at %.0f Hz, %+d dB", call, freq, snr);
}

static void clear_selection(void) {
    sel_call[0] = '\0';
    sel_locked  = false;
    qso_freq    = -1;
    cursor_freq = -1;
    show_selection();
}

/* The station on the cursor's row, if any. */
static bool cursor_call(char *call, size_t len) {
    float freq;
    int   snr;
    return row_station(call, len, &freq, &snr);
}

/* A press selects the station on its row; on another station than the
 * locked one it also ends the lock. */
static void table_press_cb(lv_event_t *e) {
    (void)e;
    char call[JS8_RX_CALL_LEN];
    press_held      = false;
    press_on_locked = sel_locked && cursor_call(call, sizeof(call)) && strcasecmp(call, sel_call) == 0;
    if (press_on_locked) return;
    if (sel_locked && cursor_call(call, sizeof(call))) {
        sel_locked = false;
        msg_update_text_fmt("%s unlocked", sel_call);
    }
    select_at_cursor(true);
}

/* Held (about half a second, LVGL's long press plus one repeat): lock the
 * station, or unlock it if it was the locked one. */
static void table_hold_cb(lv_event_t *e) {
    (void)e;
    if (press_held) return; /* one per press */
    press_held = true;
    user_touch();
    if (press_on_locked) {
        sel_locked = false;
        msg_update_text_fmt("%s unlocked: turning the knob selects again", sel_call);
    } else if (sel_call[0]) {
        char call[JS8_RX_CALL_LEN];
        if (!cursor_call(call, sizeof(call)) || strcasecmp(call, sel_call) != 0) return;
        sel_locked = true;
        msg_update_text_fmt("%s locked: the knob only scrolls; press another station to change, hold to unlock",
                            sel_call);
    }
    update_tx_bar();
}

/* A received row without a callsign under the cursor: its offset, else -1. */
static float callless_cursor_freq(void) {
    if (view_stations) return -1;
    uint16_t row = 0, col = 0;
    lv_table_get_selected_cell(table, &row, &col);
    if (row == LV_TABLE_CELL_NONE || row >= rows || row_hist[row] < 0) return -1;
    const js8_rx_msg_t *m = &history[row_hist[row]];
    return m->tx || m->from[0] ? -1 : m->freq_hz;
}

static void table_select_cb(lv_event_t *e) {
    (void)e;
    float f = auto_selecting ? -1 : callless_cursor_freq(); /* following new rows: back to the station */
    if (f != cursor_freq) {
        cursor_freq = f;
        lv_obj_invalidate(table);
    }
    if (auto_selecting) return;
    cursor_user_ms = now_wall_ms();
    if (sel_locked) return; /* scrolling to read: the locked station stays */
    select_at_cursor(false);
}

/* ---- From the worker threads to the GUI -------------------------------
 *
 * Messages, the end of each decode cycle and the transmitter's progress
 * wait in JS8's own queue, drained on the GUI thread every WF_TICK_MS
 * (ev_tick in wf_timer_cb), and waterfall rows in their own ring. The
 * shared scheduler queue (64 items, src/scheduler.cpp) drops what doesn't
 * fit, and waterfall rows alone were 15 a second: a GUI stall of a few
 * seconds lost decoded messages (B-25). The text so far of a message still
 * arriving replaces its older text still waiting, so only the newest
 * counts. */

typedef enum { EV_MESSAGE, EV_CYCLE_DONE, EV_TX_STATUS, EV_TX_DONE } ev_kind_t;

typedef struct {
    ev_kind_t kind;
    union {
        js8_rx_msg_t    msg;
        unsigned        decodes;
        js8_tx_status_t tx_status;
        tx_done_t       tx_done;
    };
} ui_event_t;

#define EVENTS   128 /* a busy slot's decodes are a few dozen */
#define EV_BATCH 16  /* handled per tick */

static pthread_mutex_t ev_lock = PTHREAD_MUTEX_INITIALIZER;
static ui_event_t      ev_queue[EVENTS];
static unsigned        ev_head, ev_count, ev_lost;

static void ui_add_message(void *arg);
static void ui_cycle_done(void *arg);
static void ui_tx_status(void *arg);
static void ui_tx_done(void *arg);

static void ev_push(const ui_event_t *e) {
    pthread_mutex_lock(&ev_lock);
    /* Newer text of a message still arriving (or its end): replaces the
     * text waiting, back to the last end of cycle (which must follow its
     * cycle's messages). */
    if (e->kind == EV_MESSAGE && e->msg.msg_id && !e->msg.tx) {
        for (unsigned k = ev_count; k-- > 0;) {
            ui_event_t *q = &ev_queue[(ev_head + k) % EVENTS];
            if (q->kind == EV_CYCLE_DONE) break;
            if (q->kind == EV_MESSAGE && q->msg.partial && !q->msg.tx && q->msg.msg_id == e->msg.msg_id) {
                *q = *e;
                pthread_mutex_unlock(&ev_lock);
                return;
            }
        }
    }
    if (ev_count == EVENTS) {
        ev_lost++;
    } else {
        ev_queue[(ev_head + ev_count) % EVENTS] = *e;
        ev_count++;
    }
    pthread_mutex_unlock(&ev_lock);
}

static void ev_clear(void) {
    pthread_mutex_lock(&ev_lock);
    ev_count = ev_lost = 0;
    pthread_mutex_unlock(&ev_lock);
}

/* The GUI thread, every WF_TICK_MS. */
static void ev_tick(void) {
    static ui_event_t batch[EV_BATCH];
    pthread_mutex_lock(&ev_lock);
    unsigned n = ev_count < EV_BATCH ? ev_count : EV_BATCH, lost = ev_lost;
    for (unsigned i = 0; i < n; i++) batch[i] = ev_queue[(ev_head + i) % EVENTS];
    ev_head  = (ev_head + n) % EVENTS;
    ev_count -= n;
    ev_lost  = 0;
    pthread_mutex_unlock(&ev_lock);

    if (lost) {
        LV_LOG_WARN("JS8: %u decoder/TX events lost, the GUI fell behind", lost);
        add_info_row("%u decodes lost: the screen fell behind", lost);
    }
    for (unsigned i = 0; i < n; i++) {
        ui_event_t *e = &batch[i];
        switch (e->kind) {
        case EV_MESSAGE: ui_add_message(&e->msg); break;
        case EV_CYCLE_DONE: ui_cycle_done(&e->decodes); break;
        case EV_TX_STATUS: ui_tx_status(&e->tx_status); break;
        case EV_TX_DONE: ui_tx_done(&e->tx_done); break;
        }
    }
}

/* ---- Receiver callbacks (worker threads) ------------------------------- */

static void ui_add_message(void *arg) {
    if (!dialog.run || !table) return;
    add_message((const js8_rx_msg_t *)arg);
}

static void on_message(const js8_rx_msg_t *m, void *ctx) {
    (void)ctx;
    ui_event_t e = {.kind = EV_MESSAGE, .msg = *m};
    ev_push(&e);
}

static void update_status(void) {
    time_t    now = (time_t)(now_wall_ms() / 1000);
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
            } else if (hb_paused()) {
                time_t    t = (time_t)(hb_paused_until / 1000);
                struct tm nt;
                gmtime_r(&t, &nt);
                snprintf(hb, sizeof(hb), "HB paused to %02d:%02d  ", nt.tm_hour, nt.tm_min);
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
    /* JS8 runs off the radio's clock by this much (Time Sync). */
    char    drift[24] = "";
    int64_t d         = js8_drift_ms();
    if (d) snprintf(drift, sizeof(drift), " drift %+.1fs", d / 1000.0);
    lv_label_set_text_fmt(status, "%s%s%s  %02d:%02d:%02dZ%s  total %u", flags, testing ? "TEST WAV  " : "",
                          where_label(), tm.tm_hour, tm.tm_min, tm.tm_sec, drift, hist_count);
}

static void ui_cycle_done(void *arg) {
    if (!dialog.run || !status) return;
    cycles++;
    cycle_decodes = *(unsigned *)arg;
    update_status();
    auto_try_send(); /* the cycle's replies, now that all of it is in */
}

static void on_cycle_done(unsigned decodes, void *ctx) {
    (void)ctx;
    ui_event_t e = {.kind = EV_CYCLE_DONE, .decodes = decodes};
    ev_push(&e);
}

static void wf_queue_clear(void) {
    pthread_mutex_lock(&wf_lock);
    wf_q_count = 0;
    pthread_mutex_unlock(&wf_lock);
}

/* ---- Decode marks (Settings: Decode marks) -------------------------------
 *
 * Desktop JS8Call's "Show decode attempts": a bracket as wide as the signal
 * over the newest waterfall rows wherever the decoder found a JS8 sync and
 * is trying to decode, coloured by the sync's strength, and yellow for what
 * it decoded (desktop's red is our TX band here). They scroll down with the
 * waterfall, so weak signals the waterfall barely shows are marked. The
 * decoder reports them from its thread, many per pass: they wait in their
 * own queue, not the scheduler's, whose 64 items are for messages. */

#define MARK_QUEUE     128                    /* per 5 ms tick; more are dropped (cosmetic) */
#define MARK_MEMORY    48                     /* brackets remembered, to recolour in place */
#define MARK_ROWS      15                     /* bracket height */
#define MARK_LINE      3                      /* line thickness */
#define MARK_SAME_ROWS (8 * WF_ROWS_PER_SEC)  /* another pass over the same signal comes sooner */

typedef struct {
    float    freq_hz;
    uint32_t row; /* lv_waterfall_get_rows() when drawn */
    uint8_t  submode;
    uint8_t  level;
    bool     used;
} mark_drawn_t;

static pthread_mutex_t mark_lock = PTHREAD_MUTEX_INITIALIZER;
static js8_rx_mark_t   mark_queue[MARK_QUEUE];
static unsigned        mark_count;
static mark_drawn_t    marks_drawn[MARK_MEMORY];
static unsigned        marks_next;

static const uint32_t mark_colors[] = {
    [JS8_MARK_WEAK]    = 0x008ca0, /* dark cyan */
    [JS8_MARK_MEDIUM]  = 0x00e6ff, /* cyan */
    [JS8_MARK_STRONG]  = 0xffffff,
    [JS8_MARK_DECODED] = 0xffd600, /* yellow */
};

/* The decoder's thread. */
static void on_mark(const js8_rx_mark_t *m, void *ctx) {
    (void)ctx;
    pthread_mutex_lock(&mark_lock);
    if (mark_count < MARK_QUEUE) mark_queue[mark_count++] = *m;
    pthread_mutex_unlock(&mark_lock);
}

static void marks_reset(void) {
    pthread_mutex_lock(&mark_lock);
    mark_count = 0;
    pthread_mutex_unlock(&mark_lock);
    memset(marks_drawn, 0, sizeof(marks_drawn));
}

/* |-----| from the lowest tone across the signal's bandwidth, its top `y`
 * rows below the newest row. */
static void mark_draw(float freq_hz, int bw_hz, int y, uint8_t level) {
    int span = filter_high - filter_low;
    if (span <= 0) return;
    lv_coord_t x1   = (lv_coord_t)((freq_hz - filter_low) * WIDTH / span);
    lv_coord_t x2   = (lv_coord_t)((freq_hz + bw_hz - filter_low) * WIDTH / span);
    lv_coord_t half = MARK_LINE / 2, mid = y + MARK_ROWS / 2;
    lv_color_t c    = lv_color_hex(mark_colors[level]);
    lv_waterfall_fill_rect(waterfall, x1 - half, y, x1 + half, y + MARK_ROWS - 1, c);
    lv_waterfall_fill_rect(waterfall, x2 - half, y, x2 + half, y + MARK_ROWS - 1, c);
    lv_waterfall_fill_rect(waterfall, x1, mid - half, x2, mid + half, c);
}

/* A new bracket, unless it's the same signal as one drawn moments ago
 * (every decode pass reports its candidates again): that one is recoloured
 * where it has scrolled to, if this is stronger or decoded. */
static void mark_show(const js8_rx_mark_t *m) {
    int      bw  = js8_speed_bandwidth_hz(js8_speed_from_submode(m->submode));
    uint32_t now = lv_waterfall_get_rows(waterfall);
    for (unsigned i = 0; i < MARK_MEMORY; i++) {
        mark_drawn_t *d = &marks_drawn[i];
        if (!d->used || d->submode != m->submode) continue;
        uint32_t age = now - d->row;
        if (age > MARK_SAME_ROWS || fabsf(d->freq_hz - m->freq_hz) > bw / 2.0f) continue;
        if (m->level > d->level) {
            mark_draw(d->freq_hz, bw, (int)age, m->level);
            d->level = m->level;
        }
        return;
    }
    mark_draw(m->freq_hz, bw, 0, m->level);
    marks_drawn[marks_next] = (mark_drawn_t){.freq_hz = m->freq_hz, .row = now, .submode = m->submode,
                                             .level = m->level, .used = true};
    marks_next = (marks_next + 1) % MARK_MEMORY;
}

static void marks_tick(void) {
    js8_rx_mark_t batch[MARK_QUEUE];
    pthread_mutex_lock(&mark_lock);
    unsigned n = mark_count;
    memcpy(batch, mark_queue, n * sizeof(batch[0]));
    mark_count = 0;
    pthread_mutex_unlock(&mark_lock);
    if (!params.js8_decode_marks.x || !waterfall) return;
    for (unsigned i = 0; i < n; i++) mark_show(&batch[i]);
}

static int64_t now_mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* At most one row per tick, each when it's due by the clock. Rows are made
 * per 735 samples at 11025 Hz and the audio clock isn't the CPU's, so a
 * queue that builds up is drained by drawing slightly faster, never by a
 * jump. */
static void wf_timer_cb(lv_timer_t *t) {
    (void)t;
    marks_tick();
    ev_tick();
    pthread_mutex_lock(&wf_lock);
    unsigned waiting = wf_q_count;
    pthread_mutex_unlock(&wf_lock);
    if (!waiting) return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t now    = (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    int64_t period = 1000000 / WF_ROWS_PER_SEC;
    if (waiting > 6) period = period * 3 / 4;
    else if (waiting > 3) period = period * 19 / 20;
    /* After a pause (no audio while transmitting) start again now. */
    if (now - wf_due_us > period) wf_due_us = now;
    if (now < wf_due_us) return;

    static float row[WIDTH];
    pthread_mutex_lock(&wf_lock);
    memcpy(row, wf_rows[wf_q_head], sizeof(row));
    wf_q_head = (wf_q_head + 1) % WF_QUEUE;
    wf_q_count--;
    pthread_mutex_unlock(&wf_lock);
    lv_waterfall_add_data(waterfall, row, WIDTH);
    wf_due_us += period;
    /* On the screen now, not at LVGL's next refresh: that comes every
     * 33 ms by a tick that runs slow, so rows landed 66-134 ms apart
     * instead of evenly. */
    lv_refr_now(NULL);
}

/* The k-th smallest of v[0..n-1], reordering v: quickselect (Hoare's
 * partition), O(n), where sorting the row was O(n log n) per row. */
static float select_nth(float *v, int n, int k) {
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        float pivot = v[lo + (hi - lo) / 2];
        int   i = lo, j = hi;
        while (i <= j) {
            while (v[i] < pivot) i++;
            while (v[j] > pivot) j--;
            if (i <= j) {
                float t = v[i];
                v[i]    = v[j];
                v[j]    = t;
                i++;
                j--;
            }
        }
        if (k <= j) hi = j;
        else if (k >= i) lo = i;
        else return v[k];
    }
    return v[k];
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

    /* One value per pixel: the strongest of the bins under it, so a narrow
     * signal between two pixels' centres doesn't flicker. */
    uint32_t bins = high_bin - low_bin;
    for (uint32_t x = 0; x < WIDTH; x++) {
        uint32_t b0 = low_bin + x * bins / WIDTH, b1 = low_bin + (x + 1) * bins / WIDTH;
        float    v  = psd[b0];
        for (uint32_t b = b0 + 1; b < b1; b++) v = psd[b] > v ? psd[b] : v;
        wf_row[x] = v;
    }

    /* Noise floor: the 30th percentile of the row, smoothed over ~2 s. */
    memcpy(wf_sel, wf_row, sizeof(wf_sel));
    float floor_now = select_nth(wf_sel, WIDTH, WIDTH * 3 / 10);
    wf_floor_db     = wf_floor_set ? wf_floor_db + 0.05f * (floor_now - wf_floor_db) : floor_now;
    wf_floor_set    = true;
    for (uint32_t i = 0; i < WIDTH; i++) wf_row[i] -= wf_floor_db;

    /* Into the ring for wf_timer_cb; far behind, the oldest goes. */
    pthread_mutex_lock(&wf_lock);
    if (wf_q_count == WF_QUEUE) {
        wf_q_head = (wf_q_head + 1) % WF_QUEUE;
        wf_q_count--;
    }
    memcpy(wf_rows[(wf_q_head + wf_q_count) % WF_QUEUE], wf_row, sizeof(wf_row));
    wf_q_count++;
    pthread_mutex_unlock(&wf_lock);
}

static void on_audio(const float *samples, unsigned n, void *ctx) {
    (void)ctx;
    if (!sg) return;

    /* One row per WF_ROW_SAMPLES of audio, however the audio is chunked.
     * Exact zeros are the beep guard's silence (real audio never is): the
     * waterfall pauses over them instead of drawing a dark band. */
    while (n) {
        unsigned take = WF_ROW_SAMPLES - wf_row_fill;
        if (take > n) take = n;
        for (unsigned i = 0; i < take; i++) {
            if (samples[i] == 0.0f) {
                take = i;
                break;
            }
        }
        if (!take) { /* a run of guard silence: skip it */
            while (n && *samples == 0.0f) samples++, n--;
            continue;
        }
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
    /* At least a bin per pixel, rounded up to a power of two: FFTW is
     * several times faster at 4096 than at 3035 (= 5 x 607). */
    int      span = filter_high - filter_low;
    unsigned want = span > 0 ? WIDTH * SAMPLE_RATE / span : 4096;
    nfft          = 256;
    while (nfft < want && nfft < 8192) nfft *= 2;
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
        .on_mark       = on_mark,
    };
    marks_reset();
    rx = js8_rx_create(SAMPLE_RATE, rx_speed_mask(), params.callsign.x, &cb);
    if (!rx) msg_schedule_text_fmt("JS8: cannot start decoder");
    js8_rx_set_decode_range(rx, filter_low, filter_high);
    js8_rx_set_qso_offset(rx, params.js8_tx_freq.x);
    js8_rx_set_sync_marks(rx, params.js8_decode_marks.x);
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

static float beep_level_before_db = -200.0f; /* receiver level, smoothed (beep log) */

static bool beep_guard_active(void) {
    return atomic_load(&beep_guard) || now_mono_ms() < atomic_load(&beep_guard_end);
}

static void audio_cb(unsigned int n, float *samples) {
    if (atomic_load(&keyed)) return; /* our own TX, or nothing useful */
    if (beep_guard_active()) {
        static const float silence[512];
        double             sq = 0;
        for (unsigned i = 0; i < n; i++) sq += (double)samples[i] * samples[i];
        atomic_fetch_add(&beep_guard_sq, (int64_t)(sq * 1e9));
        atomic_fetch_add(&beep_guard_n, n);
        for (unsigned at = 0; at < n; at += 512) js8_rx_feed(rx, silence, n - at < 512 ? n - at : 512);
        return;
    }
    if (n) {
        double sq = 0;
        for (unsigned i = 0; i < n; i++) sq += (double)samples[i] * samples[i];
        float db             = (float)(10.0 * log10(sq / n + 1e-24));
        beep_level_before_db = beep_level_before_db < -150.0f ? db : 0.9f * beep_level_before_db + 0.1f * db;
    }
    js8_rx_feed(rx, samples, n);
}

/* ---- Transmit ---------------------------------------------------------- */

/* TX thread. Reading `tx` here is safe: tx_stop_all() clears it only after
 * js8_tx_destroy() has stopped and joined this thread. */
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
    until.tv_sec += 2;
    if (pthread_mutex_timedlock(&speaker_lock, &until) == 0) pthread_mutex_unlock(&speaker_lock);
    else radio_speaker_play(false); /* never key with the speaker path switched */
    /* Per frame, so a power change made while the app is open counts. */
    base_gain_offset = tx_player_base_gain_offset();
    bool done = tx_player_play(samples, n, atomic_load(&tx_offset_active), base_gain_offset, tx_abort_check, NULL);
    atomic_store(&keyed, false);
    return done;
}

static int current_utc_hhmmss(void) {
    time_t    now = (time_t)(now_wall_ms() / 1000);
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
        /* AGN? repeats what went out, as desktop: not a message stopped
         * before it keyed. */
        snprintf(last_tx_text, sizeof(last_tx_text), "%s", st->text);
        /* Only what you send is your side of a QSO: an unattended station
         * answering SNR? and hearing "TNX 73" hasn't had one. */
        char ended[JS8_RX_CALL_LEN];
        if (!tx_auto && js8_qsos_sent(qsos, m.text, params.callsign.x, now_wall_ms(), ended, sizeof(ended)))
            log_offer(ended);
    }
    tx_status = *st;
    update_tx_bar();
}

static void on_tx_status(const js8_tx_status_t *st, void *ctx) {
    (void)ctx;
    ui_event_t e = {.kind = EV_TX_STATUS, .tx_status = *st};
    ev_push(&e);
}

static void ui_tx_done(void *arg) {
    if (!dialog.run || !tx_bar) return;
    const tx_done_t *done      = arg;
    bool             completed = done->completed;
    if (!completed) add_info_row("TX stopped");
    deliver_end(done->text, completed);
    memset(&tx_status, 0, sizeof(tx_status));
    tx_active = false;
    update_tx_bar();
    /* Auto CQ counts its minutes from the end of our CQ (your choice);
     * replies and heartbeats in between don't move it. */
    if (auto_cq && tx_cq) {
        auto_cq_from_ms = now_wall_ms();
        auto_cq_next_ms = auto_cq_from_ms + cq_interval_ms();
        if (btn_cq.disp_btn) buttons_refresh(&btn_cq);
    }
    tx_cq = false;

    /* Replies that arrived while we were sending. */
    auto_try_send();
}

static void on_tx_done(const char *text, bool completed, void *ctx) {
    (void)ctx;
    ui_event_t e = {.kind = EV_TX_DONE, .tx_done = {.completed = completed}};
    snprintf(e.tx_done.text, sizeof(e.tx_done.text), "%s", text ? text : "");
    ev_push(&e);
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
    /* Stops the current frame (tx_player drops PTT within one part) and
     * waits for the TX thread; only then is `tx` cleared. */
    js8_tx_destroy(tx);
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
        auto_cq_tick();
        update_status();
    }
    if (view_stations && ticks % 20 == 0) rebuild_station_rows(); /* ages */
}

/* The TX bar and the waterfall frame as last set. Every LVGL style or
 * text call redraws, changed or not, and the frame's redraw spills onto the
 * dialog background behind the waterfall: set only what changed (I-14). */
static struct {
    bool     fresh; /* just created: set everything */
    uint32_t bg;
    bool     recolor;
    char     text[JS8_RX_TEXT_LEN + 64];
    bool     frame;
} tx_bar_shown;

static void tx_bar_set(uint32_t bg, bool recolor, const char *text) {
    bool fresh = tx_bar_shown.fresh;
    if (fresh || tx_bar_shown.bg != bg) lv_obj_set_style_bg_color(tx_bar, lv_color_hex(bg), 0);
    if (fresh || tx_bar_shown.recolor != recolor) lv_label_set_recolor(tx_bar, recolor);
    if (fresh || strcmp(tx_bar_shown.text, text) != 0) lv_label_set_text(tx_bar, text);
    tx_bar_shown.bg      = bg;
    tx_bar_shown.recolor = recolor;
    snprintf(tx_bar_shown.text, sizeof(tx_bar_shown.text), "%s", text);
    if (fresh) { /* the frame too: its colour never changes */
        lv_obj_set_style_border_color(waterfall, lv_color_hex(0xff2020), 0);
        lv_obj_set_style_border_width(waterfall, 0, 0);
        tx_bar_shown.frame = false;
        tx_bar_shown.fresh = false;
    }
}

/* A red frame round the waterfall while keyed. */
static void wf_frame_set(bool on) {
    if (tx_bar_shown.frame == on) return;
    lv_obj_set_style_border_width(waterfall, on ? 3 : 0, 0);
    tx_bar_shown.frame = on;
}

/* "TX 1500 Hz  ready" / "... K2XYZ SNR?  starts in 9 s" / "... sending 2/3" */
static void update_tx_bar(void) {
    if (!tx_bar) return;
    char     line[JS8_RX_TEXT_LEN + 64];
    uint16_t offset = params.js8_tx_freq.x;

    if (cq_adjusting && tx_status.state == JS8_TX_IDLE) {
        snprintf(line, sizeof(line), "Auto CQ %u min after each CQ: turn the knob (%d-%d), press CQ when done",
                 params.js8_cq_interval.x, CQ_MIN_INTERVAL, CQ_MAX_INTERVAL);
        tx_bar_set(0x5a4a00, false, line);
        return;
    }
    if (hb_adjusting && tx_status.state == JS8_TX_IDLE) {
        snprintf(line, sizeof(line), "HB every %u min: turn the knob (5-30), press HB when done",
                 params.js8_hb_interval.x);
        tx_bar_set(0x5a4a00, false, line);
        return;
    }

    uint32_t bg;

    switch (tx_status.state) {
    case JS8_TX_WAITING: {
        int64_t now_ms = now_wall_ms(); /* the transmitter's slots are in JS8 time */
        int     secs   = (int)((tx_status.next_ms - now_ms + 999) / 1000);
        if (secs < 0) secs = 0;
        /* Progress before the text: a long message only loses its end. */
        snprintf(line, sizeof(line), "TX %4.0f Hz %s  %d/%d %s %d s   %s", tx_status.offset_hz,
                 js8_speed_name(tx_status.speed), tx_status.frame, tx_status.frames,
                 tx_status.frame == 1 ? "starts in" : "next in", secs, tx_status.text);
        bg = 0x5a4a00;
        break;
    }
    case JS8_TX_KEYING:
        snprintf(line, sizeof(line), "TX %4.0f Hz %s  sending %d/%d   %s", tx_status.offset_hz,
                 js8_speed_name(tx_status.speed), tx_status.frame, tx_status.frames, tx_status.text);
        bg = 0xa00000;
        break;
    default:
        /* A locked station in red (recolor only here: sent text may hold '#'). */
        snprintf(line, sizeof(line), "TX %4u Hz %s   ready%s%s", offset, js8_speed_name(cur_speed()),
                 params.callsign.x[0] ? "" : "  (set your callsign: APP > Callsign)",
                 !sel_call[0] ? "" : sel_locked ? "      #ff5050 locked: " : "      selected: ");
        if (sel_call[0]) strncat(line, sel_call, sizeof(line) - strlen(line) - 1);
        if (sel_call[0] && sel_locked) strncat(line, "#", sizeof(line) - strlen(line) - 1);
        if (auto_cq) strncat(line, "      auto CQ", sizeof(line) - strlen(line) - 1);
        bg = 0x202020;
        break;
    }
    tx_bar_set(bg, tx_status.state != JS8_TX_WAITING && tx_status.state != JS8_TX_KEYING && sel_locked, line);
    wf_frame_set(tx_status.state == JS8_TX_KEYING);
}

/* Queue `text` at our offset. Returns false (with a message shown) if it
 * can't be sent, e.g. a bad character or something already sending. */
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
    if (!tx) { /* js8_tx_create() failed when the app opened */
        msg_update_text_fmt("JS8: transmitter not running - close JS8 and open it again");
        return false;
    }
    if (tx_active || js8_tx_busy(tx)) {
        msg_update_text_fmt("Already sending - Stop TX first");
        return false;
    }

    js8_tx_preview_t pv;
    js8_tx_preview(params.callsign.x, params.qth.x, text, cur_speed(), &pv);
    if (!pv.ok) {
        msg_update_text_fmt("JS8: %s", pv.error);
        return false;
    }

    char err[JS8_TX_ERR_LEN] = "";
    atomic_store(&tx_offset_active, offset_hz);
    snprintf(tx_preview, sizeof(tx_preview), "%s", pv.preview);
    if (!js8_tx_send(tx, params.callsign.x, params.qth.x, text, (float)offset_hz, cur_speed(), err, sizeof(err))) {
        msg_update_text_fmt("JS8: %s", err);
        return false;
    }
    tx_active   = true;
    tx_auto     = automatic;
    tx_cq       = false;
    tx_quiet_ms = 0;
    msg_update_text_fmt("%sQueued: %d frame%s, %.0f s", automatic ? "Auto: " : "", pv.frames,
                        pv.frames == 1 ? "" : "s", pv.seconds);

    /* A directed message you sent yourself starts a QSO. */
    char call[JS8_RX_CALL_LEN];
    if (!automatic && starts_with_call(text, call, sizeof(call))) auto_cq_stop("replying");
    /* Anything you send by hand, except a heartbeat, pauses heartbeats. */
    if (!automatic) {
        char hb[48];
        js8_heartbeat_text(params.callsign.x, params.qth.x, hb, sizeof(hb));
        if (strcmp(text, hb) != 0) hb_pause(strncmp(text, "CQ ", 3) == 0 ? "CQ" : "you sent");
    }
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
    if (cq_adjusting) {
        cq_adjust_turn(diff);
        return;
    }
    if (hb_adjusting) {
        int v = (int)params.js8_hb_interval.x + (diff > 0 ? 1 : -1);
        if (v < JS8_HB_MIN_INTERVAL) v = JS8_HB_MIN_INTERVAL;
        if (v > JS8_HB_MAX_INTERVAL) v = JS8_HB_MAX_INTERVAL;
        params_uint16_set(&params.js8_hb_interval, (uint16_t)v);
        hb_adjust_ms = now_wall_ms();
        if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
        if (params.js8_hb.x && hb_next_ms) hb_next_ms = hb_first_ms();
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
    const char *typed = textarea_window_get();
    js8_tx_preview(params.callsign.x, params.qth.x, typed, cur_speed(), &pv);
    if (pv.ok) msg_update_text_fmt("%d frame%s, %.0f s", pv.frames, pv.frames == 1 ? "" : "s", pv.seconds);
    /* Why it can't go ("too long: 21 frames (max 20)"), as you type, not
     * the last good count frozen until Enter (B-23). */
    else if (typed[strspn(typed, " ")]) msg_update_text_fmt("JS8: %s", pv.error);
}

/* JS8 is capitals only: take lowercase (a USB keyboard without Caps Lock,
 * the on-screen "abc" page) as capitals instead of dropping it. */
static void compose_insert_cb(lv_event_t *e) {
    const char *in = lv_event_get_param(e);
    static char up[8];
    size_t      n = strlen(in);
    if (n >= sizeof(up)) return;
    bool lower = false;
    for (size_t i = 0; i <= n; i++) {
        up[i] = in[i] >= 'a' && in[i] <= 'z' ? (char)(in[i] - 'a' + 'A') : in[i];
        lower |= up[i] != in[i];
    }
    if (lower) lv_textarea_set_insert_replace(lv_event_get_target(e), up);
}

/* While typing, the text box goes to the top of the screen and the list
 * takes the space the on-screen keyboard leaves (over the waterfall), so
 * the other station's words stay readable as they arrive. */
static void compose_layout(bool typing) {
    if (!table) return;
    bool       kb_shown = typing && !keyboard_ready();
    lv_area_t  d;
    lv_obj_get_coords(dialog.obj, &d);
    lv_coord_t kb_top = lv_obj_get_height(lv_scr_act()) / 2; /* lv_keyboard: the bottom half */
    if (kb_shown) {
        lv_obj_set_pos(table, 13, 13);
        lv_obj_set_size(table, WIDTH, kb_top - d.y1 - 13 - 2);
        lv_obj_add_flag(tx_bar, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_set_pos(table, 13, 13 + WF_VISIBLE + TX_BAR_H);
        lv_obj_set_size(table, WIDTH, WF_HEIGHT - WF_VISIBLE - TX_BAR_H);
        lv_obj_clear_flag(tx_bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (at_bottom()) follow();
}

static void compose_close(void) {
    if (!composing) return;
    textarea_window_close();
    composing = false;
    compose_layout(false);
    if (table) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* Same pattern as the FT8 app's keyboard: close here and return true;
 * textarea_window's own close is then a no-op. */
static bool compose_ok_cb(void) {
    if (edit_target == EDIT_FREQ && !parse_custom(textarea_window_get(), NULL)) return false;
    if (edit_target == EDIT_SPOT_FREQ && !spot_freq_empty(textarea_window_get()) &&
        !spot_parse_freq(textarea_window_get(), NULL))
        return false;
    /* A typed log grid must be one (or nothing), not "HOME". */
    if (edit_target == EDIT_LOG_GRID && textarea_window_get()[strspn(textarea_window_get(), " ")] &&
        !js8_is_grid(textarea_window_get())) {
        msg_update_text_fmt("Not a grid: 4 or 6 characters, e.g. CN89 or CN89KG");
        return false;
    }
    if (edit_target >= EDIT_LOG_GRID) {
        log_edit_done(textarea_window_get());
        return true;
    }
    if (edit_target == EDIT_OPERATOR) {
        const char *typed = textarea_window_get();
        while (*typed == ' ') typed++;
        if (!*typed) {
            operator_call[0] = '\0';
            msg_update_text_fmt("Operator: the station call (%s)", params.callsign.x);
        } else if (js8_operator_call_valid(typed, operator_call, sizeof(operator_call))) {
            msg_update_text_fmt("Operator %s: logged as OPERATOR; %s is still sent on the air", operator_call,
                                params.callsign.x);
        } else {
            msg_update_text_fmt("Not a callsign: letters, digits and /, e.g. VA7XYZ");
            return false; /* keep the keyboard open */
        }
        save_texts();
        edit_target = 0;
        compose_close();
        return true;
    }
    if (edit_target == EDIT_GROUPS) {
        js8_groups_normalise(textarea_window_get(), groups_text, sizeof(groups_text));
        save_texts();
        if (groups_text[0]) msg_update_text_fmt("Groups: %s", groups_text);
        else msg_update_text_fmt("No groups");
        edit_target = 0;
        compose_close();
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
    /* The held message offered on Reply went as offered: it's on its way. */
    if (deliver_pending.id && strcasecmp(text, deliver_pending.text) == 0)
        deliver_start(deliver_pending.id, deliver_pending.group_call, text);
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

/* A list popup gives way to the keyboard or another view: its buttons out
 * of the group now (the list itself goes later: this runs in one of its
 * callbacks), and the focus not handed back to the table, or the keyboard
 * opens without it and can't be used. */
static void popup_leave(lv_obj_t **list) {
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(*list); i++) lv_group_remove_obj(lv_obj_get_child(*list, i));
    lv_obj_del_async(*list);
    *list = NULL;
}

/* From a list (or none) into the keyboard, editing `target` (0: a message
 * to send) with `prefill`. False if it didn't open (no callsign to send). */
static bool popup_to_keyboard(lv_obj_t **list, int target, const char *prefill) {
    if (list && *list) popup_leave(list);
    edit_target = target;
    compose_open(prefill);
    if (!composing) return false;
    lv_group_set_editing(keyboard_group, true); /* as after Reply / Send... */
    return true;
}

static void compose_open(const char *prefill) {
    if (composing) return;
    /* The callsign only for what sends (a message, an APRS beacon): a
     * frequency, alert words, INFO or a log field can be typed without one
     * (B-16). Refused, the edit mode the caller set is undone, or the next
     * Send... would open in it. */
    if (!params.callsign.x[0] && (edit_target == 0 || edit_target >= EDIT_BEACON_GRID)) {
        msg_update_text_fmt("Set your callsign first: APP > Callsign");
        edit_target        = 0;
        deliver_pending.id = 0;
        return;
    }
    composing = true;
    lv_group_remove_obj(table);
    lv_obj_set_y(textarea_window_open(compose_ok_cb, compose_cancel_cb), 0);
    compose_layout(true);

    /* Every printable ASCII character: JS8 sends them all (as desktop,
     * whose text boxes take any), $ % < > [ ] ^ | ~ \ ` included (B-22). */
    static char printable[96];
    if (!printable[0])
        for (int c = ' '; c <= '~'; c++) printable[c - ' '] = (char)c;
    lv_obj_t *text = textarea_window_text();
    lv_textarea_set_accepted_chars(text, printable);
    lv_obj_add_event_cb(text, compose_insert_cb, LV_EVENT_INSERT, NULL);
    lv_textarea_set_max_length(text, TX_TEXT_MAX);
    lv_obj_add_event_cb(text, compose_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (edit_target) {
        lv_textarea_set_max_length(text, edit_target == EDIT_LOG_GRID   ? 6
                                         : edit_target == EDIT_POTA_REF ? sizeof(last_pota) - 1
                                         : edit_target == EDIT_SOTA_REF ? sizeof(last_sota) - 1
                                         : edit_target == EDIT_ALERT_WORDS ? sizeof(alert_words) - 1
                                         : edit_target == EDIT_GROUPS ? sizeof(groups_text) - 1
                                         : edit_target == EDIT_OPERATOR ? sizeof(operator_call) - 1
                                         : edit_target == EDIT_FREQ ? 9
                                         : edit_target == EDIT_SPOT_REF ? (spot_sota ? sizeof(last_sota) : sizeof(last_pota)) - 1
                                         : edit_target == EDIT_SPOT_FREQ ? 10
                                         : edit_target == EDIT_SPOT_NOTE ? sizeof(spot_note) - 1
                                         : edit_target >= EDIT_BEACON_GRID ? APRS_COMMENT_MAX
                                                                        : TEXT_MAX);
        if (edit_target == EDIT_FREQ || edit_target == EDIT_SPOT_FREQ)
            lv_textarea_set_accepted_chars(text, "0123456789.");
        lv_obj_remove_event_cb(text, compose_changed_cb);
        if (edit_target >= EDIT_BEACON_GRID) lv_obj_add_event_cb(text, beacon_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
    if (prefill && prefill[0]) {
        textarea_window_set(prefill);
    } else {
        static const char *const placeholders[EDIT_COUNT] = {
            [0]             = " CALL MESSAGE / @ALLCALL ...",
            [EDIT_INFO]     = " INFO, e.g. X6100 5W EFHW",
            [EDIT_STATUS]   = " STATUS, e.g. PORTABLE QRV",
            [EDIT_GROUPS]   = " Your groups, e.g. @NET @CANADA",
            [EDIT_OPERATOR] = " Operator's call - empty: the station's",
            [EDIT_LOG_GRID] = " Their grid, e.g. DN17",
            [EDIT_LOG_NAME] = " Their name",
            [EDIT_LOG_NOTE] = " Comment for the log",
            [EDIT_POTA_REF] = " Your park, e.g. CA-1234",
            [EDIT_SOTA_REF] = " Your summit, e.g. VE7/LM-001",
            [EDIT_ALERT_WORDS] = " Calls or words, e.g. VE7ABC @POTA SOTA",
            [EDIT_FREQ]     = " Dial frequency in kHz, e.g. 7107",
            [EDIT_SPOT_REF] = " Park CA-1234 / summit VE7/LM-001",
            [EDIT_SPOT_FREQ] = " kHz, e.g. 7185 - empty: the JS8 dial",
            [EDIT_SPOT_NOTE] = " Comment, e.g. QRT or CQ",
            [EDIT_BEACON_GRID] = " Message (optional) - Enter sends",
            [EDIT_BEACON_GPS] = " Message (optional) - Enter sends",
        };
        lv_textarea_set_placeholder_text(text, placeholders[edit_target]);
    }
}

/* ---- Band and screen -------------------------------------------------- */

/* "7107.5" from 7107500 Hz: kHz, decimals only when needed. */
static void format_khz(int32_t hz, char *buf, size_t size) {
    snprintf(buf, size, "%d.%03d", (int)(hz / 1000), (int)(hz % 1000));
    char *end = buf + strlen(buf) - 1;
    while (*end == '0') *end-- = '\0';
    if (*end == '.') *end = '\0';
}

/* What the top bar and the info rows call where we are. */
static const char *where_label(void) {
    static char buf[32];
    if (!params.js8_custom_on.x) return cfg_digital_label_get();
    char khz[16];
    format_khz(params.js8_custom_hz.x, khz, sizeof(khz));
    snprintf(buf, sizeof(buf), "JS8 %s kHz", khz);
    return buf;
}

/* JS8 always runs in USB-D (the mode keys are locked while it's open).
 * Tuning into another band loads that band's saved mode (USB if it was
 * last used for SSB), so this follows every retune. */
static void js8_usb_dig(void) {
    if (cparam_i_get(cfg_cur_mode) != x6100_mode_usb_dig) cparam_i_set(cfg_cur_mode, x6100_mode_usb_dig);
}

/* The presets the band keys step through: JS8Call's or GhostNet's. False
 * past either end of the list (nothing changes). */
static bool load_band(int8_t dir) {
    cfg_digital_type_t set = params.js8_ghostnet.x ? CFG_DIG_TYPE_JS8_GHOSTNET : CFG_DIG_TYPE_JS8;
    bool               ok  = cfg_digital_load(dir, set);
    js8_usb_dig(); /* even with no preset found */
    if (!ok) return false;
    msg_update_text_fmt("%s", cfg_digital_label_get());
    return true;
}

static js8_stations_t *stations_for_band(void) {
    int32_t khz = cparam_i_get(cfg_fg_freq) / 1000;
    for (int i = 0; i < BAND_LISTS; i++) {
        if (band_lists[i].list && band_lists[i].dial_khz == khz) return band_lists[i].list;
    }
    for (int i = 0; i < BAND_LISTS; i++) {
        if (!band_lists[i].list) {
            band_lists[i].dial_khz = khz;
            band_lists[i].list     = js8_stations_create();
            return band_lists[i].list;
        }
    }
    /* Many custom frequencies: reuse the slots in turn. */
    int i = band_lists_next;
    band_lists_next = (band_lists_next + 1) % BAND_LISTS;
    js8_stations_reset(band_lists[i].list); /* another frequency: nobody heard yet */
    band_lists[i].dial_khz = khz;
    return band_lists[i].list;
}

/* After any change of dial frequency. */
static void retuned(void) {
    auto_cq_stop("band changed");
    /* Replies still waiting (behind our TX or the keyboard) or offered on
     * Reply answered stations on the old frequency: never send them here.
     * AGN? there isn't asking for what we sent here. */
    auto_clear();
    last_tx_text[0] = '\0';

    js8_rx_clear(rx);
    bool ended = partials_end();
    stations   = stations_for_band(); /* that band's list, as we left it */
    lv_waterfall_clear_data(waterfall);
    wf_queue_clear();
    marks_reset();
    clear_selection();
    if (view_stations || ended) rebuild_rows();
    add_info_row("%s", where_label());
    if (btn_freq.disp_btn) buttons_refresh(&btn_freq);
    update_status();
}

/* Band keys leave a custom frequency for the preset list. Not while
 * sending, as the Freq list: the rest of the message would go out on the
 * new band, and tx_player puts the old dial back after each frame. */
static void band_cb(lv_event_t *e) {
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    if (!load_band(lv_event_get_code(e) == EVENT_BAND_UP ? 1 : -1)) {
        msg_update_text_fmt("End of the %s list", params.js8_ghostnet.x ? "GhostNet" : "JS8");
        return;
    }
    if (params.js8_custom_on.x) params_bool_set(&params.js8_custom_on, false);
    retuned();
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    user_touch();

    switch (key) {
    case LV_KEY_ESC:
        LV_LOG_USER("JS8 ESC on the list: tick %u composing %d popup %d tx %d", (unsigned)lv_tick_get(), composing,
                    any_popup(), js8_tx_busy(tx));
        if (cq_adjusting) {
            cq_adjust_end();
        } else if (hb_adjusting) {
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

    /* Nothing transmits on its own when the app opens: AUTO, HB, HB ACK
     * and auto CQ start off every time (the HB interval is remembered). */
    params_bool_set(&params.js8_auto, false);
    params_bool_set(&params.js8_hb, false);
    params_bool_set(&params.js8_hb_ack, false);
    auto_cq = false;

    /* Full-screen app with its own waterfall: skip main-screen DSP. */
    dsp_set_waterfall_enabled(false);
    dsp_set_spectrum_enabled(false);

    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_UP, NULL);
    lv_obj_add_event_cb(dialog.obj, band_cb, EVENT_BAND_DOWN, NULL);

    mem_save(MEM_BACKUP_ID);
    load_band(0); /* also sets the mode */
    if (params.js8_custom_on.x) {
        if (params.js8_custom_hz.x >= CUSTOM_MIN_HZ && params.js8_custom_hz.x <= CUSTOM_MAX_HZ)
            cparam_i_set(cfg_fg_freq, params.js8_custom_hz.x);
        else
            params_bool_set(&params.js8_custom_on, false);
    }
    js8_usb_dig(); /* a custom frequency on another band loaded that band's mode */

    /* 200-3000 Hz while JS8 is open. High first: each edge is validated
     * against the other. */
    saved_filter_low  = cparam_i_get(cfg_cur_filter_low);
    saved_filter_high = cparam_i_get(cfg_cur_filter_high);
    filter_saved      = true;
    cparam_i_set(cfg_cur_filter_high, JS8_FILTER_HIGH);
    cparam_i_set(cfg_cur_filter_low, JS8_FILTER_LOW);
    /* The TX filter too, so a signal up to 3000 Hz goes out whole. Only the
     * radio is told: the setting itself is untouched, so a power loss here
     * can't leave it changed. */
    radio_set_tx_filter(JS8_TX_FILTER_LOW, JS8_TX_FILTER_HIGH);
    /* The base applies noise reduction, the noise blanker and the notches in
     * DIGI modes too, and they damage JS8's tones (the auto-notch goes for
     * exactly such steady tones). Off while the app is open, radio only. */
    radio_set_rx_dsp_off(true);

    filter_low  = cparam_i_get(cfg_cur_filter_low);
    filter_high = cparam_i_get(cfg_cur_filter_high);

    /* Waterfall, in an opaque black box. LVGL 8.3's lv_img never reports
     * that it covers what's behind it (its cover check reads the event
     * parameter as a clip area), so every row redrew the dialog's
     * background image under the waterfall: 1 MB read from file line by
     * line and alpha-blended, most of the cost of a row. A plain opaque
     * object does cover, and drawing starts there, as for the main
     * screen's waterfall. */
    lv_obj_t *wf_box = lv_obj_create(dialog.obj);
    lv_obj_remove_style_all(wf_box);
    lv_obj_set_style_bg_color(wf_box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(wf_box, LV_OPA_COVER, 0);
    lv_obj_clear_flag(wf_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(wf_box, WIDTH, WF_HEIGHT);
    lv_obj_set_pos(wf_box, 13, 13);

    waterfall = lv_waterfall_create(wf_box);
    lv_obj_add_style(waterfall, &waterfall_style, 0);
    lv_obj_clear_flag(waterfall, LV_OBJ_FLAG_SCROLLABLE);
    lv_waterfall_set_palette(waterfall, (lv_color_t *)wf_palette, 256);
    lv_waterfall_set_size(waterfall, WIDTH, WF_HEIGHT);
    lv_waterfall_set_min(waterfall, WF_MIN_DB);
    lv_waterfall_set_max(waterfall, WF_MAX_DB);
    wf_due_us = 0;
    wf_timer  = lv_timer_create(wf_timer_cb, WF_TICK_MS, NULL);
    lv_obj_set_pos(waterfall, 0, 0);

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
    qso_freq       = -1;
    sel_call[0]    = '\0';
    sel_locked     = false;
    cursor_freq    = -1;
    cursor_user_ms = 0;
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
    tx_bar_shown.fresh = true;

    /* Message list */

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_add_event_cb(table, table_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(table, table_select_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(table, table_hold_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
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
    /* sony_24, with js8_marks_24 for the end mark and the degree sign. */
    table_font          = sony_24;
    table_font.fallback = &js8_marks_24;
    lv_obj_set_style_text_font(table, &table_font, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 5, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(table, 0, LV_PART_ITEMS);

    lv_group_add_obj(keyboard_group, table);
    lv_group_set_editing(keyboard_group, true);

    rebuild_rows();
    add_info_row("%s", where_label());

    main_screen_lock_ab(true);
    keypad_set_long_time(HOLD_MS); /* page back, auto CQ etc. without the long wait */
    main_screen_lock_mode(true);
    main_screen_lock_freq(true);
    main_screen_lock_band(true);

    cycles = cycle_decodes = 0;
    ev_clear(); /* anything left from the last time JS8 was open */
    edit_target = 0; /* a keyboard mode left from last time (B-16) */
    worked_forget();
    wf_queue_clear();
    rx_start();
    update_status();

    /* Transmit: same 5 W cap and gain start as the FT8 app. */
    if (param_f_get(cfg_pwr) > TX_PLAYER_MAX_PWR_W) {
        radio_set_pwr(TX_PLAYER_MAX_PWR_W);
        msg_schedule_text_fmt("Power was limited to %0.0fW", TX_PLAYER_MAX_PWR_W);
    }
    base_gain_offset = tx_player_base_gain_offset();
    tx_start();
    /* The lists stay while the radio is on, like the messages: closing and
     * reopening JS8 used to empty them, so the next decodes came back all at
     * one time and without the ★ of those that had heard us. Stations still
     * drop off an hour after they were last heard (StationList::EXPIRE_MS). */
    stations = stations_for_band();
    if (!qsos) qsos = js8_qsos_create();
    /* Opened once per power-on; a file that couldn't be read was moved
     * aside (or is left alone): say so. */
    if (!inbox) {
        inbox = js8_inbox_open(JS8_INBOX_PATH);
        data_file_notice(js8_inbox_notice(inbox));
    }
    if (!held) {
        held = js8_held_open(JS8_HELD_PATH);
        data_file_notice(js8_held_notice(held));
    }
    if (!autop) autop = js8_auto_create();
    memset(deliveries, 0, sizeof(deliveries));
    tx_active = tx_auto = tx_cq = false;
    inbox_refresh_button();
    user_touch();
    load_texts();
    hb_next_ms         = 0;
    hb_paused_until    = 0;
    push_next_ms       = 0;
    apply_station_keep();
    hb_adjusting       = false;
    auto_clear();
    last_tx_text[0] = '\0';
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
    for (size_t i = 0; i < POPUPS; i++) {
        if (!*popups[i].list) continue;
        lv_obj_del(*popups[i].list);
        *popups[i].list = NULL;
    }
    hb_adjusting = false;
    radio_set_pwr(param_f_get(cfg_pwr));

    rx_stop();
    wf_queue_clear();
    partials_end(); /* shown as ended when JS8 opens again */

    dsp_set_waterfall_enabled(true);
    dsp_set_spectrum_enabled(true);

    /* Your filter back, before the saved band and mode return. */
    if (filter_saved) {
        cparam_i_set(cfg_cur_filter_high, saved_filter_high);
        cparam_i_set(cfg_cur_filter_low, saved_filter_low);
        radio_set_tx_filter(param_i_get(cfg_tx_filter_low), param_i_get(cfg_tx_filter_high));
        radio_set_rx_dsp_off(false);
        filter_saved = false;
    }
    mem_load(MEM_BACKUP_ID);

    main_screen_lock_mode(false);
    main_screen_lock_ab(false);
    keypad_set_long_time(0);
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

/* Press: next filter. In the Stations view, where the filter means
 * nothing, the first press goes back to the messages as they were. */
static void show_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    if (view_stations) {
        view_stations = false;
        cursor_freq   = -1;
        if (btn_stations.disp_btn) buttons_refresh(&btn_stations);
        rebuild_rows();
        msg_update_text_fmt("Messages (%s)", show == SHOW_ALL ? "all" : show == SHOW_DIRECTED ? "directed" : "no HB");
        return;
    }
    show = (show + 1) % SHOW_COUNT;
    buttons_refresh(btn);
    rebuild_rows();
}

static void clear_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    hist_head = hist_count = 0;
    info_head = info_count = 0;
    js8_stations_clear(stations);
    js8_rx_clear(rx);
    lv_waterfall_clear_data(waterfall);
    wf_queue_clear();
    marks_reset();
    clear_selection();
    rebuild_rows();
    update_status();
}

/* Time Sync, as desktop JS8Call's time drift: JS8's time (receive windows,
 * transmit slots, everything the app times) moves to where the band's
 * decodes say it should be; the radio's clock is never changed. Each decode
 * suggests a drift worked out from the one in effect when its audio was
 * captured (js8core, desktop's auto-sync maths), so decodes finishing just
 * after a change can't make the next press overshoot, and every recent
 * decode stays usable: nothing is thrown away after a press. Each station
 * counts once. The drift lasts until the radio restarts; hold Time Sync to
 * go back to the radio's clock. Needs JS8's time within a couple of seconds
 * already, or nothing decodes: set the radio's clock roughly in SETTINGS
 * first. Not while sending: it would move the frames still to go. */
static void time_sync_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    int64_t  current = js8_drift_ms(), drift;
    unsigned decodes = 0, heard = 0;
    if (!js8_sync_drift(sync_samples, SYNC_SAMPLES, now_wall_ms(), SYNC_WINDOW_MS, current, &drift, &decodes, &heard)) {
        msg_update_text_fmt("Time Sync needs %d decodes in the last 2 min (have %u). "
                            "Clock far off? Set it in SETTINGS first",
                            JS8_SYNC_MIN_DECODES, decodes);
        return;
    }
    const char *from = heard >= JS8_SYNC_MIN_STATIONS ? "stations" : "decodes";
    unsigned    n    = heard >= JS8_SYNC_MIN_STATIONS ? heard : decodes;
    if (llabs(drift - current) < 50) {
        msg_update_text_fmt("JS8 time is on (within 0.05 s of %u %s)", n, from);
        return;
    }
    js8_set_drift_ms(drift);
    msg_update_text_fmt("JS8 time moved %+.2f s (median of %u %s); drift now %+.2f s", (drift - current) / 1000.0, n,
                        from, drift / 1000.0);
    add_info_row("Time Sync: JS8 time %+.2f s, drift %+.2f s", (drift - current) / 1000.0, drift / 1000.0);
    update_status();
}

/* Hold: no drift, JS8 back on the radio's clock (desktop's Reset). */
static void time_sync_hold_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    if (!js8_drift_ms()) {
        msg_update_text_fmt("No drift: JS8 is on the radio's clock");
        return;
    }
    js8_set_drift_ms(0);
    msg_update_text_fmt("Drift reset: JS8 back on the radio's clock");
    add_info_row("Time Sync: drift reset");
    update_status();
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
    int o = offer_find(call);
    if (o >= 0) {
        char text[JS8_RX_TEXT_LEN];
        snprintf(text, sizeof(text), "%s", offers[o].text);
        deliver_pending.id = offers[o].deliver_id;
        snprintf(deliver_pending.group_call, sizeof(deliver_pending.group_call), "%s", offers[o].deliver_group_call);
        snprintf(deliver_pending.text, sizeof(deliver_pending.text), "%s", offers[o].text);
        offers[o].call[0] = '\0';
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
    auto_cq_stop("TX stopped");
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
static bool send_cq(bool automatic) {
    char text[32];
    snprintf(text, sizeof(text), "CQ CQ CQ %.4s", params.qth.x);
    if (!tx_queue_at(text, params.js8_tx_freq.x, automatic)) return false;
    tx_cq = true;
    if (automatic) hb_pause("CQ"); /* by hand, tx_queue_at did it */
    return true;
}

static int64_t cq_interval_ms(void) {
    unsigned m = params.js8_cq_interval.x;
    if (m < CQ_MIN_INTERVAL || m > CQ_MAX_INTERVAL) m = CQ_MIN_INTERVAL;
    return (int64_t)m * 60000;
}

static const char *cq_label_getter(void) {
    static char buf[24];
    if (cq_adjusting) {
        snprintf(buf, sizeof(buf), "CQ: knob\n< %u min >", params.js8_cq_interval.x);
        return buf;
    }
    if (!auto_cq) return "CQ";
    if (js8_tx_busy(tx)) return "CQ auto:\nsending";
    int secs = (int)((auto_cq_next_ms - now_wall_ms() + 999) / 1000);
    if (secs < 1) return "CQ auto:\nnow";
    if (secs < 60) snprintf(buf, sizeof(buf), "CQ auto:\n%d s", secs);
    else snprintf(buf, sizeof(buf), "CQ auto:\n%d:%02d", secs / 60, secs % 60);
    return buf;
}

/* Setting the auto CQ interval with the main knob, as for HB: press CQ
 * (or wait) to finish. */
static void cq_adjust_start(void) {
    hb_adjusting = false;
    cq_adjusting = true;
    cq_adjust_ms = now_wall_ms();
    if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
    if (btn_cq.disp_btn) buttons_refresh(&btn_cq);
    update_tx_bar();
}

static void cq_adjust_end(void) {
    if (!cq_adjusting) return;
    cq_adjusting = false;
    if (btn_cq.disp_btn) buttons_refresh(&btn_cq);
    update_tx_bar();
    if (auto_cq) msg_update_text_fmt("Auto CQ %u min after each CQ (press CQ to stop)", params.js8_cq_interval.x);
}

/* Knob turn while setting it: the next CQ moves with it. */
static void cq_adjust_turn(int32_t diff) {
    int v = (int)params.js8_cq_interval.x + (diff > 0 ? 1 : -1);
    if (v < CQ_MIN_INTERVAL) v = CQ_MIN_INTERVAL;
    if (v > CQ_MAX_INTERVAL) v = CQ_MAX_INTERVAL;
    params_uint16_set(&params.js8_cq_interval, (uint16_t)v);
    cq_adjust_ms = now_wall_ms();
    if (auto_cq && !js8_tx_busy(tx)) auto_cq_next_ms = auto_cq_from_ms + cq_interval_ms();
    if (btn_cq.disp_btn) buttons_refresh(&btn_cq);
    update_tx_bar();
}

/* Press: one CQ; while setting the interval, done; with auto CQ on, back
 * to manual (auto off). */
static void cq_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    if (cq_adjusting) {
        cq_adjust_end();
        return;
    }
    if (auto_cq) {
        auto_cq_stop("manual");
        return;
    }
    send_cq(false);
}

/* Hold: auto CQ - one now, then one js8_cq_interval minutes after each
 * ends, until someone answers, you press CQ, reply to someone, stop TX or
 * change band. The knob sets the interval straight away; holding CQ while
 * auto CQ runs sets it again. */
static void cq_hold_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    if (auto_cq) {
        cq_adjust_start();
        return;
    }
    /* Busy sending: the first CQ comes an interval from now (not right
     * after; left so, your call). Otherwise one now, and the interval
     * starts when it ends (ui_tx_done). */
    int64_t now  = now_wall_ms();
    bool    busy = tx_active;
    if (!busy && !send_cq(false)) return;
    auto_cq         = true;
    auto_cq_from_ms = now;
    auto_cq_next_ms = busy ? now + cq_interval_ms() : now;
    buttons_refresh(btn);
    add_info_row("Auto CQ on");
    cq_adjust_start();
    msg_update_text_fmt("Auto CQ on: turn the knob to set the minutes, press CQ when done");
}

static void auto_cq_stop(const char *why) {
    if (!auto_cq) return;
    auto_cq      = false;
    cq_adjusting = false;
    if (btn_cq.disp_btn) buttons_refresh(&btn_cq);
    msg_update_text_fmt("Auto CQ off: %s", why);
    add_info_row("Auto CQ off: %s", why);
    update_tx_bar();
}

/* Once a second: the next CQ when it's due (and the button's countdown). */
static void auto_cq_tick(void) {
    if (cq_adjusting && now_wall_ms() - cq_adjust_ms > HB_ADJUST_MS) cq_adjust_end();
    if (!auto_cq) return;
    int64_t now = now_wall_ms();
    if (js8_auto_idle(autop, now)) {
        auto_cq_stop("idle for an hour");
        return;
    }
    if (btn_cq.disp_btn) buttons_refresh(&btn_cq);
    /* Lists don't hold it up, only the keyboard. Waiting replies went
     * first (hb_tick runs before this). */
    if (now < auto_cq_next_ms || tx_active || composing) return;
    if (send_cq(true)) {
        auto_cq_from_ms = now;
        auto_cq_next_ms = now + cq_interval_ms(); /* restarted when it ends */
    } else {
        auto_cq_stop("could not send");
    }
}

/* Where a heartbeat or HB ACK goes, as desktop: a free spot in the
 * 500-1000 Hz heartbeat sub-band, clear of anything decoded in the last
 * 30 s (call or not). A heartbeat stays on our own offset when that is
 * 1000 Hz or below. Our chat offset (the red band) doesn't move. */
static int free_hb_offset(bool heartbeat) {
    if (heartbeat && params.js8_tx_freq.x <= 1000) return params.js8_tx_freq.x;
    float   offsets[ACTIVITY];
    int64_t times[ACTIVITY];
    for (int i = 0; i < ACTIVITY; i++) {
        offsets[i] = activity[i].hz;
        times[i]   = activity[i].ms; /* unused entries: 0, long ago */
    }
    return js8_heartbeat_offset(offsets, times, ACTIVITY, now_wall_ms());
}

static bool send_heartbeat(bool automatic) {
    if (!js8_speed_heartbeats(cur_speed())) {
        if (!automatic) msg_update_text_fmt("No heartbeats in Turbo, as in desktop JS8Call");
        return false;
    }
    char text[48];
    js8_heartbeat_text(params.callsign.x, params.qth.x, text, sizeof(text));
    return tx_queue_at(text, free_hb_offset(true), automatic);
}

/* The first automatic heartbeat: an interval from now, on the slot grid
 * (desktop's TxLoop). */
static int64_t hb_first_ms(void) {
    return js8_next_heartbeat_ms(now_wall_ms(), params.js8_hb_interval.x, js8_speed_period_s(cur_speed()));
}

/* One now. With HB on, the automatic ones count again from this one, so
 * a manual heartbeat isn't followed by an automatic one straight after. */
static void heartbeat_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    (void)btn;
    if (!send_heartbeat(false) || !params.js8_hb.x) return;
    hb_next_ms = hb_first_ms();
    add_info_row("HB timer restarted: next in %u min", params.js8_hb_interval.x);
    update_status();
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
    cursor_freq   = -1;
    buttons_refresh(btn);
    /* The Stations view ends a lock: the station stays selected, and the
     * knob selects again there. */
    if (view_stations && sel_locked) {
        sel_locked = false;
        msg_update_text_fmt("%s unlocked", sel_call);
        update_tx_bar();
    }
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

/* Keys in any list popup: ESC does `esc` (closes it), MFK moves, and the
 * VOL knob (a keypad: its turns arrive as keys at the focus) sets the
 * volume, which only the Query list and the message list did (B-19). */
static void popup_key(lv_event_t *e, void (*esc)(void)) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));
    switch (key) {
    case LV_KEY_ESC:
        esc();
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

static void query_key_cb(lv_event_t *e) {
    popup_key(e, query_close);
}

/* Every list popup that's open. */
static void close_popups(void) {
    for (size_t i = 0; i < POPUPS; i++)
        if (*popups[i].list) popups[i].close();
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

/* The Query list's message items, as desktop's call menu has them: the
 * text started in the keyboard (NULL: sent at once) and what to type. */
static const struct {
    const char *label, *prefill, *hint;
} query_msg_items[] = {
    {"Message...", "%s MSG ", "Message for %s's inbox: type it and press Enter"},
    {"Message via them...", "%s MSG TO:", "Left at %s for someone: type their call, a space, the message"},
    {"Any messages?", NULL, NULL}, /* QUERY MSGS */
    {"Fetch message #...", "%s QUERY MSG ", "Type the number of the message %s holds for you"},
    {"Relay via them...", "%s>", "Passed on by %s: type the call it's for, a space, the message"},
    {"Can they reach...?", "%s QUERY CALL ", "Ask %s if they hear a station: type its call and ?"},
};
#define QUERY_MSG_ITEMS (int)(sizeof(query_msg_items) / sizeof(query_msg_items[0]))

static void query_msg_cb(lv_event_t *e) {
    int   which = (int)(intptr_t)lv_event_get_user_data(e);
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    bool  have = selected_station(call, sizeof(call), &freq, &snr);
    if (!query_msg_items[which].prefill || !have) {
        query_close();
        if (!have) return;
        apply_hold(freq);
        char text[JS8_RX_CALL_LEN + 16];
        snprintf(text, sizeof(text), "%s QUERY MSGS", call);
        tx_queue(text);
        return;
    }
    popup_leave(&query_list); /* into the keyboard */
    apply_hold(freq);
    char prefill[JS8_RX_CALL_LEN + 16];
    snprintf(prefill, sizeof(prefill), query_msg_items[which].prefill, call);
    if (popup_to_keyboard(NULL, 0, prefill)) msg_update_text_fmt(query_msg_items[which].hint, call);
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
    for (int i = 0; i < QUERY_MSG_ITEMS; i++) {
        lv_obj_t *b = list_add_item(query_list, query_msg_items[i].label);
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

/* For tools/js8_ui_harness: the decode marks drawn lately (frequency and
 * JS8_MARK_* level of each bracket), up to `max`. */
unsigned dialog_js8_marks(float *freq_hz, uint8_t *level, unsigned max) {
    unsigned n = 0;
    for (unsigned i = 0; i < MARK_MEMORY && n < max; i++) {
        if (!marks_drawn[i].used) continue;
        freq_hz[n] = marks_drawn[i].freq_hz;
        level[n]   = marks_drawn[i].level;
        n++;
    }
    return n;
}

/* For tools/js8_ui_harness: does message-list row `row` have the green bar? */
bool dialog_js8_row_marked(unsigned row) {
    return table && row < rows && row_is_selected_station(row_hist[row]);
}

/* ---- T4: auto-reply, heartbeats ---------------------------------------- */

/* Any key, button or knob: resets the idle watchdog. */
static void user_touch(void) {
    js8_auto_user_activity(autop, now_wall_ms());
}

/* Recently heard stations, most recent first: HEARING?, QUERY CALL and
 * RETRIEVE MSG look here. */
static unsigned heard_stations(js8_heard_t *out, unsigned max) {
    static js8_station_t list[MAX_ROWS];
    if (max > MAX_ROWS) max = MAX_ROWS;
    int n = js8_stations_recent(stations, now_wall_ms(), list, (int)max);
    for (int i = 0; i < n; i++) {
        out[i].call     = list[i].call;
        out[i].snr      = list[i].snr;
        out[i].heard_ms = list[i].heard_ms;
    }
    return (unsigned)n;
}

/* The switches as they are now, for deciding again at send time. */
static js8_auto_settings_t auto_settings(void) {
    js8_auto_settings_t st = {
        .autoreply = params.js8_auto.x,
        .heartbeat = params.js8_hb.x && !hb_paused(),
        .hb_ack    = params.js8_hb_ack.x && !hb_paused(),
        .relay     = params.js8_relay.x,
        .my_call   = params.callsign.x,
        .my_grid   = params.qth.x,
        .info      = info_text,
        .status    = status_text,
        .groups    = groups_text,
        .held      = held,
    };
    return st;
}

static void reply_drop(int i) {
    memmove(&replies[i], &replies[i + 1], (size_t)(n_replies - i - 1) * sizeof(replies[0]));
    n_replies--;
}

/* An automatic reply: in the queue, then out at the first chance. */
static void auto_queue(const js8_auto_result_t *r) {
    for (int i = 0; i < n_replies; i++)
        if (strcmp(replies[i].r.text, r->text) == 0) return; /* already waiting */
    if (n_replies == REPLIES) {
        add_info_row("Auto: too many replies waiting, not sent: %s", r->text);
        return;
    }
    replies[n_replies].r       = *r;
    replies[n_replies].ms      = now_wall_ms();
    replies[n_replies].cycle   = cycles;
    replies[n_replies].checked = false;
    n_replies++;
}

/* The oldest reply that may go now, decided again as things are now: the
 * switches, the idle watchdog and the @ALLCALL cooldown. Waits for our own
 * TX and the keyboard (not a list, your choice); while a message to us is
 * still arriving nothing goes, and while any is arriving no HB ACK. */
static void auto_try_send(void) {
    int64_t now = now_wall_ms();
    for (int i = 0; i < n_replies;) {
        if (now - replies[i].ms > REPLY_WAIT_MS) {
            reply_drop(i);
            continue;
        }
        /* Desktop answers once all of a cycle's decodes are in, and drops
         * the answer if a message to us is still arriving then (an HB ACK
         * if any message is): we can't hear its next frames while we send
         * (D3: as desktop). */
        bool ended = replies[i].cycle != cycles || now - replies[i].ms > CYCLE_WAIT_MS;
        if (!replies[i].checked && ended) {
            replies[i].checked = true;
            if (message_open(true) || (replies[i].r.hb_ack && message_open(false))) {
                add_info_row("Auto: %s not sent, a message is still arriving", replies[i].r.text);
                reply_drop(i);
                continue;
            }
        }
        i++;
    }
    if (!n_replies || tx_active || composing || message_open(true)) return;

    js8_auto_settings_t st = auto_settings();
    for (int i = 0; i < n_replies;) {
        js8_auto_result_t r = replies[i].r;
        if (!replies[i].checked || (r.hb_ack && message_open(false))) {
            i++; /* it waits; the others may go */
            continue;
        }
        reply_drop(i);
        if (js8_auto_decide(autop, &r, &st, now) != JS8_AUTO_SEND) continue; /* switched off meanwhile */
        if (r.hb_ack && !js8_speed_heartbeats(cur_speed())) continue;        /* desktop: no HB ACKs in Turbo */
        int offset = r.hb_ack ? free_hb_offset(false) : params.js8_tx_freq.x;
        LV_LOG_USER("JS8 auto: '%s' at %d Hz", r.text, offset);
        if (!tx_queue_at(r.text, offset, true)) continue;
        js8_auto_sent(autop, &r, now);
        if (r.deliver_id) deliver_start(r.deliver_id, r.deliver_group_call, r.text);
        if (r.kind == JS8_REPLY_RELAY) msg_update_text_fmt("Relaying %s's message", r.to);
        add_info_row("Auto: %s", r.text);
        return; /* one at a time: the next when this one ends */
    }
}

/* AUTO off: the answer offered to a station, if it's still fresh. */
static int offer_find(const char *call) {
    int64_t now = now_wall_ms();
    for (int i = 0; i < OFFERS; i++)
        if (offers[i].call[0] && strcmp(offers[i].call, call) == 0 && now - offers[i].ms < OFFER_MS) return i;
    return -1;
}

/* One per station: a newer answer to the same station replaces its old
 * one; with all taken, the oldest goes. */
static void offer_add(const js8_auto_result_t *r) {
    int slot = 0;
    for (int i = 0; i < OFFERS; i++) {
        if (offers[i].call[0] && strcmp(offers[i].call, r->to) == 0) {
            slot = i;
            break;
        }
        if (!offers[slot].call[0]) continue;
        if (!offers[i].call[0] || offers[i].ms < offers[slot].ms) slot = i;
    }
    snprintf(offers[slot].call, sizeof(offers[slot].call), "%s", r->to);
    snprintf(offers[slot].text, sizeof(offers[slot].text), "%s", r->text);
    offers[slot].ms         = now_wall_ms();
    offers[slot].deliver_id = r->deliver_id;
    snprintf(offers[slot].deliver_group_call, sizeof(offers[slot].deliver_group_call), "%s", r->deliver_group_call);
}

/* Everything that answered stations on this frequency: waiting replies,
 * offers, the offer taken into the keyboard, messages still arriving and
 * the band activity. */
static void auto_clear(void) {
    n_replies = 0;
    memset(offers, 0, sizeof(offers));
    deliver_pending.id = 0;
    memset(open_msgs, 0, sizeof(open_msgs));
    memset(activity, 0, sizeof(activity));
}

/* Heartbeats and HB ACKs pause while you're busy: anything you send by
 * hand except a heartbeat (Reply, Send..., Query, CQ ...) pauses them until
 * HB_PAUSE_MS after the last one; they resume by themselves (the user's
 * choice: nothing heard or sent automatically pauses them, and selecting
 * or unlocking a station doesn't resume them, unlike desktop). The
 * switches stay on. */
static bool hb_paused(void) {
    return hb_paused_until && now_wall_ms() < hb_paused_until;
}

static void hb_pause(const char *why) {
    if (!params.js8_hb.x && !params.js8_hb_ack.x) return;
    bool was = hb_paused();
    hb_paused_until = now_wall_ms() + HB_PAUSE_MS;
    if (hb_adjusting) hb_adjust_end();
    if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    if (!was) add_info_row("HB and HB ACK paused %d min: %s", HB_PAUSE_MS / 60000, why);
    update_status();
}

/* The pause ran out (or HB was pressed): heartbeats carry on. One that fell
 * due meanwhile goes out at the next chance. */
static void hb_resume(void) {
    hb_paused_until = 0;
    if (btn_hbauto.disp_btn) buttons_refresh(&btn_hbauto);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    update_status();
}

static void handle_incoming(const js8_rx_msg_t *m) {
    if (js8_starts_qso(m)) {
        char why[JS8_RX_CALL_LEN + 12];
        snprintf(why, sizeof(why), "%s answered", m->from);
        auto_cq_stop(why);
    }

    js8_heard_t heard[32];
    unsigned    n = heard_stations(heard, 32);

    js8_auto_settings_t st = auto_settings();
    js8_stored_t        kept;
    js8_auto_result_t   r;
    js8_process(autop, m, &st, heard, n, last_tx_text, now_wall_ms(), inbox, &kept, &r);
    stored_received(&kept);

    switch (r.action) {
    case JS8_AUTO_SEND:
        auto_queue(&r); /* goes when this decode cycle ends (auto_try_send) */
        break;
    case JS8_AUTO_OFFER:
        offer_add(&r);
        if (r.kind == JS8_REPLY_RELAY) {
            char dest[JS8_RX_CALL_LEN];
            snprintf(dest, sizeof(dest), "%.*s", (int)strcspn(r.text, ">"), r.text);
            msg_update_text_fmt("%s asks you to relay to %s - select it and press Reply to pass it on", r.to, dest);
            add_info_row("%s: Reply passes on \"%s\" (AUTO does it by itself)", r.to, r.text);
        } else if (strcmp(r.command, ">") == 0) {
            msg_update_text_fmt("Relayed message via %s - select it and press Reply to send ACK", r.to);
            add_info_row("%s: Reply sends \"%s\" (AUTO sends it by itself)", r.to, r.text);
        } else if (strcmp(r.command, "MSG") == 0 || strcmp(r.command, "MSG TO:") == 0) {
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

/* Desktop's stored-message notices (pushNotificationHandler): every 15
 * minutes, with AUTO on and nothing else going out, "W1ABC RETRIEVE MSG 3"
 * to a station we hold a message for, heard in the last 15 minutes and not
 * told in 8 hours. One per look. */
#define PUSH_INTERVAL_MS (15 * 60 * 1000)

static void push_tick(void) {
    int64_t now = now_wall_ms();
    if (!push_next_ms) push_next_ms = now + PUSH_INTERVAL_MS;
    if (now < push_next_ms) return;
    push_next_ms = now + PUSH_INTERVAL_MS;
    if (!params.js8_auto.x || !held || js8_auto_idle(autop, now) || !params.callsign.x[0]) return;
    if (tx_active || n_replies || composing) return;
    js8_heard_t heard[32];
    unsigned    n = heard_stations(heard, 32);
    char        text[64];
    if (!js8_held_push_due(held, heard, n, now, text, sizeof(text))) return;
    LV_LOG_USER("JS8 auto: '%s'", text);
    if (tx_queue_at(text, params.js8_tx_freq.x, true)) add_info_row("Auto: %s", text);
}

/* Messages kept (Settings): every 30 s, drop rows past their time, unless
 * you're scrolled up reading. */
static void msg_age_tick(void) {
    static int64_t next;
    int64_t        now = now_wall_ms(), keep = msg_keep_ms();
    if (now < next) return;
    next = now + 30000;
    if (!keep || view_stations || !table || !at_bottom()) return;
    for (uint16_t r = 0; r < rows; r++) {
        if (row_hist[r] < 0) continue;
        if (now - hist_ms[row_hist[r]] > keep) rebuild_rows();
        return; /* the oldest message row decides */
    }
}

/* Once a second: send a heartbeat when one is due. */
static void hb_tick(void) {
    beep_log_level();
    if (hb_adjusting && now_wall_ms() - hb_adjust_ms > HB_ADJUST_MS) hb_adjust_end();
    /* The transmitter is idle but ui_tx_done never came (its message was
     * lost): don't wait for it for ever. */
    if (tx_active && !js8_tx_busy(tx)) {
        if (!tx_quiet_ms) tx_quiet_ms = now_wall_ms();
        else if (now_wall_ms() - tx_quiet_ms > TX_DONE_LOST_MS) tx_active = false;
    } else {
        tx_quiet_ms = 0;
    }
    /* Replies that waited for the keyboard or a message still arriving
     * (after a transmission, ui_tx_done does the same). */
    auto_try_send();
    push_tick();
    msg_age_tick();
    if (hb_paused_until && !hb_paused()) hb_resume();
    if (!params.js8_hb.x) {
        hb_next_ms = 0;
        return;
    }
    int64_t now = now_wall_ms();
    if (hb_paused()) return;
    if (js8_auto_idle(autop, now) || !js8_speed_heartbeats(cur_speed())) return;
    /* As on desktop, the first one comes an interval after switching on;
     * page 1's Heartbeat sends one now. */
    if (hb_next_ms == 0) {
        hb_next_ms = hb_first_ms();
        update_status();
    }
    if (now < hb_next_ms - 5000) return;   /* desktop prepares it 5 s early */
    /* Lists don't hold it up, only the keyboard. Waiting replies went
     * first (auto_try_send above). */
    if (tx_active || composing || !params.callsign.x[0]) return;
    LV_LOG_USER("JS8 auto: heartbeat (due %lld)", (long long)hb_next_ms);
    if (send_heartbeat(true)) {
        /* On desktop's fixed schedule: an interval after the one due, not
         * after this one went (later if it waited). */
        hb_next_ms = js8_following_heartbeat_ms(hb_next_ms, now, params.js8_hb_interval.x);
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
    if (hb_paused() && !hb_adjusting) return "HB:\npaused";
    snprintf(buf, sizeof(buf), hb_adjusting ? "HB: knob\n< %u min >" : "HB:\n%u min", params.js8_hb_interval.x);
    return buf;
}

/* Off -> On, straight into setting the interval with the knob; press
 * again (or wait) to finish; press once more to turn heartbeats off. */
static void hb_adjust_start(button_data_t *btn) {
    if (cq_adjusting) cq_adjust_end();
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
    if (params.js8_hb.x && hb_paused()) {
        hb_resume();
        msg_update_text_fmt("Heartbeats resumed");
        return;
    }
    params_bool_set(&params.js8_hb, !params.js8_hb.x);
    hb_next_ms      = 0;
    hb_paused_until = 0;
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
    if (hb_paused() && params.js8_hb.x) return "HB ACK:\npaused";
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

/* js8_texts.txt couldn't be read or moved aside: never write over it. */
static bool texts_writable = true;

/* A data file was moved aside (or can't be written): say so on the screen
 * and in the log. */
static void data_file_notice(const char *notice) {
    if (!notice || !notice[0]) return;
    LV_LOG_USER("JS8: %s", notice);
    msg_update_text_fmt("%s", notice);
    add_info_row("%s", notice);
}

static void load_texts(void) {
    info_text[0] = status_text[0] = last_pota[0] = last_sota[0] = alert_words[0] = spot_note[0] = '\0';
    groups_text[0] = operator_call[0] = '\0';
    char buf[2048], notice[160];
    texts_writable = js8_file_read(JS8_TEXTS_PATH, buf, sizeof(buf), notice, sizeof(notice));
    data_file_notice(notice);
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        line[strcspn(line, "\r")] = '\0';
        if (strncmp(line, "INFO=", 5) == 0) snprintf(info_text, sizeof(info_text), "%s", line + 5);
        if (strncmp(line, "STATUS=", 7) == 0) snprintf(status_text, sizeof(status_text), "%s", line + 7);
        if (strncmp(line, "POTA=", 5) == 0) snprintf(last_pota, sizeof(last_pota), "%s", line + 5);
        if (strncmp(line, "SOTA=", 5) == 0) snprintf(last_sota, sizeof(last_sota), "%s", line + 5);
        if (strncmp(line, "ALERTS=", 7) == 0) js8_alert_words_normalise(line + 7, alert_words, sizeof(alert_words));
        if (strncmp(line, "GROUPS=", 7) == 0) js8_groups_normalise(line + 7, groups_text, sizeof(groups_text));
        if (strncmp(line, "OPERATOR=", 9) == 0 && !js8_operator_call_valid(line + 9, operator_call, sizeof(operator_call)))
            operator_call[0] = '\0';
        if (strncmp(line, "SPOTMODE=", 9) == 0 && line[9]) snprintf(spot_mode, sizeof(spot_mode), "%s", line + 9);
        if (strncmp(line, "SPOTHZ=", 7) == 0) spot_typed_hz = atoi(line + 7);
        if (strncmp(line, "SPOTTYPED=", 10) == 0) spot_use_typed = atoi(line + 10) != 0;
        if (strncmp(line, "SPOTNOTE=", 9) == 0) snprintf(spot_note, sizeof(spot_note), "%s", line + 9);
    }
    if (!spot_typed_hz) spot_use_typed = false;
}

/* Written safely (js8_file_write): a power cut during a save used to leave
 * the file empty, losing every setting in it. */
static void save_texts(void) {
    if (!texts_writable) {
        msg_update_text_fmt("Not saved: %s can't be read or moved (check the SD card)", JS8_TEXTS_PATH);
        return;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf),
             "INFO=%s\nSTATUS=%s\nPOTA=%s\nSOTA=%s\nALERTS=%s\n"
             "SPOTMODE=%s\nSPOTHZ=%d\nSPOTTYPED=%d\nSPOTNOTE=%s\n"
             "GROUPS=%s\nOPERATOR=%s\n",
             info_text, status_text, last_pota, last_sota, alert_words, spot_mode, (int)spot_typed_hz,
             spot_use_typed ? 1 : 0, spot_note, groups_text, operator_call);
    if (!js8_file_write(JS8_TEXTS_PATH, buf)) msg_update_text_fmt("Can't write %s", JS8_TEXTS_PATH);
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

/* The Settings list's items (user data): the texts are edit_t values. */
#define SETTINGS_RELAY    100
#define SETTINGS_ST_KEEP  101
#define SETTINGS_MSG_KEEP 102
#define SETTINGS_MILES    103
#define SETTINGS_MARKS    104

static const char *relay_label(void) {
    return params.js8_relay.x ? "Relay: On" : "Relay: Off";
}

/* The label of a line that changes in place. */
static const char *settings_label(int which) {
    static char buf[40];
    switch (which) {
    case SETTINGS_RELAY: return relay_label();
    case SETTINGS_ST_KEEP:
        snprintf(buf, sizeof(buf), "Stations kept: %s",
                 st_keep_opts[params.js8_st_keep.x < ST_KEEP_N ? params.js8_st_keep.x : 2].label);
        return buf;
    case SETTINGS_MSG_KEEP:
        snprintf(buf, sizeof(buf), "Messages kept: %s",
                 msg_keep_opts[params.js8_msg_keep.x < MSG_KEEP_N ? params.js8_msg_keep.x : 0].label);
        return buf;
    case SETTINGS_MILES: return params.js8_miles.x ? "Distance: miles" : "Distance: km";
    case SETTINGS_MARKS: return params.js8_decode_marks.x ? "Decode marks: On" : "Decode marks: Off";
    }
    return "";
}

static void texts_item_cb(lv_event_t *e) {
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which >= SETTINGS_RELAY) { /* switched in place; the list stays */
        switch (which) {
        case SETTINGS_RELAY:
            /* Desktop's "Disable message relay (>)": it stops holding MSG
             * TO: messages for others too. */
            params_bool_set(&params.js8_relay, !params.js8_relay.x);
            msg_update_text_fmt(params.js8_relay.x
                                    ? "Relay on: relays (>) are passed on and MSG TO: messages held, as desktop JS8Call"
                                    : "Relay off: relays (>) and MSG TO: messages for others are ignored");
            break;
        case SETTINGS_ST_KEEP:
            params_uint8_set(&params.js8_st_keep, (params.js8_st_keep.x + 1) % ST_KEEP_N);
            apply_station_keep();
            if (view_stations) rebuild_rows();
            msg_update_text_fmt("Stations stay listed %s after they were last heard",
                                st_keep_opts[params.js8_st_keep.x].min ? st_keep_opts[params.js8_st_keep.x].label
                                                                       : "until the radio is switched off, however long");
            break;
        case SETTINGS_MSG_KEEP:
            params_uint8_set(&params.js8_msg_keep, (params.js8_msg_keep.x + 1) % MSG_KEEP_N);
            if (!view_stations) rebuild_rows();
            if (params.js8_msg_keep.x)
                msg_update_text_fmt("Messages leave the list %s after they arrived",
                                    msg_keep_opts[params.js8_msg_keep.x].label);
            else msg_update_text_fmt("Messages stay in the list (the newest %d)", KEEP_ROWS);
            break;
        case SETTINGS_MILES:
            params_bool_set(&params.js8_miles, !params.js8_miles.x);
            if (view_stations) rebuild_rows();
            break;
        case SETTINGS_MARKS:
            params_bool_set(&params.js8_decode_marks, !params.js8_decode_marks.x);
            js8_rx_set_sync_marks(rx, params.js8_decode_marks.x);
            if (!params.js8_decode_marks.x) marks_reset();
            msg_update_text_fmt(params.js8_decode_marks.x
                                    ? "Decode marks on: where the decoder is trying (cyan, white) and what it decoded (yellow)"
                                    : "Decode marks off");
            break;
        }
        lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), settings_label(which));
        return;
    }
    /* Straight into the keyboard. */
    popup_to_keyboard(&texts_list, which,
                      which == EDIT_INFO     ? info_text
                      : which == EDIT_STATUS ? status_text
                      : which == EDIT_GROUPS ? groups_text
                                             : operator_call);
}

static void texts_key_cb(lv_event_t *e) {
    popup_key(e, texts_close);
}

static lv_obj_t *settings_add(const char *label, int which) {
    lv_obj_t *b = list_add_item(texts_list, label);
    lv_obj_add_event_cb(b, texts_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)which);
    lv_obj_add_event_cb(b, texts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

/* Settings: INFO and STATUS (what AUTO sends for INFO? and STATUS?), Relay
 * on/off, and the groups we're in, like desktop's settings. */
static void texts_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (texts_list) { /* Settings... again closes it */
        texts_close();
        return;
    }
    if (popup_guard()) return;
    if (query_list || aprs_list || composing) return;
    lv_group_remove_obj(table);
    texts_list = lv_list_create(dialog.obj);
    lv_obj_set_size(texts_list, 560, 290);
    lv_obj_align(texts_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(texts_list, &sony_24, 0);
    lv_obj_set_style_bg_color(texts_list, lv_color_hex(0x202020), 0);
    lv_obj_t *title = lv_list_add_text(texts_list, "Settings");
    lv_obj_set_style_text_font(title, &sony_22, 0);

    char label[TEXT_MAX + 16];
    snprintf(label, sizeof(label), "INFO: %s", info_text[0] ? info_text : "(not set)");
    lv_obj_t *first = settings_add(label, EDIT_INFO);
    snprintf(label, sizeof(label), "STATUS: %s", status_text[0] ? status_text : "(not set)");
    settings_add(label, EDIT_STATUS);
    settings_add(relay_label(), SETTINGS_RELAY);
    snprintf(label, sizeof(label), "Groups: %s", groups_text[0] ? groups_text : "(none)");
    settings_add(label, EDIT_GROUPS);
    settings_add(settings_label(SETTINGS_ST_KEEP), SETTINGS_ST_KEEP);
    settings_add(settings_label(SETTINGS_MSG_KEEP), SETTINGS_MSG_KEEP);
    settings_add(settings_label(SETTINGS_MILES), SETTINGS_MILES);
    settings_add(settings_label(SETTINGS_MARKS), SETTINGS_MARKS);
    snprintf(label, sizeof(label), "Operator: %s", operator_call[0] ? operator_call : "(the station call)");
    settings_add(label, EDIT_OPERATOR);

    lv_obj_t *close = list_add_item(texts_list, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_obj_add_event_cb(close, texts_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(close, texts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, close);
    lv_group_focus_obj(first);
    lv_group_set_editing(keyboard_group, false);
}

/* ---- APRS via @APRSIS -------------------------------------------------- */

/* JS8Call stations with "spot to APRS" on forward these to APRS-IS; formats
 * as desktop JS8Call and KF7MIX's JS8Spotter send them. Raw packets are
 * "@APRSIS CMD :<addressee padded to 9>:<text>"; APRS allows 67 characters
 * of text. The gateway sends you as your plain callsign (no SSID). */
#define APRS_CMD      "@APRSIS CMD :"
#define APRS_CMD_RAW  "@APRSIS CMD " /* any APRS packet body, e.g. a position */
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

    /* "! POTA PARK MHZ MODE ..." / "SUMMIT FREQ MODE ..." / old POTAGW "CALL PARK KHZ MODE ..." */
    char w1[24] = "", w2[24] = "", w3[24] = "";
    sscanf(body, "%23s %23s %23s", w1, w2, w3);
    if (!strcmp(to, "APSPOT") && !strcmp(w1, "!") && w3[0] && (!strcmp(w2, "POTA") || !strcmp(w2, "SOTA"))) {
        if (w2[0] == 'P') snprintf(last_pota, sizeof(last_pota), "%s", w3);
        else snprintf(last_sota, sizeof(last_sota), "%s", w3);
        save_texts();
    } else if (!strcmp(to, "POTAGW") && w2[0]) {
        snprintf(last_pota, sizeof(last_pota), "%s", w2);
        save_texts();
    } else if ((!strcmp(to, "APRS2SOTA") || !strcmp(to, "SOTA")) && w1[0]) {
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

/* Your QTH grid (6 characters) and its centre. False, with the reason
 * shown, if it isn't set. Any out-pointer may be NULL. */
static bool aprs_grid(char grid[8], double *lat, double *lon) {
    char g[8];
    snprintf(g, sizeof(g), "%.6s", params.qth.x);
    if (strlen(g) < 4) {
        msg_update_text_fmt("Set your grid first: APP > QTH");
        return false;
    }
    if (grid) memcpy(grid, g, sizeof(g));
    if (lat && lon) qth_str_to_pos(g, lat, lon);
    return true;
}

/* From the firmware's gps.c (gpsd): the latest fix and its age. */
bool gps_last_fix(double *lat, double *lon, int *age_s);

/* A current GPS fix, and its 10-character grid (about 20 x 35 m). False,
 * with the reason shown, if there is none. */
static bool aprs_gps(char grid[12], double *lat_out, double *lon_out) {
    double lat, lon;
    int    age;
    if (!gps_last_fix(&lat, &lon, &age)) {
        msg_update_text_fmt("No GPS fix: plug in a GPS and wait for a fix (APP > GPS shows it)");
        return false;
    }
    if (age > 120) {
        msg_update_text_fmt("No current GPS fix (last one %d min ago)", age / 60);
        return false;
    }
    char g[12];
    if (!js8_latlon_to_grid(lat, lon, 10, g, sizeof(g))) {
        msg_update_text_fmt("GPS position out of range");
        return false;
    }
    if (grid) memcpy(grid, g, sizeof(g));
    if (lat_out) *lat_out = lat;
    if (lon_out) *lon_out = lon;
    return true;
}

/* An APRS position report: "=4916.9 N/12307.2 WG MESSAGE". To about
 * 185 m: minutes to one decimal, the last digit a space (APRS position
 * ambiguity), which JS8 sends in far fewer bits than digits; the space
 * before the message lets JS8's word compression take it whole (6 frames
 * down to 5 for "MADE IT TO CAMP"). "/G" is the grid-square symbol, the
 * one JS8Call's own grid spots use. */
static void aprs_position(double lat, double lon, const char *comment, char *out, size_t size) {
    double alat = fabs(lat), alon = fabs(lon);
    int    lat_d = (int)alat, lon_d = (int)alon;
    double lat_m = (alat - lat_d) * 60.0, lon_m = (alon - lon_d) * 60.0;
    if (lat_m >= 59.95) lat_d++, lat_m = 0; /* don't print 60.0 */
    if (lon_m >= 59.95) lon_d++, lon_m = 0;
    snprintf(out, size, "=%02d%04.1f %c/%03d%04.1f %cG %s", lat_d, lat_m, lat < 0 ? 'S' : 'N', lon_d, lon_m,
             lon < 0 ? 'W' : 'E', comment);
}

/* What Spot my grid / GPS position sends. No message: "@APRSIS GRID
 * <grid>" as desktop JS8Call sends it (the gateway adds frequency and
 * SNR). With one: the GRID command can't carry it, so a position report
 * with the message as its comment goes through CMD (the gateway passes it
 * on as it is). False, with the reason shown, without a position. */
static bool beacon_text(bool gps, const char *message, char *out, size_t size, char grid[12]) {
    double lat, lon;
    if (gps ? !aprs_gps(grid, &lat, &lon) : !aprs_grid(grid, &lat, &lon)) return false;
    while (*message == ' ') message++;
    if (!*message) {
        snprintf(out, size, "@APRSIS GRID %s", grid);
        return true;
    }
    char pos[80];
    aprs_position(lat, lon, message, pos, sizeof(pos));
    snprintf(out, size, APRS_CMD_RAW "%s", pos);
    return true;
}

static void aprs_beacon(bool gps, const char *message) {
    char text[112], grid[12];
    if (!beacon_text(gps, message, text, sizeof(text), grid)) return;
    while (*message == ' ') message++;
    if (!tx_queue(text)) return;
    if (!*message)
        add_info_row("APRS: spotting %s at %s%s", params.callsign.x, grid, gps ? " (GPS)" : "");
    else
        add_info_row("APRS: position %s (%s%s) with \"%s\"", params.callsign.x, grid, gps ? ", GPS" : "", message);
}

/* While typing a beacon message: how long it will be on the air. */
static void beacon_changed_cb(lv_event_t *e) {
    (void)e;
    char text[112], grid[12];
    if (!beacon_text(edit_target == EDIT_BEACON_GPS, textarea_window_get(), text, sizeof(text), grid)) return;
    js8_tx_preview_t pv;
    js8_tx_preview(params.callsign.x, params.qth.x, text, cur_speed(), &pv);
    if (!pv.ok) return;
    const char *msg = textarea_window_get();
    while (*msg == ' ') msg++;
    if (*msg) msg_update_text_fmt("Position + message: %d frames, %.0f s", pv.frames, pv.seconds);
    else msg_update_text_fmt("Plain position: %d frames, %.0f s - type a message, or Enter", pv.frames, pv.seconds);
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
    popup_leave(&aprs_list);

    switch (item) {
    case APRS_GRID:
    case APRS_GPS:
        /* A message to go with it, or just Enter. GPS: a fix first, so
         * nothing is typed for nothing. */
        if (item == APRS_GRID ? !aprs_grid(NULL, NULL, NULL) : !aprs_gps(NULL, NULL, NULL)) break;
        if (popup_to_keyboard(NULL, item == APRS_GRID ? EDIT_BEACON_GRID : EDIT_BEACON_GPS, NULL))
            beacon_changed_cb(NULL); /* the plain beacon's length, until you type */
        break;
    case APRS_POTA:
    case APRS_SOTA:
        spot_show(item == APRS_SOTA, (item == APRS_SOTA ? last_sota : last_pota)[0] ? SP_SEND : SP_REF);
        return;
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
    popup_key(e, aprs_close);
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

/* ---- POTA / SOTA spot form (docs/SPOTS_PLAN.md) ------------------------ */

/* POTA goes to APSPOT, which posts to pota.app (needs a pota.app account);
 * SOTA to APRS2SOTA (needs registering with sotaspots.co.uk). Both take
 * MHz. The frequency is the JS8 dial, or one you typed (your SSB run),
 * each remembered with the mode and comment. Gateway replies come back
 * over APRS only: the radio never sees them. */

static const char *const spot_modes_pota[] = {"DATA", "SSB", "CW", "FM", "AM", "FT8"}; /* APSPOT's */
static const char *const spot_modes_sota[] = {"DATA", "SSB", "CW", "FM", "AM", "DV"};  /* APRS2SOTA's */
#define SPOT_MODES 6

static lv_obj_t *spot_items[SP_COUNT];
static lv_obj_t *spot_preview;

static const char *const *spot_modes(void) {
    return spot_sota ? spot_modes_sota : spot_modes_pota;
}

/* The mode to send: the remembered one if this gateway takes it. */
static const char *spot_mode_now(void) {
    for (int i = 0; i < SPOT_MODES; i++)
        if (!strcmp(spot_modes()[i], spot_mode)) return spot_modes()[i];
    return "DATA";
}

static int32_t spot_hz(void) {
    return spot_use_typed && spot_typed_hz ? spot_typed_hz : cparam_i_get(cfg_fg_freq);
}

/* MHz with at least 3 decimals, more only when needed: "7.078", "7.1855". */
static void format_mhz(int32_t hz, char *buf, size_t size) {
    snprintf(buf, size, "%d.%06d", (int)(hz / 1000000), (int)(hz % 1000000));
    char *end = buf + strlen(buf) - 1;
    while (*end == '0' && end[-3] != '.') *end-- = '\0';
}

/* No comment typed: "JS8" while spotting the JS8 dial. */
static const char *spot_comment(void) {
    if (spot_note[0]) return spot_note;
    return spot_use_typed && spot_typed_hz ? "" : "JS8";
}

/* The APRS text after "@APRSIS CMD :<addressee>:". */
static void spot_body(char *out, size_t size) {
    char mhz[16];
    format_mhz(spot_hz(), mhz, sizeof(mhz));
    const char *note = spot_comment();
    if (spot_sota)
        snprintf(out, size, "%s %s %s %s%s%s", last_sota[0] ? last_sota : "?", mhz, spot_mode_now(), params.callsign.x,
                 note[0] ? " " : "", note);
    else
        snprintf(out, size, "! POTA %s %s %s%s%s", last_pota[0] ? last_pota : "?", mhz, spot_mode_now(), note[0] ? " " : "",
                 note);
}

static bool spot_freq_empty(const char *text) {
    while (*text == ' ') text++;
    return !*text;
}

/* kHz ("7185.5"), or MHz below 1000 ("144.2"); 1.8 MHz to 1.3 GHz. False,
 * with the reason shown, keeps the keyboard open to fix it. */
static bool spot_parse_freq(const char *text, int32_t *out) {
    char  *end;
    double v = strtod(text, &end);
    if (end == text || v <= 0) {
        msg_update_text_fmt("Type the frequency in kHz, e.g. 7185");
        return false;
    }
    double hz = v < 1000 ? v * 1e6 : v * 1e3;
    if (hz < 1800000 || hz > 1300000000) {
        msg_update_text_fmt("Out of range: 1800 kHz - 1300 MHz");
        return false;
    }
    if (out) *out = (int32_t)(hz + 0.5);
    return true;
}

static void spot_close(void) {
    if (!spot_list) return;
    lv_obj_del_async(spot_list); /* often called from one of its buttons */
    spot_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void spot_label(spot_item_t item, char *line, size_t size) {
    char mhz[16];
    switch (item) {
    case SP_SEND:
        snprintf(line, size, "Send spot");
        break;
    case SP_REF:
        if (spot_sota) snprintf(line, size, "Summit: %s", last_sota[0] ? last_sota : "(type it)");
        else snprintf(line, size, "Park: %s", last_pota[0] ? last_pota : "(type it)");
        break;
    case SP_FREQ:
        format_mhz(spot_hz(), mhz, sizeof(mhz));
        snprintf(line, size, "Frequency: %s MHz (%s)", mhz, spot_use_typed && spot_typed_hz ? "typed" : "JS8 dial");
        break;
    case SP_MODE:
        snprintf(line, size, "Mode: %s", spot_mode_now());
        break;
    case SP_NOTE:
        if (spot_note[0]) snprintf(line, size, "Comment: %s", spot_note);
        else if (spot_comment()[0]) snprintf(line, size, "Comment: %s (auto)", spot_comment());
        else snprintf(line, size, "Comment: (none)");
        break;
    case SP_CLOSE:
        snprintf(line, size, "Close");
        break;
    default:
        line[0] = '\0';
    }
}

/* Labels and the message preview, in place. */
static void spot_refresh(void) {
    char line[96];
    for (int i = 0; i < SP_COUNT; i++) {
        if (!spot_items[i]) continue;
        spot_label((spot_item_t)i, line, sizeof(line));
        lv_label_set_text(lv_obj_get_child(spot_items[i], 0), line);
    }
    spot_body(line, sizeof(line));
    lv_label_set_text_fmt(spot_preview, "%s: %s", spot_sota ? "APRS2SOTA" : "APSPOT", line);
}

static void spot_send(void) {
    if (!(spot_sota ? last_sota : last_pota)[0]) {
        msg_update_text_fmt("Type the %s first", spot_sota ? "summit" : "park");
        return;
    }
    char body[96], text[TX_TEXT_MAX + 8], out[TX_TEXT_MAX + 8];
    spot_body(body, sizeof(body));
    snprintf(text, sizeof(text), APRS_CMD "%-9s:%s", spot_sota ? "APRS2SOTA" : "APSPOT", body);
    if (!aprs_prepare(text, out, sizeof(out))) return; /* too long: says so */
    if (!tx_queue(out)) return;
    add_info_row("%s spot: %s. A JS8Call APRS gateway must hear it; the reply goes to APRS, not here",
                 spot_sota ? "SOTA" : "POTA", body);
    spot_close();
}

static void spot_item_cb(lv_event_t *e) {
    spot_item_t item = (spot_item_t)(intptr_t)lv_event_get_user_data(e);
    int         edit = 0;
    switch (item) {
    case SP_SEND:
        spot_send();
        return;
    case SP_CLOSE:
        spot_close();
        return;
    case SP_MODE: { /* next mode, in place */
        const char *cur = spot_mode_now();
        int         i   = 0;
        while (i < SPOT_MODES && strcmp(spot_modes()[i], cur) != 0) i++;
        snprintf(spot_mode, sizeof(spot_mode), "%s", spot_modes()[(i + 1) % SPOT_MODES]);
        save_texts();
        spot_refresh();
        return;
    }
    case SP_FREQ: /* the keyboard, your last one filled in; empty = the JS8 dial */
        edit = EDIT_SPOT_FREQ;
        break;
    case SP_REF:
        edit = EDIT_SPOT_REF;
        break;
    case SP_NOTE:
        edit = EDIT_SPOT_NOTE;
        break;
    default:
        return;
    }
    /* A field: into the keyboard, then back here (see log_item_cb). */
    char khz[16] = ""; /* the last one typed, even while spotting the dial */
    if (spot_typed_hz) format_khz(spot_typed_hz, khz, sizeof(khz));
    popup_to_keyboard(&spot_list, edit,
                      edit == EDIT_SPOT_REF ? (spot_sota ? last_sota : last_pota) : edit == EDIT_SPOT_FREQ ? khz : spot_note);
    if (edit == EDIT_SPOT_REF)
        msg_update_text_fmt(spot_sota ? "Summit, e.g. VE7/LM-001" : "Park, e.g. CA-1234 (POTA uses US- and CA- now, not K- or VE-)");
    else if (edit == EDIT_SPOT_FREQ)
        msg_update_text_fmt("Frequency in kHz (7185) or MHz for VHF (144.2); clear it and Enter for the JS8 dial");
    else
        msg_update_text_fmt("Comment (optional; with none, a JS8 dial spot says JS8)");
}

static void spot_key_cb(lv_event_t *e) {
    popup_key(e, spot_close);
}

/* `focus`: the item to start on - Send, or the field just edited. */
static void spot_show(bool sota, spot_item_t focus) {
    spot_sota = sota;
    lv_group_remove_obj(table);
    spot_list = lv_list_create(dialog.obj);
    lv_obj_set_size(spot_list, 560, WF_HEIGHT - 10);
    lv_obj_align(spot_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(spot_list, &sony_24, 0);
    lv_obj_set_style_bg_color(spot_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(spot_list, lv_color_white(), 0);
    lv_obj_t *t = lv_list_add_text(spot_list, sota ? "SOTA spot via APRS2SOTA (registration needed)"
                                                   : "POTA spot via APSPOT (your pota.app account)");
    lv_obj_set_style_text_font(t, &sony_22, 0);
    spot_preview = lv_list_add_text(spot_list, "");
    lv_obj_set_style_text_font(spot_preview, &sony_22, 0);
    lv_obj_set_style_text_color(spot_preview, lv_color_hex(0x1030a0), 0); /* on the list's light title bar */

    char line[96];
    for (int i = 0; i < SP_COUNT; i++) {
        spot_label((spot_item_t)i, line, sizeof(line));
        lv_obj_t *b = list_add_item(spot_list, line);
        lv_obj_add_event_cb(b, spot_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, spot_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
        spot_items[i] = b;
    }
    lv_obj_set_style_text_color(spot_items[SP_SEND], lv_color_hex(0x80ff80), 0);
    lv_obj_set_style_text_color(spot_items[SP_CLOSE], lv_color_hex(0xffc040), 0);
    spot_refresh();
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(spot_items[focus]);
    const char *ref = sota ? last_sota : last_pota;
    if (!sota && (!strncmp(ref, "K-", 2) || !strncmp(ref, "VE-", 3)))
        msg_update_text_fmt("POTA park numbers are US-/CA- now (was K-/VE-): check yours");
}

/* ---- Log QSO ----------------------------------------------------------- */

/* Like desktop JS8Call's Log QSO: the entry is filled in from the QSO and
 * goes to JS8_LOG_PATH (ADIF, on the SD card's DATA partition) and to the
 * radio's QSO database, which marks worked stations. A two-way QSO that
 * ends with 73 or SK opens it by itself (Log prompt), and nothing is
 * logged without Save. */

static bool any_popup(void) {
    for (size_t i = 0; i < POPUPS; i++)
        if (*popups[i].list) return true;
    return false;
}

static bool find_station(const char *call, js8_station_t *out) {
    return js8_stations_find(stations, call, now_wall_ms(), out);
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
    snprintf(log_entry.op_call, sizeof(log_entry.op_call), "%s", operator_call);
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
    worked_forget();

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
    popup_to_keyboard(&log_list, item == LOG_GRID ? EDIT_LOG_GRID : item == LOG_NAME ? EDIT_LOG_NAME : EDIT_LOG_NOTE,
                      item == LOG_GRID ? log_entry.grid : item == LOG_NAME ? log_entry.name : log_entry.comment);
}

/* ESC on the prompt for a QSO that ended means "not now": Log QSO then
 * takes the selected station again (B-24, your choice). */
static void log_esc(void) {
    if (log_pending[0] && strcmp(log_entry.call, log_pending) == 0) log_pending[0] = '\0';
    log_close();
}

static void log_key_cb(lv_event_t *e) {
    popup_key(e, log_esc);
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
    if (log_entry.op_call[0]) { /* Settings: operator, not the station call */
        snprintf(line, sizeof(line), "Operator %s (station %s)", log_entry.op_call, log_entry.my_call);
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
        case EDIT_LOG_GRID: { /* checked in compose_ok_cb; in capitals, as logs have them */
            char *g = value + strspn(value, " ");
            size_t n = strcspn(g, " ");
            g[n]     = '\0';
            for (char *c = g; *c; c++) *c = (char)toupper((unsigned char)*c);
            snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", g);
            log_grid_typed = true;
            break;
        }
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
        case EDIT_FREQ:
            tune_custom(value);
            return;
        case EDIT_SPOT_REF:
            if (spot_sota) snprintf(last_sota, sizeof(last_sota), "%s", value);
            else snprintf(last_pota, sizeof(last_pota), "%s", value);
            save_texts();
            if (btn_act.disp_btn) buttons_refresh(&btn_act);
            break;
        case EDIT_SPOT_FREQ: /* a frequency: spot it (remembered); empty: the JS8 dial */
            if (spot_freq_empty(value)) spot_use_typed = false;
            else spot_use_typed = spot_parse_freq(value, &spot_typed_hz);
            save_texts();
            break;
        case EDIT_SPOT_NOTE:
            snprintf(spot_note, sizeof(spot_note), "%s", value);
            save_texts();
            break;
        case EDIT_BEACON_GRID:
        case EDIT_BEACON_GPS:
            aprs_beacon(target == EDIT_BEACON_GPS, value);
            return;
        }
    }
    if (target >= EDIT_BEACON_GRID) { /* cancelled: nothing sent */
        msg_update_text_fmt("Position not sent");
        return;
    }
    if (target == EDIT_FREQ) return; /* cancelled */
    if (target >= EDIT_SPOT_REF) { /* back on the field just edited */
        spot_show(spot_sota, target == EDIT_SPOT_REF ? SP_REF : target == EDIT_SPOT_FREQ ? SP_FREQ : SP_NOTE);
        return;
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
    /* A QSO that just ended, unless you've selected another station since
     * (your choice): the ended one is still there when you select it. It's
     * forgotten with the QSO (30 min), or once logged. */
    char      call[JS8_RX_CALL_LEN];
    float     freq;
    int       snr;
    js8_qso_t q;
    bool      selected = selected_station(call, sizeof(call), &freq, &snr);
    if (log_pending[0] && !js8_qsos_get(qsos, log_pending, now_wall_ms(), &q)) log_pending[0] = '\0';
    if (log_pending[0] && (!selected || strcmp(call, log_pending) == 0)) {
        snprintf(call, sizeof(call), "%s", log_pending);
    } else if (!selected) {
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
    popup_to_keyboard(NULL, v == 1 ? EDIT_POTA_REF : EDIT_SOTA_REF, v == 1 ? last_pota : last_sota);
}

/* ---- Inbox and messages -------------------------------------------------- */

/* Like desktop JS8Call: "CALL MSG text" to us (checksum good) goes to the
 * inbox, answered with "CALL ACK" (sent by AUTO, else offered on Reply).
 * The Query list sends messages: MSG for their inbox, MSG TO: to leave one
 * at their station for someone else, QUERY MSGS to ask what they hold. */

#define INBOX_ROWS 200 /* all of them (Inbox::MAX_MESSAGES): an older unread one couldn't be opened (bug hunt 9) */
#define HELD_ROWS  100 /* HeldMessages::MAX_MESSAGES */

static int inbox_view_id; /* the message shown, 0: the list */

/* Green, like a selected Settings tab, while there are unread messages. */
static void inbox_refresh_button(void) {
    buttons_mark(&btn_inbox, inbox && js8_inbox_unread(inbox) > 0);
}

static const char *inbox_label_getter(void) {
    static char label[24];
    int         unread = js8_inbox_unread(inbox);
    if (unread) snprintf(label, sizeof(label), "Inbox\n%d new", unread);
    else snprintf(label, sizeof(label), "Inbox");
    return label;
}

/* What js8_process() kept: a MSG for us (or our group) in the inbox, or a
 * "MSG TO:W1ABC ..." held here until W1ABC asks (QUERY MSGS, QUERY MSG n,
 * our HB ack's "MSG ID n", or our RETRIEVE MSG), as desktop does. */
static void stored_received(const js8_stored_t *k) {
    char from[JS8_PATH_LEN + 16];
    js8_path_display(k->path[0] ? k->path : k->from, from, sizeof(from));
    if (k->kind == JS8_STORED_INBOX) {
        bool group = k->to[0] == '@';
        if (k->id < 0) msg_update_text_fmt("Message from %s: can't save %s, so no ACK sent", from, JS8_INBOX_PATH);
        else if (!k->resend && group) msg_update_text_fmt("New message to %s from %s - Inbox on page 3", k->to, from);
        else if (!k->resend) msg_update_text_fmt("New message from %s - Inbox on page 3", from);
        if (!k->resend) add_info_row("Message from %s in the Inbox: %s", from, k->text);
        if (!k->resend && (params.js8_alerts.x & JS8_ALERT_INBOX)) alert_beep(2);
        inbox_refresh_button();
        update_status();
    } else if (k->kind == JS8_STORED_HELD && !k->resend) {
        if (k->id < 0) {
            msg_update_text_fmt("Message from %s for %s: can't save %s, so no ACK sent", from, k->to, JS8_HELD_PATH);
            return;
        }
        msg_update_text_fmt("Holding a message from %s for %s (Inbox)", from, k->to);
        add_info_row("Holding message %d from %s for %s: %s", k->id, from, k->to, k->text);
    }
}

/* Same message as the transmitter reports it: it sends upper case, trimmed. */
static bool same_tx_text(const char *a, const char *b) {
    while (*a == ' ') a++;
    while (*b == ' ') b++;
    size_t la = strlen(a), lb = strlen(b);
    while (la && a[la - 1] == ' ') la--;
    while (lb && b[lb - 1] == ' ') lb--;
    return la == lb && strncasecmp(a, b, la) == 0;
}

/* A held message's delivery was queued: it counts once it has all gone.
 * A few can be on their way; with all taken, the oldest is forgotten. */
static void deliver_start(int id, const char *group_call, const char *text) {
    int slot = 0;
    while (slot < DELIVERIES - 1 && deliveries[slot].id) slot++;
    if (deliveries[slot].id) { /* all taken */
        memmove(&deliveries[0], &deliveries[1], (DELIVERIES - 1) * sizeof(deliveries[0]));
        slot = DELIVERIES - 1;
    }
    deliveries[slot].id = id;
    snprintf(deliveries[slot].group_call, sizeof(deliveries[slot].group_call), "%s", group_call ? group_call : "");
    snprintf(deliveries[slot].text, sizeof(deliveries[slot].text), "%s", text);
}

/* A message ended. If it was a delivery, sent in full it's delivered;
 * stopped halfway, it stays held, and their next QUERY MSGS or heartbeat
 * is offered it again. */
static void deliver_end(const char *text, bool completed) {
    for (int i = 0; i < DELIVERIES; i++) {
        if (!deliveries[i].id || !same_tx_text(text, deliveries[i].text)) continue;
        int id = deliveries[i].id;
        if (completed) {
            if (deliveries[i].group_call[0]) js8_held_group_delivered(held, id, deliveries[i].group_call);
            else js8_held_delivered(held, id);
            add_info_row("Held message %d delivered", id);
        } else {
            add_info_row("Held message %d stopped before the end: still held", id);
        }
        deliveries[i].id = 0;
        return;
    }
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
    popup_leave(&inbox_list);
}

/* "CALL MSG " / "CALL MSG TO:" in the keyboard; kind is "MSG " or "MSG TO:".
 * `call` may be a relay path ("K2XYZ>N0XYZ"). */
static void msg_compose(const char *call, const char *kind) {
    char prefill[JS8_PATH_LEN + 12];
    snprintf(prefill, sizeof(prefill), "%s %s", call, kind);
    if (!popup_to_keyboard(NULL, 0, prefill)) return;
    if (strcmp(kind, "MSG TO:") == 0)
        msg_update_text_fmt("Left at %s for someone: type their call, a space, the message", call);
    else msg_update_text_fmt("Message for %s's inbox: type it and press Enter", call);
}

static void inbox_show(int id);

/* ESC in a message: back to the list; in the list: close. */
static void inbox_esc(void) {
    if (inbox_view_id) {
        inbox_leave();
        inbox_show(0);
    } else {
        inbox_close();
    }
}

static void inbox_key_cb(lv_event_t *e) {
    popup_key(e, inbox_esc);
}

typedef enum {
    INBOX_NEW        = -1,
    INBOX_CLOSE      = -2,
    INBOX_BACK       = -3,
    INBOX_REPLY      = -4, /* desktop's "Reply to <path>": PATH MSG ... */
    INBOX_DELETE     = -5,
    INBOX_REPLY_VIA  = -6, /* "Reply to <sender> via <path>": PATH MSG TO:SENDER ... */
    INBOX_REPLY_ORIG = -7, /* "Reply to <sender>": SENDER MSG ... */
    INBOX_FETCH_NEXT = -8, /* PATH QUERY MSG n, for its NEXT MSG ID n */
    INBOX_REPLY_APRS = -9, /* from an APRS gateway: an APRS message to its DE sender */
} inbox_action_t;

/* The sender of a message an APRS gateway passed on: "... DE N0CALL". */
static bool aprs_sender(const char *text, char *call, size_t size) {
    const char *de = NULL;
    for (const char *p = strstr(text, " DE "); p; p = strstr(p + 1, " DE ")) de = p;
    if (!de) return false;
    de += 4;
    while (*de == ' ') de++;
    size_t n = strcspn(de, " ");
    if (!n || de[n + strspn(de + n, " ")] != '\0' || n >= size) return false;
    snprintf(call, size, "%.*s", (int)n, de);
    return true;
}

/* A text from a phone, passed on by an SMS gateway (sender SMS, SMSGTE):
 * "@6045551234 TEXT DE SMS" starts with the number (or the alias) it came
 * from, which the gateway needs at the front of the reply. */
static bool aprs_sms_from(const char *text, const char *sender, char *out, size_t size) {
    if (strncmp(sender, "SMS", 3) != 0) return false;
    while (*text == ' ') text++;
    size_t n = strcspn(text, " ");
    if (text[0] != '@' || n < 2 || n >= size) return false;
    for (size_t i = 1; i < n; i++) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
    }
    snprintf(out, size, "%.*s", (int)n, text);
    return true;
}
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
    case INBOX_REPLY:
    case INBOX_REPLY_VIA:
    case INBOX_REPLY_ORIG:
    case INBOX_FETCH_NEXT:
    case INBOX_REPLY_APRS: {
        js8_inbox_msg_t m;
        if (!js8_inbox_get(inbox, viewed, &m)) return;
        const char *path = m.path[0] ? m.path : m.from;
        char        orig[JS8_RX_CALL_LEN] = "";
        int         next                  = 0;
        js8_delivered_signature(m.text, orig, sizeof(orig), &next);
        char aprs_to[JS8_RX_CALL_LEN];
        if (action == INBOX_REPLY_APRS && !aprs_sender(m.text, aprs_to, sizeof(aprs_to))) return;
        if (action == INBOX_FETCH_NEXT) { /* sent at once: back to the messages */
            char text[JS8_PATH_LEN + 24];
            snprintf(text, sizeof(text), "%s QUERY MSG %d", path, next);
            inbox_close();
            tx_queue(text);
            return;
        }
        inbox_leave(); /* into the keyboard */
        if (action == INBOX_REPLY) {
            msg_compose(path, "MSG ");
        } else if (action == INBOX_REPLY_ORIG) {
            msg_compose(orig, "MSG ");
        } else if (action == INBOX_REPLY_VIA) {
            char prefill[JS8_PATH_LEN + JS8_RX_CALL_LEN + 12];
            snprintf(prefill, sizeof(prefill), "%s MSG TO:%s ", path, orig);
            if (popup_to_keyboard(NULL, 0, prefill))
                msg_update_text_fmt("Left at %s for %s: type the message and press Enter", path, orig);
        } else {
            /* An SMS: the number filled in, the cursor after it. */
            char head[64], sms[24];
            bool to_sms = aprs_sms_from(m.text, aprs_to, sms, sizeof(sms));
            snprintf(head, sizeof(head), "%s%-9.9s:%s%s", APRS_CMD, aprs_to, to_sms ? sms : "", to_sms ? " " : "");
            aprs_compose(head, "");
            lv_group_set_editing(keyboard_group, true);
            if (to_sms) msg_update_text_fmt("SMS to %s: type your message and press Enter", sms + 1);
            else msg_update_text_fmt("APRS message to %s: type it and press Enter", aprs_to);
        }
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
        } else if (popup_to_keyboard(NULL, 0, NULL)) {
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
        char when[32], from[JS8_PATH_LEN + 16];
        utc_label(m.utc_ms, when, sizeof(when), "%d %b %H:%M UTC");
        js8_path_display(m.path, from, sizeof(from));
        snprintf(line, sizeof(line), "For %s from %s  %s", m.to, from, when);
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        lv_obj_t *body = lv_list_add_text(inbox_list, m.text);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_color(body, lv_color_white(), 0);
        lv_obj_set_style_pad_ver(body, 8, 0);
        if (m.group)
            snprintf(line, sizeof(line), "For any %s member to fetch for 2 days; fetched by %d", m.to, m.got);
        else snprintf(line, sizeof(line), m.delivered ? "Delivered to %s" : "Waiting for %s to ask (QUERY MSGS)", m.to);
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
        char when[32], from[JS8_PATH_LEN + 16];
        utc_label(m.utc_ms, when, sizeof(when), "%d %b %H:%M UTC");
        js8_path_display(m.path[0] ? m.path : m.from, from, sizeof(from));
        if (m.to[0] == '@') snprintf(line, sizeof(line), "From %s to %s  %s", from, m.to, when);
        else snprintf(line, sizeof(line), "From %s  %s", from, when);
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        lv_obj_t *body = lv_list_add_text(inbox_list, m.text);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_color(body, lv_color_white(), 0);
        lv_obj_set_style_pad_ver(body, 8, 0);
        /* Desktop's Reply choices: to the path it came by; and for a
         * delivered message ("... FROM N0XYZ"), to its sender through the
         * station that held it (MSG TO:) or straight. */
        const char *path = m.path[0] ? m.path : m.from;
        char        orig[JS8_RX_CALL_LEN] = "", aprs_from[JS8_RX_CALL_LEN];
        int         next                  = 0;
        bool        signed_ = js8_delivered_signature(m.text, orig, sizeof(orig), &next) && orig[0] != '@';
        if (strcmp(path, "APRS") == 0) {
            if (aprs_sender(m.text, aprs_from, sizeof(aprs_from))) {
                char sms[24];
                if (aprs_sms_from(m.text, aprs_from, sms, sizeof(sms)))
                    snprintf(line, sizeof(line), "Reply by SMS to %s", sms);
                else snprintf(line, sizeof(line), "Reply by APRS to %s", aprs_from);
                first = inbox_add(line, INBOX_REPLY_APRS);
            }
        } else {
            snprintf(line, sizeof(line), "Reply: MSG to %s", from);
            first = inbox_add(line, INBOX_REPLY);
            if (signed_ && strcmp(orig, path) != 0) {
                snprintf(line, sizeof(line), "Reply to %s via %s (MSG TO:)", orig, from);
                inbox_add(line, INBOX_REPLY_VIA);
                snprintf(line, sizeof(line), "Reply to %s", orig);
                inbox_add(line, INBOX_REPLY_ORIG);
            }
            if (next > 0) {
                snprintf(line, sizeof(line), "Fetch the next: QUERY MSG %d", next);
                inbox_add(line, INBOX_FETCH_NEXT);
            }
        }
        lv_obj_t *del = inbox_add("Delete", INBOX_DELETE);
        if (!first) first = del;
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

        lv_obj_t *newest_new = NULL;
        for (int i = 0; i < n; i++) {
            char when[16];
            utc_label(rows[i].utc_ms, when, sizeof(when), "%d %b %H:%M");
            char from[JS8_PATH_LEN + 16];
            js8_path_display(rows[i].path[0] ? rows[i].path : rows[i].from, from, sizeof(from));
            snprintf(line, sizeof(line), "%s%s  %s  %s", rows[i].read ? "" : "* ", when, from, rows[i].text);
            lv_obj_t *b = inbox_add(line, rows[i].id);
            if (!rows[i].read) {
                lv_obj_set_style_text_color(b, lv_color_hex(0xffe080), 0);
                if (!newest_new) newest_new = b; /* newest first */
            }
        }
        if (n == 0) {
            t = lv_list_add_text(inbox_list, "No messages yet");
            lv_obj_set_style_text_font(t, &sony_22, 0);
        }
        /* Messages held here for others (MSG TO:), until they ask. */
        static js8_held_msg_t held_rows[HELD_ROWS];
        int                   nh = js8_held_list(held, held_rows, HELD_ROWS);
        if (nh > 0) {
            snprintf(line, sizeof(line), "Held for others: %d waiting", js8_held_waiting(held));
            t = lv_list_add_text(inbox_list, line);
            lv_obj_set_style_text_font(t, &sony_22, 0);
        }
        for (int i = 0; i < nh; i++) {
            char from[JS8_PATH_LEN + 16], got[16] = "";
            js8_path_display(held_rows[i].path, from, sizeof(from));
            if (held_rows[i].group) snprintf(got, sizeof(got), " (%d got it)", held_rows[i].got);
            snprintf(line, sizeof(line), "%sfor %s%s from %s  %s", held_rows[i].delivered ? "(sent) " : "",
                     held_rows[i].to, got, from, held_rows[i].text);
            lv_obj_t *b = inbox_add(line, INBOX_HELD + held_rows[i].id);
            if (held_rows[i].delivered) lv_obj_set_style_text_color(b, lv_color_hex(0x909090), 0);
        }
        lv_obj_t *close = inbox_add("Close", INBOX_CLOSE);
        lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
        if (newest_new) first = newest_new; /* straight to the newest unread (your choice) */
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
 * transmitting: the speaker path is the TX audio path then.
 *
 * The base unit plays this app's audio on the speaker only in its play
 * mode (radio_speaker_play, as voice prompts and the recorder use it);
 * without it the beep went nowhere. In play mode the audio the base sends
 * us isn't the receiver's, so the beep is kept short and beep_guard feeds
 * the decoder silence meanwhile. */

#define BEEP_HZ       1000
#define BEEP_MS       120
#define BEEP_GAP_MS   100
#define BEEP_LEAD_MS  50   /* silence first: the base switches to play mode */
#define BEEP_TAIL_MS  300  /* guard after speaker-off: capture latency */
#define BEEP_AMPL     13000 /* about -8 dBFS */
#define BEEP_EVERY_MS 3000 /* at most one alert sound per 3 s */

static int64_t beep_last_ms;

enum {
    BEEP_TONE = AUDIO_PLAY_RATE * BEEP_MS / 1000,
    BEEP_GAP  = AUDIO_PLAY_RATE * BEEP_GAP_MS / 1000,
    BEEP_LEAD = AUDIO_PLAY_RATE * BEEP_LEAD_MS / 1000,
};
static int16_t         beep_buf[BEEP_TONE + BEEP_GAP];
static int16_t         beep_lead[BEEP_LEAD];
static int             beep_count;
static atomic_bool     beeping;

/* Beep thread: audio_play() waits for room in the stream, so it can't run
 * on the LVGL thread, and it only ever gets small parts (like tx_player):
 * one call with the whole beep never finds room and hangs forever. */
/* audio_play() in parts of at most 2048 samples: a bigger write never finds
 * room in the stream and hangs. Stops early if TX keys. */
static void beep_play(const int16_t *buf, size_t n) {
    for (size_t at = 0; at < n && !atomic_load(&keyed);) {
        size_t part = LV_MIN(2048, n - at);
        audio_play((int16_t *)buf + at, part);
        at += part;
    }
}

static void *beep_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&speaker_lock);
    if (atomic_load(&keyed)) goto out;
    /* The receiver's level just before, for the log. */
    atomic_store(&beep_guard_sq, 0);
    atomic_store(&beep_guard_n, 0);
    atomic_store(&beep_guard, true);
    radio_speaker_play(true);
    beep_play(beep_lead, BEEP_LEAD);
    for (int i = 0; i < beep_count; i++) beep_play(beep_buf, BEEP_TONE + BEEP_GAP);
    audio_play_wait();
    radio_speaker_play(false);
    atomic_store(&beep_guard_end, now_mono_ms() + BEEP_TAIL_MS);
    atomic_store(&beep_guard, false);
out:
    pthread_mutex_unlock(&speaker_lock);
    atomic_store(&beeping, false);
    return NULL;
}

/* Once the guard is over: what the capture carried while the speaker
 * played, against the receiver just before (tells whether play mode
 * really cuts the receiver on this base). */
static void beep_log_level(void) {
    int64_t n = atomic_load(&beep_guard_n);
    if (!n || beep_guard_active()) return;
    double rms = sqrt((double)atomic_load(&beep_guard_sq) / 1e9 / (double)n);
    LV_LOG_USER("JS8 beep: capture %.1f dBFS during the beep (%lld samples), %.1f dBFS before",
                20.0 * log10(rms + 1e-12), (long long)n, beep_level_before_db);
    atomic_store(&beep_guard_n, 0);
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
            beep_buf[i] = (int16_t)(BEEP_AMPL * env * sinf(2.0f * (float)M_PI * BEEP_HZ * i / AUDIO_PLAY_RATE));
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
    else if ((a & JS8_ALERT_NEW) && new_station && m->from[0] && !worked_before(m->from)) {
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
        if (popup_to_keyboard(&alerts_list, EDIT_ALERT_WORDS, alert_words[0] ? alert_words : NULL))
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
    popup_key(e, alerts_close);
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

/* The selected station's speed (the one Reply and the green bar are for,
 * not the row under the cursor: with a station locked they differ, bug
 * hunt 15); else the cursor row's (a message without a call). */
static bool selected_speed(js8_speed_t *out) {
    js8_station_t st;
    if (sel_call[0] && find_station(sel_call, &st)) {
        *out = js8_speed_from_submode(st.submode);
        return true;
    }
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
    /* The offset must leave room for the wider signal below 3000 Hz. */
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

/* ---- Frequency: JS8Call's, GhostNet's, or your own ---------------------- */

/* The band keys step through JS8Call's usual dial frequencies, or with
 * GhostNet on through GhostNet's (3.575, 7.107, 14.107 MHz). A custom
 * frequency is remembered; the band keys leave it for the preset list. */

typedef enum {
    FQ_JS8,
    FQ_GHOSTNET,
    FQ_CUSTOM,
    FQ_CLOSE,
} freq_item_t;

static const char *freq_label_getter(void) {
    static char buf[32];
    if (params.js8_custom_on.x) {
        char khz[16];
        format_khz(params.js8_custom_hz.x, khz, sizeof(khz));
        snprintf(buf, sizeof(buf), "Freq:\n%s", khz);
        return buf;
    }
    return params.js8_ghostnet.x ? "Freq:\nGhostNet" : "Freq:\nJS8";
}

static void freq_close(void) {
    if (!freq_list) return;
    lv_obj_del_async(freq_list); /* often called from one of its buttons */
    freq_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* To the closest preset of the chosen list. */
static void use_presets(bool ghostnet) {
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    params_bool_set(&params.js8_ghostnet, ghostnet);
    params_bool_set(&params.js8_custom_on, false);
    load_band(0);
    retuned();
}

/* From the keyboard: kHz ("7107.5"), or MHz when under 100 ("7.1075").
 * False, with the reason shown, keeps the keyboard open to fix it. */
static bool parse_custom(const char *text, int32_t *out) {
    char  *end;
    double v = strtod(text, &end);
    if (end == text || v <= 0) {
        msg_update_text_fmt("Type the dial frequency in kHz, e.g. 7107");
        return false;
    }
    int32_t hz = (int32_t)(v < 100 ? v * 1e6 + 0.5 : v * 1e3 + 0.5);
    if (hz < CUSTOM_MIN_HZ || hz > CUSTOM_MAX_HZ) {
        msg_update_text_fmt("Out of range: 1800 - 54000 kHz");
        return false;
    }
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return false;
    }
    if (out) *out = hz;
    return true;
}

static void tune_custom(const char *text) {
    int32_t hz;
    if (!parse_custom(text, &hz)) return;
    params_int32_set(&params.js8_custom_hz, hz);
    params_bool_set(&params.js8_custom_on, true);
    cparam_i_set(cfg_fg_freq, hz);
    js8_usb_dig();
    retuned();
    msg_update_text_fmt("%s", where_label());
}

static void freq_item_cb(lv_event_t *e) {
    freq_item_t item = (freq_item_t)(intptr_t)lv_event_get_user_data(e);
    switch (item) {
    case FQ_JS8:
    case FQ_GHOSTNET:
        freq_close();
        use_presets(item == FQ_GHOSTNET);
        return;
    case FQ_CUSTOM: {
        /* Into the keyboard with the last one filled in. */
        char khz[16] = "";
        if (params.js8_custom_hz.x) format_khz(params.js8_custom_hz.x, khz, sizeof(khz));
        if (popup_to_keyboard(&freq_list, EDIT_FREQ, khz)) msg_update_text_fmt("Dial frequency in kHz, then Enter");
        return;
    }
    case FQ_CLOSE:
        freq_close();
        return;
    }
}

static void freq_key_cb(lv_event_t *e) {
    popup_key(e, freq_close);
}

static lv_obj_t *freq_add(freq_item_t item, const char *label) {
    lv_obj_t *b = list_add_item(freq_list, label);
    lv_obj_add_event_cb(b, freq_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    lv_obj_add_event_cb(b, freq_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

static void freq_show(void) {
    lv_group_remove_obj(table);
    freq_list = lv_list_create(dialog.obj);
    lv_obj_set_size(freq_list, 560, WF_HEIGHT - 10);
    lv_obj_align(freq_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(freq_list, &sony_24, 0);
    lv_obj_set_style_bg_color(freq_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(freq_list, lv_color_white(), 0);
    char line[64];
    snprintf(line, sizeof(line), "Now: %s", where_label());
    lv_obj_t *t = lv_list_add_text(freq_list, line);
    lv_obj_set_style_text_font(t, &sony_22, 0);

    bool      custom = params.js8_custom_on.x, ghost = params.js8_ghostnet.x;
    lv_obj_t *js8    = freq_add(FQ_JS8, "JS8Call frequencies (7.078, 14.078...)");
    lv_obj_t *gn     = freq_add(FQ_GHOSTNET, "GhostNet (3.575, 7.107, 14.107)");
    char      khz[16];
    if (params.js8_custom_hz.x) {
        format_khz(params.js8_custom_hz.x, khz, sizeof(khz));
        snprintf(line, sizeof(line), "Custom kHz... (last %s)", khz);
    } else {
        snprintf(line, sizeof(line), "Custom kHz...");
    }
    lv_obj_t *cu    = freq_add(FQ_CUSTOM, line);
    lv_obj_t *close = freq_add(FQ_CLOSE, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    /* The one in use stands out and has the knob. */
    lv_obj_t *cur = custom ? cu : ghost ? gn : js8;
    lv_obj_set_style_text_color(cur, lv_color_hex(0x60ff60), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(cur);
}

static void freq_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (freq_list) { /* Freq again closes it */
        freq_close();
        return;
    }
    if (popup_guard()) return;
    if (composing) return;
    freq_show();
}
