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
| `DRIFT` | Time (page 4, button 2): Auto sets the drift from one band 1.2 s late and leaves it there when the band is on time; our CQ starts on JS8's slot; Auto off ignores a later band; Settings' first line resets; a band 6 s late doesn't decode until the search (hold Time) finds it, then it does; holding again stops a search. Scenarios that move JS8's time on by minutes (NEWSTN, HBPAUSE, MAP) switch Auto off around it (`dialog_js8_time_auto`) |
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
| `SAVED` | Query > Saved messages > (just before Close): desktop's `TNX 73 GL` first, a press of the real MFK sends it at once, holding it edits one (the knob's release doesn't press Enter), Enter saves and returns to the list, ESC leaves it, empty clears; `<CALL>` refused with nothing selected, filled in with a station (`<SNR>`, `<MYGRID4>` too); kept in `js8_saved.txt` across a reopen; macros in a typed message and in the INFO answer (`s0_saved_list.ppm` ... `s2_saved_selected.ppm`) |
| `STQRZ` | the map's QRZ line over the Stations view too: a message to you (not a heartbeat SNR reply) adds the caller, newest first; hidden on the map (its own shows) and over the messages; the selected station drops off; back to the messages (Map > Messages, or Show) clears it (`q0_stations_qrz.ppm`) |
| `MAPSTACK` | the map's count tag: 12 stations, 4 spots shared (EN52 3 with a CQ tag too, DM43 2, CN89 2, two gridless 6-area calls 2), one white tag each on the bottom right; unchanged with a stacked station selected; none with *Heard me* (`w0_map_stack.ppm`, `w1_map_stack_selected.ppm`) |
| `ATGATE` | the Stations list's `@` badge: a station that passed an APRS message back over JS8 (`@APRSIS MSG TO:`, e.g. an Echo test's answer) shows `@` instead of `*`, even after calling you; heard-you `*`, others blank; page 3's second slot empty (`u0_aprs_gate.ppm`) |
| `STSORT` | the Stations view's Sort button (page 3, button 2): Heard you, SNR, Time, Distance from four stations, the selected one kept; blank over the messages, the map's button on the map (`t0_sort_heard.ppm` ... `t3_sort.ppm`) |
| `QUERYCALL` | Query list: *Can anyone reach...?* with no station selected, *Can they reach...?* with one; the `?` added when left off, never twice |
| `KNOB` | the main knob on the TX offset: clicks one at a time as R1CBU 1.0 sends them, a fast spin still speeds up (5/10 Hz per click), and only the finder's old and new band are redrawn (load per second; no band left behind after the knob or MFK selections); MFK steps through the list redraw only the rows they move between |
| `FINDER` | the green band as wide as each selected station's speed (Normal, Fast, Turbo, Slow), shown over the red band when wider; the red band on a heartbeat's free offset while it's queued and sent, back after |
| `REPLYQ` | automatic replies queue (see below) |
| `RETUNE`, `TXSAFE` | transmitting safely (see below) |
| `ROWS` | info rows survive rebuilds, a cut-off message stops growing, the Inbox lists all 200 |
| `SMS` | a phone text via an APRS gateway; Reply by SMS fills in the number; the gateway's receipt shown as delivered, not an Inbox message; 67 characters plus the `{nn}` |
| `SPEED` | all four speeds decoded together; the Speed button; Decode: All speeds / My speed in Settings |
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
- `ONLY_TXMARK=1`: your own rows' marks. A 3-frame message: ` ...` once
  the first frame keys, the end mark `♢` after the last; another stopped
  with ESC in its second frame: `(stopped 2/3)`, no end mark; a CQ with
  JS8 closed on it and reopened: `(stopped 1/1)`. Screenshots
  `c0_txmark_sent`, `c1_txmark_stopped`, `c2_txmark_reopened`. About 1.5
  minutes.
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

JS8's waterfall is drawn on the display's lower plane (`src/js8_wf.c`), as
R1CBU 1.0 draws its main-screen waterfall; LVGL draws the app on a
see-through plane over it. The harness does the same: its display is
see-through (`screen_transp`), `stubs.c` stands in for the lower plane
(`drm_primary_begin_direct()` / `drm_primary_end_direct()`, a portrait
480 x 800 buffer), and screenshots blend the two as the display hardware
does (`screen_px()` in `main.cpp`). `HARNESS_OPAQUE=1` makes the
harness's display opaque again, for timings comparable with older logs
(the waterfall then doesn't show): a see-through display costs LVGL a
little more, on the radio too.

Build without sanitizers for timings
(`cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DHARNESS_SANITIZE=OFF`).

- `ONLY_LOAD=1`: what the GUI thread does per second while JS8 sits
  there, as on the radio (LVGL's timers run, the TX bar's 250 ms update
  included, unlike `WFPERF`): idle and with live noise (waterfall rows),
  with an empty list, a full one and the Stations view. Prints ms of GUI
  work per second, pixels sent to the screen and flushes. `LOAD_S` sets
  the seconds per case (10). Package 4 measured idle 7.2 → 0.6 ms/s and
  1031 → 10 kpx/s (the TX bar restyled 4 times a second), rows with a full
  list 26.7 → 19 ms/s. The lower plane (2026-10-03): rows with a full list
  19.6 → 3.8 ms/s, 3998 → 17 kpx/s from LVGL (LVGL no longer redraws the
  see-through list for each row).
- `ONLY_STALL=1`: feeds six stations (~50 s) without running the GUI
  thread, as if it were stuck, then checks every message is in the list.
  Before package 4 the shared scheduler queue (64 items) overflowed 852
  times and all six were lost; JS8 now has its own queues.

- `ONLY_WFPERF=1`: fills the list, then adds waterfall rows as fast as they
  go through the app's path: `js8_wf_add_row()`, the put on the lower plane
  (`js8_wf_tick()`), then whatever LVGL redraws for it, flushed as the radio
  does (queue copy, 90° rotation, framebuffer copy). Prints the cost per
  row (add / plane put / LVGL render / flush, LVGL pixels): 0.05 ms and no
  LVGL pixels, where drawing the waterfall through LVGL took 1.08 ms, two
  thirds of it the list's text. `WFPERF_PROFILE=name` runs it for 4000
  rows, for a gprof build (`-pg`).
- `ONLY_WFTIME=1`: live audio; records when each new row reaches the screen
  (a put on the lower plane whose newest row changed) and prints the spread
  of the intervals (how even the scroll is).
  `WFTIME_GAPS=1` also lists every gap over 200 ms. A ~1 s gap is the
  alert beep's waterfall pause (by design): it happens when `js8_texts.txt`
  in the build directory has `ALERTS=... @POTA` left by other scenarios.
- `ONLY_WFCALM=1`: *Waterfall: Sharp / Light / Medium / Calm* with live
  noise: how much the picture changes at each step (mean |luma difference|
  between consecutive rows on the lower plane), the change that makes the
  LCD dim for a moment on every row: 1.9x, 2.6x and 3.8x less than Sharp.
- `ONLY_APRSMORE=1`: APRS > opens on Echo test (its message filled in
  and sent); More services > just before Close opens the services list,
  every item's message filled in (grid added where the service takes
  one); < Back returns to the APRS list, ESC closes it.
- `ONLY_SWR=1`: the high-SWR guard: over 3:1 for half a second while
  keyed turns AUTO, auto HB, HB ACK and auto CQ off (a 0.3 s spike and
  exactly 3:1 don't); the message on the air carries on; three beeps once
  TX is done; with nothing automatic on, high SWR changes nothing.
- `ONLY_WFRING=1`: the `lv_waterfall` widget's ring buffer (the FT8 app's)
  against a plain model, pixel by pixel as drawn; then JS8's own waterfall
  (`js8_wf.c`) the same way where the screen shows it through the hole:
  rows past the ring's height, a decode mark scrolling down, a clear, and
  the main screen redrawing the plane after a retune (`js8_wf_repaint_soon()`
  must put it back); and the plane mapping (screen x, y = plane y, 799 - x)
  against LVGL's own `LV_DISP_ROT_90` on a second display set up like the
  radio's (run it in the ASan build).
