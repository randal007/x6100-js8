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
- `ONLY_RETUNE=1`: AUTO on and the keyboard open (Send...), a station
  asks us `SNR?` (the answer waits behind the keyboard); then a band key:
  the waiting answer must not go out on the new band.

## Automatic replies

- `ONLY_REPLYQ=1`: AUTO off, two stations ask: Reply offers each one its
  own answer. AUTO on, two questions in one slot: both answered in turn.
  A `SNR?` in the same slot as the first frame of a `MSG` to us: not
  answered (as desktop), nothing keys over the message, which gets its
  ACK. A `SNR?` with the Inbox open: answered (lists don't hold replies).
  HB and HB ACK on: a heartbeat while a `MSG` to someone else is arriving
  gets no HB ACK; one on a quiet band does.

## Data files

- `ONLY_BADFILES=1`: the Inbox and `js8_texts.txt` exist but can't be read
  (chmod 000) when JS8 opens: each gets a notice row, is kept aside as
  `*.unreadable-<date>` (checked, then removed), and JS8 starts afresh
  instead of later saving over them.

## Decode marks

- `ONLY_MARKS=1`: none while off (the default); Settings > *Decode marks*
  on; one slot with a station that decodes (1000 Hz), a weak one that
  still decodes (1800 Hz) and a weaker one the decoder finds but can't
  decode (2300 Hz): yellow brackets at the first two, a not-yellow one at
  the third (`dialog_js8_marks()` lists the brackets drawn);
  `81_marks.ppm` as the decodes land, `82_marks_later.ppm` after they
  scroll under the list; off again, none. `MARKS_WEAK=` sets the weak
  station's amplitude (0.0022; the other is 0.8 of it). `feed_band()` takes
  the trailing silence as its last argument (3 s by default).

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
  `WFTIME_GAPS=1` also lists every gap over 200 ms. A ~1 s gap is the
  alert beep's waterfall pause (by design): it happens when `js8_texts.txt`
  in the build directory has `ALERTS=... @POTA` left by other scenarios.
- `ONLY_WFRING=1`: the waterfall widget's ring buffer against a plain model,
  pixel by pixel as drawn (run it in the ASan build).
