// Headless run of the real JS8 dialog: LVGL renders into memory while a
// synthetic but realistic band (several stations, interleaved multi-frame
// messages, heartbeats, a CQ, a message to me, a checksummed MSG) is fed
// through the dialog's audio callback. Screenshots are written as PPM.

#include "lvgl/lvgl.h"
#include <sqlite3.h>
#include "widgets/lv_waterfall.h"
#include "js8_wf.h"
#include "js8_history.h"
#include "widgets/lv_finder.h"
extern "C" {
void dialog_destruct(void);
void dialog_audio_samples(unsigned int n, float *samples);
void scheduler_work();
void observer_delayed_drain(void);
void ui_gps_tick(void);
uint32_t stub_audio_rate(void);
int      stub_play_rate(void);
void ui_init(void);
void ui_open(void);
void ui_press(int i);
void ui_band_up(void);
void ui_band_down(void);
int  stub_dial_hz(void);
int  stub_mode(void);
void stub_mode_setup(void);
int  stub_usb_dig(void);
void ui_key(uint32_t key);
int  ui_running(void);
int  ui_focus_is_table(void);
void ui_rotary(int32_t diff);
int  ui_tx_offset(void);
int  ui_list_has(const char *text);
const char *ui_focused_text(void);
const char *ui_focus_desc(void);
const char *ui_button_label(int i);
int         ui_button_exists(int i);
int         ui_button_marked(int i);
void ui_compose_append(const char *text);
const char *ui_compose_text(void);
void ui_compose_enter(void);
void ui_compose_cancel(void);
void ui_select_row_from(const char *call);
void ui_click_focused(void);
void ui_page(int n);
void ui_hold(int i);
int  ui_list_count(const char *text);
int  ui_popup_has(const char *text);
const char *ui_button_shown(int i);
void ui_preset_auto_hb(void);
void ui_popup_print(const char *tag);
void ui_compose_clear(void);
void ui_indevs_init(void);
int  ui_kb_select_ok(void);
void ui_usb_init(void);
bool dialog_js8_selected_call(char *call, unsigned len);
int  dialog_js8_station_rows(char *out, unsigned len);
char dialog_js8_station_star(const char *call);
const char *dialog_js8_freq_name(void);
js8_history_t *dialog_js8_history(void);
int  dialog_js8_map_stacks(const char **text);
void dialog_js8_time_auto(bool on);
const char *dialog_js8_st_qrz(void);
int  dialog_js8_finder_hz(void);
void ui_main_redraw_watch(void);
bool ui_main_redraw_due(void);
void ui_retune_by(int hz);
bool dialog_js8_wf_area(lv_area_t *a, const lv_color_t **palette);
void dialog_js8_wf_avg(int level);
void dialog_js8_autos(bool *auto_mode, bool *hb, bool *hb_ack, bool *cq);
int  dialog_js8_beeps(int *last_count);
bool dialog_js8_cursor_band(int *x, int *w);
unsigned dialog_js8_marks(float *freq_hz, uint8_t *level, unsigned max);
bool     dialog_js8_map_state(bool *world, int *popups, bool *tx_outline, int *qso_paths, int *qrz);
const char *dialog_js8_map_stats(void);
int         dialog_js8_map_extra(int *talks, int *my_dot_x, int *in_dots, const char **strip0, const char **strip1,
                                 bool *follow);
void ui_usb_event(uint32_t key, int value);
void ui_mfk_turn(int32_t diff);
void ui_mfk_set(bool down);
const char *ui_cursor_text(void);
int         ui_group_count(void);
int         ui_marked_rows(char *out, unsigned len);
void ui_keypad_set(uint32_t key, bool down);
void ui_set_alerts(unsigned bits);
void ui_set_callsign(const char *call);
void ui_vol(int dir);
const char *ui_compose_placeholder(void);
extern int stub_tx_frames;
extern int stub_usb_kbd;
extern int32_t stub_tx_offset;
extern uint32_t stub_tx_samples;
extern int64_t stub_tx_start_sys_ms;
int64_t js8_drift_ms(void);
void    js8_set_drift_ms(int64_t ms);
extern int16_t stub_tx_peak;
extern volatile int stub_tx_keyed;
extern int stub_tx_aborted;
extern float stub_tx_swr;
extern int stub_new_station_alerts;
extern char stub_last_msg[512];
extern int stub_vol_turns;
}

#include "js8core/decoder.hpp"
#include "testsignal.hpp"
#include "js8core/protocol/costas.hpp"
#include "js8core/protocol/varicode.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <glob.h>
#include <sys/stat.h>
#include <vector>

namespace vc = js8core::protocol::varicode;

static constexpr int W = 800, H = 480, RATE = 12000; /* what JS8 asks R1CBU 1.0 for */
static constexpr int NSPS = RATE * 4 / 25;             /* samples per JS8 Normal symbol (0.16 s) */
static std::vector<uint32_t> fb(W *H);

// ONLY_WFPERF: the radio's flush path, timed: fbdev.c copies each area into
// a queue, rotates it 90 degrees (the panel is portrait) and copies it into
// the framebuffer. perf_rotate picks the rotation under test.
static bool                  perf_mode;
static int                   perf_rotate; // 0: fbdev.c's rotate_sw, 1: tiled
static double                perf_flush_ms;
static long                  perf_flush_px;
static std::vector<uint32_t> perf_queue(W *H), perf_rot(W *H), perf_fb(W *H);

static void rotate_naive(const uint32_t *src, uint32_t src_w, uint32_t src_h, uint32_t *dst) {
    uint32_t dst_w = src_h;
    for (size_t src_y = 0; src_y < src_h; src_y++)
        for (size_t src_x = 0; src_x < src_w; src_x++) dst[(src_w - src_x - 1) * dst_w + src_y] = src[src_y * src_w + src_x];
}

static void rotate_tiled(const uint32_t *src, uint32_t src_w, uint32_t src_h, uint32_t *dst) {
    const uint32_t T     = 16;
    uint32_t       dst_w = src_h;
    for (uint32_t by = 0; by < src_h; by += T)
        for (uint32_t bx = 0; bx < src_w; bx += T) {
            uint32_t ey = by + T < src_h ? by + T : src_h, ex = bx + T < src_w ? bx + T : src_w;
            for (uint32_t x = bx; x < ex; x++) {
                uint32_t *d = dst + (src_w - x - 1) * dst_w;
                for (uint32_t y = by; y < ey; y++) d[y] = src[y * src_w + x];
            }
        }
}

static double now_ms_f() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The display's lower plane (stubs.c): JS8's waterfall, under the
// see-through plane LVGL draws on (fb here).
extern "C" {
extern uint32_t      harness_plane[480 * 800];
extern unsigned long harness_plane_puts, harness_plane_px;
extern void        (*harness_plane_cb)(const lv_area_t *a);
}

// What the screen shows at (x, y): LVGL's plane over the lower one, as the
// display hardware blends them.
static uint32_t screen_px(int x, int y);

// ONLY_WFTIME: when each waterfall row reaches the screen (a put on the
// lower plane whose newest row changed).
static bool                wftime_mode;
static std::vector<double> wftime_stamps;
// ONLY_WFCALM: each new row as it reaches the plane (its luma).
static bool                            calm_mode;
static std::vector<std::vector<float>> calm_rows;
static void                            wftime_plane_cb(const lv_area_t *a) {
    if (calm_mode) {
        std::vector<float> r;
        for (int y = a->y1; y <= a->y2; y++) {
            uint32_t p = harness_plane[y * 480 + a->x1];
            r.push_back(0.299f * ((p >> 16) & 255) + 0.587f * ((p >> 8) & 255) + 0.114f * (p & 255));
        }
        if (calm_rows.empty() || r != calm_rows.back()) calm_rows.push_back(r);
    }
    if (!wftime_mode) return;
    static uint64_t last_sig;
    uint64_t        sig = 1469598103934665603ull;
    for (int y = a->y1; y <= a->y2; y += 23) sig = (sig ^ harness_plane[y * 480 + a->x1]) * 1099511628211ull;
    if (sig != last_sig) wftime_stamps.push_back(now_ms_f());
    last_sig = sig;
}

// ONLY_LOAD: pixels and flushes sent to the screen.
static long load_flush_px;
static long load_flushes;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
    uint32_t w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
    load_flush_px += (long)w * h;
    load_flushes++;
    if (perf_mode) {
        double t0 = now_ms_f();
        memcpy(perf_queue.data(), px, w * h * 4);
        (perf_rotate ? rotate_tiled : rotate_naive)(perf_queue.data(), w, h, perf_rot.data());
        const uint32_t *r = perf_rot.data();
        for (uint32_t y = 0; y < w; y++, r += h) memcpy(&perf_fb[(800 - a->x2 + y) % H * W + a->y1], r, h * 4);
        perf_flush_ms += now_ms_f() - t0;
        perf_flush_px += (long)w * h;
    }
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++) fb[y * W + x] = (px++)->full;
    lv_disp_flush_ready(drv);
}

static uint32_t screen_px(int x, int y) {
    uint32_t top = fb[y * W + x], a = top >> 24;
    if (a == 255) return top;
    uint32_t bot = harness_plane[(799 - x) * 480 + y], out = 0;
    for (int sh = 0; sh < 24; sh += 8) {
        uint32_t c = (((top >> sh) & 255) * a + ((bot >> sh) & 255) * (255 - a) + 127) / 255;
        out |= c << sh;
    }
    return out | 0xff000000u;
}

static void screenshot(const char *path) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            uint32_t      p      = screen_px(x, y);
            unsigned char rgb[3] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    printf("[shot] %s\n", path);
}

// Let LVGL and the scheduler run for `ms` of wall time.
// main_redraw (ONLY_WFRING): the main screen's spectrum and waterfall
// (spectrum_process() / waterfall_process() after lv_timer_handler() in the
// radio's loop) redraw the lower plane in the pass the frequency changes,
// over JS8's waterfall: here the whole plane, magenta.
static bool main_redraw;

static void pump(int ms) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        lv_tick_inc(5);
        observer_delayed_drain(); /* the radio's main loop, in its order */
        scheduler_work();
        ui_gps_tick();
        lv_timer_handler();
        if (main_redraw && ui_main_redraw_due())
            for (auto &p : harness_plane) p = 0xffff00ffu;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

static void pump(int ms);

// ONLY_LOAD: the GUI thread's work for `ms` as on the radio: LVGL's timers
// run (the TX bar's 250 ms update included), unlike wf_bench. With `noise`,
// live noise audio in real time, so the waterfall adds its rows.
static void load_measure(const char *label, int ms, bool noise) {
    std::mt19937                    rng(9);
    std::normal_distribution<float> nd(0.0f, 0.02f);
    std::vector<float>              buf;
    long                            px0 = load_flush_px, fl0 = load_flushes;
    double                          busy = 0, fed = 0;
    auto                            t0 = std::chrono::steady_clock::now();
    for (;;) {
        double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (el * 1000 >= ms) break;
        lv_tick_inc(5);
        if (noise) {
            unsigned n = (unsigned)(el * RATE - fed);
            if (n) {
                buf.resize(n);
                for (auto &x : buf) x = nd(rng);
                dialog_audio_samples(n, buf.data());
                fed += n;
            }
        }
        double a = now_ms_f();
        observer_delayed_drain();
        scheduler_work();
        lv_timer_handler();
        busy += now_ms_f() - a;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    double s = ms / 1000.0;
    printf("[load] %-34s GUI busy %6.1f ms/s, %7.0f kpx/s to the screen, %5.1f flushes/s\n", label, busy / s,
           (load_flush_px - px0) / 1000.0 / s, (load_flushes - fl0) / s);
}

struct Station {
    const char *call, *grid, *to, *text;
    double      offset_hz;
    float       amp;
};

// ONLY_STALL: feed_band() without running the GUI thread, as if it were
// stuck (a slow redraw, a database lookup, an SD card write).
static bool feed_stalled;

// Synthesise `band` (one slot per frame, starting at the next slot
// boundary) and feed it through the dialog's audio callback in real time.
static void feed_band(const std::vector<Station> &band, std::size_t first = 0, std::size_t last = 99,
                      double late_s = 0, double tail_s = 3) {
    const auto &costas = js8core::protocol::costas(js8core::protocol::CostasType::Original);
    std::vector<std::vector<std::array<int, js8core::kJs8NumSymbols>>> tones(band.size());
    std::size_t slots = 0;
    for (std::size_t i = 0; i < band.size(); i++) {
        auto frames = vc::build_message_frames(band[i].call, band[i].grid, "", band[i].text, false, false, 0);
        for (std::size_t f = first; f < frames.size() && f < last; f++) { // frames [first, last)
            std::array<int, js8core::kJs8NumSymbols> t{};
            js8core::legacy_encode(frames[f].second, costas, frames[f].first.c_str(), t.data());
            tones[i].push_back(t);
        }
        slots = std::max(slots, tones[i].size());
        printf("[band] %-7s %4.0f Hz  %s\n", band[i].call, band[i].offset_hz, band[i].text);
    }
    auto               now  = std::chrono::system_clock::now().time_since_epoch();
    long long          ms   = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::size_t        lead = (std::size_t)((15000 - ms % 15000) * RATE / 1000);
    // A late band runs that much past its slots (ONLY_DRIFT feeds one 6 s late).
    std::vector<float> audio(lead + slots * 15 * RATE + (std::size_t)((late_s + tail_s) * RATE), 0.0f);
    for (std::size_t i = 0; i < band.size(); i++)
        for (std::size_t k = 0; k < tones[i].size(); k++) {
            std::size_t start = lead + k * 15 * RATE + RATE / 2 + (std::size_t)(late_s * RATE);
            double      phi   = 0;
            for (int s = 0; s < js8core::kJs8NumSymbols; s++) {
                double dphi = 2 * M_PI * (band[i].offset_hz + tones[i][k][s] * 6.25) / RATE;
                for (int j = 0; j < NSPS; j++) {
                    audio[start + s * NSPS + j] += band[i].amp * (float)std::sin(phi);
                    phi += dphi;
                }
            }
        }
    std::mt19937                    rng(5);
    std::normal_distribution<float> noise(0.0f, 0.02f);
    for (auto &x : audio) x += noise(rng);
    const std::size_t piece = RATE / 50;
    auto              t0    = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < audio.size(); i += piece) {
        unsigned n = (unsigned)std::min(piece, audio.size() - i);
        dialog_audio_samples(n, &audio[i]);
        auto due = t0 + std::chrono::microseconds((long long)((i + n) * 1e6 / RATE));
        if (feed_stalled) std::this_thread::sleep_until(due);
        else
            while (std::chrono::steady_clock::now() < due) pump(5);
    }
    pump(1500);
}

// A band of stations at any speeds (src/js8/testsignal), fed in real time
// from the next 30 s boundary, a slot start for every speed.
static void feed_speeds(const std::vector<x6100::js8::TestStation> &band) {
    auto               audio = x6100::js8::make_test_band(band, RATE, 0.02f, 7);
    auto               now   = std::chrono::system_clock::now().time_since_epoch();
    long long          ms    = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::size_t        lead  = (std::size_t)((30000 - ms % 30000) * RATE / 1000);
    std::vector<float> all(lead, 0.0f);
    std::mt19937                    rng(3);
    std::normal_distribution<float> noise(0.0f, 0.02f);
    for (auto &x : all) x = noise(rng);
    all.insert(all.end(), audio.begin(), audio.end());
    for (auto &st : band) printf("[band] %-7s %4.0f Hz %-6s %s\n", st.call.c_str(), st.offset_hz,
                                 js8_speed_name(st.speed), st.text.c_str());
    const std::size_t piece = RATE / 50;
    auto              t0    = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < all.size(); i += piece) {
        unsigned n = (unsigned)std::min(piece, all.size() - i);
        dialog_audio_samples(n, &all[i]);
        auto due = t0 + std::chrono::microseconds((long long)((i + n) * 1e6 / RATE));
        while (std::chrono::steady_clock::now() < due) pump(5);
    }
    pump(2000);
}

static lv_obj_t *find_obj(lv_obj_t *o, const lv_obj_class_t *cls) {
    if (lv_obj_check_type(o, cls)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(o); i++)
        if (lv_obj_t *f = find_obj(lv_obj_get_child(o, i), cls)) return f;
    return nullptr;
}

struct Stat {
    std::vector<double> v;
    void                add(double x) { v.push_back(x); }
    double              mean() const {
        double s = 0;
        for (double x : v) s += x;
        return v.empty() ? 0 : s / v.size();
    }
    double pct(double p) const {
        auto c = v;
        std::sort(c.begin(), c.end());
        return c.empty() ? 0 : c[(size_t)(p * (c.size() - 1))];
    }
};

// One waterfall row after another as fast as possible, through the dialog's
// own path: js8_wf_add_row(), the put on the lower plane (js8_wf_tick()),
// then whatever LVGL redraws for it, flushed as the radio does. (The radio
// then copies the put into its frame buffers at the page flip, twice.)
static void wf_bench(const char *name, int frames) {
    std::vector<float>                    row(788);
    std::mt19937                          rng(1);
    std::uniform_real_distribution<float> u(0, 30);
    Stat                                  add, put, render, flush, lvpx;
    lv_refr_now(NULL);
    unsigned long puts0 = harness_plane_puts, px0 = harness_plane_px;
    for (int i = 0; i < frames; i++) {
        for (auto &x : row) x = u(rng);
        double t0 = now_ms_f();
        js8_wf_add_row(row.data(), (uint16_t)row.size());
        double t1 = now_ms_f();
        js8_wf_tick();
        double t2 = now_ms_f();
        long   l0 = load_flush_px;
        perf_flush_ms = 0;
        lv_refr_now(NULL);
        double t3 = now_ms_f();
        add.add(t1 - t0);
        put.add(t2 - t1);
        render.add(t3 - t2 - perf_flush_ms);
        flush.add(perf_flush_ms);
        lvpx.add((double)(load_flush_px - l0));
    }
    unsigned long puts = harness_plane_puts - puts0;
    printf("[wfperf] %-34s add %.3f ms  plane put %.3f ms (%lu puts, %lu px each)  LVGL render %.3f ms  flush %.3f ms  "
           "LVGL px %.0f  total %.2f ms/row\n",
           name, add.mean(), put.mean(), puts, puts ? (harness_plane_px - px0) / puts : 0, render.mean(), flush.mean(),
           lvpx.mean(), add.mean() + put.mean() + render.mean() + flush.mean());
}

// Frequencies (Settings, page 6's Freq before): the JS8 / GhostNet /
// Custom list, opened the way you do on the radio.
static void open_freq() {
    ui_page(4);
    ui_press(4); // Settings
    pump(200);
    for (int i = 0; i < 20 && strncmp(ui_focused_text(), "Frequencies", 11) != 0; i++) ui_key(LV_KEY_RIGHT);
    ui_click_focused();
    pump(200);
}

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0); // keep the log if something aborts
    lv_init();
    static lv_color_t          buf[W * 60];
    static lv_color_t          full_buf[W * H]; // the radio's: one full-screen buffer
    static lv_disp_draw_buf_t  draw_buf;
    perf_mode = getenv("ONLY_WFPERF") != nullptr;
    if (perf_mode) lv_disp_draw_buf_init(&draw_buf, full_buf, nullptr, W * H);
    else lv_disp_draw_buf_init(&draw_buf, buf, nullptr, W * 60);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res  = W;
    drv.ver_res  = H;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &draw_buf;
    /* The radio's app plane is see-through (main.c): where JS8 leaves a
     * hole, its waterfall on the lower plane shows. */
    drv.screen_transp = getenv("HARNESS_OPAQUE") ? 0 : 1; /* HARNESS_OPAQUE: timing as before (no waterfall shows) */
    lv_disp_drv_register(&drv);
    harness_plane_cb  = wftime_plane_cb;

    ui_init();
    if (getenv("ONLY_MODE")) stub_mode_setup();
    if (getenv("ONLY_INBOX") || getenv("ONLY_SMS") || getenv("ONLY_REPLYQ")) unlink(JS8_INBOX_PATH); // before the dialog loads it
    if (getenv("ONLY_HELD")) unlink(JS8_HELD_PATH);
    if (getenv("ONLY_ROWS")) {
        // 60 messages, the oldest and #55 unread: only the newest 50 were
        // listed (bug hunt 9), so the oldest could never be opened.
        FILE *f = fopen(JS8_INBOX_PATH, "w");
        for (int i = 1; i <= 60; i++)
            fprintf(f, "%d\t%d000\t%s\tN0XYZ\tK2XYZ\tN0XYZ\t%s %d\n", i, i, i == 1 || i == 55 ? "U" : "R",
                    i == 1 ? "THE OLDEST UNREAD" : i == 55 ? "A NEWER UNREAD" : "READ MESSAGE", i);
        fclose(f);
    }
    if (getenv("ONLY_RELAY")) {
        unlink(JS8_INBOX_PATH);
        unlink(JS8_HELD_PATH);
        unlink(JS8_TEXTS_PATH);
    }
    if (getenv("ONLY_APRS")) unlink(JS8_TEXTS_PATH); // no park or spot settings yet
    if (getenv("ONLY_BADFILES")) {
        // An SD card read error: the Inbox and the settings file exist but
        // can't be read (before package 2 the next save wrote over them).
        FILE *f = fopen(JS8_INBOX_PATH, "w");
        fputs("1\t1000\tU\tN0XYZ\tK2XYZ\tN0XYZ\tTHE OLD MESSAGE\n", f);
        fclose(f);
        chmod(JS8_INBOX_PATH, 0);
        f = fopen(JS8_TEXTS_PATH, "w");
        fputs("INFO=THE OLD INFO\n", f);
        fclose(f);
        chmod(JS8_TEXTS_PATH, 0);
    }
    // Saved messages start as desktop's ("TNX 73 GL") when there's no file.
    if (getenv("ONLY_SAVED")) unlink(JS8_SAVED_PATH);
    if (getenv("ONLY_STSORT")) { // Sort: QSO reads the history: a new one
        unlink(JS8_HISTORY_PATH);
        unlink(JS8_HISTORY_PATH "-journal");
    }
    if (getenv("ONLY_BOOTHB")) ui_preset_auto_hb(); // switched off with auto HB on
    if (getenv("ONLY_HISTORY")) { // a new history file
        unlink(JS8_HISTORY_PATH);
        unlink(JS8_HISTORY_PATH "-journal");
    }
    ui_open();
    if (stub_audio_rate() != RATE) printf("[harness] FAIL: JS8 asked for %u Hz audio, the harness feeds %d\n", stub_audio_rate(), RATE);
    if (getenv("ONLY_GEN")) {
        // GEN / APP on the radio close the app with a list popup open.
        const char *which = getenv("ONLY_GEN");
        pump(300);
        if (!strcmp(which, "query")) {
            feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
            ui_select_row_from("N0XYZ");
            ui_page(1);
            ui_press(3); // Query >
        } else if (!strcmp(which, "alerts")) {
            ui_page(6);
            ui_press(1); // Alerts >
        } else if (!strcmp(which, "aprs")) {
            ui_page(5);
            ui_press(1); // APRS >
        } else {
            ui_page(4);
            ui_press(4); // Texts...
        }
        pump(300);
        printf("[gen] %s list open, focused '%s'\n", which, ui_focused_text());
        {
            char shot[48];
            snprintf(shot, sizeof(shot), "gen_%s.ppm", which);
            screenshot(shot);
        }
        dialog_destruct(); // what GEN does
        pump(500);
        printf("[gen] closed with the %s list open: running=%d (survived)\n", which, ui_running());
        return 0;
    }
    if (getenv("ONLY_TXBAR")) {
        // A long message: the frame progress stays on screen.
        pump(300);
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        ui_compose_append("K9DEF THIS IS A RATHER LONG MESSAGE TO SEE WHERE THE FRAME COUNT GOES IN THE BAR");
        ui_compose_enter();
        pump(600);
        screenshot("50_txbar_waiting.ppm");
        int frames = stub_tx_frames;
        for (int i = 0; i < 400 && stub_tx_frames == frames; i++) pump(50);
        pump(300);
        screenshot("51_txbar_sending.ppm");
        return 0;
    }
    if (getenv("ONLY_DRIFT")) {
        // Time (page 4, button 2): Auto follows the decodes as desktop's
        // Automatic Time Drift (one band is enough); press = Auto off/on;
        // hold = the search, for a clock too far off for anything to
        // decode. JS8's timing moves, the PC's clock doesn't.
        auto drift = [] { return (long long)js8_drift_ms(); };
        auto label = [] {
            static std::string l;
            l = ui_button_label(2);
            for (auto &c : l) c = c == '\n' ? ' ' : c;
            return l.c_str();
        };
        auto rows = [] { return ui_list_count("CQ CQ CQ"); }; // the band's two CQs
        pump(300);
        dialog_js8_time_auto(true); // as on the radio (a build dir may have it off)
        ui_page(4);
        printf("[drift] at start: %lld ms, button '%s' (want Time: Auto)\n", drift(), label());
        // Everyone 1.2 s later than our clock says: within the decoder's reach.
        std::vector<Station> late = {{"W1ABC", "FN42", "", "CQ CQ CQ FN42", 900, 0.05f},
                                     {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1400, 0.05f},
                                     {"VE7ABC", "CN89", "", "CQ CQ CQ CN89", 1900, 0.05f},
                                     {"N0XYZ", "EN34", "", "@HB HEARTBEAT EN34", 2400, 0.05f}};
        feed_band(late, 0, 99, 1.2);
        pump(1000);
        printf("[drift] Auto after one band: %lld ms (want about -1200), button '%s'\n", drift(), label());
        long long d1 = drift();
        feed_band(late, 0, 99, 1.2); // the same band: on time now
        pump(1000);
        printf("[drift] same band again: moved %lld ms (want under 100)\n", drift() - d1);
        // Our own frame starts on JS8's slot (0.5 s after a 15 s boundary of
        // drifted time), i.e. 1.2 s early by the PC's clock.
        int frames = stub_tx_frames;
        ui_page(1);
        ui_press(1); // CQ
        pump(300);
        for (int i = 0; i < 400 && stub_tx_frames == frames; i++) pump(100);
        long long slot = (stub_tx_start_sys_ms + js8_drift_ms()) % 15000;
        printf("[drift] CQ frame started %lld ms into JS8's slot (want ~500), %lld ms by the PC clock\n", slot,
               (long long)(stub_tx_start_sys_ms % 15000));
        for (int i = 0; i < 200; i++) pump(100); // let it finish
        // Auto off: a band a further second late leaves the drift alone.
        ui_page(4);
        ui_press(2);
        pump(200);
        long long d2 = drift();
        printf("[drift] pressed: button '%s' (want Time: Off), '%s'\n", label(), stub_last_msg);
        feed_band(late, 0, 99, 2.2);
        pump(1000);
        printf("[drift] Auto off, a band 1 s later: moved %lld ms (want 0)\n", drift() - d2);
        ui_press(2); // Auto again
        pump(200);
        // Settings > Reset time drift (first in the list): the radio's clock.
        ui_press(4);
        pump(300);
        printf("[drift] Settings opens on '%s' (want Reset time drift)\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        ui_key(LV_KEY_ESC);
        pump(300);
        printf("[drift] after Reset: %lld ms (want 0)\n", drift());
        // 6 s late: out of the decoder's reach, nothing decodes...
        int before = rows();
        feed_band(late, 0, 99, 6.0, 12);
        pump(1000);
        printf("[drift] 6 s late: %d new decodes (want 0), drift %lld (want 0)\n", rows() - before, drift());
        // ...until the search (hold Time) finds the band.
        ui_page(4);
        ui_hold(2);
        pump(200);
        printf("[drift] held: button '%s' marked %d (want Time: Searching, 1), '%s'\n", label(), ui_button_marked(2),
               stub_last_msg);
        feed_band(late, 0, 99, 6.0, 12);
        for (int i = 0; i < 100 && strstr(ui_button_label(2), "Searching"); i++) pump(100);
        printf("[drift] search: %lld ms (want about -6000), button '%s' marked %d\n", drift(), label(),
               ui_button_marked(2));
        printf("[drift] '%s'\n", stub_last_msg);
        before = rows();
        feed_band(late, 0, 99, 6.0);
        pump(1000);
        printf("[drift] the band decodes again: %d new decodes (want 2), drift %lld\n", rows() - before, drift());
        // A search with nothing to find stops when held again.
        ui_hold(2);
        pump(200);
        ui_hold(2);
        pump(200);
        printf("[drift] stopped: button '%s' (want Time: Auto ...), '%s'\n", label(), stub_last_msg);
        return 0;
    }
    if (getenv("ONLY_WFRING")) {
        // The waterfall widget's ring buffer against a plain model, as drawn.
        const int  WW = 13, HH = 7;
        lv_obj_t  *box = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(box);
        lv_obj_set_style_bg_color(box, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_pos(box, 100, 100);
        lv_obj_set_size(box, WW, HH);
        lv_obj_t         *wf = lv_waterfall_create(box);
        static lv_color_t pal[256];
        for (int i = 0; i < 256; i++) pal[i] = lv_color_make(i, 255 - i, (i * 7) & 255);
        lv_waterfall_set_palette(wf, pal, 256);
        lv_waterfall_set_size(wf, WW, HH);
        lv_waterfall_set_min(wf, 0);
        lv_waterfall_set_max(wf, 255);
        lv_obj_set_pos(wf, 0, 0);
        std::vector<std::vector<int>> model; // newest first
        int                           bad = 0, checks = 0;
        auto                          check = [&](int step) {
            lv_refr_now(NULL);
            lv_area_t c;
            lv_obj_get_coords(wf, &c);
            for (int y = 0; y < HH; y++)
                for (int x = 0; x < WW; x++) {
                    uint32_t got  = fb[(c.y1 + y) * W + c.x1 + x] & 0xffffff;
                    uint32_t want = y < (int)model.size() ? lv_color_to32(pal[model[y][x]]) & 0xffffff : 0;
                    checks++;
                    if (got != want && bad++ < 5)
                        printf("[wfring] step %d pixel %d,%d: %06x want %06x\n", step, x, y, got, want);
                }
        };
        for (int r = 0; r < 40; r++) {
            std::vector<float> d(WW);
            std::vector<int>   ids(WW);
            for (int x = 0; x < WW; x++) {
                ids[x] = (r * 31 + x * 17 + 5) % 256;
                d[x]   = ids[x] + 0.5f;
            }
            lv_waterfall_add_data(wf, d.data(), WW);
            model.insert(model.begin(), ids);
            if ((int)model.size() > HH) model.pop_back();
            check(r);
            if (r == 20) {
                lv_waterfall_clear_data(wf);
                model.clear();
                check(r);
            }
        }
        printf("[wfring] %d pixels checked, %d wrong\n", checks, bad);
        lv_obj_del(box);
        lv_refr_now(NULL);

        // JS8's own waterfall (js8_wf.c, the open dialog's, on the lower
        // plane): rows, marks, the ring's wrap and a clear against a model,
        // as the screen shows them where the app's plane is a hole.
        lv_area_t         wa;
        const lv_color_t *wpal = nullptr;
        if (!dialog_js8_wf_area(&wa, &wpal)) {
            printf("[wfring] FAIL: no JS8 waterfall\n");
            return 1;
        }
        const int WFW = lv_area_get_width(&wa), WFH = lv_area_get_height(&wa);
        std::vector<std::vector<uint32_t>> jmodel; // newest first, colours
        int                                jbad = 0, jchecks = 0, holes = 0;
        auto                               jcheck = [&](const char *what) {
            lv_refr_now(NULL);
            holes = 0;
            for (int y = 0; y < WFH; y++)
                for (int x = 0; x < WFW; x++) {
                    if (fb[(wa.y1 + y) * W + wa.x1 + x] >> 24) continue; // something of the app's over it
                    holes++;
                    uint32_t got  = screen_px(wa.x1 + x, wa.y1 + y) & 0xffffff;
                    uint32_t want = y < (int)jmodel.size() ? jmodel[y][x] : lv_color_to32(lv_color_black()) & 0xffffff;
                    jchecks++;
                    if (got != want && jbad++ < 5) printf("[wfring] js8 %s pixel %d,%d: %06x want %06x\n", what, x, y, got, want);
                }
        };
        for (int r = 0; r < WFH + 60; r++) { // past the ring's height: it wraps
            std::vector<float>    d(WFW);
            std::vector<uint32_t> row(WFW);
            for (int x = 0; x < WFW; x++) {
                int id = (r * 31 + x * 17 + 5) % 256;
                d[x]   = (id + 0.5f) * 30.0f / 255.0f; // WF_MIN_DB 0 .. WF_MAX_DB 30
                row[x] = lv_color_to32(wpal[id]) & 0xffffff;
            }
            js8_wf_add_row(d.data(), (uint16_t)WFW);
            js8_wf_tick();
            jmodel.insert(jmodel.begin(), row);
            if ((int)jmodel.size() > WFH) jmodel.pop_back();
            if (r % 97 == 0 || r == WFH + 59) jcheck("rows");
        }
        // A decode mark's bracket: rows 3..17 below the newest, x 200..260.
        lv_color_t mark = lv_color_hex(0xffd600);
        js8_wf_fill_rect(200, 3, 260, 17, mark);
        js8_wf_tick();
        for (int y = 3; y <= 17; y++)
            for (int x = 200; x <= 260; x++) jmodel[y][x] = lv_color_to32(mark) & 0xffffff;
        jcheck("mark");
        js8_wf_add_row(std::vector<float>(WFW, 0.0f).data(), (uint16_t)WFW); // the mark scrolls down with it
        js8_wf_tick();
        jmodel.insert(jmodel.begin(), std::vector<uint32_t>(WFW, lv_color_to32(wpal[0]) & 0xffffff));
        jmodel.pop_back();
        jcheck("mark scrolled");
        js8_wf_clear();
        js8_wf_tick();
        jmodel.clear();
        jcheck("clear");
        printf("[wfring] js8 waterfall %dx%d: %d pixels checked (%d see-through), %d wrong\n", WFW, WFH, jchecks, holes,
               jbad);

        // A retune makes the main screen redraw the plane over the waterfall
        // in that pass; js8_wf_repaint_soon() puts it back in the next ones.
        main_redraw = true;
        ui_main_redraw_watch();
        jbad = jchecks = 0;
        ui_retune_by(1000);
        pump(100);
        jmodel.clear(); // the retune clears it
        jcheck("after a retune");
        printf("[wfring] js8 waterfall after the main screen's redraw: %d pixels checked, %d wrong\n", jchecks, jbad);

        // The plane's mapping (js8_wf.c: screen (x, y) is plane
        // (y, hor_res-1-x)) against LVGL's own LV_DISP_ROT_90, which turns
        // the radio's app plane: a second display set up like the radio's,
        // single pixels drawn on it, and where they land.
        {
            static lv_color_t         rbuf[480 * 800];
            static lv_disp_draw_buf_t rdb;
            static uint32_t           rphys[480 * 800];
            static lv_disp_drv_t      rdrv;
            lv_disp_draw_buf_init(&rdb, rbuf, nullptr, 480 * 800);
            lv_disp_drv_init(&rdrv);
            rdrv.hor_res   = 480;
            rdrv.ver_res   = 800;
            rdrv.sw_rotate = 1;
            rdrv.rotated   = LV_DISP_ROT_90;
            rdrv.draw_buf  = &rdb;
            rdrv.flush_cb  = [](lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
                for (int y = a->y1; y <= a->y2; y++)
                    for (int x = a->x1; x <= a->x2; x++) rphys[y * 480 + x] = (px++)->full;
                lv_disp_flush_ready(drv);
            };
            lv_disp_t *def = lv_disp_get_default();
            lv_disp_t *rd  = lv_disp_drv_register(&rdrv);
            lv_obj_t  *scr = lv_disp_get_scr_act(rd);
            lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
            lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
            int map_bad = 0;
            const int pts[][2] = {{wa.x1, wa.y1}, {wa.x2, wa.y2}, {wa.x1, wa.y2}, {wa.x2, wa.y1}, {0, 0}, {799, 479}, {123, 45}};
            for (auto &p : pts) {
                lv_obj_t *dot = lv_obj_create(scr);
                lv_obj_remove_style_all(dot);
                lv_obj_set_style_bg_color(dot, lv_color_hex(0xff0000), 0);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
                lv_obj_set_size(dot, 1, 1);
                lv_obj_set_pos(dot, p[0], p[1]);
                lv_refr_now(rd);
                int fx = -1, fy = -1, n = 0;
                for (int i = 0; i < 480 * 800; i++)
                    if ((rphys[i] & 0xffffff) == 0xff0000) fx = i % 480, fy = i / 480, n++;
                int wx = p[1], wy = lv_disp_get_hor_res(rd) - 1 - p[0];
                if (n != 1 || fx != wx || fy != wy) {
                    map_bad++;
                    printf("[wfring] FAIL: screen %d,%d landed at plane %d,%d (%d px), js8_wf puts it at %d,%d\n", p[0], p[1],
                           fx, fy, n, wx, wy);
                }
                lv_obj_del(dot);
                lv_refr_now(rd);
            }
            printf("[wfring] plane mapping vs LVGL's rotation: %d points, %d wrong\n", (int)(sizeof(pts) / sizeof(pts[0])),
                   map_bad);
            lv_disp_remove(rd);
            lv_disp_set_default(def);
        }
        return 0;
    }
    if (getenv("ONLY_WFCALM")) {
        // Waterfall: Sharp / Light / Medium / Calm with live noise: how much
        // the picture changes at each step (every pixel takes the value of
        // the one above it), the change that makes the screen dim for a
        // moment on every row. Mean |luma difference| between consecutive rows.
        static const char *names[] = {"sharp ", "light ", "medium", "calm  "};
        pump(300);
        double change[4] = {0, 0, 0, 0};
        for (int level = 0; level < 4; level++) {
            dialog_js8_wf_avg(level);
            load_measure(names[level], 2000, true); // the average settles
            calm_rows.clear();
            calm_mode = true;
            load_measure(names[level], 8000, true);
            calm_mode = false;
            double sum = 0, lum = 0;
            long   n   = 0;
            for (size_t i = 1; i < calm_rows.size(); i++)
                for (size_t x = 0; x < calm_rows[i].size(); x++) {
                    sum += fabs(calm_rows[i][x] - calm_rows[i - 1][x]);
                    lum += calm_rows[i][x];
                    n++;
                }
            change[level] = n ? sum / n : 0;
            printf("[wfcalm] %s: %zu rows, mean luma %.1f, change per step %.2f (%.1fx less than sharp)\n", names[level],
                   calm_rows.size(), n ? lum / n : 0, change[level], change[level] > 0 ? change[0] / change[level] : 0);
        }
        dialog_js8_wf_avg(3);
        return 0;
    }
    if (getenv("ONLY_WFTIME")) {
        // Row presentation times with live audio: how even is the scroll?
        pump(300);
        wftime_mode = true;
        feed_band({{"W1ABC", "FN42", "", "@POTA ACTIVATING CA-1234", 1300, 0.05f},
                   {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1800, 0.05f}});
        wftime_mode = false;
        Stat   iv;
        double prev = 0;
        for (double t : wftime_stamps) {
            if (prev) iv.add(t - prev);
            prev = t;
        }
        double m = iv.mean(), var = 0;
        for (double x : iv.v) var += (x - m) * (x - m);
        int off = 0; // steps more than 20% away from the mean interval
        for (double x : iv.v) off += fabs(x - m) > 0.2 * m;
        if (getenv("WFTIME_GAPS"))
            for (size_t i = 1; i < wftime_stamps.size(); i++)
                if (wftime_stamps[i] - wftime_stamps[i - 1] > 200)
                    printf("[wftime] gap %.0f ms at row %zu, %.1f s in\n", wftime_stamps[i] - wftime_stamps[i - 1], i,
                           (wftime_stamps[i] - wftime_stamps[0]) / 1000);
        printf("[wftime] %zu rows: interval mean %.1f ms, sd %.1f, min %.1f, p5 %.1f, p95 %.1f, max %.1f; %d uneven (>20%%)\n",
               iv.v.size() + 1, m, sqrt(var / iv.v.size()), iv.pct(0), iv.pct(0.05), iv.pct(0.95), iv.pct(1), off);
        return 0;
    }
    if (getenv("ONLY_KEYS")) {
        // Package 5: the VOL knob in every popup (B-19); the keyboard with
        // no callsign, and no mode left behind (B-16); every character JS8
        // sends (B-22); a too-long message says so as you type (B-23).
        pump(300);
        struct {
            int         page, button;
            const char *name;
        } pops[] = {{3, 4, "Inbox"}, {4, 4, "Settings"}, {5, 1, "APRS"}, {6, 1, "Alerts"}, {0, 0, "Freq"}};
        int ok = 0; // (the Query list and the message list had it already)
        for (auto &p : pops) {
            if (p.page) {
                ui_page(p.page);
                ui_press(p.button);
                pump(200);
            } else {
                open_freq(); // Settings > Frequencies (page 6's Freq before)
            }
            if (ui_focus_is_table()) { // didn't open: ESC would close JS8
                printf("[keys] %s didn't open\n", p.name);
                continue;
            }
            int before = stub_vol_turns;
            ui_vol(+1);
            ui_vol(-1);
            printf("[keys] VOL knob in %-8s %d turns (want 2)\n", p.name, stub_vol_turns - before);
            ok += stub_vol_turns - before == 2;
            ui_key(LV_KEY_ESC);
            pump(200);
        }
        printf("[keys] VOL in popups: %d of 5 (want 5)\n", ok);

        ui_set_callsign("");
        ui_page(2);
        ui_press(3); // Send... needs a callsign
        pump(200);
        printf("[keys] Send... with no callsign: %s (want (no compose window))\n", ui_compose_placeholder());
        open_freq();
        for (int i = 0; i < 6 && !strstr(ui_focused_text(), "Custom"); i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Custom kHz...
        pump(200);
        printf("[keys] custom frequency with no callsign: '%s' (want the kHz box)\n", ui_compose_placeholder());
        ui_compose_cancel();
        pump(200);
        ui_set_callsign("K2XYZ");
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        printf("[keys] Send... after setting the callsign: '%s' (want the message box)\n", ui_compose_placeholder());

        ui_compose_append("COSTS $5, 50% <OK> [A] ^|~\\`");
        printf("[keys] typed: '%s' (want all of it)\n", ui_compose_text());
        ui_compose_clear();
        std::string lots(150, '~');
        ui_compose_append(lots.c_str());
        pump(100);
        printf("[keys] too long: '%s' (want JS8: too long: ...)\n", stub_last_msg);
        ui_compose_cancel();
        pump(200);
        return 0;
    }
    if (getenv("ONLY_LOGPEND")) {
        // B-24 (your choice): ESC on the log prompt means "not now"; Log
        // QSO then takes the selected station. A typed grid must be one
        // (BH-S3), in capitals.
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        ui_compose_append("N0XYZ SNR -10");
        ui_compose_enter();
        wait_tx();
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ SNR -08 TNX", 1320, 0.05f}});
        ui_press(3);
        pump(200);
        ui_compose_append("N0XYZ TU 73");
        ui_compose_enter();
        wait_tx();
        printf("[logpend] prompt for N0XYZ: %d\n", ui_popup_has("N0XYZ") == 1);
        ui_key(LV_KEY_ESC); // not now
        pump(300);
        feed_band({{"W1ABC", "FN42", "", "W1ABC: @HB HEARTBEAT FN42", 900, 0.05f}});
        ui_page(2);
        ui_press(1); // Show: Directed
        ui_press(1); // All
        pump(200);
        ui_select_row_from("W1ABC");
        ui_page(5);
        ui_press(2); // Log QSO
        pump(300);
        printf("[logpend] W1ABC selected, Log QSO opens W1ABC %d, N0XYZ %d (want 1, 0)\n", ui_popup_has("W1ABC") == 1,
               ui_popup_has("N0XYZ") == 1);
        // A typed grid: "HOME" refused, "cn89kg" kept as CN89KG.
        for (int i = 0; i < 6 && !strstr(ui_focused_text(), "Grid"); i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(200);
        ui_compose_clear();
        ui_compose_append("HOME");
        ui_compose_enter();
        pump(200);
        printf("[logpend] grid HOME: '%s', still typing %d (want Not a grid..., 1)\n", stub_last_msg,
               strcmp(ui_compose_placeholder(), "(no compose window)") != 0);
        ui_compose_clear();
        ui_compose_append("cn89kg");
        ui_compose_enter();
        pump(300);
        printf("[logpend] grid cn89kg: shown as CN89KG %d\n", ui_popup_has("Grid: CN89KG") == 1);
        ui_key(LV_KEY_ESC);
        pump(300);
        ui_select_row_from("N0XYZ"); // the ended QSO is still there
        ui_page(5);
        ui_press(2);
        pump(300);
        printf("[logpend] N0XYZ selected: Log QSO opens N0XYZ %d\n", ui_popup_has("N0XYZ") == 1);
        ui_key(LV_KEY_ESC);
        pump(300);
        return 0;
    }
    if (getenv("ONLY_ROWS")) {
        // Info rows survive a rebuild (bug hunt 18); a message cut off by a
        // band change stops showing " ..." (B-12); the Inbox lists all
        // messages, the oldest unread one too (bug hunt 9).
        pump(300);
        std::vector<Station> st = {
            {"VE7ABC", "CN89", "W1XYZ", "W1XYZ THIS IS A LONG MESSAGE THAT TAKES SEVERAL FRAMES TO ARRIVE", 1320, 0.05f}};
        feed_band(st, 0, 2);
        printf("[rows] growing: %d\n", ui_list_has(" ...") == 1);
        ui_band_up();
        pump(500);
        printf("[rows] after a band change: still growing %d, the text so far %d (want 0, 1)\n", ui_list_has(" ...") == 1,
               ui_list_has("VE7ABC: W1XYZ THIS") == 1);
        ui_page(1);
        ui_hold(2);  // auto HB: one now, and "Auto HB on" is an info row
        ui_press(2); // done setting the minutes
        pump(300);
        int before = ui_list_has("Auto HB on");
        ui_page(2);
        ui_press(1); // Show: Directed (the list is rebuilt)
        ui_press(1); // All
        ui_press(1); // No HB
        pump(300);
        printf("[rows] info row before %d, after three rebuilds %d (want 1, 1)\n", before, ui_list_has("Auto HB on"));
        ui_key(LV_KEY_ESC); // stop the heartbeat
        pump(500);
        ui_page(3);
        ui_press(4); // Inbox
        pump(300);
        printf("[rows] Inbox: the oldest unread listed %d, opens on the newest unread %d (want 1, 1)\n",
               ui_popup_has("THE OLDEST UNREAD") == 1, strstr(ui_focused_text(), "A NEWER UNREAD") != nullptr);
        ui_key(LV_KEY_ESC);
        pump(300);
        return 0;
    }
    if (getenv("ONLY_NEWSTN")) {
        // B-20: "New station" once per band per power-on, not again each
        // time a regular comes back after dropping off the Stations list
        // (an hour after they were last heard).
        pump(300);
        ui_set_alerts(0x01 | 0x10); // beep, new station
        feed_band({{"VA7XYZ", "CN89", "", "@HB HEARTBEAT CN89", 900, 0.05f}});
        printf("[newstn] first heard: alerts %d (want 1)\n", stub_new_station_alerts);
        dialog_js8_time_auto(false);      // Auto would put an hour's drift back within a slot
        js8_set_drift_ms(61 * 60 * 1000); // an hour on (whole slots): off the list
        pump(1500);
        feed_band({{"VA7XYZ", "CN89", "", "@HB HEARTBEAT CN89", 900, 0.05f},
                   {"W7NEW", "DM43", "", "@HB HEARTBEAT DM43", 1400, 0.05f}});
        ui_page(2);
        ui_press(1); // Show: Directed
        ui_press(1); // All: heartbeats shown
        pump(300);
        printf("[newstn] an hour later: VA7XYZ heard %d, W7NEW heard %d, alerts %d (want 1, 1, 2: only W7NEW new)\n",
               ui_list_count("VA7XYZ: @HB") == 2, ui_list_has("W7NEW: @HB") == 1, stub_new_station_alerts);
        js8_set_drift_ms(0);
        dialog_js8_time_auto(true);
        return 0;
    }
    if (getenv("ONLY_STALL")) {
        // B-25: the GUI thread stuck for a whole band's worth of messages
        // (~50 s): every decode must still reach the list afterwards. The
        // shared scheduler queue held 64 items and waterfall rows alone
        // were 15 a second, so this lost messages.
        pump(300);
        std::vector<Station> band = {{"W1ABC", "FN42", "", "W1ABC: HEARTBEAT FN42", 600, 0.05f},
                                     {"VE3KP", "FN03", "", "CQ CQ CQ FN03", 900, 0.05f},
                                     {"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO FROM THE X6100 TEST", 1320, 0.05f},
                                     {"G4ABC", "IO91", "", "@ALLCALL ANYONE ON THE BAND FOR A CHAT", 1760, 0.05f},
                                     {"KN4CRD", "EM73", "K2XYZ", "K2XYZ MSG STORED MESSAGE FOR YOU", 2150, 0.05f},
                                     {"DL1XX", "JO62", "", "DL1XX: HEARTBEAT JO62", 2380, 0.05f}};
        feed_stalled = true;
        feed_band(band); // the GUI runs again only at the end
        feed_stalled = false;
        pump(3000);
        ui_page(2);
        ui_press(1); // Show: Directed
        ui_press(1); // All (heartbeats too)
        pump(300);
        int got = 0;
        for (const char *want : {"W1ABC: @HB HEARTBEAT", "VE3KP: @ALLCALL CQ CQ CQ", "N0XYZ: K2XYZ HELLO FROM THE X6100 TEST",
                                 "G4ABC: @ALLCALL ANYONE ON THE BAND FOR A CHAT", "KN4CRD: K2XYZ MSG STORED MESSAGE",
                                 "DL1XX: @HB HEARTBEAT"}) {
            int has = ui_list_has(want) == 1;
            got += has;
            printf("[stall] %-46s %s\n", want, has ? "in the list" : "LOST");
        }
        printf("[stall] after a ~50 s GUI stall: %d of 6 messages in the list (want 6)\n", got);
        if (got < 6) {
            lv_obj_t *t = find_obj(lv_scr_act(), &lv_table_class);
            for (uint16_t r = 0; t && r < lv_table_get_row_cnt(t); r++)
                printf("[stall] row %2u: %s\n", r, lv_table_get_cell_value(t, r, 0));
        }
        return 0;
    }
    if (getenv("ONLY_LOAD")) {
        // What the GUI thread does per second while JS8 sits there (package 4,
        // I-14/I-15): idle, then with waterfall rows, empty and full list.
        // Build without sanitizers (build-perf) for meaningful numbers.
        int secs = getenv("LOAD_S") ? atoi(getenv("LOAD_S")) : 10;
        pump(1000);
        load_measure("idle, empty list", secs * 1000, false);
        load_measure("waterfall rows, empty list", secs * 1000, true);
        feed_band({{"W1ABC", "FN42", "", "@POTA ACTIVATING CA-1234", 1300, 0.05f},
                   {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1800, 0.05f},
                   {"VE7ABC", "CN89", "", "K2XYZ HELLO THERE", 900, 0.05f},
                   {"N0XYZ", "EN34", "", "CQ CQ CQ EN34", 1500, 0.05f},
                   {"KK7RFI", "DN17", "", "@HB HEARTBEAT DN17", 2200, 0.05f},
                   {"W7ABC", "CN85", "", "K2XYZ SNR -05", 2600, 0.05f}});
        feed_band({{"G4ABC", "IO91", "", "@ALLCALL ANYONE ON THE BAND FOR A CHAT TONIGHT", 1100, 0.05f},
                   {"KN4CRD", "EM73", "", "K2XYZ MSG STORED MESSAGE FOR YOU", 2000, 0.05f},
                   {"DL1XX", "JO62", "", "DL1XX: HEARTBEAT JO62", 700, 0.05f}});
        ui_page(2);
        ui_press(1); // Show: Directed
        ui_press(1); // All
        pump(500);
        printf("[load] list rows: %d\n", ui_list_count(":"));
        load_measure("idle, full list", secs * 1000, false);
        load_measure("waterfall rows, full list", secs * 1000, true);
        ui_page(3);
        ui_press(3); // Show Stations
        pump(500);
        load_measure("waterfall rows, Stations view", secs * 1000, true);
        return 0;
    }
    if (getenv("ONLY_KNOB")) {
        // The main knob moves the TX offset. R1CBU 1.0 hands JS8 every click
        // on its own (0.34 summed them per 30 ms read): a fast spin must
        // still speed up, and must not redraw the whole waterfall per click.
        pump(1000);
        auto spin = [&](const char *label, int clicks, int gap_ms, int dir) {
            int  f0 = ui_tx_offset();
            long px0 = load_flush_px, fl0 = load_flushes;
            double busy = 0;
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < clicks; i++) {
                double a = now_ms_f();
                ui_rotary(dir);
                observer_delayed_drain();
                scheduler_work();
                lv_timer_handler();
                busy += now_ms_f() - a;
                lv_tick_inc(gap_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(gap_ms));
            }
            double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            printf("[knob] %-28s %3d clicks: offset %d -> %d (%+d Hz), GUI busy %5.1f ms/s, %6.0f kpx/s, %4.1f flushes/s\n",
                   label, clicks, f0, ui_tx_offset(), ui_tx_offset() - f0, busy / s, (load_flush_px - px0) / 1000.0 / s,
                   (load_flushes - fl0) / s);
        };
        spin("slow (one click / 150 ms)", 10, 150, +1);
        pump(500);
        spin("medium (one click / 25 ms)", 40, 25, +1);
        pump(500);
        spin("fast (one click / 5 ms)", 120, 5, -1);
        // No band left behind: what the partial redraws left on screen must
        // be what a full redraw gives.
        pump(300);
        screenshot("c1_knob_partial.ppm");
        lv_obj_invalidate(lv_scr_act());
        pump(300);
        screenshot("c2_knob_full.ppm");

        // The MFK moves the selection, and with it the green cursor band.
        feed_band({{"W1ABC", "FN42", "", "CQ CQ CQ FN42", 900, 0.05f},
                   {"K9DEF", "EN52", "", "CQ CQ CQ EN52", 1800, 0.05f},
                   {"VE7ABC", "CN89", "", "CQ CQ CQ CN89", 2500, 0.05f}});
        const char *calls[] = {"W1ABC", "K9DEF", "VE7ABC", "K9DEF", "W1ABC"};
        long px0 = load_flush_px;
        for (const char *c : calls) {
            ui_select_row_from(c);
            pump(200);
        }
        printf("[knob] 5 selections: %ld kpx to the screen\n", (load_flush_px - px0) / 1000);
        // The MFK itself, a click at a time through the stations: each step
        // redraws the two rows it moves between (and the rows whose mark
        // changes), not the whole list.
        ui_indevs_init(); // the MFK
        pump(100);
        px0 = load_flush_px;
        for (int i = 0; i < 8; i++) {
            ui_mfk_turn(i < 4 ? -1 : 1);
            pump(100);
            char sel[32] = "";
            dialog_js8_selected_call(sel, sizeof(sel));
            printf("[knob] MFK step %d: %s, %ld kpx so far\n", i, sel, (load_flush_px - px0) / 1000);
        }
        printf("[knob] 8 MFK steps: %ld kpx to the screen\n", (load_flush_px - px0) / 1000);
        screenshot("c3_select_partial.ppm");
        lv_obj_invalidate(lv_scr_act());
        pump(300);
        screenshot("c4_select_full.ppm");
        return 0;
    }
    if (getenv("ONLY_WFPERF")) {
        // A full list over the waterfall, then rows as fast as they render.
        pump(300);
        feed_band({{"W1ABC", "FN42", "", "@POTA ACTIVATING CA-1234", 1300, 0.05f},
                   {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1800, 0.05f},
                   {"VE7ABC", "CN89", "", "K2XYZ HELLO THERE", 900, 0.05f},
                   {"N0XYZ", "EN34", "", "CQ CQ CQ EN34", 1500, 0.05f},
                   {"KK7RFI", "DN17", "", "@HB HEARTBEAT DN17", 2200, 0.05f},
                   {"W7ABC", "CN85", "", "K2XYZ SNR -05", 2600, 0.05f}});
        ui_page(2);
        ui_press(1); // Show: Directed
        ui_press(1); // All
        pump(500);
        screenshot("wfperf_before.ppm");
        if (getenv("WFPERF_PROFILE")) { // one case, long enough for a profile
            wf_bench(getenv("WFPERF_PROFILE"), 4000);
            return 0;
        }
        wf_bench("rows on the lower plane, full list", 300);
        screenshot("wfperf_after.ppm");
        return 0;
    }
    if (getenv("ONLY_APRS")) {
        // Until a queued message has been keyed and nothing more follows.
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        pump(300);
        ui_page(5);
        ui_press(1); // APRS >
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_click_focused(); // Spot my grid (second item): a message box
        pump(300);
        printf("[aprs] beacon box: '%s' focus %s\n", ui_compose_text(), ui_focus_desc());
        ui_compose_enter(); // no message: just the grid
        wait_tx();
        printf("[aprs] grid spot sent: %d\n", ui_list_has("@APRSIS GRID FN42"));

        // With a message: a position report carrying it.
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_click_focused();
        pump(300);
        ui_compose_append("MADE IT TO CAMP");
        ui_compose_enter();
        wait_tx();
        printf("[aprs] grid + message sent: %d\n", ui_list_has("@APRSIS CMD =4203.8 N/07157.5 WG MADE IT TO CAMP"));

        // Spot GPS position (second item): no fix -> message only.
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_key(LV_KEY_RIGHT);
        printf("[aprs] item 2: '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        setenv("HARNESS_GPS", "49.2827,-123.1207", 1);
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        ui_compose_enter();
        wait_tx();
        printf("[aprs] GPS spot sent: %d (want @APRSIS GRID CN89KG + 4)\n", ui_list_has("@APRSIS GRID CN89KG"));
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        ui_compose_append("made it to camp"); // typed in lower case
        ui_compose_enter();
        wait_tx();
        printf("[aprs] GPS + message sent: %d\n", ui_list_has("@APRSIS CMD =4917.0 N/12307.2 WG MADE IT TO CAMP"));
        // Cancel: nothing goes.
        int frames = stub_tx_frames;
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_click_focused();
        pump(300);
        ui_compose_cancel();
        pump(2000);
        printf("[aprs] cancelled beacon, nothing sent: %d\n", stub_tx_frames == frames);

        // A list stays exclusive: another bottom button only closes it.
        ui_press(1);
        pump(200);
        ui_page(4);
        printf("[aprs] after changing page, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");
        ui_page(5);
        ui_press(1);
        pump(200);
        ui_page(5);
        ui_press(0); // page button: also closes it
        pump(300);
        ui_page(5);

        ui_press(1); // APRS >
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        printf("[aprs] list open, focused '%s'\n", ui_focused_text());
        for (int i = 0; i < 4; i++) ui_key(LV_KEY_RIGHT);
        printf("[aprs] after 4 steps: '%s' (want SMS text)\n", ui_focused_text());
        ui_click_focused(); // SMS text
        pump(300);
        printf("[aprs] SMS prefill: '%s' focus: %s\n", ui_compose_text(), ui_focus_desc());
        ui_compose_append("6045551234 HELLO FROM THE X6100");
        ui_compose_enter();
        wait_tx();
        printf("[aprs] SMS sent with an ID: %d\n", ui_list_has(":@6045551234 HELLO FROM THE X6100{"));

        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_key(LV_KEY_RIGHT);
        ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // POTA spot: the form
        pump(300);
        printf("[spot] form: %d, focused '%s' (want Park, none yet)\n", ui_popup_has("POTA spot via APSPOT"),
               ui_focused_text());
        ui_click_focused(); // Park -> keyboard
        pump(200);
        ui_compose_append("CA-1234");
        ui_compose_enter();
        pump(300);
        printf("[spot] back on '%s'\n", ui_focused_text());
        printf("[spot] preview: %d\n", ui_popup_has("APSPOT: ! POTA CA-1234 14.078 DATA JS8"));
        screenshot("23_aprs_pota.ppm");
        ui_key(LV_KEY_LEFT);
        ui_click_focused(); // Send spot
        wait_tx();
        printf("[spot] POTA sent: %d\n", ui_list_has("APSPOT   :! POTA CA-1234 14.078 DATA JS8"));

        // Again: the park is remembered, focus on Send. Your SSB run instead.
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        ui_key(LV_KEY_RIGHT);
        ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        printf("[spot] again: focused '%s' (want Send spot)\n", ui_focused_text());
        for (int i = 0; i < 2; i++) ui_key(LV_KEY_RIGHT);
        printf("[spot] on '%s'\n", ui_focused_text());
        ui_click_focused(); // Frequency -> keyboard
        pump(200);
        ui_compose_append("14285");
        ui_compose_enter();
        pump(300);
        printf("[spot] back on '%s'\n", ui_focused_text());
        ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Mode: DATA -> SSB
        pump(100);
        printf("[spot] '%s', preview SSB: %d\n", ui_focused_text(), ui_popup_has("APSPOT: ! POTA CA-1234 14.285 SSB"));
        screenshot("24_aprs_pota_ssb.ppm");
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_LEFT);
        ui_click_focused(); // Send spot
        wait_tx();
        printf("[spot] SSB sent: %d, no JS8 comment: %d\n", ui_list_has("APSPOT   :! POTA CA-1234 14.285 SSB"),
               !ui_list_has("14.285 SSB JS8"));

        // SOTA: its own summit, the same frequency and mode.
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // SOTA spot
        pump(300);
        printf("[spot] SOTA form: %d, focused '%s'\n", ui_popup_has("SOTA spot via APRS2SOTA"), ui_focused_text());
        ui_click_focused();
        pump(200);
        ui_compose_append("VE7/LM-001");
        ui_compose_enter();
        pump(300);
        // Frequency back to the JS8 dial (clear it, Enter): DATA, "JS8".
        ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(200);
        printf("[spot] frequency box: '%s' (want 14285, the last typed)\n", ui_compose_text());
        ui_compose_clear();
        ui_compose_enter();
        pump(300);
        printf("[spot] back on '%s' (want JS8 dial)\n", ui_focused_text());
        ui_key(LV_KEY_RIGHT);
        for (int i = 0; i < 5; i++) ui_click_focused(); // SSB -> CW FM AM DV DATA
        pump(100);
        printf("[spot] SOTA: '%s'\n", ui_focused_text());
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_LEFT);
        ui_click_focused(); // Send spot
        wait_tx();
        printf("[spot] SOTA sent: %d\n", ui_list_has("APRS2SOTA:VE7/LM-001 14.078 DATA K2XYZ JS8"));

        // A bad frequency keeps the keyboard open.
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        for (int i = 0; i < 2; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Frequency
        pump(200);
        ui_compose_clear();
        ui_compose_append("1"); // 1 MHz: below 160m
        ui_compose_enter();
        pump(200);
        printf("[spot] bad frequency, keyboard open: '%s'\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);
        printf("[spot] cancelled, back on '%s'\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(300);
        printf("[spot] ESC closed it: %d\n", ui_focus_is_table());

        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT); // past Echo test (first item)
        for (int i = 0; i < 5; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Email
        pump(300);
        ui_compose_append("SOMEONE@EXAMPLE.COM THIS MESSAGE IS FAR TOO LONG FOR ONE APRS PACKET SORRY");
        ui_compose_enter();
        pump(300);
        printf("[aprs] too long, compose still open: %s\n", ui_focus_desc());
        ui_compose_cancel();
        pump(300);

        ui_press(1);
        pump(200);
        screenshot("24b_aprs_top.ppm"); // the list at the top, with what was sent behind it
        ui_key(LV_KEY_LEFT);
        printf("[aprs] one step back: '%s'\n", ui_focused_text());
        screenshot("24_aprs_list.ppm");
        ui_click_focused();
        pump(300);
        printf("[aprs] after Close: %s\n", ui_focus_desc());
        return 0;
    }
    if (getenv("ONLY_APRSMORE")) {
        // Echo test first in APRS >; More services > (before Close) opens
        // the services list, each message filled in; Back and Close.
        pump(300);
        ui_page(5);
        ui_press(1); // APRS >
        pump(200);
        printf("[aprsmore] list open, focused '%s' (want Echo test)\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[aprsmore] echo prefill '%s' (want @APRSIS CMD :ECHO     :TEST)\n", ui_compose_text());
        printf("[aprsmore] hint '%s'\n", stub_last_msg);
        int frames = stub_tx_frames;
        ui_compose_enter();
        for (int i = 0; i < 200 && stub_tx_frames == frames; i++) pump(100);
        for (int i = 0; i < 400; i++) pump(100);
        printf("[aprsmore] echo sent: %d\n", ui_list_has("@APRSIS CMD :ECHO     :TEST"));

        ui_press(1);
        pump(200);
        ui_key(LV_KEY_LEFT); // wraps to Close
        ui_key(LV_KEY_LEFT);
        printf("[aprsmore] before Close: '%s' (want More services >)\n", ui_focused_text());
        screenshot("24c_aprs_more_item.ppm");
        ui_click_focused();
        pump(300);
        printf("[aprsmore] services open, focused '%s' (want Weather today)\n", ui_focused_text());
        screenshot("24d_aprs_services.ppm");
        ui_click_focused();
        pump(300);
        printf("[aprsmore] weather prefill '%s' (want MPAD grid <grid> today)\n", ui_compose_text());
        printf("[aprsmore] hint '%s'\n", stub_last_msg);
        ui_compose_cancel();
        pump(300);
        printf("[aprsmore] cancelled, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");

        // Every service: its prefill, cancelled.
        for (int n = 1; n < 16; n++) {
            ui_press(1);
            pump(200);
            ui_key(LV_KEY_LEFT);
            ui_key(LV_KEY_LEFT);
            ui_click_focused(); // More services >
            pump(300);
            for (int i = 0; i < n; i++) ui_key(LV_KEY_RIGHT);
            std::string label = ui_focused_text();
            ui_click_focused();
            pump(300);
            printf("[aprsmore] %-20s '%s'\n", label.c_str(), ui_compose_text());
            ui_compose_cancel();
            pump(300);
        }

        ui_press(1);
        pump(200);
        ui_key(LV_KEY_LEFT);
        ui_key(LV_KEY_LEFT);
        ui_click_focused(); // More services >
        pump(300);
        ui_key(LV_KEY_LEFT); // wraps to Close
        ui_key(LV_KEY_LEFT);
        printf("[aprsmore] before Close: '%s' (want < Back)\n", ui_focused_text());
        screenshot("24e_aprs_services_end.ppm");
        ui_click_focused();
        pump(300);
        printf("[aprsmore] Back: focused '%s' (want Echo test)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(300);
        printf("[aprsmore] ESC closed it: %d\n", ui_focus_is_table());
        return 0;
    }
    if (getenv("ONLY_BOOTHB")) {
        // The first open after the radio was switched off with auto
        // heartbeats on: JS8 turns them off as it opens, and page 1's
        // Heartbeat button must say so (it used to show "HB auto: soon",
        // drawn before JS8 set its state, until the page changed).
        pump(300);
        std::string shown = ui_button_shown(2), now = ui_button_label(2);
        for (auto *s : {&shown, &now})
            for (auto &c : *s) c = c == '\n' ? ' ' : c;
        printf("[boothb] Heartbeat shows '%s', is '%s' (want both Heart- beat)\n", shown.c_str(), now.c_str());
        printf("[boothb] CQ shows '%s'\n", ui_button_shown(1));
        return 0;
    }
    if (getenv("ONLY_HISTORY")) {
        // The station history (step 1: recording only): a QSO with N0XYZ,
        // W1ABC's heartbeat ACK to us (kept, not a QSO) and its INFO to
        // someone else, a stranger's heartbeat (not kept).
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        js8_history_t *h = dialog_js8_history();
        printf("[history] open: %s\n", h ? "yes" : "NO");
        if (!h) return 1;
        pump(300);
        // W1ABC's heartbeat first: its grid, before any exchange.
        feed_band({{"W1ABC", "FN42", "@HB", "@HB HEARTBEAT FN42", 700, 0.05f}});
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO FROM THE PARK", 1320, 0.05f},
                   {"W1ABC", "FN42", "K2XYZ", "K2XYZ HEARTBEAT SNR -12", 700, 0.05f},
                   {"VE3KP", "FN03", "@HB", "@HB HEARTBEAT FN03", 2400, 0.05f}});
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        ui_compose_append("N0XYZ SNR -10");
        ui_compose_enter();
        wait_tx();
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ SNR -08 TNX 73", 1320, 0.05f},
                   {"W1ABC", "FN42", "N0XYZ", "N0XYZ INFO IC-705 5W EFHW", 700, 0.05f}});
        pump(500);
        // In a long QSO the calls get dropped: N0XYZ's free text, a message
        // whose first frame (the sender) was missed, then ours with N0XYZ
        // selected. All three join the QSO.
        feed_band({{"N0XYZ", "EN34", "", "NAME IS BOB QTH OMAHA", 1320, 0.05f}});
        feed_band({{"N0XYZ", "EN34", "", "RIG IS AN IC-7300 AT 100W INTO A DIPOLE UP 40 FEET", 1320, 0.05f}}, 1);
        pump(500);
        if (ui_popup_has("Save") > 0) ui_key(LV_KEY_ESC); // their 73 offered the log: not now
        pump(300);
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        ui_compose_append("GOOD COPY BOB");
        ui_compose_enter();
        wait_tx();
        js8_history_flush(h);

        static js8_hist_contact_t c[16];
        int nc = js8_history_contacts(h, "20m", c, 16);
        printf("[history] 20m contacts: %d (want 2: N0XYZ, W1ABC with FN42 from its heartbeat; not VE3KP)\n", nc);
        for (int i = 0; i < nc; i++)
            printf("[history]   %-6s grid %-6s snr %+d heard us %s reported %s%+d\n", c[i].call, c[i].grid, c[i].snr,
                   c[i].heard_us_ms ? "yes" : "no", c[i].has_reported_snr ? "" : "(none) ",
                   c[i].has_reported_snr ? c[i].reported_snr : 0);
        static js8_hist_qso_t q[8];
        int nq = js8_history_qsos(h, "N0XYZ", q, 8);
        printf("[history] N0XYZ QSOs: %d (want 1)\n", nq);
        for (int i = 0; i < nq; i++) {
            printf("[history]   %s, %d lines, logged %d\n", q[i].band, q[i].lines, q[i].logged);
            static js8_hist_line_t l[16];
            int nl = js8_history_lines(h, q[i].id, l, 16);
            for (int k = 0; k < nl; k++) printf("[history]     %s %s\n", l[k].tx ? "TX" : "RX", l[k].text);
        }
        printf("[history] W1ABC QSOs: %d (want 0: a heartbeat ACK only)\n", js8_history_qsos(h, "W1ABC", q, 8));
        js8_hist_info_t info;
        bool have = js8_history_info(h, "W1ABC", 0, &info);
        printf("[history] W1ABC INFO: %s%s%s\n", have ? info.text : "(none)", have ? " to " : "", have ? info.to : "");
        unsigned rows = 0, commits = 0, failed = 0;
        int64_t  busy = 0;
        js8_history_stats(h, &rows, &commits, &failed, &busy);
        printf("[history] written: %u rows in %u transactions, %u failed\n", rows, commits, failed);

        // N0XYZ's STATUS to us, and the screens (steps 2 and 3).
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ STATUS QRV AT THE PARK TILL 2200Z", 1320, 0.05f}});
        pump(500);
        auto rows_now = [&]() {
            static char buf[256];
            dialog_js8_station_rows(buf, sizeof(buf));
            return (const char *)buf;
        };
        auto mfk = [](int ms) {
            ui_mfk_set(true);
            pump(ms);
            ui_mfk_set(false);
            pump(300);
        };
        ui_indevs_init(); // the MFK
        ui_page(3);
        printf("[history] page 3 button 2 over the messages: '%s' (want Show History)\n", ui_button_label(2));
        ui_press(2); // Show History: the Stations view, All time
        pump(400);
        printf("[history] all time: '%s' / '%s', rows %s (want N0XYZ W1ABC, not VE3KP)\n", ui_button_label(2),
               ui_button_label(3), rows_now());
        printf("[history] message: '%s'\n", stub_last_msg);
        screenshot("x0_history_all_time.ppm");
        ui_press(2); // Heard: Recent
        pump(400);
        printf("[history] recent: '%s', rows %s (want VE3KP too)\n", ui_button_label(2), rows_now());
        ui_press(2); // back to All time
        pump(400);

        // The MFK onto N0XYZ, a short press: its History page.
        for (int i = 0; i < 4 && !strstr(rows_now(), ">N0XYZ"); i++) {
            ui_mfk_turn(1);
            pump(300);
        }
        printf("[history] cursor: %s\n", rows_now());
        mfk(120);
        ui_popup_print("[history] page:");
        screenshot("x1_history_page.ppm");
        ui_click_focused(); // the newest QSO
        pump(400);
        ui_popup_print("[history] qso:");
        screenshot("x2_history_qso.ppm");
        for (int i = 0; i < 20 && strcmp(ui_focused_text(), "< Back"); i++) ui_key(LV_KEY_DOWN);
        ui_click_focused(); // < Back
        pump(400);
        printf("[history] back on: '%s' (want the QSO)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(300);
        printf("[history] after ESC, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");

        // W1ABC: INFO only, a heartbeat ACK only.
        for (int i = 0; i < 4 && !strstr(rows_now(), ">W1ABC"); i++) {
            ui_mfk_turn(1);
            pump(300);
        }
        mfk(120);
        ui_popup_print("[history] W1ABC page:");
        screenshot("x3_history_w1abc.ppm");
        ui_key(LV_KEY_ESC);
        pump(300);
        // A hold still locks, no page.
        mfk(1500);
        printf("[history] after a hold: popup %d (want -1), msg '%s'\n", ui_popup_has("Close"), stub_last_msg);
        mfk(1500); // unlock
        // The map in All time.
        ui_page(3);
        ui_press(3); // Show Map
        pump(800);
        printf("[history] map: '%s', stats '%s'\n", ui_button_label(2), dialog_js8_map_stats());
        screenshot("x4_history_map.ppm");
        ui_press(3); // Show Messages: All time ends
        pump(300);
        printf("[history] messages again: '%s' (want Show History)\n", ui_button_label(2));

        // Settings > Clear station history: one press arms it, it disarms
        // after 5 s, two presses clear it.
        ui_page(4);
        ui_press(4); // Settings
        pump(200);
        for (int i = 0; i < 20 && !strstr(ui_focused_text(), "station history"); i++) ui_key(LV_KEY_RIGHT);
        printf("[history] settings: '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[history] one press: '%s' / '%s'\n", ui_focused_text(), stub_last_msg);
        screenshot("x5_history_clear.ppm");
        pump(5500);
        printf("[history] 5 s later: '%s' (want Clear station history...)\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        ui_click_focused();
        pump(500);
        printf("[history] two presses: '%s', stations left %d (want 0)\n", stub_last_msg,
               js8_history_station_count(h));
        ui_key(LV_KEY_ESC);
        pump(200);
        return 0;
    }
    if (getenv("ONLY_LOG")) {
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        auto show_log = [&]() {
            FILE *f = fopen(JS8_LOG_PATH, "r");
            if (!f) {
                printf("[log] no log file\n");
                return;
            }
            char line[1024];
            while (fgets(line, sizeof(line), f)) printf("[log file] %s", line);
            fclose(f);
        };
        unlink(JS8_LOG_PATH);
        pump(300);
        // N0XYZ calls us, we answer with a report, they send theirs and 73.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        ui_compose_append("N0XYZ SNR -10");
        ui_compose_enter();
        wait_tx();
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ SNR -08 TNX", 1320, 0.05f}});
        printf("[log] before 73, list focused: %s (want yes: no prompt yet)\n", ui_focus_is_table() ? "yes" : "no");
        ui_press(3); // Send... our 73 opens the prompt as it goes out
        pump(200);
        ui_compose_append("N0XYZ TU 73");
        ui_compose_enter();
        wait_tx();
        printf("[log] prompt focused '%s' (want Save to log), grid none %d\n", ui_focused_text(),
               ui_popup_has("Grid: (none)"));
        ui_key(LV_KEY_RIGHT); // on Grid, as if about to edit
        // Their last message arrives with the popup open.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ RR73 GRID EN34KS", 1320, 0.05f}});
        printf("[log] after their RR73: grid EN34KS %d, still focused '%s' (want Grid: EN34KS)\n",
               ui_popup_has("Grid: EN34KS"), ui_focused_text());
        ui_key(LV_KEY_LEFT);
        screenshot("25_log_prompt.ppm");

        // Comment: Save, Grid, Name, Comment.
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_RIGHT);
        printf("[log] item 4: '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[log] editing comment, focus: %s\n", ui_focus_desc());
        ui_compose_append("FIRST JS8 TEST");
        ui_compose_enter();
        pump(300);
        printf("[log] back in the log popup, focused '%s' (want Comment: FIRST JS8 TEST)\n", ui_focused_text());
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_LEFT);
        printf("[log] three steps back: '%s' (want Save to log)\n", ui_focused_text());
        ui_key(LV_KEY_LEFT);
        printf("[log] one more: '%s' (want Cancel)\n", ui_focused_text());
        ui_key(LV_KEY_RIGHT);
        screenshot("26_log_comment.ppm");
        ui_click_focused(); // Save to log
        pump(300);
        printf("[log] after Save, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");
        show_log();

        // Their 73 again: no second prompt.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ 73 SK", 1320, 0.05f}});
        printf("[log] popup open: %d (want -1: none)\n", ui_popup_has("Save"));
        printf("[log] second 73, list focused: %s (want yes)\n", ui_focus_is_table() ? "yes" : "no");

        // Stations view: N0XYZ is in the log (green).
        ui_page(3);
        ui_press(3);
        pump(300);
        screenshot("27_log_worked.ppm");
        ui_press(3);
        pump(300);

        // Activating a park: POTA with no park yet, set it by holding.
        ui_page(5);
        printf("[log] page 5: %s | %s | %s | %s\n", ui_button_label(1), ui_button_label(2), ui_button_label(3),
               ui_button_label(4));
        ui_press(3);
        pump(200);
        printf("[log] activation: '%s'\n", ui_button_label(3));
        ui_hold(3);
        pump(300);
        printf("[log] editing park '%s', focus: %s\n", ui_compose_text(), ui_focus_desc());
        ui_compose_clear();
        ui_compose_append("CA-1234");
        ui_compose_enter();
        pump(300);
        printf("[log] activation: '%s', list focused: %s\n", ui_button_label(3), ui_focus_is_table() ? "yes" : "no");

        // Log QSO by hand for the selected station.
        ui_select_row_from("N0XYZ");
        ui_press(2);
        pump(300);
        printf("[log] Log QSO focused '%s'\n", ui_focused_text());
        screenshot("28_log_pota.ppm");
        ui_press(1); // another button only closes it
        pump(300);
        printf("[log] APRS > with the log open, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");
        ui_press(2);
        pump(300);
        ui_click_focused(); // Save
        pump(300);
        show_log();

        ui_press(4);
        printf("[log] prompt: '%s'\n", ui_button_label(4));
        ui_press(4);
        ui_press(3); // SOTA
        ui_press(3); // Off
        printf("[log] activation: '%s'\n", ui_button_label(3));

        // GEN with the log open: popups are deleted with the dialog.
        ui_press(2);
        pump(300);
        dialog_destruct();
        pump(300);
        printf("[log] closed with the log open: running=%d\n", ui_running());
        return 0;
    }
    if (getenv("ONLY_INBOX")) {
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        pump(300);
        // A message for us, AUTO off: saved, ACK offered on Reply.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ MSG MEET AT THE PARK 1800Z", 1320, 0.05f}});
        ui_page(3);
        printf("[inbox] button: '%s' (want Inbox / 1 new), green %d (want 1)\n", ui_button_label(4),
               ui_button_marked(4));
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2); // Reply
        pump(200);
        printf("[inbox] Reply offers: '%s' (want N0XYZ ACK)\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);

        // The Inbox: straight to the unread message.
        ui_page(3);
        ui_press(4);
        pump(300);
        printf("[inbox] list focused '%s'\n", ui_focused_text());
        screenshot("30_inbox_list.ppm");
        ui_click_focused();
        pump(300);
        printf("[inbox] message view focused '%s' (want Reply: MSG to N0XYZ), shows text %d\n", ui_focused_text(),
               ui_popup_has("MEET AT THE PARK 1800Z"));
        screenshot("31_inbox_message.ppm");
        printf("[inbox] after reading: '%s' (want Inbox), green %d (want 0)\n", ui_button_label(4),
               ui_button_marked(4));
        ui_key(LV_KEY_ESC); // back to the list
        pump(300);
        printf("[inbox] ESC: back in the list, focused '%s'\n", ui_focused_text());
        ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // the message again
        pump(300);
        ui_click_focused(); // Reply
        pump(300);
        printf("[inbox] reply prefill '%s' (want N0XYZ MSG ), focus %s\n", ui_compose_text(), ui_focus_desc());
        ui_compose_append("SEE YOU THERE");
        ui_compose_enter();
        wait_tx();
        printf("[inbox] sent: %d\n", ui_list_has("N0XYZ MSG SEE YOU THERE"));

        // A resend (they missed our ACK) isn't a second message.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ MSG MEET AT THE PARK 1800Z", 1320, 0.05f}});
        ui_page(3);
        printf("[inbox] after resend: '%s' (want Inbox: no new)\n", ui_button_label(4));

        // They hold a message for us: Reply offers to fetch it.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ YES MSG ID 3", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2);
        pump(200);
        printf("[inbox] Reply offers: '%s' (want N0XYZ QUERY MSG 3)\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);

        // Query list: Any messages?
        ui_page(1);
        ui_press(3);
        pump(200);
        for (int i = 0; i < 12; i++) ui_key(LV_KEY_RIGHT);
        printf("[inbox] query item 13: '%s' (want Any messages?)\n", ui_focused_text());
        ui_click_focused();
        wait_tx();
        printf("[inbox] QUERY MSGS sent: %d\n", ui_list_has("N0XYZ QUERY MSGS"));
        ui_press(3);
        pump(200);
        for (int i = 0; i < 10; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Message...
        pump(300);
        printf("[inbox] Message... prefill '%s', focus %s\n", ui_compose_text(), ui_focus_desc());
        ui_compose_cancel();
        pump(300);

        // AUTO on: the ACK goes by itself.
        ui_page(4);
        ui_press(1);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ MSG SECOND MESSAGE", 1320, 0.05f}});
        wait_tx();
        printf("[inbox] AUTO sent ACK: %d\n", ui_list_has("N0XYZ ACK"));
        // A MSG while the Inbox is open: the ACK waits until it closes.
        ui_page(3);
        ui_press(4);
        pump(200);
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ MSG WHILE YOU READ", 1700, 0.05f}});
        printf("[inbox] with the Inbox open, nothing sent yet: %d\n", ui_list_has("W1ABC ACK"));
        ui_press(4); // close it
        wait_tx();
        printf("[inbox] after closing, ACK sent: %d\n", ui_list_has("W1ABC ACK"));
        ui_page(4);
        ui_press(1);

        // Delete the first message; close with the inbox open.
        ui_page(3);
        ui_press(4);
        pump(300);
        ui_key(LV_KEY_RIGHT); // from the newest unread one to the next one down
        printf("[inbox] on '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        ui_key(LV_KEY_RIGHT);
        printf("[inbox] on '%s' (want Delete)\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[inbox] after delete: list title has 2 messages %d\n", ui_popup_has("Inbox: 2 messages,"));
        FILE *f = fopen(JS8_INBOX_PATH, "r");
        char  line[512];
        while (f && fgets(line, sizeof(line), f)) printf("[inbox file] %s", line);
        if (f) fclose(f);
        dialog_destruct();
        pump(300);
        printf("[inbox] closed with the inbox open: running=%d\n", ui_running());
        return 0;
    }
    if (getenv("ONLY_ALERTS")) {
        pump(300);
        ui_page(6);
        ui_press(1); // Alerts >
        pump(300);
        printf("[alerts] open, focused '%s'\n", ui_focused_text());
        for (int i = 0; i < 5; i++) ui_key(LV_KEY_RIGHT);
        printf("[alerts] item 6: '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[alerts] editing words, focus %s\n", ui_focus_desc());
        ui_compose_clear();
        ui_compose_append("VE7ABC, @POTA");
        ui_compose_enter();
        pump(300);
        printf("[alerts] back: words shown %d, focused '%s'\n", ui_popup_has("Alert words: VE7ABC @POTA"),
               ui_focused_text());
        for (int i = 0; i < 3; i++) ui_key(LV_KEY_RIGHT); // Someone calls CQ
        ui_click_focused();
        printf("[alerts] toggled: '%s'\n", ui_focused_text());
        screenshot("32_alerts.ppm");
        ui_press(1); // Alerts > closes it
        pump(300);

        printf("[alerts] band: expect beeps for @POTA (double) and VE7ABC\n");
        feed_band({{"W1ABC", "FN42", "", "@POTA ACTIVATING CA-1234", 1300, 0.05f},
                   {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1800, 0.05f}});
        feed_band({{"VE7ABC", "CN89", "", "@HB HEARTBEAT CN89", 900, 0.05f}});
        feed_band({{"N0XYZ", "EN34", "", "CQ CQ CQ EN34", 1500, 0.05f}});
        ui_page(2);
        ui_press(1); // Show: No HB -> Directed
        ui_press(1); // -> All
        pump(300);
        screenshot("33_alert_rows.ppm");
        printf("[alerts] decoded through the beeps: W1ABC %d, K9DEF %d, VE7ABC %d, N0XYZ %d\n",
               ui_list_has("@POTA ACTIVATING CA-1234"), ui_list_has("HEARTBEAT EN52"), ui_list_has("HEARTBEAT CN89"),
               ui_list_has("CQ CQ CQ EN34"));
        ui_page(3);
        ui_press(3); // Stations
        pump(300);
        screenshot("34_alert_stations.ppm");
        ui_press(3);

        ui_page(6);
        ui_press(1);
        pump(200);
        ui_click_focused(); // Beep: Off
        printf("[alerts] '%s'\n", ui_focused_text());
        for (int i = 0; i < 6; i++) ui_key(LV_KEY_RIGHT);
        printf("[alerts] on '%s'\n", ui_focused_text());
        ui_click_focused(); // Test beep with beep off
        pump(200);
        ui_key(LV_KEY_ESC);
        pump(300);
        printf("[alerts] ESC closed it: list focused %s\n", ui_focus_is_table() ? "yes" : "no");
        return 0;
    }
    if (getenv("ONLY_FREQ")) {
        // Settings > Frequencies (page 6's Freq before): JS8Call's presets,
        // GhostNet's, or a custom frequency.
        pump(300);
        ui_page(6);
        printf("[freq] page 6 button 4: '%s' (want (none): moved to Settings)\n", ui_button_label(4));
        printf("[freq] setting: '%s' dial %d\n", dialog_js8_freq_name(), stub_dial_hz());
        open_freq();
        printf("[freq] popup: %d, focused '%s'\n", ui_popup_has("GhostNet (3.575"), ui_focused_text());
        screenshot("45_freq_popup.ppm");
        ui_key(LV_KEY_RIGHT);
        printf("[freq] on '%s'\n", ui_focused_text());
        ui_click_focused(); // GhostNet
        pump(300);
        printf("[freq] GhostNet: '%s' dial %d (want 14107000)\n", dialog_js8_freq_name(), stub_dial_hz());
        ui_band_down();
        pump(100);
        printf("[freq] band down: dial %d (want 7107000)\n", stub_dial_hz());
        ui_band_down();
        ui_band_down(); // nothing below 3.575
        pump(100);
        printf("[freq] bottom: dial %d (want 3575000)\n", stub_dial_hz());
        ui_band_up();
        pump(100);

        // Custom: the keyboard, kHz.
        open_freq();
        printf("[freq] focused '%s' (want GhostNet, the one in use)\n", ui_focused_text());
        ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Custom kHz...
        pump(200);
        ui_compose_clear();
        ui_compose_append("7110.5");
        ui_compose_enter();
        pump(300);
        printf("[freq] custom: '%s' dial %d (want 7110500), list focus %d\n", dialog_js8_freq_name(), stub_dial_hz(),
               ui_focus_is_table());
        printf("[freq] info row: %d (want 1: JS8 Custom), message '%s'\n", ui_list_has("JS8 Custom"), stub_last_msg);
        screenshot("46_freq_custom.ppm");

        // Out of range: nothing changes.
        open_freq();
        // Focus starts on Custom, the one in use.
        printf("[freq] on '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[freq] prefilled: '%s' (want 7110.5)\n", ui_compose_text());
        ui_compose_clear();
        ui_compose_append("99999");
        ui_compose_enter();
        pump(300);
        printf("[freq] out of range kept: dial %d (want 7110500), keyboard still open: '%s'\n", stub_dial_hz(),
               ui_compose_text());
        ui_compose_cancel();
        pump(200);

        // Band keys leave the custom frequency for the preset list (GhostNet).
        ui_band_up();
        pump(200);
        printf("[freq] band up from custom: '%s' dial %d (want 14107000)\n", dialog_js8_freq_name(), stub_dial_hz());

        // Back to JS8Call's list: the closest one.
        open_freq();
        ui_key(LV_KEY_LEFT);
        ui_click_focused();
        pump(300);
        printf("[freq] JS8: '%s' dial %d (want 14078000)\n", dialog_js8_freq_name(), stub_dial_hz());
        return 0;
    }
    if (getenv("ONLY_BADFILES")) {
        pump(300);
        auto aside = [](const char *path) {
            glob_t      g{};
            std::string pattern = std::string(path) + ".unreadable-*";
            int         n       = glob(pattern.c_str(), 0, nullptr, &g) == 0 ? (int)g.gl_pathc : 0;
            for (int i = 0; i < n; i++) { // tidy up for the next run
                chmod(g.gl_pathv[i], 0600);
                unlink(g.gl_pathv[i]);
            }
            globfree(&g);
            return n;
        };
        printf("[badfiles] inbox notice row: %d (want 1)\n", ui_list_has("js8_inbox.txt couldn't be read"));
        printf("[badfiles] texts notice row: %d (want 1)\n", ui_list_has("js8_texts.txt couldn't be read"));
        screenshot("60_badfiles.ppm");
        printf("[badfiles] inbox kept aside: %d, texts kept aside: %d (want 1, 1)\n", aside(JS8_INBOX_PATH),
               aside(JS8_TEXTS_PATH));
        return 0;
    }
    if (getenv("ONLY_TXSAFE")) {
        // Transmitting safely: a band key while sending is refused, and
        // closing the app while a frame is keyed (GEN, APP, another app)
        // stops the frame instead of crashing with PTT on.
        pump(300);
        int dial = stub_dial_hz();
        ui_page(1);
        ui_press(1); // CQ
        for (int i = 0; i < 200 && !stub_tx_keyed; i++) pump(100);
        printf("[txsafe] keyed: %d (want 1)\n", stub_tx_keyed);
        ui_band_up();
        pump(200);
        printf("[txsafe] band key while sending: dial %d, was %d (want the same)\n", stub_dial_hz(), dial);
        int aborted = stub_tx_aborted;
        dialog_destruct(); // what GEN, APP or another app does
        printf("[txsafe] closed while keyed: running=%d keyed=%d aborted=%d (want 0, 0, 1)\n", ui_running(),
               stub_tx_keyed, stub_tx_aborted - aborted);
        pump(500);
        ui_open();
        pump(500);
        ui_page(1);
        ui_press(1); // CQ again: the transmitter works after reopening
        int frames = stub_tx_frames;
        for (int i = 0; i < 200 && stub_tx_frames == frames; i++) pump(100);
        printf("[txsafe] reopened, CQ keyed: %d (want 1)\n", stub_tx_frames > frames);
        pump(3500);
        return 0;
    }
    if (getenv("ONLY_TXMARK")) {
        // Your own rows: " ..." while the message goes out, desktop's end
        // mark once its last frame has, "(stopped 2/n)" if stopped (ESC, or
        // JS8 closed while sending).
        const char *eot = " \xE2\x99\xA2";
        auto        send = [](const char *text) {
            ui_page(2);
            ui_press(3); // Send...
            pump(200);
            ui_compose_append(text);
            ui_compose_enter();
            pump(300);
        };
        auto wait_frames = [](int n) { // until n more frames have keyed
            int want = stub_tx_frames + n;
            for (int i = 0; i < 400 && stub_tx_frames < want; i++) pump(100);
            pump(300);
        };
        pump(300);
        send("K9DEF THIS ONE GOES OUT IN FULL");
        wait_frames(1);
        printf("[txmark] first frame keyed: ' ...' %d (want 1)\n", ui_list_has("GOES OUT IN FULL ..."));
        for (int i = 0; i < 900 && stub_tx_keyed; i++) pump(100);
        for (int i = 0; i < 900 && ui_list_has("GOES OUT IN FULL ...") == 1; i++) pump(100);
        char want[96];
        snprintf(want, sizeof(want), "GOES OUT IN FULL%s", eot);
        printf("[txmark] sent in full: end mark %d, ' ...' %d (want 1, 0)\n", ui_list_has(want),
               ui_list_has("GOES OUT IN FULL ..."));
        screenshot("c0_txmark_sent.ppm");

        send("K9DEF THIS ONE IS STOPPED IN ITS SECOND FRAME");
        wait_frames(2);
        ui_key(LV_KEY_ESC); // stop TX
        pump(500);
        snprintf(want, sizeof(want), "SECOND FRAME%s", eot);
        printf("[txmark] stopped in frame 2: %d, ' ...' %d, end mark %d (want 1, 0, 0)\n",
               ui_list_has("SECOND FRAME  (stopped 2/"), ui_list_has("SECOND FRAME ..."), ui_list_has(want));
        screenshot("c1_txmark_stopped.ppm");

        ui_page(1);
        ui_press(1); // CQ
        wait_frames(1);
        dialog_destruct(); // closed while it goes out
        pump(500);
        ui_open();
        pump(500);
        printf("[txmark] closed while sending, reopened: %d, ' ...' %d (want 1, 0)\n",
               ui_list_has("CQ CQ CQ FN42  (stopped 1/1)"), ui_list_has("CQ CQ CQ FN42 ..."));
        screenshot("c2_txmark_reopened.ppm");
        return 0;
    }
    if (getenv("ONLY_RETUNE")) {
        // A reply that waited behind the keyboard must not go out after a
        // change of band or frequency: it answered a station on the old one.
        pump(300);
        ui_page(4);
        ui_press(1); // AUTO on
        pump(200);
        ui_page(2);
        ui_press(3); // Send...: the keyboard, so the reply waits
        pump(300);
        int frames = stub_tx_frames;
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ SNR?", 1320, 0.05f}});
        printf("[retune] SNR? arrived while typing: frames %d (want %d: waiting)\n", stub_tx_frames, frames);
        ui_compose_cancel();
        printf("[retune] the SNR? was heard: %d (want 1)\n", ui_list_has("N0XYZ: K2XYZ SNR?"));
        ui_band_up(); // another band, before the reply's next chance
        pump(300);
        for (int i = 0; i < 200 && stub_tx_frames == frames; i++) pump(100);
        printf("[retune] after the band change: frames %d (want %d: the old band's reply dropped)\n", stub_tx_frames,
               frames);
        return 0;
    }
    if (getenv("ONLY_REPLYQ")) {
        // Package 3: automatic replies queue, wait for a message to us that
        // is still arriving, and don't wait for lists; offers per station.
        auto wait_idle = [&]() { // until nothing has keyed for 20 s
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 200 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        pump(300);
        // AUTO off: two stations ask; each one's answer is on Reply.
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ SNR?", 1500, 0.05f},
                   {"N0XYZ", "EN34", "K2XYZ", "K2XYZ GRID?", 1320, 0.05f}});
        ui_select_row_from("W1ABC");
        ui_page(2);
        ui_press(2); // Reply
        pump(200);
        printf("[replyq] Reply to W1ABC offers '%s' (want W1ABC SNR ...)\n", ui_compose_text());
        ui_compose_cancel();
        pump(200);
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2);
        pump(200);
        printf("[replyq] Reply to N0XYZ offers '%s' (want N0XYZ GRID FN42AB)\n", ui_compose_text());
        ui_compose_cancel();
        pump(200);

        // AUTO on: two questions in one slot; both answered, in turn.
        ui_page(4);
        ui_press(1); // AUTO on
        pump(200);
        feed_band({{"W7DEF", "DM43", "K2XYZ", "K2XYZ SNR?", 1700, 0.05f},
                   {"VE7ABC", "CN89", "K2XYZ", "K2XYZ GRID?", 900, 0.05f}});
        wait_idle();
        printf("[replyq] both answered: W7DEF %d, VE7ABC %d (want 1, 1)\n", ui_list_has("W7DEF SNR"),
               ui_list_has("VE7ABC GRID"));

        // A question while a message to us is still arriving: dropped, as
        // desktop, so we don't key over the message; the message gets its ACK.
        int frames = stub_tx_frames;
        feed_band({{"N7EAL", "DN17", "K2XYZ", "K2XYZ MSG MEET ON 40M AT 1800Z TOMORROW IF THE BAND IS OPEN", 1320,
                    0.05f},
                   {"KK6WVY", "CM87", "K2XYZ", "K2XYZ SNR?", 2100, 0.05f}},
                  0, 1);
        pump(3000);
        printf("[replyq] SNR? during the first frame of a MSG to us: frames sent %d (want 0)\n",
               stub_tx_frames - frames);
        feed_band({{"N7EAL", "DN17", "K2XYZ", "K2XYZ MSG MEET ON 40M AT 1800Z TOMORROW IF THE BAND IS OPEN", 1320,
                    0.05f}},
                  1);
        wait_idle();
        printf("[replyq] MSG ACKed %d, SNR? not answered %d, frames sent %d (want 1, 1, 1: the ACK)\n",
               ui_list_has("K2XYZ: N7EAL ACK"), ui_list_has("not sent, a message is still arriving"),
               stub_tx_frames - frames);

        // Lists don't hold replies (only the keyboard does).
        ui_page(3);
        ui_press(4); // Inbox: a list
        pump(300);
        frames = stub_tx_frames;
        feed_band({{"W9GHI", "EN52", "K2XYZ", "K2XYZ SNR?", 1100, 0.05f}});
        for (int i = 0; i < 200 && stub_tx_frames == frames; i++) pump(100);
        printf("[replyq] SNR? with the Inbox open: answered %d (want 1)\n", stub_tx_frames > frames);
        ui_key(LV_KEY_ESC);
        wait_idle();

        // HB ACK: not while any message is still arriving (to anyone).
        ui_page(4);
        ui_press(3); // HB ACK on
        ui_page(1);
        ui_hold(2);  // auto HB: one now (let it go), the knob sets the minutes
        ui_press(2); // done
        wait_idle();
        frames = stub_tx_frames;
        feed_band({{"N0ABC", "EM12", "W7AAA", "W7AAA MSG THE NET MOVES TO 7.078 TONIGHT AT THE USUAL TIME", 1320,
                    0.05f},
                   {"KE7XYZ", "DN06", "", "KE7XYZ: HEARTBEAT DN06", 700, 0.05f}},
                  0, 1);
        pump(3000);
        printf("[replyq] heartbeat during a MSG to someone else: frames sent %d (want 0)\n", stub_tx_frames - frames);
        feed_band({{"N0ABC", "EM12", "W7AAA", "W7AAA MSG THE NET MOVES TO 7.078 TONIGHT AT THE USUAL TIME", 1320,
                    0.05f}},
                  1);
        feed_band({{"WA7JKL", "CN87", "", "WA7JKL: HEARTBEAT CN87", 700, 0.05f}});
        wait_idle();
        printf("[replyq] heartbeat on a quiet band: HB ACK %d, none to KE7XYZ %d, frames sent %d (want 1, 0, 1)\n",
               ui_list_has("K2XYZ: WA7JKL HEARTBEAT SNR"), ui_list_has("K2XYZ: KE7XYZ HEARTBEAT SNR"),
               stub_tx_frames - frames);
        return 0;
    }
    if (getenv("ONLY_BANDS")) {
        // Each band keeps its own Stations list.
        pump(300);
        feed_speeds({{"W1ABC", "FN42", "CQ CQ CQ FN42", 700, -5, JS8_SPEED_NORMAL},
                     {"VE7ABC", "CN89", "K2XYZ HELLO", 2000, -5, JS8_SPEED_NORMAL}});
        ui_page(3);
        ui_press(3); // Stations
        pump(300);
        printf("[bands] 20m heard: %d (want 1)\n", !ui_list_has("No stations heard yet"));
        screenshot("40_bands_20m.ppm");
        ui_band_up(); // 17m
        pump(300);
        printf("[bands] 17m empty: %d (want 1)\n", ui_list_has("No stations heard yet"));
        ui_band_down(); // back to 20m
        pump(300);
        printf("[bands] 20m back: %d (want 1)\n", !ui_list_has("No stations heard yet"));
        screenshot("41_bands_20m_again.ppm");
        // Close JS8 and open it again: the list, its times and ★ stay.
        ui_key(LV_KEY_ESC);
        pump(500);
        printf("[bands] closed: %d (want 1)\n", !ui_running());
        ui_open();
        pump(1000);
        printf("[bands] reopened, stations kept: %d, rows %d (want 1, 2)\n", !ui_list_has("No stations heard yet"),
               ui_list_count(" "));
        screenshot("42_bands_reopened.ppm");
        // Show in the Stations view: back to the messages, filter unchanged.
        ui_page(2);
        std::string before = ui_button_label(1);
        ui_press(1);
        pump(300);
        printf("[bands] Show from Stations: messages %d, filter kept %d (want 1, 1)\n",
               ui_list_has("CQ CQ CQ FN42"), before == ui_button_label(1));
        ui_press(1); // now it cycles
        pump(200);
        printf("[bands] Show again cycles: %d (want 1)\n", before != ui_button_label(1));
        ui_page(3);
        ui_press(3); // Stations again, for Clear below
        pump(300);
        ui_page(2);
        ui_press(4); // Clear: this band only
        pump(300);
        printf("[bands] 20m after Clear empty: %d (want 1)\n", ui_list_has("No stations heard yet"));
        return 0;
    }
    if (getenv("ONLY_FINDER")) {
        // The green band as wide as the selected station's speed; the red
        // band on a heartbeat's own offset while it's queued and sent.
        using x6100::js8::TestStation;
        pump(300);
        feed_speeds({{"W1ABC", "FN42", "CQ CQ CQ FN42", 700, -5, JS8_SPEED_NORMAL},
                     {"K9DEF", "EN52", "CQ CQ CQ EN52", 1100, -5, JS8_SPEED_FAST},
                     {"N0XYZ", "EN34", "CQ CQ CQ EN34", 1500, -5, JS8_SPEED_TURBO},
                     {"VE7ABC", "CN89", "K2XYZ SLOW ONE", 2000, -5, JS8_SPEED_SLOW}});
        pump(300);
        struct { const char *call; int bw; } sel[] = {{"W1ABC", 50}, {"K9DEF", 80}, {"N0XYZ", 160}, {"VE7ABC", 25}};
        for (auto &c : sel) {
            ui_select_row_from(c.call);
            pump(200);
            int x = 0, w = 0;
            bool shown = dialog_js8_cursor_band(&x, &w);
            int want = c.bw * 788 / 2800 + 1;
            printf("[finder] %-6s green band shown %d, %d px wide (want about %d: %d Hz)\n", c.call, shown, w, want, c.bw);
        }
        screenshot("c5_finder_widths.ppm");

        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 300 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 60 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        int before = dialog_js8_finder_hz();
        ui_page(1);
        printf("[finder] page 1 button 2: '%s' (want Heartbeat)\n", ui_button_label(2));
        ui_press(2); // a heartbeat by hand: a free spot at 500-999 Hz
        pump(300);
        int queued = dialog_js8_finder_hz();
        screenshot("c6_finder_hb.ppm");
        printf("[finder] red band before %d, queued heartbeat %d (want 500-999)\n", before, queued);
        wait_tx();
        printf("[finder] after it went: %d (want %d)\n", dialog_js8_finder_hz(), before);
        return 0;
    }
    if (getenv("ONLY_SPEED")) {
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 300 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 330 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        using x6100::js8::TestStation;
        pump(300);
        ui_page(6);
        printf("[speed] page 6: %s | %s\n", ui_button_label(2), ui_button_label(3));
        // A band with every speed, decoded together.
        feed_speeds({{"W1ABC", "FN42", "K2XYZ NORMAL HERE", 700, -5, JS8_SPEED_NORMAL},
                     {"K9DEF", "EN52", "@HB HEARTBEAT EN52", 1100, -5, JS8_SPEED_FAST},
                     {"N0XYZ", "EN34", "CQ CQ CQ EN34", 1500, -5, JS8_SPEED_TURBO},
                     {"VE7ABC", "CN89", "K2XYZ SLOW ONE", 2000, -5, JS8_SPEED_SLOW}});
        ui_page(2);
        ui_press(1); // Show: No HB -> Directed
        ui_press(1); // -> All
        pump(300);
        printf("[speed] rows: normal %d, fast F %d, turbo T %d, slow S %d\n", ui_list_has(" 700  W1ABC"),
               ui_list_has("1100 F  K9DEF"), ui_list_has("1500 T  N0XYZ"), ui_list_has("2000 S  VE7ABC"));
        screenshot("35_speed_rows.ppm");
        ui_page(3);
        ui_press(3); // Stations
        pump(300);
        screenshot("36_speed_stations.ppm");

        // Reply to the Turbo station while on Normal: warned; hold Speed matches.
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2); // Reply
        pump(200);
        ui_compose_cancel();
        pump(200);
        ui_page(6);
        ui_hold(2);
        printf("[speed] after hold: '%s'\n", ui_button_label(2));
        ui_page(1);
        ui_press(2); // Heartbeat in Turbo: refused
        pump(200);

        // Top offset follows the speed: Turbo 2840 Hz (3000 - 160).
        ui_rotary(2000);
        ui_rotary(2000);
        pump(100);
        ui_page(6);
        ui_press(2); // -> Slow
        printf("[speed] after cycling: '%s'\n", ui_button_label(2));
        ui_press(2); // -> Normal
        ui_press(2); // -> Fast
        printf("[speed] now: '%s'\n", ui_button_label(2));
        ui_page(3);
        ui_press(3); // Stations -> Map
        ui_press(3); // -> Messages (the button cycles through the map now)
        pump(200);

        // Send at Fast: 10 s slots, 0.1 s symbols -> 79 x 4410 samples at 44.1 kHz.
        ui_page(1);
        ui_press(1); // CQ
        wait_tx();
        printf("[speed] Fast frame: %u samples (want %d: 79 symbols of 0.1 s at %d Hz)\n", stub_tx_samples, 79 * stub_play_rate() / 10, stub_play_rate());
        printf("[speed] our row: %d (offset kept at Turbo's 2840 limit)\n", ui_list_has("TX 2840 F"));
        screenshot("37_speed_tx.ppm");

        // Decode: All speeds / My speed is a Settings line (page 6's
        // button 3 is Hold now).
        ui_page(6);
        printf("[speed] page 6 button 3: '%s' (want Hold)\n", ui_button_label(3));
        ui_page(4);
        ui_press(4); // Settings
        pump(200);
        for (int i = 0; i < 25 && strncmp(ui_focused_text(), "Decode:", 7) != 0; i++) ui_key(LV_KEY_RIGHT);
        printf("[speed] settings: '%s' (want Decode: All speeds)\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[speed] decode: '%s' / '%s' (want Decode: My speed)\n", ui_focused_text(), stub_last_msg);
        screenshot("37b_decode_setting.ppm");
        ui_click_focused();
        pump(200);
        printf("[speed] decode: '%s' (want Decode: All speeds)\n", ui_focused_text());

        // Ultra (experimental): the next line, off to start; on, it's
        // decoded and on the Speed button.
        ui_key(LV_KEY_RIGHT);
        printf("[speed] next line: '%s' (want Ultra (experimental): Off)\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[speed] ultra: '%s' / '%s' (want On)\n", ui_focused_text(), stub_last_msg);
        ui_key(LV_KEY_ESC);
        pump(200);
        feed_speeds({{"KL7QXZ", "BP51", "K2XYZ ULTRA HERE", 2400, -5, JS8_SPEED_ULTRA}});
        pump(300);
        screenshot("37bb_ultra_rx.ppm");
        printf("[speed] ultra row U: %d\n", ui_list_has(" U  KL7QXZ"));
        ui_page(6);
        ui_press(2); // Fast -> Turbo
        ui_press(2); // -> Slow
        ui_press(2); // -> Ultra
        printf("[speed] with Ultra on: '%s' (want Speed: Ultra)\n", ui_button_label(2));
        ui_page(1);
        ui_press(2); // Heartbeat in Ultra: refused
        pump(200);
        printf("[speed] heartbeat in Ultra: '%s' (want refused)\n", stub_last_msg);
        ui_press(1); // CQ at Ultra
        wait_tx();
        printf("[speed] Ultra frame: %u samples (want %ld: 79 symbols of 32 ms at %d Hz)\n", stub_tx_samples,
               79 * std::lround(0.032 * stub_play_rate()), stub_play_rate());
        printf("[speed] our Ultra row: %d (offset down to Ultra's 2750 limit)\n", ui_list_has("TX 2750 U"));
        screenshot("37c_speed_ultra.ppm");

        // Ultra off while sending at it: back to Normal, and off the button.
        ui_page(4);
        ui_press(4); // Settings
        pump(200);
        for (int i = 0; i < 25 && strncmp(ui_focused_text(), "Ultra (", 7) != 0; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(200);
        printf("[speed] ultra: '%s' / '%s' (want Off)\n", ui_focused_text(), stub_last_msg);
        ui_key(LV_KEY_ESC);
        pump(200);
        ui_page(6);
        printf("[speed] after Ultra off: '%s' (want Speed: Normal)\n", ui_button_label(2));
        ui_press(2); // -> Fast
        ui_press(2); // -> Turbo
        ui_press(2); // -> Slow
        ui_press(2); // -> Normal (no Ultra)
        printf("[speed] back to '%s' (want Normal)\n", ui_button_label(2));
        return 0;
    }
    if (getenv("ONLY_HELD")) {
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        pump(300);
        ui_page(4);
        ui_press(1); // AUTO on: answers go by themselves
        // N0XYZ leaves a message here for W1ABC: held and ACKed.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ MSG TO:W1ABC MEET AT THE PARK", 1320, 0.05f}});
        wait_tx();
        printf("[held] ACK sent: %d\n", ui_list_has("N0XYZ ACK"));
        // W1ABC asks everyone what they hold (as desktop allows), then fetches it.
        feed_band({{"W1ABC", "FN42", "@ALLCALL", "@ALLCALL QUERY MSGS", 1500, 0.05f}});
        wait_tx();
        printf("[held] YES to @ALLCALL sent: %d\n", ui_list_has("W1ABC YES MSG ID 1"));
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ QUERY MSG 1", 1500, 0.05f}});
        // Stopped in its first frame: not delivered, still held.
        {
            int b = stub_tx_frames;
            for (int i = 0; i < 300 && stub_tx_frames == b; i++) pump(100);
        }
        pump(500);
        ui_key(LV_KEY_ESC);
        for (int i = 0; i < 60; i++) pump(100);
        printf("[held] stopped: still held %d, delivered row %d (want 1, 0)\n",
               ui_list_has("Held message 1 stopped before the end: still held"),
               ui_list_has("Held message 1 delivered"));
        // They ask again at once: sent again (no 5 min wait), in full this time.
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ QUERY MSG 1", 1500, 0.05f}});
        wait_tx();
        printf("[held] delivered: %d, row %d\n", ui_list_has("W1ABC MSG MEET AT THE PARK FROM N0XYZ"),
               ui_list_has("Held message 1 delivered"));
        ui_page(4);
        ui_press(1); // AUTO off

        ui_page(3);
        ui_press(4); // Inbox
        pump(300);
        printf("[held] inbox shows it: %d, sent %d\n", ui_popup_has("Held for others: 0 waiting"),
               ui_popup_has("(sent) for W1ABC from N0XYZ"));
        screenshot("40_held.ppm");
        for (int i = 0; i < 40 && !strstr(ui_focused_text(), "for W1ABC"); i++) ui_key(LV_KEY_RIGHT);
        printf("[held] on '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[held] view: %d, focused '%s'\n", ui_popup_has("Delivered to W1ABC"), ui_focused_text());
        ui_click_focused(); // Delete
        pump(300);
        printf("[held] after delete: %d (want 0)\n", ui_popup_has("Held for others"));
        for (int i = 0; i < 40 && strcmp(ui_focused_text(), "Close") != 0; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // Close
        pump(300);
        printf("[held] inbox closed: %d\n", ui_focus_is_table());

        // HW CPY? on page 1 to the selected station.
        ui_select_row_from("W1ABC");
        ui_page(1);
        printf("[held] page 1 button 4: '%s'\n", ui_button_label(4));
        ui_press(4);
        wait_tx();
        printf("[held] HW CPY? sent: %d\n", ui_list_has("W1ABC HW CPY?"));
        return 0;
    }
    if (getenv("ONLY_SAVED")) {
        // Query > Saved messages > (just before Close): ten messages, the
        // first desktop's TNX 73 GL. A press of the MFK sends one at once
        // with its macros filled in; holding the MFK edits it (Enter saves,
        // empty clears, ESC leaves it); an empty one opens the keyboard.
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        auto focus_on = [&](const char *text) {
            for (int i = 0; i < 40 && !strstr(ui_focused_text(), text); i++) ui_key(LV_KEY_RIGHT);
            return strstr(ui_focused_text(), text) != nullptr;
        };
        auto mfk = [&](int ms) { // the real knob: a press, or a hold
            ui_mfk_set(true);
            pump(ms);
            ui_mfk_set(false);
            pump(300);
        };
        auto open_saved = [&]() {
            ui_page(1);
            ui_press(3); // Query >
            pump(200);
            focus_on("Saved messages");
            ui_click_focused();
            pump(300);
        };
        auto file_lines = [&]() {
            std::string all;
            FILE       *f = fopen(JS8_SAVED_PATH, "r");
            if (!f) return std::string("(no file)");
            char line[256];
            while (fgets(line, sizeof(line), f)) all += std::string(line, strcspn(line, "\n")) + "|";
            fclose(f);
            return all;
        };
        std::string texts_before; // INFO is changed below: put the file back at the end
        if (FILE *f = fopen(JS8_TEXTS_PATH, "r")) {
            char buf[4096];
            texts_before.assign(buf, fread(buf, 1, sizeof(buf), f));
            fclose(f);
        }
        pump(300);
        ui_indevs_init();

        ui_page(1);
        ui_press(3); // Query >, nothing selected
        pump(200);
        ui_key(LV_KEY_LEFT); // wraps to Close
        ui_key(LV_KEY_LEFT);
        printf("[saved] before Close: '%s' (want Saved messages >)\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[saved] list: focused '%s' (want 1  TNX 73 GL), 9 empty: %d, title: %d\n", ui_focused_text(),
               ui_popup_has("10  (empty)"), ui_popup_has("press sends, hold edits"));
        printf("[saved] message line '%s' (want 1: TNX 73 GL)\n", stub_last_msg);
        screenshot("s0_saved_list.ppm");
        ui_key(LV_KEY_LEFT);
        printf("[saved] before the first: '%s' (want Close), then '", ui_focused_text());
        ui_key(LV_KEY_LEFT);
        printf("%s' (want < Back)\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[saved] Back: the Query list, on '%s' (want Can anyone reach...?)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(200);

        // A press of the MFK sends it at once.
        open_saved();
        mfk(100);
        printf("[saved] pressed: list closed %d (want 1), '%s'\n", ui_focus_is_table(), stub_last_msg);
        wait_tx();
        printf("[saved] sent TNX 73 GL: %d (want 1)\n", ui_list_has("TNX 73 GL"));

        // Hold the MFK on the empty second one: the keyboard, empty; the
        // knob's release doesn't press Enter in it.
        open_saved();
        ui_key(LV_KEY_RIGHT);
        printf("[saved] on '%s' (want 2  (empty))\n", ui_focused_text());
        mfk(900);
        printf("[saved] hold: focus %s, text '%s' (want empty), placeholder '%s'\n", ui_focus_desc(), ui_compose_text(),
               ui_compose_placeholder());
        printf("[saved] hint '%s'\n", stub_last_msg);
        ui_compose_append("<CALL> UR <SNR> QTH <MYGRID4>");
        printf("[saved] while typing: '%s' (frames, macros counted filled in)\n", stub_last_msg);
        screenshot("s1_saved_edit.ppm");
        ui_compose_enter();
        pump(300);
        printf("[saved] Enter: back in the list on '%s' (want 2  <CALL> UR <SNR> QTH FN42), '%s'\n", ui_focused_text(),
               stub_last_msg);

        // Nothing selected: refused, the list stays.
        ui_click_focused();
        pump(200);
        printf("[saved] no station: '%s', list still open: %d (want 1)\n", stub_last_msg, !ui_focus_is_table());
        ui_key(LV_KEY_ESC);
        pump(200);

        // A station selected: <CALL> and <SNR> filled in, sent at once.
        feed_band({{"N0XYZ", "EN34", "@ALLCALL", "@ALLCALL CQ CQ CQ EN34", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        open_saved();
        ui_key(LV_KEY_RIGHT);
        printf("[saved] selected N0XYZ: '%s'\n", ui_focused_text());
        printf("[saved] message line '%s'\n", stub_last_msg);
        screenshot("s2_saved_selected.ppm");
        mfk(100);
        wait_tx();
        printf("[saved] sent with macros: %d (want 1)\n", ui_list_has("N0XYZ UR ") == 1 && ui_list_has("QTH FN42") == 1);

        // Hold then ESC: unchanged. Hold, clear, Enter: emptied.
        open_saved();
        mfk(900);
        printf("[saved] editing '%s' (want TNX 73 GL)\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);
        printf("[saved] ESC: on '%s' (want 1  TNX 73 GL)\n", ui_focused_text());
        mfk(900);
        ui_compose_clear();
        ui_compose_enter();
        pump(300);
        printf("[saved] cleared: on '%s' (want 1  (empty)), '%s'\n", ui_focused_text(), stub_last_msg);
        // An empty one: a press writes it.
        ui_click_focused();
        pump(300);
        printf("[saved] press on empty: focus %s, placeholder '%s'\n", ui_focus_desc(), ui_compose_placeholder());
        ui_compose_append("73 DE <MYCALL>");
        ui_compose_enter();
        pump(300);
        printf("[saved] written: '%s' (want 1  73 DE K2XYZ)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(200);
        printf("[saved] file: %s\n", file_lines().c_str());

        // Kept when JS8 opens again.
        dialog_destruct();
        pump(300);
        ui_open();
        pump(300);
        open_saved();
        printf("[saved] reopened: '%s' (want 1  73 DE K2XYZ), second kept: %d\n", ui_focused_text(),
               ui_popup_has("UR ") == 1);
        ui_key(LV_KEY_ESC);
        pump(200);

        // Macros typed in a message (Send...), as desktop fills them in.
        ui_page(2);
        ui_press(3); // Send...
        pump(300);
        ui_compose_append("@ALLCALL <MYGRID4> <NOSUCH> TEST");
        ui_compose_enter();
        wait_tx();
        printf("[saved] typed macros: %d (want 1)\n", ui_list_has("@ALLCALL FN42 <NOSUCH> TEST"));

        // INFO with a macro: answered (offered on Reply, AUTO off) filled in.
        ui_page(4);
        ui_press(4); // Settings...
        pump(200);
        for (int i = 0; i < 25 && strncmp(ui_focused_text(), "INFO", 4) != 0; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        ui_compose_clear();
        ui_compose_append("X6100 QTH <MYGRID4>");
        ui_compose_enter();
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ INFO?", 1320, 0.05f}});
        pump(6000);
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2); // Reply: the answer offered
        pump(300);
        printf("[saved] INFO answer offered: '%s' (want N0XYZ INFO X6100 QTH FN42)\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);
        if (FILE *f = fopen(JS8_TEXTS_PATH, "w")) {
            fwrite(texts_before.data(), 1, texts_before.size(), f);
            fclose(f);
        }
        return 0;
    }
    if (getenv("ONLY_ATGATE")) {
        // The Stations list's star column: '@' for a station that passed an
        // APRS message back over JS8 (here our Echo test's answer), '*' for
        // one that heard us, blank otherwise; '@' wins over '*'.
        pump(300);
        feed_band({{"NR4U", "EM95", "@APRSIS", "@APRSIS MSG TO:K2XYZ TEST DE ECHO", 700, 0.05f},
                   {"K1AAA", "FN42", "K2XYZ", "K2XYZ HELLO", 1500, 0.04f}});
        feed_band({{"VE7BBB", "CN89", "@HB", "@HB HEARTBEAT CN89", 1900, 0.06f},
                   {"W1GW", "FN31", "@APRSIS", "@APRSIS MSG TO:VE7ABC HI DE SMS", 1100, 0.05f}});
        pump(1000);
        ui_page(3);
        printf("[atgate] page 3 button 2: '%s' (want Show History: Hold moved to page 6)\n", ui_button_label(2));
        ui_press(3); // Show Stations
        pump(300);
        for (const char *c : {"NR4U", "W1GW", "K1AAA", "VE7BBB"})
            printf("[atgate] %-6s star '%c'\n", c, dialog_js8_station_star(c) ? dialog_js8_station_star(c) : '?');
        printf("[atgate] want NR4U '@', W1GW '@', K1AAA '*', VE7BBB ' '\n");
        screenshot("u0_aprs_gate.ppm");
        // NR4U calls us too: still '@'.
        feed_band({{"NR4U", "EM95", "K2XYZ", "K2XYZ GM", 700, 0.05f}});
        pump(1000);
        printf("[atgate] NR4U after calling us: '%c' (want '@')\n", dialog_js8_station_star("NR4U"));
        return 0;
    }
    if (getenv("ONLY_STSORT")) {
        // Page 3's second button in the Stations view: Sort, Heard you ->
        // SNR -> Time -> Distance (from FN42AB). The selected station stays
        // selected. Over the messages it's blank; on the map, the map's.
        auto rows_now = [&]() {
            static char buf[256];
            dialog_js8_station_rows(buf, sizeof(buf));
            return (const char *)buf;
        };
        pump(300);
        // Older: VK2EEE's heartbeat, K1AAA calling us (no grid heard).
        feed_band({{"VK2EEE", "QF56", "@HB", "@HB HEARTBEAT QF56", 700, 0.03f},
                   {"K1AAA", "FN42", "K2XYZ", "K2XYZ HELLO", 1500, 0.012f}});
        // Newer, a slot later: JA1CCC's CQ (strong), VE7BBB's heartbeat.
        feed_band({{"JA1CCC", "PM95", "@ALLCALL", "@ALLCALL CQ CQ CQ PM95", 1100, 0.2f},
                   {"VE7BBB", "CN89", "@HB", "@HB HEARTBEAT CN89", 1900, 0.06f}});
        pump(1000);
        ui_select_row_from("JA1CCC");
        ui_page(3);
        printf("[stsort] messages: button 2 '%s' (want empty)\n", ui_button_label(1));
        ui_press(3); // Show Stations
        pump(300);
        for (int i = 0; i < 4 && !strstr(ui_button_label(1), "Heard you"); i++) {
            ui_press(1);
            pump(300);
        }
        printf("[stsort] %-24s %s\n", "Sort: Heard you", rows_now());
        screenshot("t0_sort_heard.ppm");
        // SNR, Time, Distance, then QSO: only K1AAA (it called us; the
        // others only sent heartbeats and a CQ).
        for (int k = 0; k < 4; k++) {
            ui_press(1);
            pump(300);
            std::string label = ui_button_label(1);
            for (auto &c : label) c = c == '\n' ? ' ' : c;
            printf("[stsort] %-24s %s   '%s'\n", label.c_str(), rows_now(), stub_last_msg);
            char shot[32];
            snprintf(shot, sizeof(shot), "t%d_sort.ppm", k + 1);
            screenshot(shot);
        }
        char sel[16] = "";
        dialog_js8_selected_call(sel, sizeof(sel));
        printf("[stsort] still selected: %s (want JA1CCC)\n", sel);
        ui_press(1); // round to Heard you
        pump(300);
        printf("[stsort] round again: '%s'\n", ui_button_label(1));
        ui_press(3); // Show Map
        pump(500);
        printf("[stsort] map: button 2 '%s' (want Map:)\n", ui_button_label(1));
        ui_press(3); // back to the messages
        pump(300);
        printf("[stsort] messages again: button 2 '%s' (want empty)\n", ui_button_label(1));
        return 0;
    }
    if (getenv("ONLY_STQRZ")) {
        // The map's QRZ line over the Stations view too: who sent you a
        // message while the messages weren't showing (not heartbeat SNR
        // replies), newest first; hidden on the map (it has its own) and
        // over the messages; a selected station drops off; back to the
        // messages (Show or Map > Messages) clears it.
        auto qrz = [] { return dialog_js8_st_qrz(); };
        int  map_qrz = 0;
        pump(300);
        ui_page(3);
        ui_press(3); // Show Stations
        pump(300);
        printf("[stqrz] Stations view, nobody called: '%s' (want empty)\n", qrz());
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HOW ARE YOU", 1320, 0.05f},
                   {"K9ABC", "EN52", "K2XYZ", "K2XYZ HEARTBEAT SNR -08", 1800, 0.04f}});
        pump(500);
        printf("[stqrz] N0XYZ called, K9ABC acked a heartbeat: '%s' (want QRZ  N0XYZ)\n", qrz());
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ GM OM", 900, 0.05f}});
        pump(500);
        printf("[stqrz] W1ABC too: '%s' (want QRZ 2  W1ABC N0XYZ)\n", qrz());
        screenshot("q0_stations_qrz.ppm");
        ui_press(3); // Show Map
        pump(500);
        dialog_js8_map_state(nullptr, nullptr, nullptr, nullptr, &map_qrz);
        printf("[stqrz] map: Stations' line '%s' (want empty), the map's QRZ %d (want 2)\n", qrz(), map_qrz);
        ui_press(3); // Messages
        pump(300);
        dialog_js8_map_state(nullptr, nullptr, nullptr, nullptr, &map_qrz);
        printf("[stqrz] messages: '%s', QRZ %d (want empty, 0)\n", qrz(), map_qrz);
        ui_press(3); // Stations again
        pump(300);
        printf("[stqrz] Stations again: '%s' (want empty: the list showed them)\n", qrz());
        feed_band({{"VE7ABC", "CN89", "K2XYZ", "K2XYZ TNX FER CALL", 1500, 0.05f}});
        pump(500);
        printf("[stqrz] VE7ABC called: '%s' (want QRZ  VE7ABC)\n", qrz());
        ui_indevs_init(); // the real MFK, as on the radio (and ONLY_MAP)
        char sel[16] = "";
        // VE7ABC is on top, under the cursor: a station is selected when the
        // cursor moves onto it, so one step down and back.
        ui_mfk_turn(1);
        pump(150);
        ui_mfk_turn(-1);
        pump(300);
        dialog_js8_selected_call(sel, sizeof(sel));
        printf("[stqrz] VE7ABC selected (%s): '%s' (want empty)\n", sel, qrz());
        feed_band({{"G4ABC", "IO91", "K2XYZ", "K2XYZ HELLO", 1100, 0.05f}});
        pump(500);
        printf("[stqrz] G4ABC called: '%s' (want QRZ  G4ABC)\n", qrz());
        ui_page(2);
        ui_press(1); // Show: back to the messages
        pump(300);
        ui_page(3);
        ui_press(3); // Stations
        pump(300);
        printf("[stqrz] Show to the messages and back: '%s' (want empty)\n", qrz());
        return 0;
    }
    if (getenv("ONLY_QUERYCALL")) {
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        auto focus_on = [&](const char *text) {
            for (int i = 0; i < 40 && !strstr(ui_focused_text(), text); i++) ui_key(LV_KEY_RIGHT);
            return strstr(ui_focused_text(), text) != nullptr;
        };
        pump(300);

        // Nothing selected: Query > opens with the @ALLCALL item only.
        ui_page(1);
        printf("[querycall] page 1 button 3: '%s' (want Query >)\n", ui_button_label(3));
        ui_press(3);
        pump(200);
        printf("[querycall] no selection: list for @ALLCALL %d (want 1), focused '%s' (want Can anyone reach...?)\n",
               ui_popup_has("To @ALLCALL"), ui_focused_text());
        printf("[querycall] station items shown: %d (want 0)\n", ui_popup_has("Can they reach"));
        screenshot("c0_query_allcall.ppm");
        ui_click_focused();
        pump(300);
        printf("[querycall] prefill '%s' (want @ALLCALL QUERY CALL )\n", ui_compose_text());
        ui_compose_append("w1abc");
        ui_compose_enter();
        wait_tx();
        printf("[querycall] sent with the ?: %d (want 1)\n", ui_list_has("@ALLCALL QUERY CALL W1ABC?"));

        // A station selected: Can they reach...?, typed without the '?'.
        feed_band({{"N0XYZ", "EN34", "@ALLCALL", "@ALLCALL CQ CQ CQ EN34", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(1);
        ui_press(3);
        pump(200);
        printf("[querycall] selected: on Can they reach %d, Can anyone reach also there %d (want 1 1)\n",
               focus_on("Can they reach"), ui_popup_has("Can anyone reach"));
        ui_click_focused();
        pump(300);
        printf("[querycall] prefill '%s' (want N0XYZ QUERY CALL )\n", ui_compose_text());
        ui_compose_append("ve7abc");
        ui_compose_enter();
        wait_tx();
        printf("[querycall] sent with the ?: %d (want 1)\n", ui_list_has("N0XYZ QUERY CALL VE7ABC?"));

        // Typed with the '?' already: still just one.
        ui_page(1);
        ui_press(3);
        pump(200);
        focus_on("Can they reach");
        ui_click_focused();
        pump(300);
        ui_compose_append("w7xyz?");
        ui_compose_enter();
        wait_tx();
        printf("[querycall] one ?: %d, two: %d (want 1 0)\n", ui_list_has("N0XYZ QUERY CALL W7XYZ?"),
               ui_list_has("W7XYZ??"));
        return 0;
    }
    if (getenv("ONLY_RELAY")) {
        auto wait_tx = [&]() {
            int b = stub_tx_frames;
            for (int i = 0; i < 200 && stub_tx_frames == b; i++) pump(100);
            int last;
            do {
                last = stub_tx_frames;
                for (int i = 0; i < 170 && stub_tx_frames == last; i++) pump(100);
            } while (stub_tx_frames != last);
            pump(500);
        };
        auto focus_on = [&](const char *text) {
            for (int i = 0; i < 40 && !strstr(ui_focused_text(), text); i++) ui_key(LV_KEY_RIGHT);
            return strstr(ui_focused_text(), text) != nullptr;
        };
        pump(300);
        ui_page(4);
        printf("[relay] page 4 button 4: '%s' (want Settings...)\n", ui_button_label(4));
        ui_press(1); // AUTO on

        // N0XYZ asks us to pass a message on to W1ABC: sent on, as desktop.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ>W1ABC HELLO THERE", 1320, 0.05f}});
        wait_tx();
        printf("[relay] passed on: %d (want 1)\n", ui_list_has("HELLO THERE *DE* N0XYZ"));

        // W1ABC passes us VE7ABC's message: inbox, ACK back along the path.
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ>MSG MEET AT 1800Z *DE* VE7ABC", 1500, 0.05f}});
        wait_tx();
        printf("[relay] ACK back via W1ABC: %d (want 1)\n", ui_list_has("VE7ABC ACK"));

        // AUTO off: Reply offers it, *DE* and all.
        ui_page(4);
        ui_press(1); // AUTO off
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ>W1ABC SECOND ONE", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2); // Reply
        pump(200);
        printf("[relay] Reply offers: '%s' (want W1ABC>SECOND ONE *DE* N0XYZ)\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);

        // Settings: Relay off, in place.
        ui_page(4);
        ui_press(4); // Settings...
        pump(200);
        printf("[relay] Settings open on '%s'\n", ui_focused_text());
        printf("[relay] on the Relay line: %d, '%s'\n", focus_on("Relay"), ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[relay] after a press: '%s' (want Relay: Off), list still open %d\n", ui_focused_text(),
               ui_popup_has("Groups:"));
        screenshot("50_settings.ppm");
        ui_key(LV_KEY_ESC);
        pump(300);
        ui_press(1); // AUTO on
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ>W1ABC THIRD ONE", 1320, 0.05f}});
        for (int i = 0; i < 150; i++) pump(100);
        printf("[relay] Relay off, not passed on: %d (want 0)\n", ui_list_has("THIRD ONE *DE*"));

        // Relay on again, and a group.
        ui_press(4);
        pump(200);
        focus_on("Relay");
        ui_click_focused();
        pump(200);
        printf("[relay] back on: '%s'\n", ui_focused_text());
        printf("[relay] on the Groups line: %d\n", focus_on("Groups"));
        ui_click_focused();
        pump(300);
        printf("[relay] editing groups, focus: %s\n", ui_focus_desc());
        ui_compose_append("net, @pnw");
        ui_compose_enter();
        pump(300);
        ui_press(4);
        pump(200);
        focus_on("Groups");
        printf("[relay] groups saved: '%s' (want Groups: @NET @PNW)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(300);
        feed_band({{"N0XYZ", "EN34", "@NET", "@NET MSG NET TONIGHT AT 0100Z", 1320, 0.05f}});
        wait_tx();
        printf("[relay] group message ACKed: %d (want 1)\n", ui_list_has("N0XYZ ACK"));
        ui_page(4);
        ui_press(1); // AUTO off

        // The Inbox: relayed message shown with its path, Reply goes back along it.
        ui_page(3);
        ui_press(4);
        pump(300);
        printf("[relay] inbox shows the path: %d, the group message: %d\n", ui_popup_has("VE7ABC via W1ABC"),
               ui_popup_has("NET TONIGHT"));
        screenshot("51_inbox_relayed.ppm");
        printf("[relay] on the relayed one: %d\n", focus_on("VE7ABC via W1ABC"));
        ui_click_focused();
        pump(300);
        printf("[relay] view focused '%s' (want Reply: MSG to VE7ABC via W1ABC)\n", ui_focused_text());
        screenshot("52_inbox_relayed_view.ppm");
        ui_click_focused();
        pump(300);
        printf("[relay] reply prefill '%s' (want W1ABC>VE7ABC MSG )\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);
        return 0;
    }
    if (getenv("ONLY_MODE")) {
        // Opened on 14.2 MHz in USB with a custom 27.245 MHz (CB) saved, that
        // band last used in USB: the app must still run in USB-D (the mode
        // keys are locked while it's open). A beta tester on CB was stuck in USB.
        pump(300);
        // A CB frequency is no amateur band: the [msg] lines above end with
        // "JS8 Custom", not the nearest preset's band (VE7NHW: "JS8 10m").
        printf("[mode] opened: dial %d, mode %d (want 27245000, %d USB-D)\n", stub_dial_hz(), stub_mode(),
               stub_usb_dig());
        open_freq();
        for (int i = 0; i < 10 && !strstr(ui_focused_text(), "Custom"); i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        ui_compose_clear();
        ui_compose_append("3575.8");
        ui_compose_enter();
        pump(300);
        printf("[mode] custom 3575.8: dial %d, mode %d (want 3575800, %d)\n", stub_dial_hz(), stub_mode(),
               stub_usb_dig());
        return 0;
    }
    if (getenv("ONLY_LOCK")) {
        // Hold MFK on a station: locked, turning only scrolls; a press on
        // another station selects it and unlocks; holding the locked one
        // unlocks it. Through LVGL's encoder, as the radio's MFK.
        ui_indevs_init();
        auto mfk = [](int ms) {
            ui_mfk_set(true);
            pump(ms);
            ui_mfk_set(false);
            pump(200);
        };
        auto selected = []() {
            static char c[32];
            c[0] = 0;
            dialog_js8_selected_call(c, sizeof(c));
            return (const char *)c;
        };
        auto cursor_to = [&](const char *call) { // up to the top, then down to it
            for (int dir : {-1, 1})
                for (int i = 0; i < 30 && !strstr(ui_cursor_text(), call); i++) {
                    ui_mfk_turn(dir);
                    pump(80);
                }
            return strstr(ui_cursor_text(), call) != nullptr;
        };
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO FROM THE PARK", 1320, 0.05f},
                   {"W1ABC", "FN42", "", "@ALLCALL CQ CQ FN42", 1800, 0.05f},
                   {"VE7ABC", "CN89", "K2XYZ", "K2XYZ SNR?", 900, 0.05f}});
        printf("[lock] objects the knob reaches: %d (1: a hold can't leave the list)\n", ui_group_count());
        bool on = cursor_to("N0XYZ: ");
        printf("[lock] cursor on N0XYZ: %d, selected '%s' (turning selects)\n", on, selected());
        mfk(900); // hold
        printf("[lock] after a hold: selected '%s'\n", selected());
        screenshot("60_locked.ppm");
        for (int i = 0; i < 3; i++) {
            ui_mfk_turn(1);
            pump(80);
        }
        printf("[lock] turned 3: cursor '%.20s', selected '%s' (want N0XYZ)\n", ui_cursor_text(), selected());
        on = cursor_to("W1ABC: ");
        printf("[lock] cursor on W1ABC: %d, selected '%s' (still N0XYZ)\n", on, selected());
        mfk(150); // a press on another station
        printf("[lock] pressed W1ABC: selected '%s' (want W1ABC)\n", selected());
        on = cursor_to("VE7ABC: ");
        printf("[lock] unlocked, turning selects again: found %d, cursor '%s', selected '%s' (want VE7ABC)\n", on,
               ui_cursor_text(), selected());
        mfk(900); // lock VE7ABC
        cursor_to("N0XYZ: ");
        printf("[lock] VE7ABC locked, cursor on N0XYZ: selected '%s' (want VE7ABC)\n", selected());
        cursor_to("VE7ABC: ");
        mfk(900); // hold the locked one: unlock
        cursor_to("W1ABC: ");
        printf("[lock] held again, unlocked: selected '%s' (want W1ABC)\n", selected());
        // Opening the Stations view ends a lock; the station stays selected.
        cursor_to("N0XYZ: ");
        mfk(900); // lock N0XYZ
        ui_page(3);
        ui_press(3); // Stations
        pump(300);
        printf("[lock] Stations view: selected '%s' (want N0XYZ, kept)\n", selected());
        ui_mfk_turn(1);
        pump(200);
        if (!strcmp(selected(), "N0XYZ")) {
            ui_mfk_turn(-2);
            pump(200);
        }
        printf("[lock] Stations view, turned: selected '%s' (want another station: unlocked)\n", selected());
        return 0;
    }
    if (getenv("ONLY_SMS")) {
        // A text from a phone, passed on by an APRS gateway: Reply fills in
        // the phone number for the SMS gateway.
        pump(300);
        feed_band({{"VA7GW", "CN89", "", "@APRSIS MSG TO:K2XYZ @6045551234 2 WAY DE SMS", 1320, 0.05f},
                   {"W7GW", "DN17", "", "@APRSIS MSG TO:K2XYZ ACK04} DE SMS", 1800, 0.05f}});
        ui_page(3);
        ui_press(4); // Inbox
        pump(300);
        for (int i = 0; i < 4 && !strstr(ui_focused_text(), "@6045551234"); i++) ui_key(LV_KEY_RIGHT);
        printf("[sms] inbox on '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[sms] message view focused '%s' (want Reply by SMS to @6045551234)\n", ui_focused_text());
        screenshot("70_sms_message.ppm");
        ui_click_focused();
        pump(300);
        printf("[sms] reply prefill '%s' (want @APRSIS CMD :SMS      :@6045551234 )\n", ui_compose_text());
        ui_compose_append("GOT IT");
        ui_compose_enter();
        int b = stub_tx_frames;
        for (int i = 0; i < 300 && stub_tx_frames == b; i++) pump(100);
        printf("[sms] sent: %d (want @APRSIS CMD :SMS      :@6045551234 GOT IT{nn})\n",
               ui_list_has("@APRSIS CMD :SMS      :@6045551234 GOT IT{"));
        // Its id, as sent: "...GOT IT{07}".
        char id[8] = "";
        if (lv_obj_t *t = find_obj(lv_scr_act(), &lv_table_class))
            for (uint16_t r = 0; r < lv_table_get_row_cnt(t); r++) {
                const char *v = lv_table_get_cell_value(t, r, 0), *p = v ? strstr(v, "GOT IT{") : nullptr;
                if (p) sscanf(p + 7, "%7[0-9A-Z]", id);
            }
        for (int i = 0; i < 300 && stub_tx_keyed; i++) pump(100);
        pump(3500);
        // The gateway's receipt for it (bug hunt S7): reported, not an Inbox
        // message. (ACK04} above, for a message this radio didn't send, is
        // reported too, and kept out of the Inbox as well.)
        char receipt[64];
        snprintf(receipt, sizeof(receipt), "@APRSIS MSG TO:K2XYZ ACK%s} DE SMS", id);
        feed_band({{"W7GW", "DN17", "", receipt, 1800, 0.05f}});
        char want[80];
        snprintf(want, sizeof(want), "SMS {%s} to @6045551234 delivered", id);
        printf("[sms] receipt for {%s}: '%s' shown %d (want 1)\n", id, want, ui_list_has(want));
        printf("[sms] unknown receipt ACK04: reported %d (want 1)\n", ui_list_has("message {04} delivered"));
        ui_page(3);
        ui_press(4); // Inbox
        pump(300);
        printf("[sms] Inbox: receipts kept out %d, the phone's text there %d (want 1, 1)\n",
               ui_popup_has("ACK") == 0, ui_popup_has("@6045551234 2 WAY") == 1);
        ui_key(LV_KEY_ESC);
        pump(300);

        // 67 characters of text, the id on top (bug hunt S1: 63 before).
        std::string text67 = "@6045551234 " + std::string(55, 'A'); // 67
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        ui_compose_append(("@APRSIS CMD :SMS      :" + text67).c_str());
        stub_last_msg[0] = '\0';
        ui_compose_enter();
        pump(300);
        printf("[sms] 67 characters: '%s' (want Queued...)\n", stub_last_msg);
        for (int i = 0; i < 300 && !stub_tx_keyed; i++) pump(100);
        for (int i = 0; i < 400 && stub_tx_keyed; i++) pump(100);
        pump(3500);
        ui_press(3);
        pump(200);
        ui_compose_append(("@APRSIS CMD :SMS      :" + text67 + "B").c_str());
        ui_compose_enter();
        pump(300);
        printf("[sms] 68 characters: '%s' (want APRS allows 67...)\n", stub_last_msg);
        ui_compose_cancel();
        pump(300);

        // Winlink: no id (WLNK-1 answers with its own reply; an id only
        // brought a second ACK). SMS above still got one. First let the
        // 9-frame SMS finish (quiet for a whole slot).
        for (int quiet = 0, i = 0; quiet < 170 && i < 3000; i++) {
            pump(100);
            quiet = stub_tx_keyed ? 0 : quiet + 1;
        }
        ui_press(3); // Send...
        pump(200);
        ui_compose_append("@APRSIS CMD :WLNK-1   :SP TEST@EXAMPLE.COM HELLO");
        ui_compose_enter();
        for (int i = 0; i < 300 && !stub_tx_keyed; i++) pump(100);
        for (int i = 0; i < 400 && stub_tx_keyed; i++) pump(100);
        pump(3500);
        printf("[sms] Winlink sent without an id: %d, with one: %d (want 1, 0)\n",
               ui_list_has("WLNK-1   :SP TEST@EXAMPLE.COM HELLO") == 1 && ui_list_has("HELLO{") != 1,
               ui_list_has("HELLO{") == 1);
        return 0;
    }
    if (getenv("ONLY_FREQMARK")) {
        // Rows without a callsign (a QSO's later lines): the cursor on one
        // selects nobody but marks every row on its frequency; selecting a
        // station also marks the call-less rows on its frequency.
        ui_indevs_init();
        auto selected = []() {
            static char c[32];
            c[0] = 0;
            dialog_js8_selected_call(c, sizeof(c));
            return (const char *)c;
        };
        auto cursor_to = [&](const char *text) {
            for (int dir : {-1, 1})
                for (int i = 0; i < 30 && !strstr(ui_cursor_text(), text); i++) {
                    ui_mfk_turn(dir);
                    pump(80);
                }
            return strstr(ui_cursor_text(), text) != nullptr;
        };
        char marked[2048];
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO FROM THE PARK", 1320, 0.05f},
                   {"W1ABC", "FN42", "VE7ABC", "VE7ABC GOOD COPY", 1800, 0.05f}});
        feed_band({{"N0XYZ", "EN34", "", "RR THANKS FOR THE REPORT", 1322, 0.05f},
                   {"W1ABC", "FN42", "", "NICE SIGNAL HERE", 1801, 0.05f}});
        bool on = cursor_to("N0XYZ: ");
        int  n  = ui_marked_rows(marked, sizeof(marked));
        printf("[fmark] on N0XYZ: %d, selected '%s', %d marked: %s\n", on, selected(), n, marked);
        printf("[fmark]   (want N0XYZ's row and RR THANKS..., not W1ABC's)\n");
        on = cursor_to("W1ABC: ");
        for (int i = 0; i < 5 && !strstr(ui_cursor_text(), "RR THANKS"); i++) { // step down onto the next rows
            ui_mfk_turn(1);
            pump(80);
        }
        n = ui_marked_rows(marked, sizeof(marked));
        printf("[fmark] W1ABC selected, stepped onto '%.40s' (no call): selected '%s' (want W1ABC), %d marked: %s\n",
               ui_cursor_text(), selected(), n, marked);
        printf("[fmark]   (want N0XYZ's HELLO and RR THANKS: that row's frequency)\n");
        screenshot("61_freq_marked.ppm");
        on = cursor_to("W1ABC: ");
        n  = ui_marked_rows(marked, sizeof(marked));
        printf("[fmark] on W1ABC: %d, selected '%s', %d marked: %s\n", on, selected(), n, marked);
        return 0;
    }
    if (getenv("ONLY_LOOKS")) {
        // Commands in colour, end marks, relay stations "via", bearing,
        // km / miles, and the Settings lines for them.
        pump(300);
        feed_band({{"N0XYZ", "EN34", "", "N0XYZ: @HB HEARTBEAT EN34", 700, 0.05f},
                   {"VE7ABC", "CN89", "", "@ALLCALL CQ CQ CQ CN89", 1800, 0.05f},
                   {"W1ABC", "FN42", "K2XYZ", "K2XYZ SNR?", 1320, 0.05f}});
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ>HELLO *DE* VE7XYZ", 1320, 0.05f},
                   {"G4ABC", "IO91", "", "G4ABC: @ALLCALL CQ CQ CQ IO91", 2200, 0.05f}});
        pump(500);
        screenshot("70_messages.ppm");
        ui_page(3);
        ui_press(3); // Show Stations
        pump(500);
        screenshot("71_stations_km.ppm");
        ui_page(4);
        ui_press(4); // Settings
        pump(200);
        for (int i = 0; i < 25 && !strstr(ui_focused_text(), "Distance"); i++) ui_key(LV_KEY_RIGHT);
        printf("[looks] on '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[looks] after a press: '%s' (want Distance: miles)\n", ui_focused_text());
        for (int i = 0; i < 25 && !strstr(ui_focused_text(), "Stations kept"); i++) ui_key(LV_KEY_LEFT);
        printf("[looks] '%s'", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf(" -> '%s' (want 2 hours)\n", ui_focused_text());
        ui_key(LV_KEY_RIGHT);
        printf("[looks] '%s'", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf(" -> '%s' (want 15 min)\n", ui_focused_text());
        screenshot("72_settings.ppm");
        ui_key(LV_KEY_ESC);
        pump(300);
        screenshot("73_stations_miles.ppm");
        return 0;
    }
    if (getenv("ONLY_OPERATOR")) {
        // Settings: an operator call, logged as OPERATOR (desktop's), shown
        // in the Log popup; the station call stays on the air.
        unlink(JS8_LOG_PATH);
        pump(300);
        ui_page(4);
        ui_press(4); // Settings
        pump(200);
        for (int i = 0; i < 25 && !strstr(ui_focused_text(), "Operator"); i++) ui_key(LV_KEY_RIGHT);
        printf("[op] '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        ui_compose_append("va7-xyz");
        ui_compose_enter();
        pump(200);
        printf("[op] bad call: keyboard still open %d\n", ui_compose_text()[0] != 0);
        ui_compose_clear();
        ui_compose_append("va7xyz");
        ui_compose_enter();
        pump(300);
        ui_press(4);
        pump(200);
        for (int i = 0; i < 25 && !strstr(ui_focused_text(), "Operator"); i++) ui_key(LV_KEY_RIGHT);
        printf("[op] saved: '%s' (want Operator: VA7XYZ)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(5);
        ui_press(2); // Log QSO
        pump(300);
        printf("[op] log popup shows it: %d, focused '%s'\n", ui_popup_has("Operator VA7XYZ (station K2XYZ)"),
               ui_focused_text());
        screenshot("74_log_operator.ppm");
        ui_click_focused(); // Save to log
        pump(300);
        FILE *f = fopen(JS8_LOG_PATH, "r");
        char  line[1024];
        while (f && fgets(line, sizeof(line), f))
            if (strstr(line, "<call:")) printf("[op] logged: %s", strstr(line, "<station_callsign"));
        if (f) fclose(f);
        return 0;
    }
    if (getenv("ONLY_PARTIAL")) {
        // A long message shows as it arrives, one decode cycle at a time.
        pump(300);
        std::vector<Station> st = {
            {"N0XYZ", "EN34", "K2XYZ", "K2XYZ THIS IS A LONG MESSAGE THAT TAKES SEVERAL FRAMES TO ARRIVE", 1320, 0.05f}};
        feed_band(st, 0, 2); // the first two frames
        printf("[partial] after 2 frames: growing row %d, complete row %d\n", ui_list_has("N0XYZ: K2XYZ THIS"),
               ui_list_has("TO ARRIVE"));
        printf("[partial] marked in progress: %d\n", ui_list_has(" ..."));
        screenshot("38_partial.ppm");
        feed_band(st, 2);
        printf("[partial] after the rest: complete %d, still marked %d\n",
               ui_list_has("N0XYZ: K2XYZ THIS IS A LONG MESSAGE THAT TAKES SEVERAL FRAMES TO ARRIVE"),
               ui_list_has(" ..."));
        printf("[partial] rows with the message: %d (want 1: updated in place)\n", ui_list_count("N0XYZ: K2XYZ THIS"));
        screenshot("39_partial_done.ppm");
        return 0;
    }
    if (getenv("ONLY_AUTOCQ")) {
        // 2. Auto CQ: hold CQ; a CQ a minute until someone answers.
        pump(300);
        ui_page(1);
        ui_hold(1);
        pump(300);
        printf("[autocq] after hold: '%s' (want CQ: knob < 1 min >)\n", ui_button_label(1));
        ui_rotary(1); // 2 min
        pump(100);
        printf("[autocq] knob: '%s' (want < 2 min >)\n", ui_button_label(1));
        ui_press(1); // done setting, auto CQ keeps going
        pump(300);
        printf("[autocq] after press: '%s' (want sending)\n", ui_button_label(1));
        // The interval counts from the END of the CQ, not from when it was queued.
        for (int i = 0; i < 90 && strstr(ui_button_label(1), "sending"); i++) pump(1000);
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        int frames = stub_tx_frames;
        pump(1500);
        printf("[autocq] just after it ended: '%s' (want ~1:59)\n", ui_button_label(1));
        for (int i = 0; i < 1800 && stub_tx_frames == frames; i++) pump(100);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double gap = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        printf("[autocq] end of CQ to next CQ on air: %.1f s (want 120-135: 2 min, then the next slot)\n", gap);
        printf("[autocq] the next one: CQ rows %d (want 2)\n", ui_list_count("CQ CQ CQ"));
        screenshot("u02_autocq.ppm");
        // Someone answers: auto CQ off.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        printf("[autocq] after N0XYZ answered: '%s' (want CQ), info row %d\n", ui_button_label(1),
               ui_list_has("Auto CQ off: N0XYZ answered"));
        // Press CQ goes back to manual.
        for (int i = 0; i < 40; i++) pump(500); // let any TX finish
        ui_hold(1);
        pump(300);
        printf("[autocq] hold again: '%s' (want < 2 min >: remembered)\n", ui_button_label(1));
        ui_press(1); // done setting
        pump(300);
        ui_hold(1); // auto CQ on: hold sets the interval again
        pump(300);
        printf("[autocq] hold while on: '%s' (want knob)\n", ui_button_label(1));
        ui_key(LV_KEY_ESC); // ESC ends setting, auto CQ stays on
        pump(300);
        printf("[autocq] after ESC: running %d\n", strstr(ui_button_label(1), "CQ auto") != NULL);
        ui_press(1); // stop
        pump(300);
        printf("[autocq] then press: '%s' (want CQ), info row %d\n", ui_button_label(1),
               ui_list_has("Auto CQ off: manual"));

        // Heartbeat works the same way: hold = auto (one now), press = off.
        for (int i = 0; i < 40; i++) pump(500);
        ui_page(1);
        int hb0 = stub_tx_frames;
        ui_hold(2);
        printf("[hb] held Heartbeat: '%s' (want HB: knob), info row %d\n", ui_button_label(2), ui_list_has("Auto HB on"));
        ui_press(2); // done setting
        for (int i = 0; i < 300 && stub_tx_frames == hb0; i++) pump(100);
        printf("[hb] one sent at once: %d (want 1), button '%s' (want HB auto: countdown)\n", stub_tx_frames - hb0,
               ui_button_label(2));
        for (int i = 0; i < 50; i++) pump(100);
        ui_press(2); // auto off
        pump(300);
        printf("[hb] then press: '%s' (want Heart-beat), info row %d\n", ui_button_label(2),
               ui_list_has("Auto HB off: manual"));
        return 0;
    }
    if (getenv("ONLY_SWR")) {
        // High-SWR guard: over 3:1 for half a second while keyed turns AUTO,
        // auto HB, HB ACK and auto CQ off; the message carries on; three
        // beeps once TX is done. A short spike as it keys doesn't.
        auto autos = [](const char *when) {
            bool a, hb, ack, cq;
            dialog_js8_autos(&a, &hb, &ack, &cq);
            printf("[swr] %s: AUTO %d, HB %d, HB ACK %d, auto CQ %d\n", when, a, hb, ack, cq);
        };
        auto beeps = [](const char *when) {
            int last  = 0;
            int count = dialog_js8_beeps(&last);
            printf("[swr] %s: beep sounds %d, last %d beep(s)\n", when, count, last);
        };
        auto wait_keyed = [] { for (int i = 0; i < 300 && !stub_tx_keyed; i++) pump(100); };
        auto wait_done  = [] { // unkeyed for 3 s: the message's last frame is over
            for (int i = 0, quiet = 0; i < 900 && quiet < 30; i++) {
                pump(100);
                quiet = stub_tx_keyed ? 0 : quiet + 1;
            }
        };
        pump(300);
        ui_page(4);
        ui_press(1); // AUTO on
        ui_press(3); // HB ACK on
        ui_page(1);
        ui_hold(1);  // auto CQ: one now
        ui_press(1); // done setting its minutes
        ui_hold(2);  // auto HB (busy: the first an interval from now)
        ui_press(2);
        autos("all on (want 1 1 1 1)");
        int last   = 0;
        int beeps0 = dialog_js8_beeps(&last);
        wait_keyed();
        printf("[swr] keyed: %d (want 1)\n", stub_tx_keyed);
        stub_tx_swr = 4.5f; // a spike, under half a second
        pump(300);
        stub_tx_swr = 1.3f;
        pump(400);
        autos("after a 0.3 s spike (want 1 1 1 1)");
        stub_tx_swr = 3.0f; // exactly 3:1 is not over it
        pump(800);
        autos("at 3.0:1 for 0.8 s (want 1 1 1 1)");
        int frames = stub_tx_frames;
        stub_tx_swr = 3.4f;
        pump(800);
        autos("at 3.4:1 for 0.8 s (want 0 0 0 0)");
        printf("[swr] still keyed: %d (want 1), aborted %d (want 0), row %d (want 1), msg '%s'\n", stub_tx_keyed,
               stub_tx_aborted, ui_list_has("High SWR 3.4:1"), stub_last_msg);
        beeps("while keyed (want no new sound)");
        stub_tx_swr = 1.3f;
        wait_done();
        printf("[swr] the message went out: frames from the trip on %d (want >= 1), aborted %d (want 0)\n",
               stub_tx_frames - frames + 1, stub_tx_aborted);
        int count = dialog_js8_beeps(&last);
        printf("[swr] after TX: new beep sounds %d (want 1), %d beep(s) (want 3)\n", count - beeps0, last);
        pump(5000);
        autos("5 s later (want 0 0 0 0)");
        frames = stub_tx_frames;
        pump(20000);
        printf("[swr] 20 s more: frames sent %d (want 0)\n", stub_tx_frames - frames);
        // No autos on: high SWR on a CQ by hand changes nothing, no beep.
        count = dialog_js8_beeps(&last);
        ui_press(1); // CQ
        wait_keyed();
        stub_tx_swr = 4.0f;
        pump(1000);
        stub_tx_swr = 1.3f;
        wait_done();
        printf("[swr] high SWR, nothing automatic on: new beep sounds %d (want 0)\n", dialog_js8_beeps(&last) - count);
        return 0;
    }
    if (getenv("ONLY_HBPAUSE")) {
        // Page 1's Heartbeat works as CQ: press = one now, hold = auto (one
        // now, the knob sets the minutes), press while auto = off. Auto
        // heartbeats pause for what you send by hand (not a heartbeat, not a
        // CQ) and when someone calls you; they come back 10 min after.
        // Wait for a queued transmission to key (it waits for its slot), then end.
        auto tx_done = [](int before) {
            for (int i = 0; i < 300 && stub_tx_frames == before; i++) pump(100);
            for (int i = 0; i < 300 && stub_tx_keyed; i++) pump(100);
            pump(3500);
        };
        pump(300);
        ui_page(4);
        printf("[hbpause] page 4: '%s' | %s | '%s' | '%s' (want AUTO | (none) | HB ACK | Settings)\n", ui_button_label(1),
               ui_button_exists(2) ? "a button" : "(none)", ui_button_label(3), ui_button_label(4));
        ui_press(1); // AUTO on
        ui_press(3); // HB ACK on
        ui_page(1);
        printf("[hbpause] Heartbeat button: '%s' (want Heart-beat)\n", ui_button_label(2));
        int sent = stub_tx_frames;
        ui_hold(2); // auto HB: one now, and the knob sets the minutes
        printf("[hbpause] held: '%s' (want HB: knob), info row %d (want 1)\n", ui_button_label(2),
               ui_list_has("Auto HB on"));
        ui_press(2); // done setting
        tx_done(sent);
        printf("[hbpause] one sent at once: %d (want 1), button '%s' (want HB auto: a countdown)\n", stub_tx_frames - sent,
               ui_button_label(2));
        // A CQ by hand, then auto CQ: heartbeats carry on.
        sent = stub_tx_frames;
        ui_press(1); // CQ
        tx_done(sent);
        printf("[hbpause] after a CQ: '%s' (want a countdown, not paused)\n", ui_button_label(2));
        ui_hold(1); // auto CQ
        ui_press(1); // done setting its minutes
        tx_done(stub_tx_frames);
        ui_press(1); // auto CQ off
        printf("[hbpause] after auto CQ: '%s' (want a countdown), paused row %d (want 0)\n", ui_button_label(2),
               ui_list_has("HB and HB ACK paused"));
        // Someone calls us: paused, the switches stay on.
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        pump(300);
        printf("[hbpause] N0XYZ called: '%s' (want HB auto: paused), row %d (want 1)\n", ui_button_label(2),
               ui_list_has("paused 10 min: N0XYZ called"));
        ui_page(4);
        printf("[hbpause] HB ACK: '%s' (want paused)\n", ui_button_label(3));
        // Someone's heartbeat while paused: no HB ACK.
        int frames = stub_tx_frames;
        feed_band({{"W7XYZ", "DM43", "", "W7XYZ: HEARTBEAT DM43", 1800, 0.04f}});
        pump(3000);
        printf("[hbpause] heartbeat heard while paused: frames sent %d (want 0)\n", stub_tx_frames - frames);
        // 11 min later (JS8 time; a whole number of slots): back by itself.
        dialog_js8_time_auto(false); // Auto would put the drift back within a slot
        js8_set_drift_ms(11 * 60 * 1000);
        pump(1500);
        ui_page(1);
        printf("[hbpause] 11 min later: '%s' (want a countdown)\n", ui_button_label(2));
        js8_set_drift_ms(0);
        dialog_js8_time_auto(true);
        // HW CPY? by hand pauses them again.
        ui_select_row_from("N0XYZ");
        ui_press(4); // HW CPY?
        pump(300);
        printf("[hbpause] after HW CPY?: '%s' (want paused), row %d (want 1)\n", ui_button_label(2),
               ui_list_has("paused 10 min: you sent"));
        tx_done(stub_tx_frames);
        // Hold while paused: carries on, the knob sets the minutes.
        ui_hold(2);
        ui_rotary(-1);
        printf("[hbpause] held while paused: '%s' (want HB: knob < 29 min >)\n", ui_button_label(2));
        ui_press(2);
        printf("[hbpause] done: '%s' (want a countdown)\n", ui_button_label(2));
        // Press while auto: off.
        ui_press(2);
        pump(200);
        printf("[hbpause] pressed: '%s' (want Heart-beat), row %d (want 1)\n", ui_button_label(2),
               ui_list_has("Auto HB off: manual"));
        // Off, a press is one heartbeat, nothing after it.
        sent = stub_tx_frames;
        ui_press(2);
        tx_done(sent);
        printf("[hbpause] one by hand: %d sent (want 1), button '%s' (want Heart-beat)\n", stub_tx_frames - sent,
               ui_button_label(2));
        return 0;
    }
    if (getenv("ONLY_MAPSTACK")) {
        // A busy band where stations share a spot (the same grid square,
        // or the same call area for those without a grid): one count tag
        // on the mark's bottom right corner, the CQ tag still on the top
        // right.
        ui_indevs_init(); // the MFK
        pump(300);
        feed_band({{"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 600, 0.05f},
                   {"K9GHI", "EN52", "", "@HB HEARTBEAT EN52", 1000, 0.05f},
                   {"W9JKL", "EN52", "", "@ALLCALL CQ CQ EN52", 1400, 0.05f},
                   {"W7XYZ", "DM43", "", "@HB HEARTBEAT DM43", 1800, 0.05f},
                   {"N7ABC", "DM43", "", "@HB HEARTBEAT DM43", 2200, 0.05f},
                   {"VE3KP", "FN03", "", "@HB HEARTBEAT FN03", 2600, 0.05f}});
        feed_band({{"W6AAA", "", "W7XYZ", "W7XYZ HW CPY?", 600, 0.05f},
                   {"K6BBB", "", "N7ABC", "N7ABC SNR?", 1000, 0.05f},
                   {"VE7AAA", "CN89", "", "@HB HEARTBEAT CN89", 1400, 0.05f},
                   {"VA7BBB", "CN89", "", "@HB HEARTBEAT CN89", 1800, 0.05f},
                   {"K5LOW", "EM12", "", "@ALLCALL CQ CQ EM12", 2200, 0.05f},
                   {"N0XYZ", "", "K2XYZ", "K2XYZ HEARTBEAT SNR -12", 2600, 0.05f}});
        pump(1000);
        ui_page(3);
        ui_press(3); // Show Stations
        pump(300);
        ui_press(3); // Show Map
        pump(800);
        const char *tags = "";
        int         n    = dialog_js8_map_stacks(&tags);
        printf("[mapstack] %d count tag(s): %s (want EN52:3 DM43:2 CN89:2, US call area 6:2)\n", n, tags);
        screenshot("w0_map_stack.ppm");
        // Select one of a stack: still one tag, the count unchanged.
        for (int i = 0; i < 12; i++) {
            char sel[16] = "";
            dialog_js8_selected_call(sel, sizeof(sel));
            if (strcmp(sel, "K9GHI") == 0) break;
            ui_mfk_turn(1);
            pump(300);
        }
        pump(600);
        char sel[16] = "";
        dialog_js8_selected_call(sel, sizeof(sel));
        n = dialog_js8_map_stacks(&tags);
        printf("[mapstack] selected %s: %d count tag(s): %s\n", sel, n, tags);
        screenshot("w1_map_stack_selected.ppm");
        // Heard me only: N0XYZ alone, no tags.
        ui_page(2);
        ui_press(1); // Show: Heard me
        pump(600);
        n = dialog_js8_map_stacks(&tags);
        printf("[mapstack] heard me only: %d count tag(s) '%s' (want 0)\n", n, tags);
        return 0;
    }
    if (getenv("ONLY_MAP")) {
        // Show Map (docs/MAP_PLAN.md): the third view, stations placed by
        // grid or callsign, the view button, the Show filter, DX switching
        // to the world view, new-station pop-ups, Time Sync in Settings.
        auto state = [](const char *what) {
            bool world = false, tx = false;
            int  pops = 0, qso = 0, qrz = 0;
            bool on   = dialog_js8_map_state(&world, &pops, &tx, &qso, &qrz);
            printf("[map] %s: map %s, %s view, %d ring(s)%s, %d red path(s), QRZ %d\n", what, on ? "on" : "off",
                   world ? "world" : "close-in", pops, tx ? ", TX outline" : "", qso, qrz);
        };
        // The QSO log the map reads for NEW grids/countries: W7XYZ in DM43
        // and VE3KP in FN03 worked (so the US and Canada aren't new), EN52
        // and Japan never.
        {
            sqlite3 *db = nullptr;
            std::remove(JS8_QSO_DB_PATH);
            if (sqlite3_open(JS8_QSO_DB_PATH, &db) == SQLITE_OK)
                sqlite3_exec(db,
                             "CREATE TABLE qso_log (remote_callsign TEXT, remote_grid TEXT);"
                             "INSERT INTO qso_log VALUES ('W7XYZ', 'DM43'), ('VE3KP', 'FN03nq'), ('N0XYZ', NULL);",
                             nullptr, nullptr, nullptr);
            sqlite3_close(db);
        }
        ui_indevs_init(); // the MFK
        pump(300);
        // W7XYZ, K9DEF and VE3KP send grids; N0XYZ reports our signal
        // (heard us, no grid: US call area 0); VE6ABC asks SNR? (Alberta).
        feed_band({{"W7XYZ", "DM43", "", "@HB HEARTBEAT DM43", 800, 0.05f},
                   {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1200, 0.05f},
                   {"VE3KP", "FN03", "", "@HB HEARTBEAT FN03", 1600, 0.05f},
                   {"N0XYZ", "", "K2XYZ", "K2XYZ HEARTBEAT SNR -12", 2000, 0.05f},
                   {"VE6ABC", "", "K2XYZ", "K2XYZ SNR?", 2400, 0.05f},
                   {"K5LOW", "EM12", "", "@ALLCALL CQ CQ EM12", 2700, 0.004f}}); // weak: a smaller square
        ui_page(3);
        printf("[map] page 3 slot 2 before the map: '%s' (want empty)\n", ui_button_label(1));
        printf("[map] stations button: '%s'\n", ui_button_label(3));
        ui_press(3); // Show Stations
        pump(300);
        screenshot("89_stations_cq.ppm"); // K5LOW called CQ: its row is green for 5 min
        printf("[map] then: '%s' (want Show Map)\n", ui_button_label(3));
        ui_press(3); // Show Map
        pump(500);
        state("opened");
        printf("[map] view button: '%s' (want Map: Auto), stations button '%s' (want Show Messages)\n",
               ui_button_label(1), ui_button_label(3));
        printf("[map] stats: '%s' (want 6 heard  2 hear you  DX ...)\n", dialog_js8_map_stats());
        auto extra = [](const char *what) {
            int         talks = 0, dot = -1, in = 0;
            const char *s0 = "", *s1 = "";
            bool        follow = false;
            dialog_js8_map_extra(&talks, &dot, &in, &s0, &s1, &follow);
            printf("[map] %s: %d grey line(s), my dot %s%d, %d incoming dot(s) so far%s\n      strip: '%s'\n             '%s'\n",
                   what, talks, dot < 0 ? "hidden " : "at x ", dot, in, follow ? ", following" : "", s0, s1);
            return dot;
        };
        extra("opened (want the strip: K2XYZ SNR? from VE6ABC last... as the list shows)");
        screenshot("90_map.ppm");
        ui_mfk_turn(1);
        pump(400);
        ui_mfk_turn(1);
        pump(400);
        char sel[16] = "";
        dialog_js8_selected_call(sel, sizeof(sel));
        printf("[map] MFK selected %s\n", sel);
        screenshot("91_map_selected.ppm");
        ui_page(2);
        ui_press(1); // Show: Heard me
        pump(400);
        printf("[map] Show in the map: '%s' (want Heard me)\n", ui_button_label(1));
        screenshot("92_map_heard_me.ppm");
        ui_press(1); // back to All heard
        pump(300);
        ui_page(3);
        ui_press(1); // Map: Close-in
        pump(300);
        ui_press(1); // Map: World
        pump(500);
        state("World pressed");
        printf("[map] view button: '%s' (want Map: World)\n", ui_button_label(1));
        screenshot("93_map_world.ppm");
        ui_press(1); // back to Auto
        pump(300);
        // Other stations' QSOs: grey lines, a relay's hops too.
        feed_band({{"W7XYZ", "DM43", "K9DEF", "K9DEF HW CPY?", 800, 0.05f},
                   {"VE3KP", "FN03", "N0XYZ", "N0XYZ>K5LOW HELLO", 1600, 0.05f}},
                  0, 99, 0, 0);
        pump(600);
        extra("W7XYZ to K9DEF, VE3KP relays via N0XYZ to K5LOW (want 3 grey lines, strip = those two)");
        screenshot("9b_map_talk.ppm");
        // Transmitting: our square gets a red outline, gone when it ends.
        ui_page(1);
        int frames = stub_tx_frames;
        ui_press(1); // CQ
        for (int i = 0; i < 200 && !stub_tx_keyed; i++) pump(100);
        pump(600);
        state("CQ keyed (want TX outline)");
        screenshot("97_map_tx.ppm");
        for (int i = 0; i < 300 && (stub_tx_keyed || stub_tx_frames == frames); i++) pump(100);
        pump(1500);
        state("CQ sent (want no outline; the opening messages to us may still be red, 30 s)");
        // A station we know messages us: its path turns red, it rings, and
        // QRZ shows it; a heartbeat reply to us at the same time doesn't.
        feed_band({{"K9DEF", "EN52", "K2XYZ", "K2XYZ HOW COPY MY SIGNAL", 1200, 0.05f},
                   {"VE3KP", "FN03", "K2XYZ", "K2XYZ HEARTBEAT SNR -05", 1700, 0.05f}},
                  0, 99, 0, 0);
        pump(600);
        extra("just after (want 4 so far: K9DEF's frames and VE3KP's reply, one dot each)");
        // (VE3KP's one-frame reply came in the first slot: its 30 s of red
        // are over by the end of K9DEF's three frames, and it never rang.)
        state("K9DEF messaged us, VE3KP HB reply (want 1 red path, 1 ring, QRZ 1)");
        screenshot("98_map_incoming.ppm");
        for (int i = 0; i < 40; i++) pump(1000); // past the 30 s
        state("40 s later (want no red path)");
        // We message the selected station (K5LOW): its path is red while
        // we send, and a white dot runs along it as the message goes out.
        frames = stub_tx_frames;
        ui_press(4); // HW CPY? to W7XYZ
        for (int i = 0; i < 200 && !stub_tx_keyed; i++) pump(100);
        pump(600);
        state("HW CPY? to K5LOW keyed (want TX outline, 1 red path)");
        int dot0 = extra("keyed (want my dot on the path to the selected station, K5LOW)");
        screenshot("99_map_outgoing.ppm");
        int dot1 = dot0;
        for (int i = 0; i < 20 && stub_tx_keyed; i++) {
            pump(500);
            int x = extra("sending");
            if (x >= 0) dot1 = x;
        }
        printf("[map] my dot moved %d px while keyed (want < 0: west, toward K5LOW)\n", dot1 - dot0);
        for (int i = 0; i < 300 && (stub_tx_keyed || stub_tx_frames == frames); i++) pump(100);
        pump(1500);
        state("sent (want no red path, QRZ still 1: K5LOW didn't call)");
        // A station in Japan: Auto switches to the world, and it pops up.
        feed_band({{"JA1ABC", "PM95", "", "@HB HEARTBEAT PM95", 1400, 0.05f}}, 0, 99, 0, 0);
        pump(300);
        state("JA1ABC heard");
        printf("[map] stats: '%s' (want DX JA1ABC)\n", dialog_js8_map_stats());
        screenshot("94_map_dx_popup.ppm");
        pump(9000);
        state("9 s later");
        screenshot("95_map_popup_gone.ppm");
        // Follow (hold Map:): you and the selected station, JA1ABC.
        for (int i = 0; i < 20 && strcmp(sel, "JA1ABC") != 0; i++) {
            ui_mfk_turn(i < 10 ? 1 : -1); // down the list, then up
            pump(300);
            dialog_js8_selected_call(sel, sizeof(sel));
        }
        ui_page(3);
        ui_hold(1);
        pump(500);
        printf("[map] held Map: '%s' (want Map: Follow), selected %s\n", ui_button_label(1), sel);
        extra("following");
        screenshot("9c_map_follow.ppm");
        ui_press(1); // leaves Follow
        pump(300);
        printf("[map] pressed: '%s' (want Map: Auto)\n", ui_button_label(1));
        // Half an hour on: the others fade; a fresh one doesn't; K5LOW's CQ
        // tag is long gone.
        dialog_js8_time_auto(false); // Auto would put the half hour back within a slot
        js8_set_drift_ms(30 * 60 * 1000);
        feed_band({{"W7XYZ", "DM43", "", "@HB HEARTBEAT DM43", 800, 0.05f}}, 0, 99, 0, 0);
        pump(1500);
        screenshot("9a_map_faded.ppm");
        js8_set_drift_ms(0);
        dialog_js8_time_auto(true);
        pump(300);
        // Time Sync is page 4's Time button; its reset is first in Settings.
        ui_page(4);
        ui_press(4); // Settings
        pump(300);
        printf("[map] Settings opens on '%s' (want Reset time drift)\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        ui_key(LV_KEY_ESC);
        pump(300);
        // Back to the messages: the waterfall returns.
        ui_page(3);
        ui_press(3); // Show Messages
        pump(400);
        state("Show Messages (want QRZ 0)");
        printf("[map] view button off the map: '%s' (want empty), stations button '%s' (want Show Stations)\n",
               ui_button_label(1), ui_button_label(3));
        screenshot("96_back_to_messages.ppm");
        return 0;
    }
    if (getenv("ONLY_MARKS")) {
        // Decode marks (Settings): brackets where the decoder tries (by sync
        // strength) and yellow where it decoded; off by default.
        auto marks = [](const char *what) {
            float   f[64];
            uint8_t l[64];
            unsigned n = dialog_js8_marks(f, l, 64);
            printf("[marks] %s: %u bracket(s):", what, n);
            for (unsigned i = 0; i < n; i++) printf(" %.0f/%s", f[i], l[i] == 3 ? "decoded" : l[i] == 2 ? "white" : l[i] == 1 ? "cyan" : "dim");
            printf("\n");
            return n;
        };
        auto near = [](float hz, int want_level) {
            float   f[64];
            uint8_t l[64];
            unsigned n = dialog_js8_marks(f, l, 64);
            for (unsigned i = 0; i < n; i++)
                if (fabsf(f[i] - hz) < 30 && (want_level < 0 ? l[i] != 3 : l[i] == want_level)) return 1;
            return 0;
        };
        pump(300);
        feed_band({{"N0XYZ", "EN34", "", "@HB HEARTBEAT EN34", 1320, 0.05f}});
        pump(1500);
        marks("switched off (default)");
        ui_page(4);
        ui_press(4); // Settings
        pump(200);
        for (int i = 0; i < 25 && !strstr(ui_focused_text(), "Decode marks"); i++) ui_key(LV_KEY_RIGHT);
        printf("[marks] on '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(200);
        printf("[marks] after a press: '%s' (want Decode marks: On)\n", ui_focused_text());
        screenshot("80_marks_setting.ppm");
        ui_key(LV_KEY_ESC);
        pump(200);
        // A station that decodes, and weaker ones: the decoder finds their
        // sync but can't decode them (or barely).
        float weak = getenv("MARKS_WEAK") ? (float)atof(getenv("MARKS_WEAK")) : 0.0022f;
        feed_band({{"W1ABC", "FN42", "", "@HB HEARTBEAT FN42", 1000, 0.05f},
                   {"K9DEF", "EN52", "", "@HB HEARTBEAT EN52", 1800, weak},
                   {"G4XYZ", "IO91", "", "@HB HEARTBEAT IO91", 2300, weak * 0.8f}},
                  0, 99, 0, 0);
        // The decode comes as the signals end: its brackets cross the clear
        // strip at the top in the next ~3 s.
        for (int i = 0; i < 100 && !near(1000, 3); i++) pump(50);
        pump(700);
        screenshot("81_marks.ppm");
        pump(2900);
        marks("after a slot");
        printf("[marks] decoded at 1000 Hz: %d, at 1800 Hz (weak): %d (want 1, 1)\n", near(1000, 3), near(1800, 3));
        printf("[marks] at 2300 Hz (weaker): tried %d, decoded %d (want 1, 0)\n", near(2300, -1), near(2300, 3));
        screenshot("82_marks_later.ppm"); // scrolled under the list
        // Off again: no more brackets.
        ui_page(4);
        ui_press(4);
        pump(200);
        for (int i = 0; i < 25 && !strstr(ui_focused_text(), "Decode marks"); i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(200);
        printf("[marks] switched: '%s' (want Off)\n", ui_focused_text());
        ui_key(LV_KEY_ESC);
        pump(200);
        feed_band({{"W1ABC", "FN42", "", "@HB HEARTBEAT FN42", 1000, 0.05f}});
        pump(2000);
        marks("off again");
        return 0;
    }
    if (getenv("ONLY_COMPOSE")) {
        // 7. Pre-typing a reply while their long message is still arriving.
        stub_usb_kbd = getenv("USB") ? 1 : 0;
        pump(300);
        std::vector<Station> st = {
            {"N0XYZ", "EN34", "K2XYZ",
             "K2XYZ GOOD EVENING THE WEATHER HERE IS TURNING TO FALL AND I HOPE THE BUGS ARE FEW 73", 1320, 0.05f}};
        feed_band({{"W1ABC", "FN42", "", "@ALLCALL CQ CQ FN42", 1800, 0.05f},
                   {"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(2); // Reply
        pump(200);
        ui_compose_append("THANKS FOR THE");
        feed_band(st, 0, 3);
        screenshot(stub_usb_kbd ? "u07_compose_usb.ppm" : "u07_compose_kb.ppm");
        feed_band(st, 3);
        screenshot(stub_usb_kbd ? "u07_compose_usb_done.ppm" : "u07_compose_kb_done.ppm");
        ui_compose_cancel();
        pump(300);
        screenshot("u07_after.ppm");
        return 0;
    }
    if (getenv("ONLY_QSOFREQ")) {
        // Directed view also shows whatever is on the selected station's frequency.
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(2);
        ui_press(1); // Show: No HB -> Directed
        pump(300);
        feed_band({{"N0XYZ", "EN34", "", "GOOD COPY HERE", 1320, 0.05f},
                   {"W1ABC", "FN42", "", "NICE DAY THERE", 1800, 0.05f}});
        printf("[qso] on their frequency, no call: shown=%d (want 1)\n", ui_list_has("GOOD COPY HERE"));
        printf("[qso] other station, not directed: shown=%d (want 0)\n", ui_list_has("NICE DAY THERE"));
        screenshot("21_directed_qso.ppm");
        ui_press(1); // Directed -> All
        pump(300);
        printf("[qso] with Show: All, other station shown=%d (want 1: it was decoded)\n", ui_list_has("NICE DAY THERE"));

        // Query list: Close is one step back from the first item.
        ui_select_row_from("N0XYZ");
        ui_page(1);
        ui_press(3); // Query >
        pump(200);
        printf("[query] open, focused '%s'\n", ui_focused_text());
        ui_key(LV_KEY_LEFT);
        pump(200);
        printf("[query] one step back: '%s' (want Close)\n", ui_focused_text());
        screenshot("22_query_close.ppm");
        ui_click_focused();
        pump(300);
        printf("[query] after Close, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");
        ui_press(3); // open again...
        pump(200);
        ui_press(3); // ...and Query > closes it
        pump(300);
        printf("[query] Query > twice, list focused: %s\n", ui_focus_is_table() ? "yes" : "no");
        ui_press(3); // open it again, then press Clear: only closes the list
        pump(200);
        ui_press(4);
        pump(300);
        printf("[query] Clear with the list open: list focused %s, messages kept %d\n",
               ui_focus_is_table() ? "yes" : "no", ui_list_has("GOOD COPY HERE"));

        // The time search started and stopped (page 4, hold Time twice).
        ui_page(4);
        ui_hold(2);
        pump(200);
        printf("[query] Time held: '%s' (want Time: Searching)\n", ui_button_label(2));
        ui_hold(2);
        pump(200);
        printf("[query] held again: '%s' (want Time: Auto), '%s'\n", ui_button_label(2), stub_last_msg);
        return 0;
    }
    if (getenv("ONLY_URGENT")) {
        // Beta 2 urgent fixes (docs/BETA2_URGENT_PLAN.md), one block each.
        ui_indevs_init();
        // Real knob presses: held for `ms`, then released.
        auto mfk_click = [](int ms = 150) {
            ui_mfk_set(true);
            pump(ms);
            ui_mfk_set(false);
            pump(150);
        };
        auto mfk_turn = [](int steps) {
            ui_mfk_turn(steps);
            pump(100);
        };
        auto esc = [](int ms = 150) {
            ui_keypad_set(LV_KEY_ESC, true);
            pump(ms);
            ui_keypad_set(LV_KEY_ESC, false);
            pump(150);
        };
        pump(300);

        // 1. CQ doesn't pause auto heartbeats (it used to; the user's change).
        ui_page(1);
        int hb0 = stub_tx_frames;
        ui_hold(2);  // auto HB: one now
        ui_press(2); // done setting
        for (int i = 0; i < 300 && stub_tx_frames == hb0; i++) pump(100);
        for (int i = 0; i < 300 && stub_tx_keyed; i++) pump(100);
        pump(3500);
        printf("[cq-hb] HB before CQ: '%s'\n", ui_button_label(2));
        ui_press(1); // CQ
        pump(200);
        printf("[cq-hb] HB after CQ: '%s' (want HB auto: a countdown, not paused)\n", ui_button_label(2));
        printf("[cq-hb] paused row shown=%d (want 0)\n", ui_list_has("HB and HB ACK paused"));
        ui_key(LV_KEY_ESC); // stop the CQ
        pump(500);

        // 3. Holding the page button goes back a page.
        ui_page(2);
        ui_hold(0);
        printf("[page] hold on 1 -> '%s' (want (JS8 6:6))\n", ui_button_label(0));
        ui_hold(0);
        printf("[page] hold on 6 -> '%s' (want (JS8 5:6))\n", ui_button_label(0));
        ui_press(0);
        printf("[page] press on 5 -> '%s' (want (JS8 6:6))\n", ui_button_label(0));
        ui_page(1);
        ui_press(3); // Query > opens a list
        pump(200);
        ui_hold(0);
        pump(200);
        printf("[page] hold with a list open -> '%s' (want (JS8 1:6)), list closed: %s\n", ui_button_label(0),
               ui_focus_is_table() ? "yes" : "no");

        // 8. Show: No HB hides SNR reports too (mostly heartbeat answers).
        if (!getenv("SKIP_SNR")) {
            ui_page(2);
            feed_band({{"W1ABC", "FN42", "N0XYZ", "N0XYZ SNR -12", 1500, 0.05f}});
            printf("[snr] No HB: report shown=%d (want 0), label '%s'\n", ui_list_has("SNR -12"), ui_button_label(1));
            ui_press(1); // No HB -> Directed
            ui_press(1); // Directed -> All
            pump(200);
            printf("[snr] All: report shown=%d (want 1)\n", ui_list_has("SNR -12"));
            ui_press(1); // back to No HB
            pump(200);
        }

        // 6. ESC in a text box closes only the text box.
        if (!getenv("SKIP_ESC")) {
        ui_page(2);
        ui_press(3); // Send...
        pump(200);
        printf("[esc] Send... open, focus: %s\n", ui_focus_desc());
        esc();
        printf("[esc] after ESC: running %d (want 1), focus: %s (want message list)\n", ui_running(), ui_focus_desc());
        if (!ui_running()) return 0;
        ui_press(3);
        pump(200);
        esc(1400); // a long press, past LVGL's 1 s long-press time
        printf("[esc] after a long ESC: running %d (want 1), focus: %s\n", ui_running(), ui_focus_desc());
        if (!ui_running()) return 0;
        }

        // 5. Log QSO: Enter in the Name field doesn't log.
        unlink(JS8_LOG_PATH);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(5);
        ui_press(2); // Log QSO
        pump(200);
        printf("[log] open, focused '%s'\n", ui_focused_text());
        mfk_turn(2);
        printf("[log] after 2 MFK steps: '%s' (want Name: (none))\n", ui_focused_text());
        mfk_click();
        printf("[log] after MFK press, focus: %s (want keyboard)\n", ui_focus_desc());
        ui_compose_append("BOB");
        printf("[log] OK key found: %d\n", ui_kb_select_ok());
        mfk_click();
        pump(300);
        FILE *lf = fopen(JS8_LOG_PATH, "r");
        printf("[log] after Enter: logged %d (want 0), focused '%s' (want Name: BOB)\n", lf != NULL,
               ui_focused_text());
        if (lf) fclose(lf);
        screenshot("u05_log_after_name.ppm");
        // Again with a USB keyboard: no on-screen keyboard, Enter goes to
        // the text box itself.
        stub_usb_kbd = 1;
        for (int i = 0; i < 6 && strncmp(ui_focused_text(), "Name", 4) != 0; i++) mfk_turn(1);
        printf("[log-usb] on '%s'\n", ui_focused_text());
        mfk_click();
        printf("[log-usb] editing, focus: %s (want textarea)\n", ui_focus_desc());
        ui_compose_clear();
        ui_compose_append("ROBERT");
        ui_keypad_set(LV_KEY_ENTER, true);
        pump(120);
        ui_keypad_set(LV_KEY_ENTER, false);
        pump(300);
        lf = fopen(JS8_LOG_PATH, "r");
        printf("[log-usb] after Enter: logged %d (want 0), focused '%s' (want Name: ROBERT)\n", lf != NULL,
               ui_focused_text());
        if (lf) fclose(lf);

        // 4. The selected station stays selected while new rows arrive.
        ui_page(2);
        ui_select_row_from("N0XYZ");
        ui_page(1);
        ui_press(1); // our CQ: a new row below theirs
        pump(300);
        feed_band({{"W1ABC", "FN42", "", "@ALLCALL CQ CQ FN42", 1800, 0.05f}});
        char selc[32] = "";
        dialog_js8_selected_call(selc, sizeof(selc));
        printf("[sel] after our CQ and W1ABC's: selected '%s' (want N0XYZ)\n", selc);
        screenshot("u04_selected.ppm");
        ui_page(2);
        ui_press(2); // Reply
        pump(200);
        printf("[sel] Reply prefill '%s' (want N0XYZ )\n", ui_compose_text());
        ui_compose_cancel();
        pump(200);

        // 9. Fast typing on a USB keyboard: each key goes down before the
        // previous one is up. Once all queued at once, once as they come.
        ui_usb_init();
        ui_press(0); // leave the log
        pump(200);
        ui_page(2);
        auto type_rolled = [](const char *text, int gap_ms) {
            for (const char *p = text; *p; p++) {
                bool same = p > text && p[-1] == *p; // one key can't go down twice
                if (same) ui_usb_event((uint8_t)p[-1], 0);
                ui_usb_event((uint8_t)*p, 1);
                if (p > text && !same) ui_usb_event((uint8_t)p[-1], 0);
                if (gap_ms) pump(gap_ms);
            }
            ui_usb_event((uint8_t)text[strlen(text) - 1], 0);
            pump(100);
        };
        ui_press(3); // Send...
        pump(200);
        type_rolled("CQ TEST DE K2XYZ", 0);
        printf("[usb] rolled, queued at once: '%s' (want CQ TEST DE K2XYZ)\n", ui_compose_text());
        ui_compose_clear();
        type_rolled("HELLO WORLD 73", 25);
        printf("[usb] rolled, 25 ms apart: '%s' (want HELLO WORLD 73)\n", ui_compose_text());
        ui_compose_clear();
        type_rolled("vk2abc hw cpy?", 25);
        printf("[usb] lowercase: '%s' (want VK2ABC HW CPY?)\n", ui_compose_text());
        ui_compose_cancel();
        pump(200);
        stub_usb_kbd = 0;
        return 0;
    }
    if (getenv("ONLY_TEXTS")) {
        // Page 4 -> Texts... -> INFO: the keyboard must get the focus.
        pump(300);
        ui_page(2);
        ui_press(3); // Send...
        pump(300);
        printf("[texts] Send... compose (works on the radio), focus: %s\n", ui_focus_desc());
        ui_compose_cancel();
        pump(300);
        ui_page(4);
        ui_press(4);
        pump(200);
        printf("[texts] list open, focus: %s\n", ui_focus_desc());
        // Time Sync comes first in Settings now: find INFO.
        for (int i = 0; i < 25 && strncmp(ui_focused_text(), "INFO", 4) != 0; i++) ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // INFO
        pump(300);          // lets the list's async delete run
        printf("[texts] editing INFO, focus: %s\n", ui_focus_desc());
        screenshot("20_texts_edit.ppm");
        ui_compose_append("X6100 5W EFHW");
        ui_compose_enter();
        pump(300);
        printf("[texts] after Enter, focus: %s\n", ui_focus_desc());
        ui_press(4); // Texts... then Close
        pump(200);
        ui_key(LV_KEY_LEFT);
        pump(200);
        printf("[texts] one step back: '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        printf("[texts] after Close, focus: %s\n", ui_focus_desc());
        return 0;
    }
    pump(300);
    screenshot("01_open.ppm");

    // Hold starts Off (the default); the scenarios below were written for
    // On, so switch it (page 6, button 3) and back to page 2.
    ui_page(6);
    printf("[hold] default: '%s'\n", ui_button_label(3));
    ui_press(3);
    ui_page(2);

    // Stations sharing the band; multi-frame ones overlap in time.
    std::vector<Station> stations = {
        {"W1ABC", "FN42", "", "W1ABC: HEARTBEAT FN42", 620, 0.05f},
        {"VE3KP", "FN03", "", "CQ CQ CQ FN03", 910, 0.04f},
        {"N0XYZ", "EM48", "K2XYZ", "K2XYZ HELLO FROM THE X6100 TEST", 1320, 0.06f},
        {"G4ABC", "IO91", "", "@ALLCALL ANYONE ON THE BAND FOR A CHAT", 1760, 0.02f},
        {"KN4CRD", "EM73", "K2XYZ", "K2XYZ MSG STORED MESSAGE FOR YOU", 2150, 0.03f},
        {"DL1XX", "JO62", "", "DL1XX: HEARTBEAT JO62", 2380, 0.015f},
        // K9ABC acknowledges our heartbeat, as desktop JS8Call's auto-reply does.
        {"K9ABC", "EN52", "K2XYZ", "K2XYZ HEARTBEAT SNR -08", 1100, 0.03f},
    };

    const auto &costas = js8core::protocol::costas(js8core::protocol::CostasType::Original);
    std::vector<std::vector<std::array<int, js8core::kJs8NumSymbols>>> tones(stations.size());
    std::size_t slots = 0;
    for (std::size_t i = 0; i < stations.size(); i++) {
        auto &s      = stations[i];
        auto  frames = vc::build_message_frames(s.call, s.grid, s.to, s.text, false, false, 0);
        for (auto &[frame, bits] : frames) {
            std::array<int, js8core::kJs8NumSymbols> t{};
            js8core::legacy_encode(bits, costas, frame.c_str(), t.data());
            tones[i].push_back(t);
        }
        printf("[band] %-7s %4.0f Hz  %zu frame(s)  %s\n", s.call, s.offset_hz, frames.size(), s.text);
        slots = std::max(slots, frames.size());
    }

    // Silence to the next slot, one warm-up slot, the signal slots, one tail.
    auto               now     = std::chrono::system_clock::now().time_since_epoch();
    long long          ms      = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    std::size_t        lead    = (std::size_t)((15000 - ms % 15000) * RATE / 1000) + 15 * RATE;
    std::vector<float> audio(lead + (slots + 1) * 15 * RATE, 0.0f);

    for (std::size_t i = 0; i < stations.size(); i++) {
        for (std::size_t k = 0; k < tones[i].size(); k++) {
            std::size_t start = lead + k * 15 * RATE + RATE / 2;
            double      phi   = 0;
            for (int s = 0; s < js8core::kJs8NumSymbols; s++) {
                double dphi = 2 * M_PI * (stations[i].offset_hz + tones[i][k][s] * 6.25) / RATE;
                for (int j = 0; j < NSPS; j++) {
                    audio[start + s * NSPS + j] += stations[i].amp * (float)std::sin(phi);
                    phi += dphi;
                }
            }
        }
    }
    std::mt19937                    rng(3);
    std::normal_distribution<float> noise(0.0f, 0.02f);
    for (auto &s : audio) s += noise(rng);

    // Feed through the dialog's audio callback in real time, in 20 ms
    // pieces like PulseAudio, with the production clock guard active.
    const std::size_t piece = RATE / 50;
    auto              t0    = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < audio.size(); i += piece) {
        unsigned n = (unsigned)std::min(piece, audio.size() - i);
        dialog_audio_samples(n, &audio[i]);
        auto due = t0 + std::chrono::microseconds((long long)((i + n) * 1e6 / RATE));
        while (std::chrono::steady_clock::now() < due) pump(5);
    }
    pump(3000);
    screenshot("02_band_no_hb.ppm");

    // Show: All (includes heartbeats), then Directed.
    ui_press(1);
    pump(200);
    screenshot("03_directed.ppm");
    ui_press(1);
    pump(200);
    screenshot("04_all.ppm");

    // MFK two steps back: the selection moves up and the finder marks it.
    ui_key(LV_KEY_LEFT);
    ui_key(LV_KEY_LEFT);
    pump(200);
    screenshot("04b_mfk_select.ppm");

    // ---- Transmit: reply to N0XYZ, who called us. Back to "All" first.
    ui_page(2);
    ui_press(1);
    pump(100);
    ui_select_row_from("N0XYZ");
    ui_press(2); // Reply
    pump(200);
    printf("[tx] compose prefilled: '%s'\n", ui_compose_text());
    ui_compose_append("SNR?");
    pump(200);
    screenshot("06_compose.ppm");
    ui_compose_enter();
    pump(1000);
    screenshot("07_tx_queued.ppm");
    // Wait for the slot boundary and the first frame to key.
    for (int i = 0; i < 180 && stub_tx_frames == 0; i++) pump(100);
    pump(1500);
    screenshot("08_tx_keying.ppm");
    for (int i = 0; i < 60 && stub_tx_frames == 1; i++) pump(100);
    pump(500);
    printf("[tx] frames keyed %d, offset %d Hz, %u samples, peak %d\n", stub_tx_frames, stub_tx_offset,
           stub_tx_samples, stub_tx_peak);
    screenshot("09_tx_done.ppm");

    // A long message, stopped with ESC during its first frame; the next ESC closes.
    ui_page(2);
    ui_press(3); // Send...
    pump(200);
    ui_compose_append("@ALLCALL TESTING A LONGER MESSAGE FROM THE X6100");
    ui_compose_enter();
    for (int i = 0; i < 180 && stub_tx_frames == 1; i++) pump(100);
    pump(500);
    ui_key(LV_KEY_ESC);
    pump(1500);
    printf("[tx] after ESC during TX: running=%d frames keyed=%d\n", ui_running(), stub_tx_frames);
    screenshot("10_tx_stopped.ppm");

    // ---- T3. We're on page 2. Pages cycle 1 -> 2 -> 3 -> 1.
    auto wait_keyed = [&](int before) {
        for (int i = 0; i < 180 && stub_tx_frames == before; i++) pump(100);
        pump(300);
    };
    auto wait_done = [&]() { pump(3500); };

    ui_page(3);
    ui_press(3); // Show Stations
    pump(300);
    screenshot("11_stations.ppm");

    ui_select_row_from("N0XYZ");
    ui_page(1);
    ui_press(3); // Query >
    pump(300);
    screenshot("12_query.ppm");
    ui_key(LV_KEY_DOWN); // "SNR?" -> "Send SNR"
    pump(100);
    ui_click_focused();
    int before = stub_tx_frames;
    wait_keyed(before);
    printf("[t3] Send SNR keyed at %d Hz\n", stub_tx_offset);
    screenshot("13_send_snr.ppm");
    wait_done();

    before = stub_tx_frames;
    ui_page(1);
    ui_press(2); // Heartbeat
    wait_keyed(before);
    printf("[t3] heartbeat keyed at %d Hz (want 500-999, clear of stations)\n", stub_tx_offset);
    wait_done();

    ui_page(6);
    ui_press(3); // Hold: On -> Off
    ui_select_row_from("N0XYZ");
    ui_page(2);
    ui_press(2); // Reply
    pump(200);
    ui_compose_append("73");
    ui_compose_enter();
    before = stub_tx_frames;
    wait_keyed(before);
    printf("[t3] reply with Hold off keyed at %d Hz (N0XYZ is at 1320)\n", stub_tx_offset);
    wait_done();
    screenshot("14_after_t3.ppm");

    // ---- T4: AUTO, HB ACK (page 4), auto heartbeats (page 1: hold Heartbeat).
    ui_page(4);
    ui_press(1); // AUTO on
    ui_press(3); // HB ACK on
    ui_page(1);
    before = stub_tx_frames;
    ui_hold(2); // auto HB: one now, and the knob sets the interval
    printf("[t4] HB after hold: '%s'\n", ui_button_label(2));
    ui_rotary(-1);
    ui_rotary(-1);
    ui_rotary(-1);
    printf("[t4] HB after knob -3: '%s'\n", ui_button_label(2));
    screenshot("15a_hb_interval.ppm");
    pump(9000); // setting the interval ends on its own
    printf("[t4] HB after 9 s: '%s'\n", ui_button_label(2));
    // As auto CQ does: one right away (the user's choice), then every 27 min.
    wait_keyed(before);
    printf("[t4] first heartbeat keyed at %d Hz (want 500-999), frames %d (was %d)\n", stub_tx_offset, stub_tx_frames,
           before);
    screenshot("15_auto_on.ppm");
    wait_done();

    // Someone's heartbeat: expect an automatic ack in the HB sub-band.
    before = stub_tx_frames;
    feed_band({{"W7XYZ", "DM43", "", "W7XYZ: HEARTBEAT DM43", 1800, 0.04f}});
    wait_keyed(before);
    printf("[t4] heartbeat ack keyed at %d Hz (frames %d)\n", stub_tx_offset, stub_tx_frames);
    screenshot("16_hb_ack.ppm");
    wait_done();

    // A query to us: expect an automatic SNR reply at our offset (heartbeats
    // pause now: someone called us).
    before = stub_tx_frames;
    feed_band({{"VE7ABC", "CN89", "K2XYZ", "K2XYZ SNR?", 1650, 0.04f}});
    wait_keyed(before);
    printf("[t4] SNR? auto-reply keyed at %d Hz (frames %d)\n", stub_tx_offset, stub_tx_frames);
    screenshot("17_auto_reply.ppm");
    wait_done();

    // AUTO off: a GRID? query is offered, not sent.
    ui_page(4);
    ui_press(1); // AUTO off
    before = stub_tx_frames;
    feed_band({{"G0ABC", "IO91", "K2XYZ", "K2XYZ GRID?", 1250, 0.04f}});
    pump(2000);
    printf("[t4] after GRID? with AUTO off: frames %d (was %d)\n", stub_tx_frames, before);
    screenshot("18_offer.ppm");
    ui_page(2);
    ui_select_row_from("G0ABC");
    ui_press(2); // Reply
    pump(300);
    printf("[t4] Reply offers: '%s'\n", ui_compose_text());
    screenshot("19_offer_reply.ppm");
    ui_compose_cancel();
    pump(300);
    printf("[t4] list focused after cancelling: %s\n", ui_focus_is_table() ? "yes" : "no");

    // Band change, then close with ESC.
    ui_band_up();
    pump(200);
    screenshot("05_band_change.ppm");

    ui_key(LV_KEY_ESC);
    pump(200);
    printf("[done] dialog running after ESC: %s\n", ui_running() ? "yes (BUG)" : "no");
    return ui_running() ? 1 : 0;
}
