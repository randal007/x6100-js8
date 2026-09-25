/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Tests for src/js8 (JS8 receive).
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "assembler.hpp"
#include "classify.hpp"
#include "receiver.hpp"
#include "js8_ops.h"
#include "qsolog.hpp"

#include <unistd.h>
#include "tx.hpp"
#include "render.hpp"
#include "resampler.hpp"

#include "js8core/decoder.hpp"
#include "js8core/protocol/costas.hpp"
#include "js8core/protocol/varicode.hpp"

#include <chrono>
#include <cmath>
#include <complex>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace x6100::js8;
namespace vc = js8core::protocol::varicode;

namespace {

// Power of a single frequency (Goertzel), normalised to a sine's amplitude.
double tone_amplitude(const std::vector<float> &x, double freq, double rate, std::size_t skip = 0) {
    double      w = 2.0 * M_PI * freq / rate, c = 2.0 * std::cos(w);
    double      s1 = 0, s2 = 0;
    std::size_t n = x.size() - skip;
    for (std::size_t i = skip; i < x.size(); ++i) {
        double s = x[i] + c * s1 - s2;
        s2       = s1;
        s1       = s;
    }
    double power = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt(power) / n;
}

std::vector<float> sine(double freq, double rate, std::size_t n, double amp = 0.5) {
    std::vector<float> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = (float)(amp * std::sin(2.0 * M_PI * freq * i / rate));
    return v;
}

// Build frames for `text` as JS8Call would send them, then render and
// assemble them as the receiver does. Returns the assembled message.
std::string roundtrip(const std::string &mycall, const std::string &selected, const std::string &text) {
    auto            frames = vc::build_message_frames(mycall, "FN42", selected, text, false, false, 0);
    FrameRenderer   renderer;
    std::string     got;
    MessageAssembler assembler([&](const RxFrame &m) { got = m.text; });
    for (auto &[frame, bits] : frames) {
        RxFrame f;
        f.type    = bits;
        f.freq_hz = 1500;
        f.text    = renderer.render(frame, &f.type, f.freq_hz);
        assembler.add(f);
    }
    return got;
}

RxFrame frame(const std::string &text, int type, float freq = 1000.0f, std::int64_t t_ms = 0) {
    RxFrame f;
    f.text         = text;
    f.type         = type;
    f.freq_hz      = freq;
    f.timestamp_ms = t_ms;
    return f;
}

} // namespace

/* ---- Resampler -------------------------------------------------------- */

TEST_CASE("resampler converts 11025 Hz to 12000 Hz at the right length", "[js8][resampler]") {
    RationalResampler r(160, 147);
    std::vector<float> in(11025 * 2, 0.0f), out;
    r.process(in.data(), in.size(), out);
    REQUIRE(std::abs((long)out.size() - 24000) <= 1);
}

TEST_CASE("resampler preserves in-band tones", "[js8][resampler]") {
    for (double f : {300.0, 1000.0, 1500.0, 2500.0, 3500.0}) {
        RationalResampler  r(160, 147);
        auto               in = sine(f, 11025, 11025 * 2);
        std::vector<float> out;
        r.process(in.data(), in.size(), out);
        INFO("tone " << f << " Hz");
        CHECK(tone_amplitude(out, f, 12000, 200) == Catch::Approx(0.5).epsilon(0.02));
    }
}

TEST_CASE("resampler suppresses images that would fold into the JS8 band", "[js8][resampler]") {
    // An input tone f leaves an image at 11025 - f; at 12 kHz that folds to
    // 12000 - (11025 - f) = f + 975 Hz, inside the JS8 passband.
    for (double f : {500.0, 1500.0, 2000.0}) {
        RationalResampler  r(160, 147);
        auto               in = sine(f, 11025, 11025 * 2);
        std::vector<float> out;
        r.process(in.data(), in.size(), out);
        double wanted = tone_amplitude(out, f, 12000, 200);
        double image  = tone_amplitude(out, f + 975.0, 12000, 200);
        INFO("tone " << f << " Hz, image " << 20 * std::log10(image / wanted) << " dB");
        CHECK(20 * std::log10(image / wanted) < -60.0);
    }
}

/* ---- Render + assemble round trips ------------------------------------ */

TEST_CASE("frames built by JS8Call's encoder render and reassemble", "[js8][render]") {
    CHECK(roundtrip("W1ABC", "", "W1ABC: HEARTBEAT FN42") == "W1ABC: @HB HEARTBEAT FN42");
    CHECK(roundtrip("W1ABC", "", "CQ CQ CQ FN42") == "W1ABC: @ALLCALL CQ CQ CQ FN42");
    CHECK(roundtrip("W1ABC", "K2XYZ", "K2XYZ SNR?") == "W1ABC: K2XYZ SNR?");
    CHECK(roundtrip("W1ABC", "K2XYZ", "K2XYZ SNR -12") == "W1ABC: K2XYZ SNR -12");
    CHECK(roundtrip("W1ABC", "K2XYZ", "K2XYZ GRID?") == "W1ABC: K2XYZ GRID?");
    CHECK(roundtrip("W1ABC", "K2XYZ", "K2XYZ HELLO FROM THE X6100 TEST") ==
          "W1ABC: K2XYZ HELLO FROM THE X6100 TEST");
    CHECK(roundtrip("W1ABC", "", "@ALLCALL HELLO EVERYONE ON THE BAND") ==
          "W1ABC: @ALLCALL HELLO EVERYONE ON THE BAND");
    CHECK(roundtrip("W1ABC", "", "JUST SOME FREE TEXT WITHOUT A CALL") == "JUST SOME FREE TEXT WITHOUT A CALL");
}

TEST_CASE("every printable character survives encoding", "[js8][render]") {
    for (char c = 33; c < 127; c++) {
        if (c >= 'a' && c <= 'z') continue;
        std::string m = std::string("K2XYZ HELLO ") + c + c + " WORLD";
        INFO("char " << c);
        CHECK(roundtrip("W1ABC", "K2XYZ", m).rfind("W1ABC: " + m, 0) == 0);
        CHECK(is_sendable_char(c));
    }
}

TEST_CASE("APRS gateway commands survive encoding", "[js8][render][aprs]") {
    for (const char *m : {"@APRSIS GRID CN89LH", "@APRSIS CMD :SMS      :@6045551234 TEST FROM JS8{01}",
                          "@APRSIS CMD :EMAIL-2  :TEST@EXAMPLE.COM HELLO{01}",
                          "@APRSIS CMD :POTAGW   :VE7NHW CA-1234 7078 JS8 QRV",
                          "@APRSIS CMD :APRS2SOTA:VE7/LM-001 7.078 DATA VE7NHW QRV",
                          "@APRSIS CMD :WLNK-1   :SP TEST@EXAMPLE.COM SUBJECT"}) {
        INFO(m);
        // CMD carries a 3-character checksum after the text, like MSG.
        CHECK(roundtrip("VE7NHW", "", m).rfind(std::string("VE7NHW: ") + m, 0) == 0);
    }
}

TEST_CASE("a compound sender's helper frame is folded into the header", "[js8][render]") {
    CHECK(roundtrip("EA8/W1ABC", "K2XYZ", "K2XYZ HELLO FROM THE CANARIES") ==
          "EA8/W1ABC: K2XYZ HELLO FROM THE CANARIES");
}

TEST_CASE("renderer passes through frames it cannot unpack", "[js8][render]") {
    FrameRenderer r;
    int           type = 0;
    CHECK(r.render("short", &type, 1000) == "short");
    CHECK(r.render("has a space!", &type, 1000) == "has a space!");
}

/* ---- Multipart text (ported from DecodeViewModelMultipartAssemblyTest.kt) */

TEST_CASE("multipart text joining matches the Android port", "[js8][assembler]") {
    auto directed = [](const std::string &t) { return frame(t, FRAME_FIRST); };
    auto data     = [](const std::string &t) { return frame(t, FRAME_DATA); };

    // Data frames split mid-token and concatenate without a space.
    CHECK(assemble_multipart_text({data("N5EKS GRID EM10"), data("SM87MJ")}) == "N5EKS GRID EM10SM87MJ");
    // A buffered command strips its separator, so the space is put back.
    CHECK(assemble_multipart_text({directed("NT5DF: N5EKS GRID"), data("EM13TE")}) == "NT5DF: N5EKS GRID EM13TE");
    // An unbuffered command's payload brings its own space.
    CHECK(assemble_multipart_text({directed("NT5DF: N5EKS STATUS"), data(" IDLE AND MONITORING")}) ==
          "NT5DF: N5EKS STATUS IDLE AND MONITORING");
    CHECK(assemble_multipart_text({directed("2W0OXE: @RAYNET"), data("TEST")}) == "2W0OXE: @RAYNET TEST");
    CHECK(assemble_multipart_text(
              {directed("NT5DF: N5EKS MSG"), data("HELLO WORL"), data("D HOW ARE Y"), data("OU")}) ==
          "NT5DF: N5EKS MSG HELLO WORLD HOW ARE YOU");
    // A blanked helper frame leaves no leading space.
    CHECK(assemble_multipart_text({directed(""), data("NT5DF: N5EKS GRID EM13")}) == "NT5DF: N5EKS GRID EM13");
    CHECK(assemble_multipart_text({directed("K0OG: KN4CRD SNR +2")}) == "K0OG: KN4CRD SNR +2");
}

/* ---- Assembler buffering ---------------------------------------------- */

TEST_CASE("assembler groups frames by offset and emits on the last frame", "[js8][assembler]") {
    std::vector<std::string> out;
    MessageAssembler         a([&](const RxFrame &m) { out.push_back(m.text); });

    // Two stations interleaved; the second frame of A drifts 5 Hz.
    a.add(frame("A1AA: B1BB", FRAME_FIRST, 1000));
    a.add(frame("C1CC: D1DD", FRAME_FIRST, 1500));
    a.add(frame(" HELLO", FRAME_DATA, 1005));
    a.add(frame(" THERE", FRAME_DATA | FRAME_LAST, 1500));
    REQUIRE(out.size() == 1);
    CHECK(out[0] == "C1CC: D1DD THERE");
    a.add(frame(" WORLD", FRAME_DATA | FRAME_LAST, 998));
    REQUIRE(out.size() == 2);
    CHECK(out[1] == "A1AA: B1BB HELLO WORLD");
}

TEST_CASE("assembler treats first+last frames as complete messages", "[js8][assembler]") {
    std::vector<std::string> out;
    MessageAssembler         a([&](const RxFrame &m) { out.push_back(m.text); });
    a.add(frame("A1AA: B1BB", FRAME_FIRST, 1000));
    // A new single-frame message at the same offset replaces the stale buffer.
    a.add(frame("A1AA: B1BB SNR -5", FRAME_FIRST | FRAME_LAST, 1002));
    REQUIRE(out == std::vector<std::string>{"A1AA: B1BB SNR -5"});
    a.add(frame(" LATE", FRAME_DATA | FRAME_LAST, 1000));
    CHECK(out.back() == " LATE");
}

TEST_CASE("assembler flushes incomplete messages after 90 s", "[js8][assembler]") {
    std::vector<std::string> out;
    MessageAssembler         a([&](const RxFrame &m) { out.push_back(m.text); });
    a.add(frame("A1AA: B1BB", FRAME_FIRST, 1000, 0));
    a.add(frame(" PARTIAL", FRAME_DATA, 1000, 15'000));
    a.flush_stale(60'000);
    CHECK(out.empty());
    a.flush_stale(90'001);
    REQUIRE(out.size() == 1);
    CHECK(out[0] == "A1AA: B1BB PARTIAL");
}

/* ---- Classification --------------------------------------------------- */

TEST_CASE("classify recognises heartbeats, CQs and messages to me", "[js8][classify]") {
    auto hb = classify("W1ABC: @HB HEARTBEAT FN42", "K2XYZ");
    CHECK(hb.heartbeat);
    CHECK(hb.from == "W1ABC");
    CHECK_FALSE(hb.to_me);

    auto cq = classify("W1ABC: @ALLCALL CQ CQ CQ FN42", "K2XYZ");
    CHECK(cq.cq);
    CHECK(cq.to_group);
    CHECK(cq.to == "@ALLCALL");

    auto dm = classify("W1ABC: K2XYZ SNR?", "k2xyz");
    CHECK(dm.to_me);
    CHECK(dm.to == "K2XYZ");

    // Portable and prefixed forms of my call still count as me.
    CHECK(classify("W1ABC: K2XYZ/P HELLO", "K2XYZ").to_me);
    CHECK(classify("W1ABC: VE3/K2XYZ HELLO", "K2XYZ").to_me);
    CHECK_FALSE(classify("W1ABC: K2XYA HELLO", "K2XYZ").to_me);
    CHECK_FALSE(classify("free text", "K2XYZ").to_me);
}

TEST_CASE("base_callsign strips prefixes and suffixes", "[js8][classify]") {
    CHECK(base_callsign("EA8/G4ABC/P") == "G4ABC");
    CHECK(base_callsign("k1abc") == "K1ABC");
    CHECK(base_callsign("VE3/K1ABC") == "K1ABC");
}

/* ---- Buffered-command checksums --------------------------------------- */

TEST_CASE("valid MSG, MSG TO: and relay checksums are verified and stripped", "[js8][checksum]") {
    for (auto [sent, shown] : std::vector<std::pair<std::string, std::string>>{
             {"K2XYZ MSG THIS IS A STORED MESSAGE FOR YOU", "W1ABC: K2XYZ MSG THIS IS A STORED MESSAGE FOR YOU"},
             {"K2XYZ MSG TO: N0CALL SEE YOU TOMORROW", "W1ABC: K2XYZ MSG TO: N0CALL SEE YOU TOMORROW"},
             {"K2XYZ>N0CALL HELLO VIA RELAY", "W1ABC: K2XYZ > N0CALL HELLO VIA RELAY"},
         }) {
        auto text = roundtrip("W1ABC", "K2XYZ", sent);
        INFO(sent << " -> " << text);
        CHECK(verify_command_checksum(text) == Checksum::Valid);
        CHECK(text == shown);
    }
}

TEST_CASE("a corrupted buffered message fails its checksum and is left as-is", "[js8][checksum]") {
    auto text = roundtrip("W1ABC", "K2XYZ", "K2XYZ MSG THIS IS A STORED MESSAGE FOR YOU");
    auto pos  = text.find("STORED");
    REQUIRE(pos != std::string::npos);
    text[pos] = 'X';
    auto before = text;
    CHECK(verify_command_checksum(text) == Checksum::Invalid);
    CHECK(text == before);
}

TEST_CASE("commands without a buffered payload have no checksum", "[js8][checksum]") {
    for (auto sent : {"K2XYZ QUERY MSGS", "K2XYZ SNR?", "K2XYZ HELLO THERE", "K2XYZ GRID FN42AB"}) {
        auto text = roundtrip("W1ABC", "K2XYZ", sent);
        INFO(text);
        CHECK(verify_command_checksum(text) == Checksum::None);
    }
}

/* ---- End to end: radio-rate audio through the receiver ---------------- */

TEST_CASE("receiver decodes a multi-frame message from 11025 Hz audio", "[js8][receiver][slow]") {
    constexpr int    RATE = 11025;
    constexpr double NSPS = 0.160 * RATE; // 1764 samples per symbol

    auto frames = vc::build_message_frames("W1ABC", "FN42", "K2XYZ", "K2XYZ HELLO FROM THE X6100 TEST", false,
                                           false, 0);
    REQUIRE(frames.size() == 3);

    std::mutex               mu;
    std::condition_variable  cv;
    std::vector<std::string> messages;
    std::vector<std::string> decoded_frames;
    std::size_t              cycles = 0;

    Receiver::Config cfg;
    cfg.input_rate           = RATE;
    cfg.realign_threshold_ms = 0; // audio is fed much faster than real time

    Receiver::Callbacks cb;
    cb.on_frame = [&](const RxFrame &f) {
        std::lock_guard<std::mutex> l(mu);
        decoded_frames.push_back(f.text);
    };
    cb.on_message = [&](const RxFrame &m) {
        std::lock_guard<std::mutex> l(mu);
        messages.push_back(m.text);
        cv.notify_all();
    };
    cb.on_cycle_done = [&](std::size_t) {
        std::lock_guard<std::mutex> l(mu);
        cycles++;
        cv.notify_all();
    };
    Receiver rx(cfg, cb);

    // The ring aligns to the wall clock on the first buffer, so start the
    // audio with silence up to the next 15 s slot boundary.
    const std::int64_t now      = wall_ms();
    const std::int64_t to_slot  = 15'000 - (now % 15'000);
    std::vector<float> audio((std::size_t)(to_slot * RATE / 1000), 0.0f);

    // Plus one whole silent slot. Its decode builds the decoder's FFT plans
    // (seconds on a slow machine), so we wait for it before the real signal
    // and never have a decode skipped because the previous one is running.
    audio.resize(audio.size() + 15 * RATE, 0.0f);
    const std::size_t warmup_end = audio.size();

    const auto &costas = js8core::protocol::costas(js8core::protocol::CostasType::Original);
    for (auto &[text, bits] : frames) {
        int tones[js8core::kJs8NumSymbols];
        js8core::legacy_encode(bits, costas, text.c_str(), tones);

        std::size_t slot_start = audio.size();
        audio.resize(slot_start + 15 * RATE, 0.0f);
        double phi   = 0;
        auto   start = slot_start + RATE / 2; // JS8 Normal's 0.5 s start delay
        for (int s = 0; s < js8core::kJs8NumSymbols; ++s) {
            double dphi = 2 * M_PI * (1200.0 + tones[s] * 6.25) / RATE;
            for (int i = 0; i < (int)NSPS; ++i) {
                audio[start + (std::size_t)(s * NSPS) + i] += 0.1f * (float)std::sin(phi);
                phi += dphi;
            }
        }
    }
    audio.resize(audio.size() + 15 * RATE, 0.0f); // one more slot to trigger the last decode

    std::mt19937                    rng(7);
    std::normal_distribution<float> noise(0.0f, 0.01f);
    for (auto &s : audio) s += noise(rng);

    // Feed at 10x real time: fast, but in steady pieces like a sound card,
    // and slow enough that each slot's decode finishes before the next one
    // is due (the engine skips a decode while one is still running).
    constexpr std::size_t PIECE = RATE / 10; // 100 ms of audio per 10 ms
    auto feed_range = [&](std::size_t from, std::size_t to) {
        for (std::size_t i = from; i < to; i += PIECE) {
            rx.feed(&audio[i], std::min<std::size_t>(PIECE, to - i));
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    };
    feed_range(0, warmup_end);
    {
        std::unique_lock<std::mutex> l(mu);
        REQUIRE(cv.wait_for(l, std::chrono::seconds(60), [&] { return cycles > 0; }));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    feed_range(warmup_end, audio.size());

    std::unique_lock<std::mutex> l(mu);
    bool got = cv.wait_for(l, std::chrono::seconds(120), [&] { return !messages.empty(); });
    INFO("frames decoded: " << decoded_frames.size());
    REQUIRE(got);
    CHECK(messages[0] == "W1ABC: K2XYZ HELLO FROM THE X6100 TEST");
    CHECK(decoded_frames.size() == 3);
}

TEST_CASE("receiver re-snaps to the clock when audio and wall time disagree", "[js8][receiver]") {
    Receiver::Config cfg;
    cfg.input_rate = 11025;
    Receiver rx(cfg, {});

    // 5 s of audio in well under 2 s of wall time: the sample count runs ahead.
    std::vector<float> piece(11025 / 10, 0.0f);
    for (int i = 0; i < 50; ++i) rx.feed(piece.data(), piece.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    CHECK(rx.realign_count() == 0); // not checked until 2 s have passed

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    rx.feed(piece.data(), piece.size());
    for (int i = 0; i < 40 && rx.realign_count() == 0; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(rx.realign_count() == 1);
}

TEST_CASE("receiver fills an audio gap with silence instead of realigning", "[js8][receiver]") {
    // The dialog stops feeding audio while it transmits. A plain realign would
    // leave minute-old audio in the ring, and it would decode again as new.
    Receiver::Config cfg;
    cfg.input_rate = 11025;
    std::mutex               mu;
    std::vector<std::string> logs;
    Receiver::Callbacks      cb;
    cb.on_log = [&](const std::string &s) {
        std::lock_guard<std::mutex> lk(mu);
        logs.push_back(s);
    };
    Receiver rx(cfg, cb);

    std::vector<float> piece(11025 / 10, 0.0f);
    rx.feed(piece.data(), piece.size());
    std::this_thread::sleep_for(std::chrono::milliseconds(2600)); // ~2.5 s missing
    rx.feed(piece.data(), piece.size());

    bool filled = false;
    for (int i = 0; i < 40 && !filled; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> lk(mu);
        for (auto &l : logs) filled |= l.rfind("gap:", 0) == 0;
    }
    CHECK(filled);
    CHECK(rx.realign_count() == 0);
}

TEST_CASE("lat/lon to Maidenhead grid", "[js8][ops][aprs]") {
    char g[12];
    REQUIRE(js8_latlon_to_grid(41.714775, -72.727260, 6, g, sizeof(g))); // W1AW
    CHECK(std::string(g) == "FN31PR");
    REQUIRE(js8_latlon_to_grid(0.0, 0.0, 10, g, sizeof(g)));
    CHECK(std::string(g) == "JJ00AA00AA");
    REQUIRE(js8_latlon_to_grid(-33.8688, 151.2093, 6, g, sizeof(g))); // Sydney
    CHECK(std::string(g) == "QF56OD");
    REQUIRE(js8_latlon_to_grid(90.0, 180.0, 4, g, sizeof(g))); // the far corner stays in range
    CHECK(std::string(g) == "RR99");
    // Longer grids extend the shorter ones.
    char g6[8], g10[12];
    REQUIRE(js8_latlon_to_grid(49.2827, -123.1207, 6, g6, sizeof(g6)));
    REQUIRE(js8_latlon_to_grid(49.2827, -123.1207, 10, g10, sizeof(g10)));
    CHECK(std::string(g10).rfind(g6, 0) == 0);
    CHECK(std::string(g6) == "CN89KG"); // downtown Vancouver; L starts at -123.0833
    CHECK_FALSE(js8_latlon_to_grid(95.0, 0.0, 6, g, sizeof(g)));
    CHECK_FALSE(js8_latlon_to_grid(0.0, 0.0, 7, g, sizeof(g)));
    CHECK_FALSE(js8_latlon_to_grid(0.0, 0.0, 10, g, 8));
}

TEST_CASE("clock correction is the negated median DT", "[js8][ops]") {
    float c = 0;
    const float few[] = {1.0f, 1.2f};
    CHECK_FALSE(js8_clock_correction(few, 2, &c));
    const float dts[] = {1.1f, 0.9f, 5.0f, 1.0f, -3.0f}; // outliers don't matter
    REQUIRE(js8_clock_correction(dts, 5, &c));
    CHECK(c == Catch::Approx(-1.0f));
    const float even[] = {0.2f, 0.4f, 0.6f, 0.8f};
    REQUIRE(js8_clock_correction(even, 4, &c));
    CHECK(c == Catch::Approx(-0.5f));
}

TEST_CASE("DT is 0 on time and positive when a signal starts late", "[js8][receiver][slow]") {
    // The sign the Time Sync button relies on: our clock 1 s fast means
    // signals appear 1 s late, DT = +1, correction -1 s.
    constexpr int    RATE = 11025;
    constexpr double NSPS = 0.160 * RATE;

    auto frames = vc::build_message_frames("W1ABC", "FN42", "", "W1ABC: @HB HEARTBEAT FN42", false, false, 0);
    REQUIRE(frames.size() == 1);

    for (double late : {0.0, 1.0}) {
        std::mutex         mu;
        std::condition_variable cv;
        std::vector<float> dts;
        std::size_t        cycles = 0;

        Receiver::Config cfg;
        cfg.input_rate           = RATE;
        cfg.realign_threshold_ms = 0;
        Receiver::Callbacks cb;
        cb.on_frame = [&](const RxFrame &f) {
            std::lock_guard<std::mutex> l(mu);
            dts.push_back(f.dt);
            cv.notify_all();
        };
        cb.on_cycle_done = [&](std::size_t) {
            std::lock_guard<std::mutex> l(mu);
            cycles++;
            cv.notify_all();
        };
        Receiver rx(cfg, cb);

        const std::int64_t now     = wall_ms();
        const std::int64_t to_slot = 15'000 - (now % 15'000);
        std::vector<float> audio((std::size_t)(to_slot * RATE / 1000), 0.0f);
        audio.resize(audio.size() + 15 * RATE, 0.0f); // warm-up slot
        const std::size_t warmup_end = audio.size();

        const auto &costas = js8core::protocol::costas(js8core::protocol::CostasType::Original);
        int         tones[js8core::kJs8NumSymbols];
        js8core::legacy_encode(frames[0].second, costas, frames[0].first.c_str(), tones);
        std::size_t slot_start = audio.size();
        audio.resize(slot_start + 30 * RATE, 0.0f);
        double phi   = 0;
        auto   start = slot_start + (std::size_t)((0.5 + late) * RATE);
        for (int s = 0; s < js8core::kJs8NumSymbols; ++s) {
            double dphi = 2 * M_PI * (1200.0 + tones[s] * 6.25) / RATE;
            for (int i = 0; i < (int)NSPS; ++i) {
                audio[start + (std::size_t)(s * NSPS) + i] += 0.1f * (float)std::sin(phi);
                phi += dphi;
            }
        }
        std::mt19937                    rng(7);
        std::normal_distribution<float> noise(0.0f, 0.01f);
        for (auto &x : audio) x += noise(rng);

        constexpr std::size_t PIECE = RATE / 10;
        auto feed_range = [&](std::size_t from, std::size_t to) {
            for (std::size_t i = from; i < to; i += PIECE) {
                rx.feed(&audio[i], std::min<std::size_t>(PIECE, to - i));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        };
        feed_range(0, warmup_end);
        {
            std::unique_lock<std::mutex> l(mu);
            REQUIRE(cv.wait_for(l, std::chrono::seconds(60), [&] { return cycles > 0; }));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        feed_range(warmup_end, audio.size());

        std::unique_lock<std::mutex> l(mu);
        REQUIRE(cv.wait_for(l, std::chrono::seconds(120), [&] { return !dts.empty(); }));
        INFO("late " << late << " s, DT " << dts[0]);
        CHECK(dts[0] == Catch::Approx(late).margin(0.2));
    }
}

/* ---- WAV files and test mode ------------------------------------------ */

#include "js8_rx.h"
#include "testsignal.hpp"
#include "wav.hpp"

#include <cstdio>
#include <filesystem>

TEST_CASE("WAV files round-trip", "[js8][wav]") {
    auto path = (std::filesystem::temp_directory_path() / "js8_wav_roundtrip.wav").string();
    std::vector<float> in = {0.0f, 0.5f, -0.5f, 0.99f, -1.0f, 0.25f};
    REQUIRE(write_wav(path, 12000, in));

    WavAudio    wav;
    std::string err;
    REQUIRE(read_wav(path, wav, err));
    CHECK(wav.rate == 12000);
    REQUIRE(wav.samples.size() == in.size());
    for (std::size_t i = 0; i < in.size(); i++) CHECK(wav.samples[i] == Catch::Approx(in[i]).margin(1.0 / 32767));
    std::remove(path.c_str());

    CHECK_FALSE(read_wav("/nonexistent/x.wav", wav, err));
    CHECK_FALSE(err.empty());
}

namespace {
struct Collected {
    std::mutex               mu;
    std::condition_variable  cv;
    std::vector<std::string> messages;
};
void collect_message(const js8_rx_msg_t *m, void *ctx) {
    auto                       *c = static_cast<Collected *>(ctx);
    std::lock_guard<std::mutex> l(c->mu);
    c->messages.push_back(m->text);
    c->cv.notify_all();
}
} // namespace

// Real time: up to 15 s to the slot boundary plus 45 s of audio.
TEST_CASE("test mode plays a 12 kHz WAV through the decoder", "[js8][wav][.slow]") {
    std::vector<TestStation> band = {
        {"W1ABC", "FN42", "K2XYZ HELLO FROM A WAV FILE", 1200, -8},
        {"VE3KP", "FN03", "CQ CQ CQ FN03", 800, -5},
    };
    auto path = (std::filesystem::temp_directory_path() / "js8_testmode.wav").string();
    REQUIRE(write_wav(path, 12000, make_test_band(band, 12000)));

    Collected   c;
    js8_rx_cb_t cb{};
    cb.on_message = collect_message;
    cb.ctx        = &c;
    js8_rx_t *rx  = js8_rx_create(11025, JS8_SUBMODE_NORMAL, "K2XYZ", &cb);
    REQUIRE(rx);

    char  err[128] = {};
    float starts   = js8_rx_play_wav(rx, path.c_str(), err, sizeof(err));
    INFO(err);
    REQUIRE(starts >= 0.5f);
    CHECK(js8_rx_wav_active(rx));

    {
        std::unique_lock<std::mutex> l(c.mu);
        c.cv.wait_for(l, std::chrono::seconds(90), [&] { return c.messages.size() >= 2; });
        std::sort(c.messages.begin(), c.messages.end());
        REQUIRE(c.messages.size() == 2);
        CHECK(c.messages[0] == "VE3KP: @ALLCALL CQ CQ CQ FN03");
        CHECK(c.messages[1] == "W1ABC: K2XYZ HELLO FROM A WAV FILE");
    }
    js8_rx_destroy(rx);
    std::remove(path.c_str());
}

/* ---- Transmit (phase T1) ---------------------------------------------- */

#include "tx.hpp"

TEST_CASE("plan_message builds JS8Call's frames and previews what others see", "[js8][tx]") {
    struct Case {
        const char *typed, *preview;
    };
    for (auto [typed, preview] : std::vector<Case>{
             {"k2xyz snr?", "W1ABC: K2XYZ SNR?"},
             {"K2XYZ   HELLO FROM THE X6100", "W1ABC: K2XYZ HELLO FROM THE X6100"},
             {"CQ CQ CQ FN42", "W1ABC: @ALLCALL CQ CQ CQ FN42"},
             {"@ALLCALL QRV ON 20M", "W1ABC: @ALLCALL QRV ON 20M"},
             {"K2XYZ MSG SEE YOU TOMORROW", "W1ABC: K2XYZ MSG SEE YOU TOMORROW"},
         }) {
        auto plan = plan_message("w1abc", "fn42", typed);
        INFO(typed << " -> " << plan.preview << " / " << plan.error);
        REQUIRE(plan.ok());
        CHECK(plan.preview == preview);
    }
}

TEST_CASE("untargeted free text is sent with our callsign", "[js8][tx]") {
    auto plan = plan_message("W1ABC", "FN42", "JUST TESTING THE NEW RADIO");
    REQUIRE(plan.ok());
    CHECK(plan.preview.find("W1ABC") != std::string::npos);
    CHECK(plan.preview.find("JUST TESTING THE NEW RADIO") != std::string::npos);
}

TEST_CASE("plan_message keeps the spaces inside an APRS command", "[js8][tx][aprs]") {
    auto p = plan_message("VE7NHW", "CN89", "  @aprsis cmd :SMS      :@6045551234 hi{01}\n");
    REQUIRE(p.ok());
    CHECK(p.text == "@APRSIS CMD :SMS      :@6045551234 HI{01}");
}

TEST_CASE("plan_message refuses what it can't send", "[js8][tx]") {
    CHECK_FALSE(plan_message("", "FN42", "K2XYZ SNR?").ok());
    CHECK_FALSE(plan_message("W1ABC", "FN42", "   ").ok());
    auto bad = plan_message("W1ABC", "FN42", "K2XYZ CTRL\x01HERE"); // printable ASCII only
    CHECK_FALSE(bad.ok());
    CHECK(bad.error.find("can't send") != std::string::npos);
    CHECK(plan_message("W1ABC", "FN42", "K2XYZ 50% OFF").ok());
    std::string longtext(400, 'A');
    CHECK_FALSE(plan_message("W1ABC", "FN42", longtext).ok());
}

TEST_CASE("next_tx_start_ms follows JS8 slot timing", "[js8][tx]") {
    const std::int64_t slot = 1'700'000'010'000; // a 15 s boundary
    REQUIRE(slot % 15000 == 0);
    CHECK(next_tx_start_ms(slot) == slot + 500);            // still in time for this slot
    CHECK(next_tx_start_ms(slot + 499) == slot + 500);
    CHECK(next_tx_start_ms(slot + 500) == slot + 15500);    // too late: next slot
    CHECK(next_tx_start_ms(slot + 14999) == slot + 15500);
}

TEST_CASE("TX waveform stays in its channel and at full scale", "[js8][tx]") {
    auto plan = plan_message("W1ABC", "FN42", "K2XYZ SNR?");
    REQUIRE(plan.ok());
    auto audio = synth_frame(plan.frames[0].tones, 1500, 11025);
    CHECK(audio.size() == (std::size_t)TX_SYMBOLS * 1764);
    float peak = 0;
    for (float x : audio) peak = std::max(peak, std::abs(x));
    CHECK(peak == Catch::Approx(1.0f).margin(0.01));
    // Energy well away from the 1500-1550 Hz channel is small.
    double in_band  = tone_amplitude(audio, 1525, 11025);
    double off_band = tone_amplitude(audio, 1700, 11025);
    CHECK(20 * std::log10(off_band / in_band) < -30.0);
}

TEST_CASE("TX audio at the radio's rate decodes in our receiver (loopback)", "[js8][tx][.slow]") {
    constexpr int RATE = 11025;
    auto          plan = plan_message("W1ABC", "FN42", "K2XYZ HELLO FROM THE TRANSMITTER");
    REQUIRE(plan.ok());
    REQUIRE(plan.frames.size() > 1);

    std::mutex               mu;
    std::condition_variable  cv;
    std::vector<std::string> messages;
    std::size_t              cycles = 0;
    Receiver::Config         cfg;
    cfg.input_rate           = RATE;
    cfg.realign_threshold_ms = 0;
    Receiver::Callbacks cb;
    cb.on_message = [&](const RxFrame &m) {
        std::lock_guard<std::mutex> l(mu);
        messages.push_back(m.text);
        cv.notify_all();
    };
    cb.on_cycle_done = [&](std::size_t) {
        std::lock_guard<std::mutex> l(mu);
        cycles++;
        cv.notify_all();
    };
    Receiver rx(cfg, cb);

    // Lay the frames out as the transmitter would: one per slot, 0.5 s in,
    // at 30 dB below full scale plus noise.
    const std::int64_t now  = wall_ms();
    std::size_t        lead = (std::size_t)((15'000 - now % 15'000) * RATE / 1000) + 15 * RATE;
    std::vector<float> audio(lead + (plan.frames.size() + 1) * 15 * RATE, 0.0f);
    for (std::size_t i = 0; i < plan.frames.size(); i++) {
        auto        wave  = synth_frame(plan.frames[i].tones, 1200, RATE);
        std::size_t start = lead + i * 15 * RATE + RATE / 2;
        for (std::size_t k = 0; k < wave.size(); k++) audio[start + k] += 0.03f * wave[k];
    }
    std::mt19937                    rng(9);
    std::normal_distribution<float> noise(0.0f, 0.01f);
    for (auto &x : audio) x += noise(rng);

    const std::size_t piece = RATE / 10;
    for (std::size_t i = 0; i < lead; i += piece) {
        rx.feed(&audio[i], std::min(piece, lead - i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    {
        std::unique_lock<std::mutex> l(mu);
        REQUIRE(cv.wait_for(l, std::chrono::seconds(60), [&] { return cycles > 0; }));
    }
    for (std::size_t i = lead; i < audio.size(); i += piece) {
        rx.feed(&audio[i], std::min(piece, audio.size() - i));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::unique_lock<std::mutex> l(mu);
    REQUIRE(cv.wait_for(l, std::chrono::seconds(60), [&] { return !messages.empty(); }));
    CHECK(messages[0] == plan.preview);
    CHECK(messages[0] == "W1ABC: K2XYZ HELLO FROM THE TRANSMITTER");
}

namespace {
// Instant clock for Transmitter tests: waiting just advances time.
struct FakeClock : Transmitter::Clock {
    std::atomic<std::int64_t> t{1'700'000'003'000}; // partway into a slot
    std::int64_t now_ms() override { return t; }
    bool wait_until(std::int64_t ms, const std::atomic<bool> &cancelled) override {
        if (cancelled) return false;
        if (ms > t) t = ms;
        return true;
    }
};
} // namespace

TEST_CASE("Transmitter sends frames in consecutive slots", "[js8][tx]") {
    FakeClock clock;
    struct Keyed {
        std::int64_t at;
        int          index, count;
        std::size_t  samples;
    };
    std::vector<Keyed> keyed;
    bool               completed = false;
    std::mutex         mu;
    std::condition_variable cv;
    bool               done = false;

    Transmitter::Callbacks cb;
    cb.play = [&](const std::vector<float> &a, const TxFrame &, int i, int n) {
        keyed.push_back({clock.now_ms(), i, n, a.size()});
        clock.t += 12'640; // the frame's air time
        return true;
    };
    cb.on_done = [&](const std::string &, bool c) {
        std::lock_guard<std::mutex> l(mu);
        completed = c;
        done      = true;
        cv.notify_all();
    };
    Transmitter tx(11025, cb, &clock);

    auto plan = plan_message("W1ABC", "FN42", "K2XYZ HELLO FROM THE TRANSMITTER");
    REQUIRE(plan.frames.size() == 3);
    std::string why;
    REQUIRE(tx.send(plan, 1200, &why));
    CHECK_FALSE(tx.send(plan, 1200, &why)); // one message at a time
    CHECK(why == "already sending");

    std::unique_lock<std::mutex> l(mu);
    REQUIRE(cv.wait_for(l, std::chrono::seconds(10), [&] { return done; }));
    CHECK(completed);
    REQUIRE(keyed.size() == 3);
    const std::int64_t t0   = 1'700'000'003'000;
    const std::int64_t slot = t0 - t0 % 15'000 + 15'000; // next boundary after the fake clock's start
    for (int i = 0; i < 3; i++) {
        CHECK(keyed[i].at == slot + i * 15'000 + 500);
        CHECK(keyed[i].index == i);
        CHECK(keyed[i].count == 3);
        CHECK(keyed[i].samples == (std::size_t)TX_SYMBOLS * 1764);
    }
    CHECK_FALSE(tx.busy());
}

TEST_CASE("Transmitter stop abandons the rest of the message", "[js8][tx]") {
    FakeClock   clock;
    int         played = 0;
    bool        completed = true, done = false;
    std::mutex  mu;
    std::condition_variable cv;
    Transmitter *txp = nullptr;

    Transmitter::Callbacks cb;
    cb.play = [&](const std::vector<float> &, const TxFrame &, int, int) {
        played++;
        clock.t += 12'640;
        if (played == 1) {
            // The user presses Stop while the first frame is on the air.
            std::thread([&] { txp->stop(); }).detach();
            while (!txp->stopping()) std::this_thread::yield();
            return false;
        }
        return true;
    };
    cb.on_done = [&](const std::string &, bool c) {
        std::lock_guard<std::mutex> l(mu);
        completed = c;
        done      = true;
        cv.notify_all();
    };
    Transmitter tx(11025, cb, &clock);
    txp = &tx;

    REQUIRE(tx.send(plan_message("W1ABC", "FN42", "K2XYZ HELLO FROM THE TRANSMITTER"), 1200));
    std::unique_lock<std::mutex> l(mu);
    REQUIRE(cv.wait_for(l, std::chrono::seconds(10), [&] { return done; }));
    CHECK(played == 1);
    CHECK_FALSE(completed);
}

TEST_CASE("Transmitter rejects out-of-range offsets", "[js8][tx]") {
    FakeClock   clock;
    Transmitter tx(11025, {}, &clock);
    auto        plan = plan_message("W1ABC", "FN42", "K2XYZ SNR?");
    std::string why;
    CHECK_FALSE(tx.send(plan, 300, &why));
    CHECK_FALSE(tx.send(plan, 2480, &why));
    CHECK(why.find("offset") != std::string::npos);
}

/* ---- T3: queries, heartbeats, station list ---------------------------- */

#include "commands.hpp"
#include "stations.hpp"

TEST_CASE("one-press messages have desktop JS8Call's wording and survive the air", "[js8][t3]") {
    struct Case {
        Query       q;
        const char *text, *seen;
    };
    for (auto [q, text, seen] : std::vector<Case>{
             {Query::SnrQ, "N0XYZ SNR?", "W1ABC: N0XYZ SNR?"},
             {Query::SendSnr, "N0XYZ SNR -12", "W1ABC: N0XYZ SNR -12"},
             {Query::GridQ, "N0XYZ GRID?", "W1ABC: N0XYZ GRID?"},
             {Query::MyGrid, "N0XYZ GRID FN42AB", "W1ABC: N0XYZ GRID FN42AB"},
             {Query::InfoQ, "N0XYZ INFO?", "W1ABC: N0XYZ INFO?"},
             {Query::StatusQ, "N0XYZ STATUS?", "W1ABC: N0XYZ STATUS?"},
             {Query::HearingQ, "N0XYZ HEARING?", "W1ABC: N0XYZ HEARING?"},
             {Query::AgnQ, "N0XYZ AGN?", "W1ABC: N0XYZ AGN?"},
             {Query::RR, "N0XYZ RR", "W1ABC: N0XYZ RR"},
             {Query::SeventyThree, "N0XYZ 73", "W1ABC: N0XYZ 73"},
         }) {
        auto t = query_text(q, "n0xyz", -12, "fn42ab");
        CHECK(t == text);
        auto plan = plan_message("W1ABC", "FN42AB", t);
        INFO(t << " -> " << plan.preview << plan.error);
        REQUIRE(plan.ok());
        CHECK(plan.preview == seen);
    }
    CHECK(query_text(Query::SendSnr, "N0XYZ", 5, "") == "N0XYZ SNR +05");
    CHECK(query_text(Query::SendSnr, "N0XYZ", -99, "").empty()); // desktop sends nothing out of range
    CHECK(query_text(Query::MyGrid, "N0XYZ", 0, "").empty());
    CHECK(query_text(Query::SnrQ, "", 0, "").empty());
}

TEST_CASE("heartbeat is sent the way desktop JS8Call sends it", "[js8][t3]") {
    CHECK(heartbeat_text("w1abc", "fn42ab") == "W1ABC: HEARTBEAT FN42");
    auto plan = plan_message("W1ABC", "FN42AB", heartbeat_text("W1ABC", "FN42AB"));
    REQUIRE(plan.ok());
    CHECK(plan.frames.size() == 1);
    CHECK(plan.preview == "W1ABC: @HB HEARTBEAT FN42");
}

TEST_CASE("free heartbeat offsets follow desktop's rule", "[js8][t3]") {
    std::mt19937       rng(1);
    const std::int64_t now = 1'000'000;

    for (int i = 0; i < 50; i++) {
        int f = find_free_offset({}, now, rng);
        CHECK(f >= 500);
        CHECK(f < 1000);
    }

    // Everything but 700 Hz heard in the last 30 s. Desktop's search only
    // tries random slots, so it finds the one clear slot some of the time
    // and otherwise falls back to 500 Hz; never anything else.
    std::vector<OffsetActivity> busy;
    for (int f = 500; f < 1000; f += 50)
        if (f != 700) busy.push_back({(float)f, now - 5'000});
    int found = 0;
    for (int i = 0; i < 200; i++) {
        int f = find_free_offset(busy, now, rng);
        INFO(f);
        bool clear = true;
        for (auto &a : busy) clear &= std::fabs(a.offset_hz - f) >= 50;
        CHECK((clear || f == 500));
        found += clear;
    }
    CHECK(found > 50);

    // Activity older than 30 s doesn't count.
    std::vector<OffsetActivity> old;
    for (int f = 500; f < 1000; f += 10) old.push_back({(float)f, now - 31'000});
    int f = find_free_offset(old, now, rng);
    CHECK(f >= 500);
    CHECK(f < 1000);

    // A solid band falls back to 500 Hz, as desktop does.
    std::vector<OffsetActivity> full;
    for (int f2 = 450; f2 < 1050; f2 += 10) full.push_back({(float)f2, now});
    CHECK(find_free_offset(full, now, rng) == 500);
}

namespace {
// Run `text` from `call` through JS8Call's encoder and our decoder, then
// classify it as the dialog does.
StationEvent heard(const std::string &call, const std::string &grid, const std::string &text, int snr,
                   std::int64_t when, const std::string &my_call = "K2XYZ") {
    auto plan = plan_message(call, grid, text);
    REQUIRE(plan.ok());
    auto         mc = classify(plan.preview, my_call);
    StationEvent ev;
    ev.from    = mc.from;
    ev.to      = mc.to;
    ev.text    = plan.preview;
    ev.to_me   = mc.to_me;
    ev.snr     = snr;
    ev.freq_hz = 1000;
    ev.when_ms = when;
    return ev;
}
} // namespace

TEST_CASE("station list marks who heard us, with the SNR they reported", "[js8][t3]") {
    StationList list;
    const std::int64_t t0 = 10'000'000;

    list.add(heard("VE3KP", "FN03", "CQ CQ CQ FN03", -7, t0), "K2XYZ");
    list.add(heard("DL1XX", "JO62", "DL1XX: HEARTBEAT JO62", -24, t0 + 1000), "K2XYZ");
    // K9ABC acknowledges our heartbeat, as desktop JS8Call's auto-reply does.
    list.add(heard("K9ABC", "EN52", "K2XYZ HEARTBEAT SNR -08", -15, t0 + 2000), "K2XYZ");
    // G4ABC sends us a message (heard us, but no SNR report).
    list.add(heard("G4ABC", "IO91", "K2XYZ HELLO FROM LONDON", -19, t0 + 3000), "K2XYZ");
    // Our own heartbeat echoing back isn't a station.
    list.add(heard("K2XYZ", "FN42", "K2XYZ: HEARTBEAT FN42", 0, t0 + 4000), "K2XYZ");

    auto s = list.sorted(t0 + 5000);
    REQUIRE(s.size() == 4);
    CHECK(s[0].call == "G4ABC"); // heard us, most recently
    CHECK(s[0].heard_me);
    CHECK_FALSE(s[0].reported_snr.has_value());
    CHECK(s[1].call == "K9ABC");
    CHECK(s[1].heard_me);
    REQUIRE(s[1].reported_snr.has_value());
    CHECK(*s[1].reported_snr == -8);
    CHECK(s[1].snr == -15);
    CHECK(s[2].call == "DL1XX"); // then the rest, most recent first
    CHECK(s[2].grid == "JO62");
    CHECK_FALSE(s[2].heard_me);
    CHECK(s[3].call == "VE3KP");
    CHECK(s[3].grid == "FN03");

    // An hour later they've all expired.
    CHECK(list.sorted(t0 + StationList::EXPIRE_MS + 10'000).empty());
}

/* ---- T4: auto-reply, heartbeat acks, heartbeat timing ------------------ */

#include "autoreply.hpp"

namespace {
// A message from `call` built by JS8Call's encoder and decoded by ours.
Incoming incoming(const std::string &call, const std::string &text, int snr, const std::string &my_call = "K2XYZ") {
    auto plan = plan_message(call, "EN52", text);
    REQUIRE(plan.ok());
    auto     mc = classify(plan.preview, my_call);
    Incoming in;
    in.from      = mc.from;
    in.to        = mc.to;
    in.text      = plan.preview;
    in.to_me     = mc.to_me;
    in.to_group  = mc.to_group;
    in.heartbeat = mc.heartbeat;
    in.snr       = snr;
    return in;
}
AutoSettings settings() {
    AutoSettings s;
    s.my_call = "K2XYZ";
    s.my_grid = "FN42AB";
    s.info    = "X6100 5W EFHW";
    s.status  = "IDLE";
    return s;
}
} // namespace

TEST_CASE("auto-reply builds desktop JS8Call's answers to queries", "[js8][t4]") {
    auto                     s     = settings();
    std::vector<std::string> heard = {"N0XYZ", "VE3KP", "K2XYZ", "G4ABC", "DL1XX", "W1ABC"};

    struct Case {
        const char *asked, *answer;
    };
    for (auto [asked, answer] : std::vector<Case>{
             {"K2XYZ SNR?", "N0XYZ SNR -12"},
             {"K2XYZ GRID?", "N0XYZ GRID FN42AB"},
             {"K2XYZ INFO?", "N0XYZ INFO X6100 5W EFHW"},
             {"K2XYZ STATUS?", "N0XYZ STATUS IDLE"},
             // up to 4, not the asker, not us
             {"K2XYZ HEARING?", "N0XYZ HEARING VE3KP G4ABC DL1XX W1ABC"},
             {"K2XYZ AGN?", "N0XYZ HELLO AGAIN"},
         }) {
        auto r = build_reply(incoming("N0XYZ", asked, -12), s, heard, "N0XYZ HELLO AGAIN");
        INFO(asked);
        REQUIRE(r.has_value());
        CHECK(r->text == answer);
        CHECK(r->kind == ReplyKind::Query);
        // What we'd send decodes back as a normal message from us.
        CHECK(plan_message("K2XYZ", "FN42AB", r->text).ok());
    }
}

TEST_CASE("auto-reply ignores what desktop JS8Call ignores", "[js8][t4]") {
    auto s = settings();
    // Queries to someone else, or to a group.
    CHECK_FALSE(build_reply(incoming("N0XYZ", "W1ABC SNR?", -5), s, {}, "").has_value());
    CHECK_FALSE(build_reply(incoming("N0XYZ", "@ALLCALL SNR?", -5), s, {}, "").has_value());
    // Low-confidence decodes.
    auto low           = incoming("N0XYZ", "K2XYZ SNR?", -5);
    low.low_confidence = true;
    CHECK_FALSE(build_reply(low, s, {}, "").has_value());
    // Nothing to say: no INFO text, no grid, nothing heard, nothing sent yet.
    auto empty = s;
    empty.info = empty.status = empty.my_grid = "";
    CHECK_FALSE(build_reply(incoming("N0XYZ", "K2XYZ INFO?", -5), empty, {}, "").has_value());
    CHECK_FALSE(build_reply(incoming("N0XYZ", "K2XYZ STATUS?", -5), empty, {}, "").has_value());
    CHECK_FALSE(build_reply(incoming("N0XYZ", "K2XYZ GRID?", -5), empty, {}, "").has_value());
    CHECK_FALSE(build_reply(incoming("N0XYZ", "K2XYZ HEARING?", -5), empty, {"N0XYZ"}, "").has_value());
    CHECK_FALSE(build_reply(incoming("N0XYZ", "K2XYZ AGN?", -5), empty, {}, "").has_value());
    // Ordinary chat isn't a query.
    CHECK_FALSE(build_reply(incoming("N0XYZ", "K2XYZ HELLO THERE", -5), s, {}, "").has_value());
}

TEST_CASE("heartbeats get desktop's ack; acks and our own traffic don't", "[js8][t4]") {
    auto s = settings();
    auto r = build_reply(incoming("K9ABC", "K9ABC: HEARTBEAT EN52", -8), s, {}, "");
    REQUIRE(r.has_value());
    CHECK(r->kind == ReplyKind::HeartbeatAck);
    CHECK(r->text == "K9ABC HEARTBEAT SNR -08");
    auto plan = plan_message("K2XYZ", "FN42AB", r->text);
    REQUIRE(plan.ok());
    CHECK(plan.preview == "K2XYZ: K9ABC HEARTBEAT SNR -08");

    // Someone's ack to us, or to another station: not acked again.
    CHECK_FALSE(build_reply(incoming("K9ABC", "K2XYZ HEARTBEAT SNR -08", -8), s, {}, "").has_value());
    CHECK_FALSE(build_reply(incoming("K9ABC", "W1ABC HEARTBEAT SNR -08", -8), s, {}, "").has_value());
    // Our own heartbeat heard back.
    CHECK_FALSE(build_reply(incoming("K2XYZ", "K2XYZ: HEARTBEAT FN42", -8), s, {}, "").has_value());
    // A CQ isn't a heartbeat.
    CHECK_FALSE(build_reply(incoming("VE3KP", "CQ CQ CQ FN03", -8), s, {}, "").has_value());
}

TEST_CASE("the switches: AUTO sends, off offers; HB ACK needs AUTO and HB", "[js8][t4]") {
    AutoPolicy         p;
    const std::int64_t t = 1'000'000'000;
    p.user_activity(t);
    auto s = settings();

    auto query = *build_reply(incoming("N0XYZ", "K2XYZ SNR?", -12), s, {}, "");
    auto ack   = *build_reply(incoming("K9ABC", "K9ABC: HEARTBEAT EN52", -8), s, {}, "");

    // All off (the default): offer the query reply, ignore heartbeats.
    CHECK(p.decide(query, s, t) == AutoPolicy::Action::Offer);
    CHECK(p.decide(ack, s, t) == AutoPolicy::Action::Ignore);

    s.autoreply = true;
    CHECK(p.decide(query, s, t) == AutoPolicy::Action::Send);
    s.hb_ack = true; // without HB networking: still no acks
    CHECK(p.decide(ack, s, t) == AutoPolicy::Action::Ignore);
    s.heartbeat = true;
    CHECK(p.decide(ack, s, t) == AutoPolicy::Action::Send);
    s.autoreply = false; // HB + HB ACK without AUTO: no acks either
    CHECK(p.decide(ack, s, t) == AutoPolicy::Action::Ignore);
    s.autoreply = true;

    // Rate limits: same station and command within 5 min; acks within 15 min.
    p.sent(query, t);
    CHECK(p.decide(query, s, t + 60'000) == AutoPolicy::Action::Ignore);
    CHECK(p.decide(query, s, t + AutoPolicy::QUERY_REPEAT_MS + 1) == AutoPolicy::Action::Send);
    p.sent(ack, t);
    CHECK(p.decide(ack, s, t + 10 * 60'000) == AutoPolicy::Action::Ignore);
    CHECK(p.decide(ack, s, t + AutoPolicy::HB_ACK_REPEAT_MS + 1) == AutoPolicy::Action::Send);

    // Idle watchdog: an hour without a key press stops automatic TX.
    AutoPolicy idle;
    idle.user_activity(t);
    CHECK(idle.decide(query, s, t + AutoPolicy::IDLE_MS + 1) == AutoPolicy::Action::Offer);
    CHECK(idle.decide(ack, s, t + AutoPolicy::IDLE_MS + 1) == AutoPolicy::Action::Ignore);
    idle.user_activity(t + AutoPolicy::IDLE_MS + 2);
    CHECK(idle.decide(query, s, t + AutoPolicy::IDLE_MS + 3) == AutoPolicy::Action::Send);
}

TEST_CASE("a QSO starts with any message to us except a heartbeat ack", "[js8][t4]") {
    CHECK(starts_qso(incoming("N0XYZ", "K2XYZ HELLO", -5)));
    CHECK(starts_qso(incoming("N0XYZ", "K2XYZ SNR?", -5)));
    CHECK_FALSE(starts_qso(incoming("K9ABC", "K2XYZ HEARTBEAT SNR -08", -5)));
    CHECK_FALSE(starts_qso(incoming("VE3KP", "CQ CQ CQ FN03", -5)));
    CHECK_FALSE(starts_qso(incoming("N0XYZ", "W1ABC HELLO", -5)));
}

TEST_CASE("heartbeat timing follows desktop's scheduleHeartbeat", "[js8][t4]") {
    std::mt19937       rng(3);
    const std::int64_t now = 1'700'000'007'400; // 7.4 s past a 15 s boundary
    int                later = 0;
    for (int i = 0; i < 400; i++) {
        auto t = next_heartbeat_ms(now, 10, rng);
        // next boundary (…015 s) + 1 s + 10 min, sometimes one slot later
        const std::int64_t base = (now / 1000 + 14) / 15 * 15 * 1000 + 1000 + 10 * 60'000;
        CHECK((t == base || t == base + 15'000));
        later += t != base;
    }
    CHECK(later > 60);  // about 25 %
    CHECK(later < 140);
    // Interval clamped to 5-30 min.
    auto lo = next_heartbeat_ms(now, 1, rng), hi = next_heartbeat_ms(now, 99, rng);
    CHECK(lo - now < 6 * 60'000 + 20'000);
    CHECK(hi - now > 29 * 60'000);
}

// ---- QSO log ---------------------------------------------------------------

TEST_CASE("a two-way QSO ending in 73 is offered for logging once", "[js8][log]") {
    QsoTracker t;
    const std::string me = "VE7NHW";
    CHECK_FALSE(t.sent("VE7NHW: N7EAL SNR -12", me, 1000));
    CHECK_FALSE(t.received("N7EAL", "N7EAL: VE7NHW SNR -08 TNX", true, -11, me, 16000));
    CHECK_FALSE(t.received("N7EAL", "N7EAL: VE7NHW GRID DN17", true, -9, me, 31000));
    auto ended = t.sent("VE7NHW: N7EAL TU 73!", me, 46000);
    REQUIRE(ended);
    CHECK(*ended == "N7EAL");
    CHECK_FALSE(t.received("N7EAL", "N7EAL: VE7NHW 73 SK", true, -9, me, 61000)); // once

    auto q = t.get("N7EAL", 62000);
    REQUIRE(q);
    CHECK(q->start_ms == 1000);
    CHECK(q->sent_snr == -12);
    CHECK(q->rcvd_snr == -8);
    CHECK(q->heard_snr == -9);
    CHECK(q->grid == "DN17");

    t.logged("N7EAL");
    CHECK_FALSE(t.get("N7EAL", 62000));
}

TEST_CASE("heartbeat acks and one-way traffic aren't QSOs", "[js8][log]") {
    QsoTracker t;
    const std::string me = "VE7NHW";
    // They ack our heartbeat, we ack theirs: reports, but no QSO.
    CHECK_FALSE(t.received("KK6WVY", "KK6WVY: VE7NHW HEARTBEAT SNR -23", true, -20, me, 0));
    CHECK_FALSE(t.sent("VE7NHW: KK6WVY HEARTBEAT SNR -20", me, 15000));
    CHECK_FALSE(t.received("KK6WVY", "KK6WVY: VE7NHW 73", true, -20, me, 30000)); // we never sent
    auto q = t.get("KK6WVY", 30000);
    REQUIRE(q);
    CHECK(q->rcvd_snr == -23);
    CHECK(q->sent_snr == -20);
    CHECK_FALSE(q->we_sent);

    // Groups, our own call and other people's QSOs are ignored.
    CHECK_FALSE(t.sent("VE7NHW: @ALLCALL CQ CQ CN89 73", me, 0));
    CHECK_FALSE(t.received("W1ABC", "W1ABC: K2XYZ 73", false, -5, me, 0));
    CHECK_FALSE(t.get("W1ABC", 0));
    CHECK_FALSE(t.get("@ALLCALL", 0));
}

TEST_CASE("QSOs match base calls and expire", "[js8][log]") {
    QsoTracker t;
    const std::string me = "VE7NHW";
    t.sent("VE7NHW: VA7XYZ/P HELLO", me, 0);
    CHECK(t.received("VA7XYZ", "VA7XYZ: VE7NHW RR SK", true, -3, me, 15000));
    auto q = t.get("VA7XYZ/P", 15000);
    REQUIRE(q);
    CHECK(q->call == "VA7XYZ");

    // Half an hour later it's a new QSO.
    std::int64_t later = 15000 + QsoTracker::EXPIRE_MS + 1;
    CHECK_FALSE(t.get("VA7XYZ", later));
    t.sent("VE7NHW: VA7XYZ HI AGAIN", me, later);
    q = t.get("VA7XYZ", later);
    REQUIRE(q);
    CHECK(q->start_ms == later);
    CHECK_FALSE(q->they_sent);
    CHECK_FALSE(q->offered);
}

TEST_CASE("ADIF records are desktop JS8Call's plus power and POTA/SOTA", "[js8][log]") {
    LogEntry e;
    e.call     = "N7EAL";
    e.grid     = "DN17";
    e.rst_sent = format_snr(-12);
    e.rst_rcvd = format_snr(5);
    e.on_ms    = 1790000000000LL;          // 2026-09-21 14:13:20 UTC
    e.off_ms   = 1790000000000LL + 125000; // 14:15:25
    e.freq_hz  = 7078000 + 1500;
    e.my_call  = e.op_call = "VE7NHW";
    e.my_grid  = "CN89KG";
    e.comment  = "FIRST X6100 JS8";
    e.tx_pwr_w = 5;
    e.pota_ref = "CA-1234";
    CHECK(adif_record(e) ==
          "<call:5>N7EAL <gridsquare:4>DN17 <mode:4>MFSK <submode:3>JS8 <rst_sent:3>-12 <rst_rcvd:3>+05 "
          "<qso_date:8>20260921 <time_on:6>141320 <qso_date_off:8>20260921 <time_off:6>141525 <band:3>40m "
          "<freq:8>7.079500 <station_callsign:6>VE7NHW <my_gridsquare:6>CN89KG <comment:15>FIRST X6100 JS8 "
          "<operator:6>VE7NHW <tx_pwr:1>5 <my_sig:4>POTA <my_sig_info:7>CA-1234 <eor>\n");

    LogEntry s = e;
    s.pota_ref.clear();
    s.sota_ref = "VE7/LM-001";
    s.grid.clear();
    s.comment  = "two\nlines";
    auto r     = adif_record(s);
    CHECK(r.find("<my_sota_ref:10>VE7/LM-001 <eor>") != std::string::npos);
    CHECK(r.find("my_sig") == std::string::npos);
    CHECK(r.find("gridsquare:") == r.find("my_gridsquare:") + 3); // only ours
    CHECK(r.find("<comment:9>two lines") != std::string::npos);
}

TEST_CASE("the ADIF file gets one header", "[js8][log]") {
    char path[] = "/tmp/js8_log_test_XXXXXX";
    int  fd     = mkstemp(path);
    REQUIRE(fd >= 0);
    close(fd);
    LogEntry e;
    e.call = "N7EAL";
    e.on_ms = e.off_ms = 1790000000000LL;
    e.freq_hz          = 14078000;
    std::string err;
    REQUIRE(adif_append(path, e, err));
    e.call = "KN6OEH";
    REQUIRE(adif_append(path, e, err));

    FILE *f = fopen(path, "r");
    REQUIRE(f);
    std::string all;
    char        buf[512];
    while (fgets(buf, sizeof(buf), f)) all += buf;
    fclose(f);
    unlink(path);
    CHECK(all.rfind("X6100 JS8 log", 0) == 0);
    CHECK(all.find("<eoh>") == all.rfind("<eoh>"));
    CHECK(all.find("<call:5>N7EAL") < all.find("<call:6>KN6OEH"));
    CHECK(all.find("<band:3>20m") != std::string::npos);

    CHECK_FALSE(adif_append("/nonexistent/dir/log.adi", e, err));
    CHECK(err.find("can't open") == 0);
}

TEST_CASE("ADIF bands", "[js8][log]") {
    CHECK(adif_band(1842000) == "160m");
    CHECK(adif_band(3578000) == "80m");
    CHECK(adif_band(5357000) == "60m");
    CHECK(adif_band(7078000) == "40m");
    CHECK(adif_band(10130000) == "30m");
    CHECK(adif_band(14078000) == "20m");
    CHECK(adif_band(18104000) == "17m");
    CHECK(adif_band(21078000) == "15m");
    CHECK(adif_band(24922000) == "12m");
    CHECK(adif_band(28078000) == "10m");
    CHECK(adif_band(50318000) == "6m");
    CHECK(adif_band(9000000).empty());
    CHECK(format_snr(0) == "+00");
    CHECK(format_snr(-3) == "-03");
}

TEST_CASE("the QSO log C API", "[js8][log]") {
    js8_qsos_t *q = js8_qsos_create();
    REQUIRE(q);
    char ended[JS8_RX_CALL_LEN] = "";
    CHECK_FALSE(js8_qsos_sent(q, "K2XYZ: N0XYZ HELLO", "K2XYZ", 0, ended, sizeof(ended)));
    js8_rx_msg_t m{};
    snprintf(m.from, sizeof(m.from), "N0XYZ");
    snprintf(m.text, sizeof(m.text), "N0XYZ: K2XYZ SNR +03 73");
    m.to_me = true;
    m.snr   = -7;
    CHECK(js8_qsos_received(q, &m, "K2XYZ", 15000, ended, sizeof(ended)));
    CHECK(std::string(ended) == "N0XYZ");
    js8_qso_t out;
    REQUIRE(js8_qsos_get(q, "N0XYZ", 15000, &out));
    CHECK(out.two_way);
    CHECK(out.has_rcvd_snr);
    CHECK(out.rcvd_snr == 3);
    CHECK_FALSE(out.has_sent_snr);
    CHECK(out.heard_snr == -7);
    js8_qsos_logged(q, "N0XYZ");
    CHECK_FALSE(js8_qsos_get(q, "N0XYZ", 15000, &out));
    CHECK(std::string(js8_log_band(7078000)) == "40m");
    js8_qsos_destroy(q);
}

TEST_CASE("grids: 4 to 10 characters read, 6 kept, not RR73, most precise kept", "[js8][log]") {
    CHECK(is_grid("DN17"));
    CHECK(is_grid("DN17AB"));
    CHECK(is_grid("CN89KG12"));
    CHECK(is_grid("CN89KG12AB"));
    CHECK_FALSE(is_grid("RR73"));
    CHECK_FALSE(is_grid("DN1"));
    CHECK_FALSE(is_grid("DN17A"));
    CHECK_FALSE(is_grid("SN17")); // fields stop at R
    CHECK_FALSE(is_grid("DN17AB1"));

    CHECK(find_grid("VE7NHW GRID DN17 TU 73") == "DN17");
    CHECK(find_grid("VE7NHW MY QTH DN17AB NAME BOB") == "DN17AB");
    CHECK(find_grid("VE7NHW RR73") == "");
    CHECK(find_grid("@HB HEARTBEAT CN89") == "CN89");
    CHECK(find_grid("VE7NHW GRID CN89KG12AB") == "CN89KG"); // logged as 6
    CHECK(find_grid("VE7NHW QTH CN89KG12") == "CN89KG");

    CHECK(better_grid("", "DN17") == "DN17");
    CHECK(better_grid("DN17AB", "DN17") == "DN17AB");  // less precise: keep
    CHECK(better_grid("DN17", "DN17AB") == "DN17AB");
    CHECK(better_grid("DN17AB", "CN89") == "CN89");    // they moved
    CHECK(better_grid("DN17AB", "") == "DN17AB");
}

TEST_CASE("the QSO keeps the grid they sent anywhere in a message", "[js8][log]") {
    QsoTracker        t;
    const std::string me = "VE7NHW";
    t.sent("VE7NHW: N7EAL GRID?", me, 0);
    t.received("N7EAL", "N7EAL: VE7NHW GRID DN17AB TNX", true, -9, me, 15000);
    t.received("N7EAL", "N7EAL: VE7NHW RR73", true, -9, me, 30000);
    t.received("N7EAL", "N7EAL: VE7NHW HB DN17", true, -9, me, 45000);
    auto q = t.get("N7EAL", 45000);
    REQUIRE(q);
    CHECK(q->grid == "DN17AB");

    StationList st;
    StationEvent ev{"N7EAL", "VE7NHW", "N7EAL: VE7NHW GRID DN17AB", true, -9, 1500, 1000};
    st.add(ev, me);
    ev.text = "N7EAL: VE7NHW RR73";
    st.add(ev, me);
    ev.text = "N7EAL: @HB HEARTBEAT DN17";
    ev.to_me = false;
    st.add(ev, me);
    auto list = st.sorted(1000);
    REQUIRE(list.size() == 1);
    CHECK(list[0].grid == "DN17AB");
}
