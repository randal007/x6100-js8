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
#include "util.h"
#include "widgets/lv_finder.h"
#include "widgets/lv_waterfall.h"

#include <liquid/liquid.h>

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SAMPLE_RATE      (AUDIO_CAPTURE_RATE / AUDIO_DECIM)
#define WIDTH            771
#define WF_HEIGHT        325
#define WF_VISIBLE       55     /* waterfall rows left uncovered by the list */
#define JS8_WIDTH_HZ     50     /* 8 tones x 6.25 Hz, JS8 Normal */
#define JS8_SLOT_SEC     15.0f
#define PSD_INTERVAL_MS  200
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

/* ---- State (UI thread unless noted) ------------------------------------ */

static js8_rx_t *rx;

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
static uint64_t last_psd_ms;

/* ---- Buttons ---------------------------------------------------------- */

static buttons_page_t page_1;
static buttons_page_t page_2;

static button_data_t btn_p1    = {.type = BTN_TEXT, .label = "(JS8 1:2)", .press = button_next_page_cb, .next = &page_2};
static button_data_t btn_show  = {.type = BTN_TEXT_FN, .label_fn = show_label_getter, .press = show_cb};
static button_data_t btn_clear = {.type = BTN_TEXT, .label = "Clear", .press = clear_cb};

static button_data_t btn_p2        = {.type = BTN_TEXT, .label = "(JS8 2:2)", .press = button_next_page_cb, .next = &page_1};
static button_data_t btn_time_sync = {.type = BTN_TEXT, .label = "Time\nSync", .press = time_sync_cb};
static button_data_t btn_test_wav  = {.type = BTN_TEXT_FN, .label_fn = test_wav_label_getter, .press = test_wav_cb};

static buttons_page_t page_1 = {{&btn_p1, &btn_show, &btn_clear}};
static buttons_page_t page_2 = {{&btn_p2, &btn_time_sync, &btn_test_wav}};

static dialog_t dialog = {
    .run          = false,
    .construct_cb = construct_cb,
    .destruct_cb  = destruct_cb,
    .audio_cb     = audio_cb,
    .key_cb       = key_cb,
    .btn_page     = &page_1,
};

dialog_t *dialog_js8 = &dialog;

/* ---- Message list ----------------------------------------------------- */

static bool passes_filter(const js8_rx_msg_t *m) {
    switch (show) {
    case SHOW_ALL:      return true;
    case SHOW_NO_HB:    return !m->heartbeat;
    case SHOW_DIRECTED: return m->to_me || (m->to_group && !m->heartbeat && !m->cq);
    default:            return true;
    }
}

static void format_row(const js8_rx_msg_t *m, char *buf, size_t size) {
    int hh = m->utc / 10000, mm = (m->utc / 100) % 100, ss = m->utc % 100;

    snprintf(buf, size, "%02d:%02d:%02d %+3d %4.0f  %s%s%s%s",
             hh, mm, ss, m->snr, m->freq_hz,
             m->low_confidence ? "[" : "",
             m->text,
             m->low_confidence ? "]" : "",
             m->checksum < 0 ? "  (bad checksum)" : "");
}

/* Keep following new rows if the selection is on the last one. */
static bool at_bottom(void) {
    uint16_t sel_row = 0, sel_col = 0;
    lv_table_get_selected_cell(table, &sel_row, &sel_col);
    return rows == 0 || sel_row == LV_TABLE_CELL_NONE || sel_row + 1 >= rows;
}

static void follow(void) {
    static uint32_t key = LV_KEY_DOWN;
    lv_event_send(table, LV_EVENT_KEY, &key);
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

    bool scroll = at_bottom();
    if (rows >= MAX_ROWS) return;
    append_row(buf, -1);
    if (scroll) follow();
}

/* Rebuild the list from history, newest KEEP_ROWS matching messages. */
static void rebuild_rows(void) {
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

static void add_message(const js8_rx_msg_t *m) {
    int slot = hist_head;
    history[slot] = *m;
    hist_head     = (hist_head + 1) % HISTORY;
    if (hist_count < HISTORY) hist_count++;

    /* Rows pointing at the slot we just overwrote would now show the wrong
     * message; the ring is larger than MAX_ROWS so this only happens after a
     * long session, and rebuilding fixes it. */
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
    } else {
        const js8_rx_msg_t *m = &history[h];
        if (m->to_me) {
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

static void table_press_cb(lv_event_t *e) {
    (void)e;
    uint16_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row >= rows || row_hist[row] < 0) {
        lv_finder_set_value(finder, filter_low - 1000); /* off-screen */
        lv_obj_invalidate(finder);
        return;
    }
    const js8_rx_msg_t *m = &history[row_hist[row]];
    lv_finder_set_value(finder, (int16_t)(m->freq_hz + 0.5f));
    lv_obj_invalidate(finder);
    if (m->from[0]) {
        msg_update_text_fmt("%s at %.0f Hz, %+d dB", m->from, m->freq_hz, m->snr);
    }
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
    lv_label_set_text_fmt(status, "%s%s  %02d:%02d:%02dZ  last %u  total %u", testing ? "TEST WAV  " : "",
                          cfg_digital_label_get(), tm.tm_hour, tm.tm_min, tm.tm_sec, cycle_decodes, hist_count);

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

static void ui_waterfall_add(void *arg) {
    wf_data_t *d = (wf_data_t *)arg;
    if (dialog.run && waterfall) lv_waterfall_add_data(waterfall, d->psd, d->size);
    free(d->psd);
}

static void on_audio(const float *samples, unsigned n, void *ctx) {
    (void)ctx;
    if (!sg) return;

    spgramf_write(sg, (float *)samples, n);

    uint64_t now = get_time();
    if (now - last_psd_ms < PSD_INTERVAL_MS) return;
    last_psd_ms = now;

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
    scheduler_put(ui_waterfall_add, &d, sizeof(d));
}

/* ---- Receiver lifecycle ----------------------------------------------- */

static void rx_start(void) {
    int span = filter_high - filter_low;
    nfft     = (uint16_t)(span > 0 ? WIDTH * SAMPLE_RATE / span : 4096);
    sg       = spgramf_create(nfft, LIQUID_WINDOW_HANN, nfft, nfft / 2);
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
    js8_rx_feed(rx, samples, n);
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
    lv_waterfall_clear_data(waterfall);
    lv_finder_set_value(finder, filter_low - 1000);
    add_info_row("%s", cfg_digital_label_get());
    update_status();
}

static void key_cb(lv_event_t *e) {
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    switch (key) {
    case LV_KEY_ESC:
        dialog_destruct();
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
    lv_waterfall_set_min(waterfall, -27);
    lv_obj_set_pos(waterfall, 13, 13);

    /* Finder marks the offset of the selected message. */

    finder = lv_finder_create(waterfall);
    lv_finder_set_range(finder, filter_low, filter_high);
    lv_finder_set_width(finder, JS8_WIDTH_HZ);
    lv_finder_set_value(finder, filter_low - 1000);
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

    /* Message list */

    table = lv_table_create(dialog.obj);
    lv_obj_remove_style(table, NULL, LV_STATE_ANY | LV_PART_MAIN);
    lv_obj_add_event_cb(table, table_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(table, key_cb, LV_EVENT_KEY, NULL);
    lv_obj_add_event_cb(table, table_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
    lv_obj_set_size(table, WIDTH, WF_HEIGHT - WF_VISIBLE);
    lv_obj_set_pos(table, 13, 13 + WF_VISIBLE);
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
}

static void destruct_cb(void) {
    rx_stop();

    dsp_set_waterfall_enabled(true);
    dsp_set_spectrum_enabled(true);

    mem_load(MEM_BACKUP_ID);

    main_screen_lock_mode(false);
    main_screen_lock_ab(false);
    main_screen_lock_freq(false);
    main_screen_lock_band(false);

    /* LVGL objects are children of dialog.obj, deleted by dialog_destruct()
     * right after this returns. */
    waterfall = finder = table = status = NULL;
}

/* ---- Buttons ---------------------------------------------------------- */

static const char *show_label_getter(void) {
    static const char *const labels[SHOW_COUNT] = {"Show:\nAll", "Show:\nNo HB", "Show:\nDirected"};
    return labels[show];
}

static void show_cb(button_data_t *btn) {
    show = (show + 1) % SHOW_COUNT;
    buttons_refresh(btn);
    rebuild_rows();
}

static void clear_cb(button_data_t *btn) {
    (void)btn;
    hist_head = hist_count = 0;
    js8_rx_clear(rx);
    lv_waterfall_clear_data(waterfall);
    lv_finder_set_value(finder, filter_low - 1000);
    rebuild_rows();
    update_status();
}

/* Snap the system clock to the nearest 15 s boundary, for use when you know
 * a transmission just started. Same approach as the FT8 app. */
static void time_sync_cb(button_data_t *btn) {
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
