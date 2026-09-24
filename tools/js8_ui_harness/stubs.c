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
#include <string.h>
#include <time.h>

params_t    params;
lv_group_t *keyboard_group;
uint32_t    EVENT_BAND_UP;
uint32_t    EVENT_BAND_DOWN;

/* Filter edges the dialog reads through the computed-param API. */
static int               dummy_low, dummy_high;
ComputedParamInt        *cfg_cur_filter_low  = (ComputedParamInt *)&dummy_low;
ComputedParamInt        *cfg_cur_filter_high = (ComputedParamInt *)&dummy_high;
int32_t cparam_i_get(const ComputedParamInt *p) {
    return p == cfg_cur_filter_low ? 100 : 3000;
}

static const char *band_labels[] = {"JS8 40m", "JS8 30m", "JS8 20m", "JS8 17m"};
static int         band          = 2;
bool cfg_digital_load(int8_t dir, cfg_digital_type_t type) {
    if (type != CFG_DIG_TYPE_JS8) fprintf(stderr, "STUB: wrong digital type %d\n", type);
    band = (band + dir + 4) % 4;
    return true;
}
const char *cfg_digital_label_get(void) { return band_labels[band]; }

/* Buttons: remember the loaded page so the driver can press them. */
buttons_page_t *stub_page;
void            buttons_load_page(buttons_page_t *page) { stub_page = page; }
void            buttons_unload_page() { stub_page = NULL; }
buttons_page_t *buttons_get_cur_page() { return stub_page; }
void            buttons_refresh(button_data_t *d) {
    if (d->type == BTN_TEXT_FN) printf("[button] %s\n", d->label_fn());
}
void button_next_page_cb(button_data_t *d) { stub_page = d->next; }

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
