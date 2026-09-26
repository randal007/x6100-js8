/* Stand-ins for radio hardware and main-screen plumbing, so the real
 * dialog_js8.c / dialog.c / LVGL / src/js8 can run headless on a PC. */

#include "buttons.h"
#include "cfg/cfg_api.h"
#include "cfg/digital_modes.h"
#include "dsp.h"
#include "keyboard.h"
#include "main_screen.h"
#include "params/params.h"
#include "radio.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

params_t    params;
lv_group_t *keyboard_group;
uint32_t    EVENT_BAND_UP;
uint32_t    EVENT_BAND_DOWN;

/* Filter edges the dialog reads through the computed-param API. */
static int               dummy_low, dummy_high, dummy_fg;
ComputedParamInt        *cfg_cur_filter_low  = (ComputedParamInt *)&dummy_low;
ComputedParamInt        *cfg_cur_filter_high = (ComputedParamInt *)&dummy_high;
ComputedParamInt        *cfg_fg_freq         = (ComputedParamInt *)&dummy_fg; /* dial, Hz */
/* The dial: a preset (see presets[] below) or any frequency set. */
static int dial_hz = 14078000;
/* The radio's filter: 100-2900 Hz (a typical USB setting) until the app sets it. */
static int32_t filter_low = 100, filter_high = 2900;
int32_t cparam_i_get(const ComputedParamInt *p) {
    if (p == cfg_fg_freq) return dial_hz;
    return p == cfg_cur_filter_low ? filter_low : filter_high;
}
void cparam_i_set(ComputedParamInt *p, int32_t v) {
    if (p == cfg_fg_freq) {
        dial_hz = v;
        printf("[radio] dial %d Hz\n", v);
        return;
    }
    if (p == cfg_cur_filter_low) filter_low = v;
    else if (p == cfg_cur_filter_high) filter_high = v;
    printf("[radio] filter %d-%d Hz\n", filter_low, filter_high);
}

/* The same lookups as DigitalModesTable: next above, closest, next below. */
static const struct {
    const char *label;
    int         hz, type;
} presets[] = {
    {"JS8 40m", 7078000, CFG_DIG_TYPE_JS8},
    {"JS8 30m", 10130000, CFG_DIG_TYPE_JS8},
    {"JS8 20m", 14078000, CFG_DIG_TYPE_JS8},
    {"JS8 17m", 18104000, CFG_DIG_TYPE_JS8},
    {"GhostNet 80m", 3575000, CFG_DIG_TYPE_JS8_GHOSTNET},
    {"GhostNet 40m", 7107000, CFG_DIG_TYPE_JS8_GHOSTNET},
    {"GhostNet 20m", 14107000, CFG_DIG_TYPE_JS8_GHOSTNET},
};
static const char *preset_label = "JS8 20m";
bool cfg_digital_load(int8_t dir, cfg_digital_type_t type) {
    int best = -1;
    for (int i = 0; i < (int)(sizeof(presets) / sizeof(presets[0])); i++) {
        if (presets[i].type != (int)type) continue;
        int f = presets[i].hz;
        if (dir > 0 && f > dial_hz && (best < 0 || f < presets[best].hz)) best = i;
        if (dir < 0 && f < dial_hz && (best < 0 || f > presets[best].hz)) best = i;
        if (dir == 0 && (best < 0 || abs(f - dial_hz) < abs(presets[best].hz - dial_hz))) best = i;
    }
    if (best < 0) return false;
    dial_hz      = presets[best].hz;
    preset_label = presets[best].label;
    return true;
}
const char *cfg_digital_label_get(void) { return preset_label; }
int         stub_dial_hz(void) { return dial_hz; }

/* Buttons: remember the loaded page so the driver can press them. */
buttons_page_t *stub_page;
void            buttons_load_page(buttons_page_t *page) { stub_page = page; }
void            buttons_unload_page() { stub_page = NULL; }
buttons_page_t *buttons_get_cur_page() { return stub_page; }
void            buttons_refresh(button_data_t *d) {
    if (d->type == BTN_TEXT_FN) printf("[button] %s\n", d->label_fn());
}
void button_next_page_cb(button_data_t *d) { stub_page = d->next; }
void button_prev_page_cb(button_data_t *d) { stub_page = d->prev; }

void knobs_display(bool v) { (void)v; }
void waterfall_refresh_period_set(uint8_t k) { (void)k; }
void waterfall_refresh_reset() {}
void main_screen_keys_enable(bool v) { printf("[main] keys %s\n", v ? "enabled" : "disabled"); }
void main_screen_lock_freq(bool l) { (void)l; }
void main_screen_lock_band(bool l) { (void)l; }
void main_screen_lock_mode(bool l) { (void)l; }
void main_screen_lock_ab(bool l) { printf("[main] locks %s\n", l ? "on" : "off"); }
void mem_save(uint16_t id) { printf("[mem] save %u\n", id); }
void mem_load(uint16_t id) { printf("[mem] load %u\n", id); }
void dsp_set_waterfall_enabled(bool v) { (void)v; }
void dsp_set_spectrum_enabled(bool v) { (void)v; }
uint16_t radio_change_vol(int16_t d) { printf("[radio] vol %+d\n", d); return 0; }

static void vmsg(const char *tag, const char *fmt, va_list ap) {
    printf("[%s] ", tag);
    vprintf(fmt, ap);
    printf("\n");
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

static int dummy_pwr;
ParamFloat *cfg_pwr = (ParamFloat *)&dummy_pwr;
float param_f_get(const ParamFloat *p) { (void)p; return 10.0f; }  /* radio set to 10 W */
/* The saved TX filter (settings defaults 160-3000 Hz). */
static int dummy_txf_low, dummy_txf_high;
ParamInt  *cfg_tx_filter_low  = (ParamInt *)&dummy_txf_low;
ParamInt  *cfg_tx_filter_high = (ParamInt *)&dummy_txf_high;
int32_t    param_i_get(const ParamInt *p) { return p == cfg_tx_filter_low ? 160 : 3000; }
void  radio_set_pwr(float w) { printf("[radio] power %.0f W\n", w); }
void  radio_set_tx_filter(uint16_t low, uint16_t high) { printf("[radio] TX filter %u-%u Hz\n", low, high); }
int   stub_usb_kbd;                                                /* a USB keyboard is plugged in */
bool  keyboard_ready() { return stub_usb_kbd; }                   /* else show the on-screen keyboard */
void  params_uint16_set(params_uint16_t *var, uint16_t x) { var->x = x; }
void  params_int32_set(params_int32_t *var, int32_t x) { var->x = x; }

float tx_player_base_gain_offset(void) { return -9.4f; }

/* Record what would go to the radio; stay "keyed" for 3 s so the harness can
 * screenshot the keying state, instead of the real 12.6 s. */
int      stub_tx_frames;
int32_t  stub_tx_offset;
uint32_t stub_tx_samples;
int16_t  stub_tx_peak;
bool tx_player_play(int16_t *samples, uint32_t n, int32_t offset, float gain, tx_abort_fn_t abort_check, void *ctx) {
    (void)gain;
    int16_t peak = 0;
    for (uint32_t i = 0; i < n; i++) if (samples[i] > peak) peak = samples[i];
    stub_tx_frames++;
    stub_tx_offset  = offset;
    stub_tx_samples = n;
    stub_tx_peak    = peak;
    printf("[radio] PTT on: frame %d, %u samples (%.2f s at 44.1 kHz), offset %d Hz, peak %d\n", stub_tx_frames, n,
           n / 44100.0, offset, peak);
    for (int i = 0; i < 30; i++) {
        if (abort_check && abort_check(ctx)) {
            printf("[radio] PTT off: aborted\n");
            return false;
        }
        usleep(100000);
    }
    printf("[radio] PTT off\n");
    return true;
}
void params_bool_set(params_bool_t *var, bool x) { var->x = x; }

/* GPS: a fix from HARNESS_GPS="lat,lon" (age 5 s), none otherwise. */
bool gps_last_fix(double *lat, double *lon, int *age_s) {
    const char *e = getenv("HARNESS_GPS");
    if (!e || sscanf(e, "%lf,%lf", lat, lon) != 2) return false;
    *age_s = 5;
    return true;
}
void params_uint8_set(params_uint8_t *var, uint8_t x) { var->x = x; }

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
    return 0;
}
void audio_play_wait(void) { printf("[audio] drained\n"); }
