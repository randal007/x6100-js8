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
#include "render.hpp"
#include "resampler.hpp"

#include "js8core/decoder.hpp"
#include "js8core/protocol/costas.hpp"
#include "js8core/protocol/varicode.hpp"

#include <chrono>
#include <cmath>
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
