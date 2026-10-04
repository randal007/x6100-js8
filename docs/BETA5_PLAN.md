# Beta 5: plan

VE7NHW's list for beta 5 (2026-10-02), with notes on where each item
touches the code. **Highest priorities:** time sync, high-SWR protection,
the two upstream merges, and station history. Nothing here is started
yet; items get ticked here and moved into the README's "New in beta 5"
as they land.

## Core priorities

- [ ] **Time sync overhaul + automatic time sync**, behaving like desktop
  JS8Call's auto sync. Today Time Sync is a button (Settings) that sets a
  JS8-only drift from the stations heard (README "Time Sync").
- [x] **High-SWR auto-shutdown modes**, for unattended stations
  (automatic replies, heartbeats, beacons). The radio already reports SWR
  during TX (`vswr` in `radio.c`).
- [x] **High-SWR beep.**
  **Both done in 2e152e6:** over 3:1 for half a second while keyed turns
  AUTO, heartbeats, HB ACK and auto CQ off (the message on the air carries
  on), three beeps once TX is done. Harness ONLY_SWR. On the user's card
  since 2026-10-04 (image CI 37183047679 = fe42a8d: waterfall four levels +
  SWR guard + APRS Echo test / More services); to be tried on the air.
- [ ] **Update to the latest gdyuldin release**
  ([x6100_gui releases](https://github.com/gdyuldin/x6100_gui/releases)):
  the list said v1.0.1; **v1.0.2 came out 2026-10-02**. Cleaner UI with
  more room for the map. **First, and without the Murus fork** (user,
  2026-10-02: build on 1.0.2, drop 1KO125's WeFax/NavTex/channel list for
  now). **Studied:** [upgrade-1.0.2/](upgrade-1.0.2/README.md) (trial
  merge, every upstream commit, the power trap in the settings
  migrations, the order of work, decisions for the user).
- [ ] **Merge the Murus team's SSTV release** (custom-r1cbu
  [releases](https://github.com/TheMurusTeam/custom-r1cbu/releases)):
  the list said beta 7; **beta 8 came out 2026-10-02** (SSTV transmit
  added). **After the 1.0.2 move:** the Murus team is bringing their fork
  to 1.0.2 themselves, so it's merged once, from that (WeFax, NavTex, the
  channel list and SSTV together). See "Merge notes" below.

## Heartbeat and CQ

- [x] The heartbeat timer no longer pauses during CQ.
- [x] Heartbeats pause only when someone actually answers the CQ.
- [x] The heartbeat timer works like CQ's (hold = Auto).
  **Done (port-1.0.2), the user's choices:** no CQ pauses heartbeats
  (single or auto); any call to you that starts a QSO pauses them (not a
  heartbeat ACK, not a low-confidence decode), as does anything else you
  send by hand; page 1's Heartbeat: press = one now, hold = auto (one now,
  the knob sets 5-30 min), press while auto = off, hold while paused =
  carry on; page 4's "HB: N min" button is gone, its slot left empty.
  Harness ONLY_HBPAUSE rewritten for these rules.
- [x] The TX bar (red rectangle) moves to the heartbeat's frequency when a
  heartbeat goes out. **Done (port-1.0.2):** from queued to finished, HB
  ACKs too (user's choices), finder_sync() in src/dialog_js8.c.

These change decision D1 from the code review (heartbeats paused for
10 minutes after anything you sent by hand, CQ included, and nothing
heard paused them; [docs/review](review/)).

## Stations and history

- [ ] Full history for every station: QSOs, chat, INFO and STATUS.
- [ ] Pressing a station (list or map) opens a menu with its past QSOs
  and history.
- [x] A Sort button on the Stations list (SNR, last heard ...). **Done
  (user's choices):** page 3's second button in the Stations view (blank
  over the messages, the map's view button on the map): *Sort: Heard you*
  (as before: who heard you first, then the newest), *SNR* (strongest
  first), *Time* (newest first), *Distance* (farthest first, no grid
  last); remembered (`js8_st_sort`); the selected station stays selected.
  `js8_stations_sort()` (`src/js8/js8_ops.cpp`, unit test), harness
  ONLY_STSORT. The distance column is wider so 5-digit km fit.
- [ ] QRZ light on the Stations list and the map, to see at once who is
  calling you. The map already has a QRZ line (calls that sent you a
  message while the map or Stations view was open).

## APRS and messaging

- [x] An APRS commands submenu: weather and other simple services
  (research in [feature-ideas.md](feature-ideas.md): MPAD is worldwide,
  WXBOT US only; answers come back through every relay station that
  heard you). **Done (user's layout):** Echo test first in APRS >, and
  More services > just before Close opens a list of MPAD (incl. Email my
  position, `posmsg`), WXBOT, WXNOW, WHO-IS and JOKE messages, each filled in (grid added where the service
  takes one), with Back and Close. Harness ONLY_APRSMORE.
- [ ] APRS badge on the Stations list: `@` in the star's place for a
  station seen relaying APRS messages back over JS8 (two-way gateway).
- [x] **Winlink: no ACK request.** **Done after beta 4.1** (WLNK-1 dropped from want_id; check on the air that WLNK-1 still answers). Winlink works on the air, but the
  WLNK-1 gateway answers with its own message anyway, so the `{nn}` we
  add to ask for a receipt brings a second ACK. Stop adding it for WLNK-1
  (`want_id` in `aprs_prepare()`, `src/dialog_js8.c`); SMS and email keep
  theirs.
- ~~Contact book for SMS~~: dropped (user, 2026-10-04: not worth the complexity).
- [x] Saved messages / macros, as desktop JS8Call has. **Done (user's
  choices):** Query > *Saved messages >* just before Close: ten messages
  (desktop's `TNX 73 GL` to start, as its Settings > Saved Messages);
  press = send at once (desktop's default, *Immediately transmit ...
  Saved ... messages*), hold the MFK = edit (Enter saves, empty clears,
  ESC leaves it), an empty one opens the keyboard; `js8_saved.txt` on
  DATA. Desktop's macros (JS8Call-improved 44fa092 `buildMacroValues()`
  / `replaceMacros()`): `<MYCALL> <MYGRID4> <MYGRID12> <MYINFO>
  <MYSTATUS> <MYCQ> <MYHB> <MYREPLY> <MYVERSION> <MYIDLE>`, and for the
  selected station `<CALL> <SNR> <TDELTA>`; filled in in saved messages
  (unknown ones dropped), typed messages (left as typed) and the INFO and
  STATUS answers, as desktop. One difference: a saved message with
  `<CALL>` and nothing selected is refused (desktop would send it with
  the call left out). `src/js8/macros.{hpp,cpp}` (unit tests), harness
  ONLY_SAVED. Not done: desktop's *Save Current Message* (keyboard ->
  saved); the user's list is edited in place instead.

## Screen and radio

- [x] The green receive bar is as wide as the speed the station was last
  heard at (today it is always Normal's width). **Done (port-1.0.2):** the
  Stations entry's speed, else the row's (selected_speed()); JS8 draws it
  itself (cursor_box), lv_finder (shared with FT8) untouched; hidden only
  where the red band covers it exactly. Harness ONLY_FINDER.
- [x] **The waterfall's CPU use** (user, 2026-10-03: it lags now and then;
  could it cost decodes?). **Measured on the radio** (`x6100-cpulog`, a
  read-only per-thread CPU logger over the USB console; 21 min in the
  Messages view on beta 4.1 + MFK fix): the four cores averaged 43 % and
  were never all busy at once, so the decoder (bursts of ~4 s per
  Normal slot) always had a core. The GUI thread was the bottleneck: 79 %
  of its core on average, over 90 % for 112 s in 47 short stretches (the
  "random" lags); during each heartbeat (no rows drawn) it fell from ~80 to
  29 %, so the waterfall cost about half a core. The PC harness showed
  why: each row redrew the whole see-through list over it, two thirds of a
  row's cost (a cached list image didn't help: LVGL pastes see-through
  images about as slowly). **Done (after 4.1):** `src/js8_wf.c` draws
  the waterfall on the display's lower plane, as R1CBU 1.0's main-screen
  waterfall is (`drm_primary_begin_direct()`), through a hole in the app's
  plane; the display hardware puts the list on top. A row is now 788
  straight copies (a column-major ring), and LVGL redraws nothing: harness
  ONLY_LOAD rows with a full list 19.6 → 3.8 ms/s, ONLY_WFPERF 1.08 →
  0.05 ms a row. ONLY_WFRING checks the ring pixel by pixel and the plane
  mapping against LVGL's own 90° rotation. **On the radio** (CI
  37158604891, flashed 2026-10-03; a short 2.3 min log on a quiet band,
  `research/cpulog/2026-10-03-2343Z`): the GUI thread averaged 29 % (was
  79 %), peak 61 %, never 70 % or more; all cores 107 % of 400 (was 171).
  29 % is what it used before while no rows were drawn, so the
  waterfall's drawing cost is all but gone. To do: a 20 min log on a busy
  band. **Health lines** in the app log (stderr, UTC-stamped): `decode:`
  once a minute (passes, busy time, longest pass, decodes, windows that
  waited behind a running pass, js8core patch 13), `dropped ... of audio`,
  `gap`, `realign`, and `GUI: stalls over 200 ms / waterfall rows
  dropped` when they happen; JS8's threads are named (`js8-rx`,
  `js8-decode`, `js8-tx`, `js8-beep`). The first run raised three
  questions for the next session: (1) `gap: 12-13 s of audio missing`
  three times, seemingly at frequency changes (opening JS8 and two band
  changes) or a transmission: `js8_rx_clear()` only clears messages, so
  either the audio really paused (would the waterfall freeze for 12 s
  after a band change?) or we were keyed; if it's our own TX, say so in
  the line instead of "missing". (2) The first decode pass after opening
  JS8 took 12.9 s (23 windows waited that minute): decoders built on
  first use? (3) No decodes in 5 minutes (a quiet band, the user says;
  confirm on a busy one). LVGL's own `[User]` lines go to stdout, which is
  block-buffered, so they land late and out of order; the JS8 lines on
  stderr are in real time.
- [x] **Smooth waterfall scrolling (flicker).** The user sees a slight
  flicker in JS8's waterfall, there since the early betas (not from the
  lower-plane change), and only while it scrolls: during a CQ, when it
  stops, the screen is steady. A 240 fps slow-motion video (Samsung S24
  Ultra, 2026-10-03, phone `DCIM/Camera/20261003_164702.mp4`; analysis in
  `research/flicker-video/`) shows why: each new row (15 a second) makes
  the waterfall area about 10 % darker for a moment. The dip sweeps right
  to left in about one refresh (the panel is portrait and scans across
  the landscape screen) and fades over 20-25 ms; 1.0's own S-meter,
  frequency and buttons don't dip. Each row shifts the whole speckled
  waterfall by a pixel, so every pixel changes at once, and the LCD's
  pixels darken faster than they brighten: the average dips on every
  step, and at 15 Hz the eye sees it. **Fix (user's choice): smooth
  scrolling**, the waterfall moving a fraction of a pixel on every screen
  refresh (sub-pixel blend of the two nearest row positions at the
  display's ~60 Hz) instead of a whole pixel 15 times a second, so the
  panel makes 60 small changes a second that the eye can't follow. With
  the waterfall on the lower plane, that's a blend of 788 x 337 pixels
  per refresh written with `drm_primary_begin_direct()`: do it with NEON
  (as drm.c's `neon_blend_argb8888`), or on a worker thread on an idle
  core, and measure the CPU with `x6100-cpulog`. Other options looked at:
  less speckle (smoother rows), or 30 rows a second (rejected: a 12.6 s
  JS8 frame would no longer fit on the waterfall). **Tried and dropped
  (dbc3a69, reverted):** `js8_wf.c` kept RING_EXTRA = 2 rows above
  the screen; each new row raises the picture a row (`off` += 1) and it
  glides down at a row per row period (1.5x while more than a row is
  waiting), so screen row y is ring row y + off: a blend of the two
  nearest rows (`blend_col`, NEON on the radio, x/255 rounded), put on the
  plane at most once a main-loop pass (MIN_PUT_US 8 ms) and only when the
  offset (1/256 row) changed: nothing while transmitting. Settings line
  *Waterfall: Smooth / Steps* (`js8_wf_smooth`, default on) for A/B.
  Harness: ONLY_WFRING checks the picture held 0.5 and 1.0 row up pixel
  by pixel; ONLY_WFPERF times both (PC: 0.08 ms a row in steps, 1.17 ms
  in smooth, 4 puts of 0.29 ms; the PC has no NEON path). Cost on the
  radio still to measure: `research/wf-smooth-bench/bench_arm` (static,
  copy over the console and run: steps copy, smooth blend and drm.c's two
  copies per update), then `x6100-cpulog` with Smooth vs Steps. **On the
  radio it was far too costly:** the GUI thread averaged 83 % (peak 114 %)
  in Smooth, against 29 % in Steps (`research/cpulog/2026-10-04-0405Z`),
  and the user saw a new irregular flicker a couple of times a second (the
  thread overloaded, the glide stalling). `bench_arm` on the radio, per
  update of 788 x 337 px: the whole-row copy 5.9 ms, the blend 8.0 ms,
  drm.c's two copies into the frame buffers 9.9 ms, so 60 updates a
  second need ~1070 ms a second: more than a core, and the memory, not the
  CPU, is the limit (so another core wouldn't help). Even Steps costs
  ~16 ms a row (~24 % of a core) for the same reason. Real smooth
  scrolling would need the display hardware to scroll (a plane of its own
  and a source offset per refresh: zero copies), which means changing
  1.0's shared display driver: an idea for upstream, not for us.
  **Instead (user's choice): Waterfall: Calm** (`js8_wf_calm`, default
  on; Settings *Waterfall: Calm / Sharp*): `wf_emit_row()` averages each
  row with the ones before (exponentially, WF_CALM_A 0.35 of the new row;
  restarted on retune / clear), so every step changes the picture less.
  Harness ONLY_WFCALM with live noise: 3.9x less change per step (mean
  |luma difference| between rows 27.2 -> 6.9), same CPU; the noise
  background is darker (mean luma 26 -> 15.5), signals stand out more.
  On the radio (74727e6) the user asked for something in between, so the
  setting became four levels (`js8_wf_avg` 0-3, default 3; Settings
  cycles *Sharp > Light > Medium > Calm*; new-row share 1.0 / 0.65 / 0.5 /
  0.35): ONLY_WFCALM change per step 27.2 / 14.2 / 10.5 / 7.1 (1.9x,
  2.6x, 3.8x less than Sharp). CPU on the radio with Calm (74727e6,
  `research/cpulog/2026-10-04-0531Z`, 5.3 min): GUI thread 26 % average,
  peak 56 %, never 70 % or more (this morning 79 %; Smooth 83 %); all
  cores 103 % of 400; decoder 24-26 % a minute, passes under 2.5 s. To
  do: the user's pick by eye.
- [ ] A small ALC rework for low power (under 1 W) into an amplifier.
  The TX audio path (`tx_player.c`) is shared with the FT8 app.
- [ ] GPS time and location (USB GPS dongle ordered; testing when it
  arrives). The firmware already reads gpsd for the APRS beacon.

## Bugs

- [x] **Can they reach...? (QUERY CALL)** only works when you type the `?`
  yourself after the call (the one thing that failed in on-air testing). Desktop sends `CALL QUERY CALL W1ABC?`: add
  the `?` for you. **Done:** `query_call_question()` (`src/js8/directed.cpp`),
  called by `plan_message()`, so the preview, the frames and our own row all
  have it; the Query list's hint no longer asks for it. Why desktop didn't
  answer without it isn't clear from its code (JS8Call-improved 1c6e27a's
  `parseQueryCallArg` drops a trailing `?` and `parseCallsigns` finds the call
  either way; the checksum isn't the cause either): desktop's menu always
  sends the `?`, so now we do too.
- [x] **An @ALLCALL QUERY CALL** ("can anyone reach W1ABC?") to send to
  everyone, as well as to one station. **Done:** *Can anyone reach...?* in
  the Query list, which now also opens with no station selected (that item
  only). Harness `ONLY_QUERYCALL`, unit test "a QUERY CALL gets desktop's '?'".

- [x] **The waterfall stutters while the MFK steps through the list**
  (VE7NHW, after beta 4.1: only while scrolling stations, the green bar
  following). lv_table redrew the whole list on every step and scrolled
  with an animation (a full redraw per frame), and a new selection redrew
  the whole list again for the green marks; on R1CBU 1.0 every redrawn
  pixel is also rotated in software. **Done:** `table_key_pre_cb()` in
  `src/dialog_js8.c` takes the list's arrow keys before lv_table and
  redraws only the two rows, scrolling at once; the marks redraw only the
  rows whose mark changes (`marks_invalidate()`). Harness ONLY_KNOB: 8 MFK
  steps 944 → 293 kpx; partial and full redraws match. Display only, the
  decoder is untouched.

## Carried over from beta 4

On the air with beta 4 (VE7NHW, up to 2026-10-02): relays, messaging,
store and forward and Winlink all work; only QUERY CALL failed (above).

- [ ] On-air tests still to do: POTA and SOTA spots through the gateways.
- [ ] Show Map: a Setting for your own square's colour (orange for now),
  polish from use on the air ([MAP_PLAN.md](MAP_PLAN.md)).

## Low priority (after the features)

- [ ] Performance: each part's CPU use on the radio, spread over its four
  cores. First measurement and the waterfall fix done (above, "Screen and
  radio"); the GUI thread's remaining ~29 % with no rows is still to look at.
- [ ] Leftovers in the [fix plan](review/fix-plan.md) ("Not now": I-02,
  I-04, I-10 with B-11).

## Merge notes (checked 2026-10-02)

**Murus team beta 8** (still based on gdyuldin 0.34.2, like our beta 6
import, branch `1ko125-beta6`). Against beta 6 it adds `dialog_sstv.c`,
`src/sstv/` (decoder, encoder), `sstv_resources.c`, picture and text
templates in `rootfs/usr/share/x6100/sstv/`, and changes `buttons.cpp`,
`main.c`, `main_screen.c`, `params/params.h`, the WeFax decoder and
`CMakeLists.txt`.

- `ACTION_APP_SSTV` is appended after `ACTION_APP_NAVTEX`, the same place
  as our `ACTION_APP_JS8`, so both have the same value. These numbers are
  stored in each card's settings: JS8 keeps its number (the rule in
  [UPSTREAM_README.md](UPSTREAM_README.md), "Merging upstream") and SSTV
  goes after it.
- The release notes describe a built-in web server for uploading pictures
  from a phone. It isn't in the GUI source tarball: find out where it
  lives (their SD image or Buildroot side) before promising it.

**gdyuldin v1.0.x** (168 commits after the upstream snapshot we had).
Larger: a new UI and themes (Simple, Black, Flat), DRM rendering with two
planes, the audio path moved from 44.1 kHz to 48 kHz, a reworked CW
decoder, wfview and Bluetooth RFCOMM. Parts that meet JS8's code:

- themes: the map's frame is fitted to each theme's dialog border;
- rendering: the waterfall and map drawing fixes (exact invalidation,
  opaque boxes) were measured on 0.34.2's drawing path;
- audio: JS8's receive/transmit audio and the alert beep;
- the stored-number rule again (migrations 4–5, `MODE_JS8 = 8`,
  `ACTION_APP_JS8`).

(Superseded 2026-10-02: the Murus team is moving to 1.0.2 themselves; we
wait for theirs.) The Murus team hasn't moved to 1.0 yet, so merging both means bringing
their SSTV app onto 1.0 ourselves.
