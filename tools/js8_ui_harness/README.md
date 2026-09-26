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
