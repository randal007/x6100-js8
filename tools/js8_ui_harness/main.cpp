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
extern int stub_tx_frames;
extern int32_t stub_tx_offset;
extern uint32_t stub_tx_samples;
extern int16_t stub_tx_peak;
}

#include "js8core/decoder.hpp"
#include "js8core/protocol/costas.hpp"
#include "js8core/protocol/varicode.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <thread>
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
static void feed_band(const std::vector<Station> &band) {
    const auto &costas = js8core::protocol::costas(js8core::protocol::CostasType::Original);
    std::vector<std::vector<std::array<int, js8core::kJs8NumSymbols>>> tones(band.size());
    std::size_t slots = 0;
    for (std::size_t i = 0; i < band.size(); i++) {
        auto frames = vc::build_message_frames(band[i].call, band[i].grid, "", band[i].text, false, false, 0);
        for (auto &[frame, bits] : frames) {
            std::array<int, js8core::kJs8NumSymbols> t{};
            js8core::legacy_encode(bits, costas, frame.c_str(), t.data());
            tones[i].push_back(t);
        }
        slots = std::max(slots, frames.size());
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
        } else if (!strcmp(which, "aprs")) {
            ui_page(5);
            ui_press(1); // APRS >
        } else {
            ui_page(4);
            ui_press(4); // Texts...
        }
        pump(300);
        printf("[gen] %s list open, focused '%s'\n", which, ui_focused_text());
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
        ui_key(LV_KEY_LEFT);
        printf("[aprs] one step back: '%s'\n", ui_focused_text());
        screenshot("24_aprs_list.ppm");
        ui_click_focused();
        pump(300);
        printf("[aprs] after Close: %s\n", ui_focus_desc());
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
