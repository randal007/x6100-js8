/* Stand-ins for radio hardware and main-screen plumbing, so the real
 * dialog_js8.c / dialog.c / LVGL / src/js8 can run headless on a PC. The
 * settings are R1CBU 1.0's real ones (src/cfg, linked; driver.c opens a
 * fresh params.db): the dial, mode, filter and frequency lists included. */

#include "audio.h"
#include "buttons.h"
#include "cfg/cfg_api.h"
#include "cfg/digital_modes.h"
#include "dsp.h"
#include "gps.h"
#include "keyboard.h"
#include "main_screen.h"
#include "pubsub_ids.h"
#include "radio.h"
#include "lv_drivers/display/drm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

lv_group_t     *keyboard_group;
lv_event_code_t EVENT_BAND_UP;
lv_event_code_t EVENT_BAND_DOWN;

int stub_mode(void) { return cparam_i_get(cfg.cur.mode()); }
int stub_dial_hz(void) { return cparam_i_get(cfg.cur.fg_freq()); }

/* Receive audio: the app subscribes (dsp_audio_subscribe_float); the
 * harness feeds it through dialog_audio_samples(), as dsp.cpp would. */
static audio_float_cb_t audio_sub_cb;
static uint32_t         audio_sub_rate;
static bool             audio_sub_on;
uint32_t dsp_audio_subscribe_float(audio_float_cb_t cb, uint32_t target_rate_hz) {
    audio_sub_cb   = cb;
    audio_sub_rate = target_rate_hz;
    printf("[dsp] audio subscription at %u Hz\n", target_rate_hz);
    return 1;
}
void dsp_audio_set_active(uint32_t id, bool active) {
    (void)id;
    audio_sub_on = active;
    printf("[dsp] audio %s\n", active ? "on" : "off");
}
uint32_t stub_audio_rate(void) { return audio_sub_rate; }
int      stub_play_rate(void) { return AUDIO_PLAY_RATE; }
void     dialog_audio_samples(unsigned int n, float *samples) {
    if (audio_sub_cb && audio_sub_on) audio_sub_cb(n, samples);
}

/* Buttons: remember the loaded page so the driver can press them, and the
 * text each button was last drawn with (as the real ones keep it until
 * refreshed). */
buttons_page_t *stub_page;
char            stub_shown[BUTTONS][48];
static void     stub_draw(int i) {
    button_data_t *b = stub_page ? stub_page->items[i] : NULL;
    snprintf(stub_shown[i], sizeof(stub_shown[i]), "%s",
             !b ? "" : b->type == BTN_TEXT_FN ? b->label_fn() : b->label ? b->label : "");
}
void buttons_load_page(buttons_page_t *page) {
    stub_page = page;
    for (int i = 0; i < BUTTONS; i++) stub_draw(i);
}
void            buttons_unload_page() { stub_page = NULL; }
buttons_page_t *buttons_get_cur_page() { return stub_page; }
void            buttons_refresh(button_data_t *d) {
    if (d->type == BTN_TEXT_FN) printf("[button] %s\n", d->label_fn());
    for (int i = 0; stub_page && i < BUTTONS; i++)
        if (stub_page->items[i] == d) stub_draw(i);
}
/* The real one tints the button green (btn_active_style); record it. */
void buttons_mark(button_data_t *d, bool val) {
    if (d->mark != val && d->type == BTN_TEXT_FN) printf("[button] %s: %s\n", d->label_fn(), val ? "green" : "plain");
    d->mark = val;
}
void button_next_page_cb(button_data_t *d) { buttons_load_page(d->next); }
void button_prev_page_cb(button_data_t *d) { buttons_load_page(d->prev); }

void knobs_display(bool v) { (void)v; }
void waterfall_refresh_period_set(uint8_t k) { (void)k; }
void waterfall_refresh_reset() {}
void main_screen_keys_enable(bool v) { printf("[main] keys %s\n", v ? "enabled" : "disabled"); }
void mem_save(uint16_t id) { printf("[mem] save %u\n", id); }
void mem_load(uint16_t id) { printf("[mem] load %u\n", id); }
void waterfall_set_enabled(bool v) { printf("[main] waterfall %s\n", v ? "on" : "off"); }
void spectrum_set_enabled(bool v) { (void)v; }

/* R1CBU 1.0's lower display plane (lv_drivers/display/drm.c): the panel is
 * portrait, 480 x 800, so screen (x, y) is plane (y, 799 - x). JS8 draws its
 * waterfall there; LVGL draws the app on a see-through plane above it, and
 * the harness's screenshots put the two together as the display does. Each
 * put lands at once (the radio applies it at the next page flip). */
uint32_t      harness_plane[480 * 800];
unsigned long harness_plane_puts, harness_plane_px;
void        (*harness_plane_cb)(const lv_area_t *a);
static lv_color_t plane_queue[800 * 480 * 2]; /* drm.c's MAX_DIRTY_BUF */
bool drm_primary_begin_direct(drm_direct_ctx_t *ctx, uint32_t pixels_needed) {
    if (pixels_needed > sizeof(plane_queue) / sizeof(plane_queue[0])) return false;
    ctx->buf        = plane_queue;
    ctx->max_pixels = sizeof(plane_queue) / sizeof(plane_queue[0]);
    return true;
}
void drm_primary_end_direct(const lv_area_t *a) {
    int w = a->x2 - a->x1 + 1;
    if (a->x1 < 0 || a->y1 < 0 || a->x2 >= 480 || a->y2 >= 800) {
        printf("[plane] FAIL: area %d,%d-%d,%d off the plane\n", a->x1, a->y1, a->x2, a->y2);
        return;
    }
    for (int y = a->y1; y <= a->y2; y++)
        memcpy(&harness_plane[y * 480 + a->x1], &plane_queue[(size_t)(y - a->y1) * w], (size_t)w * 4);
    harness_plane_puts++;
    harness_plane_px += (unsigned long)w * (a->y2 - a->y1 + 1);
    if (harness_plane_cb) harness_plane_cb(a);
}
int      stub_vol_turns; /* radio_change_vol() calls */
uint16_t radio_change_vol(int16_t d) { stub_vol_turns++; printf("[radio] vol %+d\n", d); return 0; }

int  stub_new_station_alerts; /* "New station: ..." shown */
char stub_last_msg[512];      /* the message line's last text */
static void vmsg(const char *tag, const char *fmt, va_list ap) {
    char text[512];
    vsnprintf(text, sizeof(text), fmt, ap);
    if (strncmp(text, "New station: ", 13) == 0) stub_new_station_alerts++;
    snprintf(stub_last_msg, sizeof(stub_last_msg), "%s", text);
    printf("[%s] %s\n", tag, text);
}
void msg_update_text_fmt(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vmsg("msg", fmt, ap); va_end(ap); }
void msg_schedule_text_fmt(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vmsg("msg", fmt, ap); va_end(ap); }

uint64_t get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
void event_send(lv_obj_t *obj, lv_event_code_t code, void *param) { lv_event_send(obj, code, param); }

/* ---- Transmit stubs -------------------------------------------------- */

#include "tx_player.h"
#include <unistd.h>

void  radio_set_pwr(float w) { printf("[radio] power %.0f W\n", w); }
void  radio_set_tx_filter(uint16_t low, uint16_t high) { printf("[radio] TX filter %u-%u Hz\n", low, high); }
int   stub_usb_kbd;                                                /* a USB keyboard is plugged in */
bool  keyboard_ready() { return stub_usb_kbd; }                   /* else show the on-screen keyboard */

float tx_player_base_gain_offset(void) { return -9.4f; }

/* Record what would go to the radio; stay "keyed" for 3 s so the harness can
 * screenshot the keying state, instead of the real 12.6 s. */
int      stub_tx_frames;
int32_t  stub_tx_offset;
uint32_t stub_tx_samples;
int16_t  stub_tx_peak;
int64_t stub_tx_start_sys_ms;
volatile int stub_tx_keyed;   /* inside tx_player_play: PTT on */
int          stub_tx_aborted; /* frames cut short by abort_check */
bool tx_player_play(int16_t *samples, uint32_t n, int32_t offset, float gain, tx_abort_fn_t abort_check, void *ctx) {
    (void)gain;
    int16_t peak = 0;
    for (uint32_t i = 0; i < n; i++) if (samples[i] > peak) peak = samples[i];
    stub_tx_keyed = 1;
    stub_tx_frames++;
    {   /* when the frame started, by the PC's own clock (no JS8 drift) */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        stub_tx_start_sys_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    }
    stub_tx_offset  = offset;
    stub_tx_samples = n;
    stub_tx_peak    = peak;
    printf("[radio] PTT on: frame %d, %u samples (%.2f s at %d kHz), offset %d Hz, peak %d\n", stub_tx_frames, n,
           n / (double)AUDIO_PLAY_RATE, AUDIO_PLAY_RATE / 1000, offset, peak);
    for (int i = 0; i < 30; i++) {
        if (abort_check && abort_check(ctx)) {
            printf("[radio] PTT off: aborted\n");
            stub_tx_aborted++;
            stub_tx_keyed = 0;
            return false;
        }
        usleep(100000);
    }
    printf("[radio] PTT off\n");
    stub_tx_keyed = 0;
    return true;
}

/* ONLY_SWR: the SWR the radio reports while keyed (tx_info), a new reading
 * at every look. */
float          stub_tx_swr = 1.3f;
static uint8_t stub_tx_info_id;
bool tx_info_refresh(uint8_t *prev_msg_id, float *alc_p, float *pwr_p, float *vswr_p) {
    if (!stub_tx_keyed) return false;
    *prev_msg_id = ++stub_tx_info_id;
    if (alc_p) *alc_p = 0.0f;
    if (pwr_p) *pwr_p = 5.0f;
    if (vswr_p) *vswr_p = stub_tx_swr;
    return true;
}
/* GPS: gpsd's latest report, a 3D fix from HARNESS_GPS="lat,lon" (driver.c
 * announces it with MSG_GPS once a second), no fix otherwise. */
void gps_get_snapshot(struct gps_data_t *out) {
    memset(out, 0, sizeof(*out));
    const char *e = getenv("HARNESS_GPS");
    if (e && sscanf(e, "%lf,%lf", &out->fix.latitude, &out->fix.longitude) == 2) out->fix.mode = MODE_3D;
}

/* The radio's QSO database: remembers calls saved this run. */
#include "qso_log.h"
static char worked_calls[32][32];
static int  worked_n;
char *util_canonize_callsign(const char *call, bool strip_slashes) {
    (void)strip_slashes;
    return strdup(call);
}
qso_log_band_t qso_log_freq_to_band(uint64_t freq_hz) {
    switch (freq_hz / 1000000) {
    case 7: return BAND_40M;
    case 10: return BAND_30M;
    case 14: return BAND_20M;
    case 18: return BAND_17M;
    default: return BAND_OTHER;
    }
}
qso_log_record_t qso_log_record_create(const char *local_call, const char *remote_call, time_t qso_time,
                                       qso_log_mode_t mode, int rsts, int rstr, uint64_t freq_hz, const char *name,
                                       const char *qth, const char *local_grid, const char *remote_grid) {
    (void)local_call; (void)qso_time; (void)mode; (void)name; (void)qth; (void)local_grid; (void)remote_grid;
    qso_log_record_t r = {0};
    snprintf(r.remote_call, sizeof(r.remote_call), "%s", remote_call);
    r.rsts = rsts;
    r.rstr = rstr;
    r.freq_mhz = freq_hz / 1e6f;
    return r;
}
int qso_log_record_save(qso_log_record_t qso) {
    printf("[qso_log] save %s rsts %d rstr %d %.6f MHz\n", qso.remote_call, qso.rsts, qso.rstr, qso.freq_mhz);
    if (worked_n < 32) snprintf(worked_calls[worked_n++], 32, "%s", qso.remote_call);
    return 0;
}
qso_log_search_worked_t qso_log_search_worked(const char *callsign, qso_log_mode_t mode, qso_log_band_t band) {
    (void)mode; (void)band;
    for (int i = 0; i < worked_n; i++)
        if (strcmp(worked_calls[i], callsign) == 0) return SEARCH_WORKED_SAME_MODE;
    return SEARCH_WORKED_NO;
}

/* The speaker: alert beeps. The real audio_play() waits for that much room
 * in the PulseAudio stream and never finds it for big writes (a 9702-sample
 * beep froze the radio), so every caller writes parts of 2048 samples. */
int audio_play(int16_t *buf, size_t samples) {
    (void)buf;
    if (samples > 2048) {
        printf("[audio] FAIL: %zu samples in one audio_play() hangs the radio\n", samples);
        abort();
    }
    printf("[audio] beep %zu samples\n", samples);
    usleep(samples * 1000000ull / AUDIO_PLAY_RATE); /* in real time, as the radio plays it */
    return 0;
}
void audio_play_wait(void) {
    usleep(150000); /* what's still in the stream */
    printf("[audio] drained\n");
}
void keypad_set_long_time(uint32_t ms) { printf("[keypad] hold time %u ms\n", (unsigned)ms); }
void radio_set_rx_dsp_off(bool off) { printf("[radio] NR/NB/notches %s\n", off ? "off" : "back to the settings"); }
void radio_speaker_play(bool on) { printf("[radio] speaker play %s\n", on ? "on" : "off"); }

/* ONLY_MODE: on 14.2 MHz in USB with a custom 27.245 MHz (CB) saved, and
 * CB last used in USB too: the settings manager loads a band's own mode
 * when the dial moves into it. */
void stub_mode_setup(void) {
    cparam_i_set(cfg.cur.fg_freq(), 27245000);
    cparam_i_set(cfg.cur.mode(), x6100_mode_usb);
    cparam_i_set(cfg.cur.fg_freq(), 14200000);
    cparam_i_set(cfg.cur.mode(), x6100_mode_usb);
    param_i_set(cfg.js8.custom_on(), true);
    param_i_set(cfg.js8.custom_hz(), 27245000);
    printf("[mode] setup: dial %d mode %d\n", stub_dial_hz(), stub_mode());
}
int stub_usb_dig(void) { return x6100_mode_usb_dig; }
