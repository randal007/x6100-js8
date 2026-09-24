# Research notes

These notes record what was checked before any JS8 code went into the
firmware. Numbers come from runs on an x86 workstation unless noted.

## 1. The firmware

- The 1KO125 repository contains only a README. Its source ships inside
  each release's `x6100_gui-*.tar.gz`. That source is upstream `v0.34.2`
  plus WeFax, NavTex, a channel list and a broadcast database. The CI
  workflow is unchanged from upstream.
- Each app is a `dialog_t` with construct, destruct, audio, rotary and key
  callbacks. Apps are launched from the APP button pages in
  `src/buttons.cpp` and dispatched through `ACTION_APP_*` in
  `src/main_screen.c`.
- Dialog audio arrives on the PulseAudio thread as float ±1.0 at
  **11025 Hz** (44.1 kHz decimated ×4 in `dsp_put_audio_samples`).
- FT8 is the closest model. `dialog_ft8.c` has the UI, and `src/ft8/` has
  the audio worker thread, decoder wrapper, TX player (ALC-corrected, 5 W
  cap) and GFSK synthesis. The FT8 DSP comes from `ft8_lib`, built as a
  buildroot package.
- Band presets live in the SQLite `digital_modes` table (type 0 = FT8,
  1 = FT4). Existing SD cards are upgraded through the ordered list in
  `src/params/migrations.c`.
- Hardware: Allwinner R16 (quad Cortex-A7), 512 MB RAM. The image is
  buildroot 2022.11 with GCC 12.3, FFTW 3.3.10 (single precision) and
  liquid-dsp 1.4.0. Boost 1.80 is available as a package.

## 2. JS8 vs FT8

| | FT8 | JS8 |
|---|---|---|
| Symbols / spacing / slot (Normal) | 79 / 6.25 Hz / 15 s | same |
| Costas sync (Normal) | 4,2,5,6,1,3,0 ×3 | same ("original"). Fast, Turbo and Slow use three different "modified" arrays |
| FEC | LDPC (174,91) | LDPC **(174,87)** |
| CRC | 14-bit | **12-bit**, poly 0xC06, final xor 42 |
| Tone mapping | Gray-coded | direct |
| Block order | data, parity | **parity, data** |
| Payload | 77-bit structured | 3-bit frame type + 72 bits (12 six-bit characters) |
| Submodes | FT4 | Fast 10 s, Turbo 6 s, Slow 30 s (Ultra is defined but disabled) |

JS8Call works at 12 kHz. Normal-mode symbols come to a whole number of
samples at 11025 Hz, but Fast and Turbo don't (1102.5 and 551.25), so the
radio's audio is resampled to 12 kHz.

## 3. Options considered

1. **Port desktop JS8Call.** The decoder is clean C++ (`JS8.cpp`), but the
   message layer is Qt-based (`varicode.cpp`) and application behaviour is
   spread across an 11k-line `mainwindow.cpp`.
2. **Extend ft8_lib.** Its decoder is hard-wired to the FT8/FT4 constants,
   and none of the JS8 message layer exists.
3. **JS8Call-improved/Android-port `core/`** (chosen). The Android port
   separated a Qt-free core: modem, decoder, UTC-aligned scheduler,
   varicode, JSC compression. It needs only C++20, FFTW3f, header-only
   Boost and vendored Eigen/CRCpp, all of which the X6100 image can
   provide.

No existing native JS8 implementation for the X6100 was found (checked
2026-09).

## 4. Verification of the chosen core

- **Builds with GCC and libstdc++** once two missing standard includes are
  added. Its varicode round-trip test passes: heartbeat, compound,
  directed, fast data, and portable and compound callsigns.
- **Decoder fidelity:** `legacy_decoder.cpp` was compared against the
  desktop `JS8.cpp` in the same repository. The only differences are
  logging, NaN/Inf guards, removal of Qt plumbing, and belief-propagation
  iterations raised from 30 to 60.
- **End-to-end:** text → `build_message_frames` → tones → 12 kHz 8-FSK with
  AWGN → `legacy_decode` → unpack. SNR is referenced to 2500 Hz. Three
  frames per level:

  | SNR | decoded |
  |---|---|
  | 0, −10, −16 dB | 3/3 |
  | −20 dB | 1/3 |
  | −22, −24 dB | 0/3 |

  This is plain FSK in pure AWGN with one decode window, and the knee near
  −20 dB matches FT8-class codes. JS8Call's own "−24 dB" figure is a
  reported SNR estimate, not an AWGN threshold. On-air comparison against
  desktop JS8Call is still to do.
- **Cost:** ~25 ms per single-signal Normal decode on x86. First-call setup
  (FFT plans, buffers) was ~1.4 s before patch 4 and ~0.36 s after. Peak
  RSS in the test harness was ~73 MB before and ~44 MB after.

## 5. What still has to be written for the radio

The Android app keeps multi-frame assembly, command parsing, auto-reply,
relay and heartbeat logic in Kotlin (`DecodeViewModel.kt`,
`JS8EngineService.kt`, `Js8Commands.kt`). Frame-to-text rendering is C++ in
its JNI layer. Both are ported to `src/js8/` in C++, with the Kotlin unit
tests carried over.

## 6. Timing

The engine aligns its 60 s ring to the UTC minute once and then counts
samples. On the radio, audio gaps (for example during TX or dialog
restarts) and the difference between the codec clock and system time
would drift the decode windows. The RX wrapper compares its sample count
with the wall clock and calls `request_realign()` (local patch 3) when
they differ by more than 100 ms. JS8, like FT8, also needs the system clock
within about ±1 s of UTC. The FT8 app's "Time Sync" approach and the
engine's per-decode `drift_ms` estimate are both available for this.
