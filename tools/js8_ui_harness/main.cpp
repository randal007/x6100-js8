// Headless run of the real JS8 dialog: LVGL renders into memory while a
// synthetic but realistic band (several stations, interleaved multi-frame
// messages, heartbeats, a CQ, a message to me, a checksummed MSG) is fed
// through the dialog's audio callback. Screenshots are written as PPM.

#include "lvgl/lvgl.h"
extern "C" {
void dialog_destruct(void);
void dialog_audio_samples(unsigned int n, float *samples);
void scheduler_work();
void ui_init(void);
void ui_open(void);
void ui_press(int i);
void ui_band_up(void);
void ui_key(uint32_t key);
int  ui_running(void);
int  ui_focus_is_table(void);
void ui_rotary(int32_t diff);
int  ui_list_has(const char *text);
const char *ui_focused_text(void);
const char *ui_focus_desc(void);
const char *ui_button_label(int i);
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
void ui_compose_clear(void);
void ui_indevs_init(void);
int  ui_kb_select_ok(void);
void ui_usb_init(void);
void ui_usb_event(uint32_t key, int value);
void ui_mfk_turn(int32_t diff);
void ui_mfk_set(bool down);
void ui_keypad_set(uint32_t key, bool down);
extern int stub_tx_frames;
extern int stub_usb_kbd;
extern int32_t stub_tx_offset;
extern uint32_t stub_tx_samples;
extern int16_t stub_tx_peak;
}

#include "js8core/decoder.hpp"
#include "testsignal.hpp"
#include "js8core/protocol/costas.hpp"
#include "js8core/protocol/varicode.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace vc = js8core::protocol::varicode;

static constexpr int W = 800, H = 480, RATE = 11025;
static std::vector<uint32_t> fb(W *H);

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px) {
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++) fb[y * W + x] = (px++)->full;
    lv_disp_flush_ready(drv);
}

static void screenshot(const char *path) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(NULL);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (auto p : fb) {
        unsigned char rgb[3] = {(unsigned char)(p >> 16), (unsigned char)(p >> 8), (unsigned char)p};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    printf("[shot] %s\n", path);
}

// Let LVGL and the scheduler run for `ms` of wall time.
static void pump(int ms) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        lv_tick_inc(5);
        scheduler_work();
        lv_timer_handler();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

static void pump(int ms);

struct Station {
    const char *call, *grid, *to, *text;
    double      offset_hz;
    float       amp;
};

// Synthesise `band` (one slot per frame, starting at the next slot
// boundary) and feed it through the dialog's audio callback in real time.
static void feed_band(const std::vector<Station> &band, std::size_t first = 0, std::size_t last = 99) {
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
    std::vector<float> audio(lead + slots * 15 * RATE + 3 * RATE, 0.0f);
    for (std::size_t i = 0; i < band.size(); i++)
        for (std::size_t k = 0; k < tones[i].size(); k++) {
            std::size_t start = lead + k * 15 * RATE + RATE / 2;
            double      phi   = 0;
            for (int s = 0; s < js8core::kJs8NumSymbols; s++) {
                double dphi = 2 * M_PI * (band[i].offset_hz + tones[i][k][s] * 6.25) / RATE;
                for (int j = 0; j < 1764; j++) {
                    audio[start + s * 1764 + j] += band[i].amp * (float)std::sin(phi);
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

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0); // keep the log if something aborts
    lv_init();
    static lv_color_t          buf[W * 60];
    static lv_disp_draw_buf_t  draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, W * 60);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res  = W;
    drv.ver_res  = H;
    drv.flush_cb = flush_cb;
    drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&drv);

    ui_init();
    if (getenv("ONLY_INBOX")) unlink(JS8_INBOX_PATH); // before the dialog loads it
    if (getenv("ONLY_HELD")) unlink(JS8_HELD_PATH);
    ui_open();
    if (getenv("ONLY_GEN")) {
        // GEN / APP on the radio close the app with a list popup open.
        const char *which = getenv("ONLY_GEN");
        pump(300);
        if (!strcmp(which, "query")) {
            feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
            ui_select_row_from("N0XYZ");
            ui_page(2);
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
        ui_click_focused(); // Spot my grid (first item)
        wait_tx();
        printf("[aprs] grid spot sent: %d\n", ui_list_has("@APRSIS GRID FN42"));

        // Spot GPS position (second item): no fix -> message only.
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT);
        printf("[aprs] item 2: '%s'\n", ui_focused_text());
        ui_click_focused();
        pump(300);
        setenv("HARNESS_GPS", "49.2827,-123.1207", 1);
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        wait_tx();
        printf("[aprs] GPS spot sent: %d (want @APRSIS GRID CN89KG + 4)\n", ui_list_has("@APRSIS GRID CN89KG"));

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
        ui_key(LV_KEY_RIGHT);
        ui_key(LV_KEY_RIGHT);
        ui_click_focused(); // POTA spot
        pump(300);
        printf("[aprs] POTA prefill: '%s'\n", ui_compose_text());
        ui_compose_append("VE-1234");
        printf("[aprs] POTA typed:   '%s'\n", ui_compose_text());
        screenshot("23_aprs_pota.ppm");
        ui_compose_enter();
        wait_tx();
        ui_press(1);
        pump(200);
        ui_key(LV_KEY_RIGHT);
        ui_key(LV_KEY_RIGHT);
        ui_click_focused();
        pump(300);
        printf("[aprs] POTA again (remembers park): '%s'\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);

        ui_press(1);
        pump(200);
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
        ui_page(1);
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
        printf("[inbox] button: '%s' (want Inbox / 1 new)\n", ui_button_label(4));
        ui_select_row_from("N0XYZ");
        ui_page(1);
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
        printf("[inbox] after reading: '%s' (want Inbox)\n", ui_button_label(4));
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
        ui_page(1);
        ui_press(2);
        pump(200);
        printf("[inbox] Reply offers: '%s' (want N0XYZ QUERY MSG 3)\n", ui_compose_text());
        ui_compose_cancel();
        pump(300);

        // Query list: Any messages?
        ui_page(2);
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
        ui_key(LV_KEY_RIGHT); // from the unread one to the older, read one
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
        ui_page(1);
        ui_press(1); // Show: No HB -> Directed
        ui_press(1); // -> All
        pump(300);
        screenshot("33_alert_rows.ppm");
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
        ui_page(1);
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
        ui_page(1);
        ui_press(2); // Reply
        pump(200);
        ui_compose_cancel();
        pump(200);
        ui_page(6);
        ui_hold(2);
        printf("[speed] after hold: '%s'\n", ui_button_label(2));
        ui_page(2);
        ui_press(2); // Heartbeat in Turbo: refused
        pump(200);

        // Top offset follows the speed: Turbo 2340 Hz.
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
        ui_press(3); // back to messages
        pump(200);

        // Send at Fast: 10 s slots, 0.1 s symbols -> 79 x 4410 samples at 44.1 kHz.
        ui_page(2);
        ui_press(1); // CQ
        wait_tx();
        printf("[speed] Fast frame: %u samples (want %d)\n", stub_tx_samples, 79 * 4410);
        printf("[speed] our row: %d (offset kept at Turbo's 2340 limit)\n", ui_list_has("TX 2340 F"));
        screenshot("37_speed_tx.ppm");

        ui_page(6);
        ui_press(3); // Decode: My speed
        printf("[speed] decode: '%s'\n", ui_button_label(3));
        ui_press(3);
        ui_press(2); // Fast -> Turbo
        ui_press(2); // -> Slow
        ui_press(2); // -> Normal
        printf("[speed] back to '%s'\n", ui_button_label(2));
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
        // W1ABC asks what we hold, then fetches it.
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ QUERY MSGS", 1500, 0.05f}});
        wait_tx();
        printf("[held] YES sent: %d\n", ui_list_has("W1ABC YES MSG ID 1"));
        feed_band({{"W1ABC", "FN42", "K2XYZ", "K2XYZ QUERY MSG 1", 1500, 0.05f}});
        wait_tx();
        printf("[held] delivered: %d\n", ui_list_has("W1ABC MSG MEET AT THE PARK FROM N0XYZ"));
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
    if (getenv("ONLY_QSOFREQ")) {
        // Directed view also shows whatever is on the selected station's frequency.
        pump(300);
        feed_band({{"N0XYZ", "EN34", "K2XYZ", "K2XYZ HELLO", 1320, 0.05f}});
        ui_select_row_from("N0XYZ");
        ui_page(1);
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
        ui_page(2);
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

        // Time Sync from the decodes (can't actually set the PC clock here).
        ui_page(3);
        ui_press(1);
        pump(200);
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

        // 1. CQ switches heartbeats off.
        ui_page(4);
        ui_press(2); // HB on (into the knob interval setting)
        ui_press(2); // done setting
        printf("[cq-hb] HB before CQ: '%s'\n", ui_button_label(2));
        ui_page(2);
        ui_press(1); // CQ
        pump(200);
        ui_page(4);
        printf("[cq-hb] HB after CQ: '%s' (want HB:\nOff)\n", ui_button_label(2));
        printf("[cq-hb] info row shown=%d (want 1)\n", ui_list_has("HB and HB ACK off: CQ"));

        // 3. Holding the page button goes back a page.
        ui_page(1);
        ui_hold(0);
        printf("[page] hold on 1 -> '%s' (want (JS8 6:6))\n", ui_button_label(0));
        ui_hold(0);
        printf("[page] hold on 6 -> '%s' (want (JS8 5:6))\n", ui_button_label(0));
        ui_press(0);
        printf("[page] press on 5 -> '%s' (want (JS8 6:6))\n", ui_button_label(0));
        ui_page(2);
        ui_press(3); // Query > opens a list
        pump(200);
        ui_hold(0);
        pump(200);
        printf("[page] hold with a list open -> '%s' (want (JS8 1:6)), list closed: %s\n", ui_button_label(0),
               ui_focus_is_table() ? "yes" : "no");

        // 8. Show: No HB hides SNR reports too (mostly heartbeat answers).
        if (!getenv("SKIP_SNR")) {
            ui_page(1);
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
        ui_page(1);
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

        // 9. Fast typing on a USB keyboard: each key goes down before the
        // previous one is up. Once all queued at once, once as they come.
        ui_usb_init();
        ui_press(0); // leave the log
        pump(200);
        ui_page(1);
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
        ui_page(1);
        ui_press(3); // Send...
        pump(300);
        printf("[texts] Send... compose (works on the radio), focus: %s\n", ui_focus_desc());
        ui_compose_cancel();
        pump(300);
        ui_page(4);
        ui_press(4);
        pump(200);
        printf("[texts] list open, focus: %s\n", ui_focus_desc());
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
    // On, so switch it (page 3, button 2) and back to page 1.
    ui_page(3);
    printf("[hold] default: '%s'\n", ui_button_label(2));
    ui_press(2);
    ui_page(1);

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
                for (int j = 0; j < 1764; j++) {
                    audio[start + s * 1764 + j] += stations[i].amp * (float)std::sin(phi);
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
    ui_page(1);
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
    ui_page(1);
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

    // ---- T3. We're on page 1. Pages cycle 1 -> 2 -> 3 -> 1.
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
    ui_page(2);
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
    ui_page(2);
    ui_press(2); // Heartbeat
    wait_keyed(before);
    printf("[t3] heartbeat keyed at %d Hz (want 500-999, clear of stations)\n", stub_tx_offset);
    wait_done();

    ui_page(3);
    ui_press(2); // Hold: On -> Off
    ui_select_row_from("N0XYZ");
    ui_page(1);
    ui_press(2); // Reply
    pump(200);
    ui_compose_append("73");
    ui_compose_enter();
    before = stub_tx_frames;
    wait_keyed(before);
    printf("[t3] reply with Hold off keyed at %d Hz (N0XYZ is at 1320)\n", stub_tx_offset);
    wait_done();
    screenshot("14_after_t3.ppm");

    // ---- T4: AUTO, HB, HB ACK. We're on page 1; go to page 4.
    ui_page(4);
    before = stub_tx_frames;
    ui_press(1); // AUTO on
    ui_press(2); // HB on: first heartbeat at the next chance, and the knob sets the interval
    printf("[t4] HB after press: '%s'\n", ui_button_label(2));
    ui_rotary(-1);
    ui_rotary(-1);
    ui_rotary(-1);
    printf("[t4] HB after knob -3: '%s'\n", ui_button_label(2));
    screenshot("15a_hb_interval.ppm");
    pump(9000); // setting the interval ends on its own
    printf("[t4] HB after 9 s: '%s'\n", ui_button_label(2));
    ui_press(3); // HB ACK on
    pump(300);
    screenshot("15_auto_on.ppm");
    // As on desktop: no heartbeat right away, the first one is an interval out.
    pump(20000);
    printf("[t4] 20 s after HB on: frames %d (was %d, want no change)\n", stub_tx_frames, before);

    // Someone's heartbeat: expect an automatic ack in the HB sub-band.
    before = stub_tx_frames;
    feed_band({{"W7XYZ", "DM43", "", "W7XYZ: HEARTBEAT DM43", 1800, 0.04f}});
    wait_keyed(before);
    printf("[t4] heartbeat ack keyed at %d Hz (frames %d)\n", stub_tx_offset, stub_tx_frames);
    screenshot("16_hb_ack.ppm");
    wait_done();

    // A query to us: expect an automatic SNR reply at our offset, and the
    // QSO turns HB and HB ACK off.
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
    ui_page(1);
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
