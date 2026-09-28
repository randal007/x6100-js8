# JS8 UI harness

Runs the real JS8 app headless on a PC: `src/dialog_js8.c`, `src/dialog.c`,
the waterfall and finder widgets, `styles.c` with the real theme images, and
the `src/js8` decoder, all on stock LVGL 8.3.11. Only radio hardware and
main-screen plumbing are stubbed (`stubs.c`).

The scenario in `main.cpp` builds a busy band with JS8Call's own encoder:
six stations, interleaved multi-frame messages, heartbeats, a CQ, a message
to "me" (K2XYZ), an @ALLCALL message and a checksummed MSG. It feeds that
audio through the dialog's audio callback **in real time**, in 20 ms pieces
like PulseAudio, with the production clock guard on. It then presses the
Show button, changes band and closes with ESC, saving a screenshot
(`*.ppm`) at each step. A run takes about 90 s.

```sh
./fetch_deps.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
mkdir -p out && cd out && ../build/js8_ui_harness
for f in *.ppm; do magick "$f" "${f%.ppm}.png"; done
```

The exit status is non-zero if the dialog is still running after ESC. It is
built with ASan/UBSan by default (`-DHARNESS_SANITIZE=OFF` to disable); both
`Debug` and `RelWithDebInfo` builds decode all six messages.

Audio has to arrive in real time. The receiver compares its sample count
with the wall clock and re-snaps its decode windows when they disagree, as
it must on the radio, so feeding faster than real time breaks decoding by
design. (The library tests in `tests/test_js8.cpp` switch that guard off to
run faster.)

## Transmitting safely

- `ONLY_TXSAFE=1`: a CQ; while the stub radio is keyed, a band key (must be
  refused, dial unchanged), then `dialog_destruct()` as GEN, APP or another
  app does (the frame must be aborted, PTT off, no crash: it crashed in
  `js8_tx_stopping()` before the fix); reopened, a CQ goes out again.
- `ONLY_RETUNE=1`: AUTO on and the Freq list open, a station asks us
  `SNR?` (the answer waits behind the list); then a retune: the waiting
  answer must not go out on the new frequency.

## Heartbeat pause

- `ONLY_HBPAUSE=1`: AUTO, HB and HB ACK on; a message to us, an automatic
  SNR reply and a manual heartbeat must not pause heartbeats; `HW CPY?` by
  hand does (both buttons `paused`, switches still on, no HB ACK to a
  heartbeat heard meanwhile); 11 min later (JS8 drift moved forward) they
  resume by themselves; pressing HB while paused resumes at once.

## Waterfall measurements

Build without sanitizers for timings
(`cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DHARNESS_SANITIZE=OFF`).

- `ONLY_WFPERF=1`: fills the list, then adds waterfall rows as fast as they
  render, with a full-screen draw buffer and the radio's flush path
  (queue copy, 90° rotation, framebuffer copy), and prints the cost per row
  split into add / render / flush, with parts of the screen hidden to show
  their share. `WFPERF_PROFILE=full|bare` runs one case for 4000 rows, for
  a gprof build (`-pg`).
- `ONLY_WFTIME=1`: live audio; records when each new row reaches the screen
  and prints the spread of the intervals (how even the scroll is).
- `ONLY_WFRING=1`: the waterfall widget's ring buffer against a plain model,
  pixel by pixel as drawn (run it in the ASan build).
