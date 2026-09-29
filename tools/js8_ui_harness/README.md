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

## Scenarios

With no switch the full run above. `ONLY_<NAME>=1` runs one scenario
instead (each prints `[tag] ... (want ...)` lines to compare):

| Switch | What it checks |
|---|---|
| `ALERTS` | the Alerts list, beeps and purple alert-word rows |
| `APRS` | APRS list: grid and GPS beacons, position + message, POTA/SOTA spot form, SMS, email, Winlink |
| `AUTOCQ` | hold CQ: auto CQ interval from the end of each CQ, stops when answered; a manual heartbeat restarts the HB timer |
| `BADFILES` | unreadable Inbox / settings files kept aside, not written over (see below) |
| `BANDS` | each band keeps its own Stations list |
| `COMPOSE` | typing a reply while their long message is still arriving |
| `DRIFT` | Time Sync as a drift: JS8's timing moves, the clock doesn't; refused while sending; hold resets |
| `FREQ` | page 6 Freq: JS8Call's presets, GhostNet's, a custom frequency |
| `FREQMARK` | rows without a callsign marked by frequency |
| `GEN=query\|aprs\|texts\|alerts` | GEN / APP closing the app with that list open |
| `HBPAUSE` | heartbeat pause (see below) |
| `HELD` | messages held for others (store and forward), fetched as desktop does |
| `KEYS` | the VOL knob in every popup, the keyboard with no callsign, every sendable character, too long |
| `INBOX` | a message for us saved and ACKed, the Inbox view (don't run while building: real-time feeding starves) |
| `LOAD` | the GUI thread's work per second, idle and with waterfall rows (see below) |
| `LOCK` | hold MFK to lock a station; turning only scrolls |
| `LOG` | a QSO to 73, the Log popup, ADIF |
| `LOGPEND` | ESC on the log prompt, then Log QSO takes the selected station; a typed grid checked |
| `LOOKS` | coloured commands, end marks, relay "via" stations, bearing, km/miles, Settings lines |
| `MARKS` | decode marks (see below) |
| `MAP` | Show Map: Messages → Stations → Map, stations by grid and by prefix, paths, the MFK selection in red, Show *Heard me*, the view button (Auto / Close-in / World), a DX station switching Auto to the world with its 8 s pop-up, Time Sync first in Settings, back to Messages (`90_map.ppm` ... `96_back_to_messages.ppm`); the TX outline and red paths (`97`-`99`); from a test QSO log it writes (`js8_qso_log.db` in the build dir): NEW GRID / NEW DXCC pop-ups and white outlines, a weak CQ caller's small square and CQ tag (its Stations row green, `89_stations_cq.ppm`), the stats line, and the squares faded half an hour on (`9a_map_faded.ppm`); other stations' QSOs and a relay as grey lines, the last-two-messages strip (`9b_map_talk.ppm`), the dots (yours moving toward the station you send to, incoming ones counted), Follow on JA1ABC by holding Map: (`9c_map_follow.ppm`) |
| `MODE` | opened in USB with a custom CB frequency saved: always USB-D |
| `NEWSTN` | "New station" alerts once per band per power-on, not again an hour later |
| `OPERATOR` | Settings operator call, logged as OPERATOR |
| `PARTIAL` | a long message growing in place as it arrives |
| `QSOFREQ` | Directed view shows what's on the selected station's frequency |
| `RELAY` | relays passed on and received, ACK back along the path |
| `REPLYQ` | automatic replies queue (see below) |
| `RETUNE`, `TXSAFE` | transmitting safely (see below) |
| `ROWS` | info rows survive rebuilds, a cut-off message stops growing, the Inbox lists all 200 |
| `SMS` | a phone text via an APRS gateway; Reply by SMS fills in the number |
| `SPEED` | all four speeds decoded together; the Speed button |
| `STALL` | the GUI stuck for ~50 s while a band's messages arrive: none lost (see below) |
| `TEXTS` | Settings: INFO/STATUS keyboard gets the focus |
| `TXBAR` | frame progress in the TX bar during a long message |
| `URGENT` | beta 2's urgent fixes (docs/BETA2_URGENT_PLAN.md); its four "[page]" wants predate the page 1/2 swap |
| `WFPERF`, `WFRING`, `WFTIME` | waterfall measurements (see below) |

Scenarios that feed two multi-frame stations must give them different
offsets, or they garble each other. The data files (`js8_texts.txt`,
`js8_inbox.txt` ...) live in the **build** directory
(`JS8_TEXTS_PATH` = `${CMAKE_BINARY_DIR}/...`), not the working one, and
scenarios leave settings there (alert words, operator call) that later runs
see.

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

- `ONLY_LOAD=1`: what the GUI thread does per second while JS8 sits
  there, as on the radio (LVGL's timers run, the TX bar's 250 ms update
  included, unlike `WFPERF`): idle and with live noise (waterfall rows),
  with an empty list, a full one and the Stations view. Prints ms of GUI
  work per second, pixels sent to the screen and flushes. `LOAD_S` sets
  the seconds per case (10). Package 4 measured idle 7.2 → 0.6 ms/s and
  1031 → 10 kpx/s (the TX bar restyled 4 times a second), rows with a full
  list 26.7 → 19 ms/s.
- `ONLY_STALL=1`: feeds six stations (~50 s) without running the GUI
  thread, as if it were stuck, then checks every message is in the list.
  Before package 4 the shared scheduler queue (64 items) overflowed 852
  times and all six were lost; JS8 now has its own queues.

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
