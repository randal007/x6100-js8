// Headless run of the real JS8 dialog: LVGL renders into memory while a
// synthetic but realistic band (several stations, interleaved multi-frame
// messages, heartbeats, a CQ, a message to me, a checksummed MSG) is fed
// through the dialog's audio callback. Screenshots are written as PPM.

#include "lvgl/lvgl.h"
extern "C" {
void dialog_audio_samples(unsigned int n, float *samples);
void scheduler_work();
void ui_init(void);
void ui_open(void);
void ui_press(int i);
void ui_band_up(void);
void ui_key(uint32_t key);
int  ui_running(void);
void ui_compose_append(const char *text);
const char *ui_compose_text(void);
void ui_compose_enter(void);
void ui_select_row_from(const char *call);
void ui_click_focused(void);
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

struct Station {
    const char *call, *grid, *to, *text;
    double      offset_hz;
    float       amp;
};

int main() {
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
    pump(300);
    screenshot("01_open.ppm");

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

    ui_press(0); // page 2
    ui_press(0); // page 3
    ui_press(4); // Show Stations
    pump(300);
    screenshot("11_stations.ppm");

    ui_select_row_from("N0XYZ");
    ui_press(0); // page 1
    ui_press(0); // page 2
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
    ui_press(2); // Heartbeat
    wait_keyed(before);
    printf("[t3] heartbeat keyed at %d Hz (want 500-999, clear of stations)\n", stub_tx_offset);
    wait_done();

    ui_press(0); // page 3
    ui_press(3); // Hold: On -> Off
    ui_select_row_from("N0XYZ");
    ui_press(0); // page 1
    ui_press(2); // Reply
    pump(200);
    ui_compose_append("73");
    ui_compose_enter();
    before = stub_tx_frames;
    wait_keyed(before);
    printf("[t3] reply with Hold off keyed at %d Hz (N0XYZ is at 1320)\n", stub_tx_offset);
    wait_done();
    screenshot("14_after_t3.ppm");

    // Band change, then close with ESC.
    ui_band_up();
    pump(200);
    screenshot("05_band_change.ppm");

    ui_key(LV_KEY_ESC);
    pump(200);
    printf("[done] dialog running after ESC: %s\n", ui_running() ? "yes (BUG)" : "no");
    return ui_running() ? 1 : 0;
}
