# Beta 5: plan

> **Released as beta 4.5 on 2026-10-04** (user's call): tag `js8-beta4.5`,
> the tested image CI 37258042913 (19bbc77); notes in
> [releases/js8-beta4.5.md](releases/js8-beta4.5.md). **Next, the only items
> the user wants now:** GPS (USB dongle) and trying a Bluetooth keyboard.
> The other open items below (ALC under 1 W, the map's home colour, POTA /
> SOTA on the air, the Murus merge, performance) are parked as ideas.
> **2026-10-09:** VE7NHW's new list is the next section: APRS NACK, Echo
> test, @ALLCALL QUERY MSGS, the held-message pickup bug, Show's blank
> rows, a default STATUS, Ultra (done), plus GPS and the Bluetooth keyboard
> (still wanted for beta 5).

## VE7NHW's list (2026-10-09)

Notes only so far: nothing below is changed in the code yet.

- [ ] **Received APRS messages show a NACK command.** VE7NHW: every
  message coming back from APRS shows NACK, although nothing on the
  APRS side sends one. **Checked so far (2026-10-09):** `" NACK"` is
  directed command code 2 in js8core, old desktop JS8Call and current
  JS8Call-improved (7fd8ecd) alike, so it isn't a command-table mismatch.
  Nobody sends it on purpose: JS8Call-improved only lists it in
  Varicode.cpp and DirectedMessageParser.cpp, and our code only has it
  in the parse tables (`src/js8/directed.cpp`, `assembler.cpp`). The
  relay station's inbound APRS (AprsInboundRelay.cpp) sends
  `@APRSIS MSG to:<CALL> <text> DE <SENDER>` (its `to:` is lowercase now;
  it drops APRS `ack` messages and strips the `{id}`). **Next:** a
  screenshot of a row showing it (or the radio's `app_logs` on DATA),
  then find where the word comes from: the decoded frames, our parse
  (classify / `parse_directed`), or how the row is drawn (commands are
  coloured yellow). Side finding on findu.com: on 2026-10-06 00:06Z,
  something sent APRS **rejects** to SMS in VE7NHW's name (`rej40}`,
  `rej19004`, `rej19005`, each followed by SMS's "Invalid Command or
  Msg!"). Our app never sends `rej`, so it was likely a relay station
  answering for us. Worth a look while we're in there.
- [x] **Echo test: drop it, and find another way to earn the `@` badge.**
  **Done 2026-10-09:** removed from APRS > (user agreed); the `@` badge
  needs nothing new (any relayed APRS answer earns it). Harness ONLY_APRS /
  ONLY_APRSMORE updated.
  VE7NHW: Echo never comes back. **Checked on findu.com
  (msg.cgi?call=ECHO, 2026-10-09):** the ECHO service works and our
  command is right. It answered VE7NHW `ECHO:TEST{97` 6 s after the
  2026-10-05 06:23Z test (and again 2026-10-06 00:03Z), and it answers
  other people daily. So the answer reached APRS-IS and was lost on the
  way back over JS8, or reached the radio and wasn't shown. One
  suspect on our side: the relayed text would be
  `@APRSIS MSG TO:VE7NHW ECHO:TEST DE ECHO`, and the `ECHO:` looks like a
  `CALL:` start to a parser (worth a harness test before removing).
  MPAD, JOKE, EMAIL-2, WLNK-1 and SMS all answered VE7NHW on 2026-10-08/09.
  **User's plan:** remove Echo test from the APRS list. The `@` badge
  already comes from hearing any station send `@APRSIS MSG TO:...`
  (`Station::aprs_gate`, `src/js8/stations.cpp`), so it keeps working
  from SMS and other services' answers. Pick a new "test both
  directions" item if one is wanted (e.g. a short MPAD or JOKE request).
- [x] **"Anyone have messages for me?" (`@ALLCALL QUERY MSGS`) in the
  Query list, always** **Done 2026-10-09 (user: keep every station item, no
  unselect needed):** *Anyone have messages?* right below *Any messages?*,
  sent at once, also with no station selected (`allcall` flag on
  `query_msg_items`). Harness ONLY_QUERYCALL., with or without a station selected. VE7NHW:
  dropping a selection is hard, so in practice the @ALLCALL items are only
  seen right after the app opens. **Checked:** there's no
  `@ALLCALL QUERY MSGS` item at all today; with no selection the list has
  only *Can anyone reach...?* (`QUERY_ALLCALL_ITEM`, `query_open()` /
  `query_msg_cb()` in `src/dialog_js8.c`). A selection is only dropped by
  Clear or a band change (`clear_selection()`). Our side already answers
  it as desktop (YES MSG ID n to the asker, never NO to a group). Maybe
  also an easy way to unselect (to decide).
- [x] **Bug: a station retrieving a message we hold gets nothing.** **Solved
  2026-10-09 (radio's js8_history.db):** desktop sent `QUERY MSG [1]`,
  `QUERY MSG [ID1]` and `QUERY MSG [ID 1]`: its menu fills in
  `QUERY MSG [ID]` and the brackets were left in. Desktop itself takes only
  a bare number (`toInt()`), so it wouldn't answer them either. **Fix
  (user's choice): we take the id with or without the brackets**
  (`msg_id_arg()`, `src/js8/inbox.cpp`; `[3]`, `[ID3]`, `[ID 3]`, `ID 3`),
  unit tests with the on-air texts. The other way worked on the air:
  `QUERY MSG 3` from the X6100, desktop delivered. Earlier notes: When
  someone sends the command to pick up a message stored here, nothing
  happens and we never send it. **Where:** `QUERY MSG n` is handled in
  `src/js8/autoreply.cpp` (~line 220). It answers only from the held
  store (`MSG TO:` messages left here for someone), only for the
  addressee (or anyone, for a group), not to @ALLCALL, and a `Stored`
  answer goes out only with AUTO on (otherwise it's offered on Reply).
  Desktop (processCommandActivity.cpp ~885) does the same from its one
  inbox. **VE7NHW's on-air test (2026-10-09), AUTO on:** desktop JS8Call
  on a Hermes Lite 2 at low power, the X6100 at 1 W into a dummy load,
  about −15 dB both ways, the two set to different calls. Desktop (as
  A) left `X6100 MSG TO:B ...`, then (as B) asked `QUERY MSGS` and the
  X6100 answered `YES MSG ID n`. Desktop's `QUERY MSG n` then got
  **nothing** back. So the held message, the addressee match and AUTO
  were all fine (QUERY MSGS uses the same lookups as QUERY MSG). The
  answer is lost between decoding and sending. **Checked:** the unit
  tests feed `QUERY MSG n` as finished text (`incoming()`), and the UI
  harness never sends it over audio, so the real path is untested:
  desktop's frames → assembler → `classify` checksum (`" QUERY"` is
  16-bit checksummed; a bad checksum drops the message silently) →
  `js8_process()` → AUTO's send. **Next:** a test that sends desktop's
  exact frames for `K2XYZ QUERY MSG 3` through the decoder (our encoder
  matches desktop bit-for-bit, 868d051) and checks the answer goes out;
  the radio's `app_logs` from that test would also show what it decoded.
  Bench for retesting: the same HL2 + X6100 setup.
  **Ruled out on the PC (2026-10-09):** desktop's frames for
  `K2XYZ QUERY MSG n` (js8core's port of desktop's encoder; desktop's
  Varicode and command tables unchanged up to 7fd8ecd) decode on our side
  and pass the checksum. The whole sequence through `js8_process()` (MSG
  TO: kept, QUERY MSGS → YES MSG ID 1, QUERY MSG 1 → SEND
  `W1ABC MSG MEET AT THE PARK FROM N0XYZ`, deliver 1) works with AUTO on.
  The send queue (`auto_try_send()`) shows an "Auto: ... not sent" line
  for every drop, and `tx_queue_at()` a message for every failure. AUTO's
  idle watchdog is 60 min. **Waiting on the radio:** read (only) its
  `js8_history.db` (the exact text decoded from desktop), `js8_held.txt`
  (the message and its id) and `app_logs` ("JS8 auto: ..." lines), plus
  the calls used on each side and the id it gave.
- [ ] **Show (All / Directed / No HB): blank rows after switching.**
  **Not reproduced yet (2026-10-09):** harness ONLY_SHOWSCROLL (64 rows,
  21 after the switch, more than a screenful; hooks
  `dialog_js8_test_message`, `dialog_js8_list_overscroll`) ends exactly at
  the newest row, with or without a change, so the first idea (the list
  scrolled past its new end) isn't it. `list_scroll_end()` now backs up a
  list scrolled past its end anyway (harmless). **Suspect:** a repaint on
  the radio: the MFK path redraws only the rows that change
  (`table_key_pre_cb`, `table_invalidate_row`) and R1CBU rotates every
  redrawn pixel in software; the harness runs stock LVGL. **Next:** radio
  screenshots (`x6100-screenshot`) right after a Show press with blank
  rows, then after the MFK bump.
  Changing what Show shows doesn't pull the remaining rows down to the
  bottom at once, leaving blank space. `show_cb()` → `rebuild_rows()`
  (`src/dialog_js8.c`); likely the table's row count or scroll position
  isn't reset. Reproduce in the harness.
- [x] ~~Picking up a message from another station leaves it there~~:
  **checked, that's how JS8Call works.** VE7NHW pulled a relayed message
  from another station and ACKed it, and it stayed in their inbox.
  Desktop's `markMsgDelivered()` (JS8Call-improved UI_Constructor.cpp)
  only changes the message's type to `DELIVERED`, so later QUERY MSGS and
  heartbeats don't offer it again; it never deletes it. Ours does the
  same (`delivered` in `js8_held.txt`). No change.
- [x] **A default station STATUS for this app** **Done 2026-10-09 (user:
  desktop's default):** `IDLE <MYIDLE> VERSION <MYVERSION>` (JS8Call-improved
  Configuration.cpp "MyStatus"; our `<MYIDLE>` matches desktop's: 0M, 12M,
  3H, 2D). Every card so far saved `STATUS=` empty without anyone choosing
  it, so it's given once: `STATUSDEF=1` in js8_texts.txt marks that, and a
  STATUS cleared later stays empty, as desktop. Harness ONLY_STATUSDEF.
  (VE7NHW: details to talk
  over later). Today STATUS is empty until set in Settings.
- [x] **Ultra speed (experimental; desktop's "JS8 60").** **Done
  2026-10-09 (user's choices: names stay Turbo and Ultra, letters T and U,
  not desktop's new "JS8 40" / "JS8 60"; a Setting first, decoding always
  later if the CPU allows).** Desktop JS8Call-improved 7fd8ecd (shown
  since #371, 2026-09-27): submode 8, 384 samples a symbol (31.25 baud,
  250 Hz wide), 4 s slots, start delay 100 ms, modified Costas, −18 dB,
  rxThreshold 50 Hz, no heartbeats or HB ACKs, scheduled as Turbo; desktop
  decodes every speed every cycle, with no setting. Ours: speed table row
  `JS8_SPEED_ULTRA` ('U', `JS8_SUBMODE_ULTRA` = js8core's I bit; js8core's
  Ultra decoder constants match desktop's); js8core local patch 14 (Ultra
  takes Turbo's decode schedule, UPSTREAM.md); Settings *Ultra
  (experimental): On / Off* (`js8_ultra`, off to start): on, Ultra is
  decoded at every speed and the Speed button steps through it; off while
  on Ultra goes back to Normal; `js8_speed` allows 4. Heartbeat texts name
  the speed ("No heartbeats in Ultra"). Time: Auto learns only from Normal
  and Slow, as desktop. Tests: unit speed table, Ultra slots, loopback at
  every speed incl. Ultra, five speeds on one band; harness ONLY_SPEED
  (Setting, a `U` row, the button, HB refused, an Ultra frame's length,
  Setting off → Normal). **To check on the radio:** CPU with Ultra on
  (x6100-cpulog), and an Ultra QSO with desktop (the HL2 bench).
- [ ] **GPS** from a USB dongle (time and position). The user has the dongle;
  work on it **at the radio** (it must be plugged into the radio's HOST
  port). **Findings 2026-10-09 (code + AetherX6100Buildroot aca5e53):**
  - **How it reaches the GUI:** gpsd (`localhost:2947`); `src/gps.c`
    (R1CBU) publishes each report as MSG_GPS. Subscribers: the GPS screen
    (`dialog_gps.c`) and JS8 (`gps_msg_cb`, for *Spot GPS position*: done).
  - **Clock, no code (in principle):** the image runs gpsd at boot
    (`/etc/init.d/S50gpsd`, `-n /dev/ttyACM0`) and ntpd with `-g`
    (`S49ntp`), `/etc/ntp.conf`: `server 127.127.28.0 minpoll 4 maxpoll 4`
    + `fudge 127.127.28.0 time1 0.0 refid GPS` (gpsd's shared memory). So
    ntpd should set the clock minutes after a fix. The GUI never sets the
    clock itself. rtc1 isn't written (it drifts again without the GPS;
    optional: one `hwclock -w -u -f /dev/rtc1` after a sync, never in a
    loop, [i2c-0 is shared]).
  - **Catch 1, the device name:** gpsd watches **/dev/ttyACM0 only**.
    u-blox dongles show as ttyACM0 (works); Prolific / CH340 / CP210x ones
    show as **ttyUSB0** (gpsd never sees them). If so, the fix is in the
    radio's Linux layer (shared, not JS8; flag it to the user): add the
    device to S50gpsd, or a udev rule that runs `gpsdctl add` on hotplug.
    A JS8-only stopgap would be `gpsdctl add /dev/ttyUSB0` when JS8 opens.
  - **Catch 2, the grid:** only the GPS screen (APP > GPS) saves the GPS
    grid into QTH (`param_t_set(cfg.qth())`, while that screen is open).
    Recommended (JS8-only): JS8 uses the GPS grid while there's a current
    fix, for heartbeats, CQ, GRID answers, `<MYGRID4>` / `<MYGRID12>` and
    the map's home square; else the saved QTH.
  - **Catch 3, Time: Auto vs a clock step (needs code):** Auto's drift is
    learned against the old clock (e.g. −2.9 s); when ntpd then steps the
    clock by that much, JS8 time ends up ~2.9 s off and stops decoding
    until a search. Detect a system-clock step (CLOCK_REALTIME vs
    CLOCK_MONOTONIC) and reset the JS8 drift / restart Auto; check the
    receiver's ring realign (`check_clock`) does the right thing too.
  - **Precision:** NMEA time without PPS is usually 0.1-0.5 s late
    (`time1 0.0` doesn't correct it); fine for JS8, Auto absorbs it.
  - **First steps at the radio (read-only):** plug the dongle into HOST;
    `dmesg | tail` (ttyACM0 or ttyUSB0, chip), `ps | grep -E 'gpsd|ntpd'`,
    `gpspipe -w -n 5` (if installed) or APP > GPS, `ntpq -p` (GPS refid,
    offset, `*` once selected), `date -u` against the PC. Then decide on
    catch 1, write catches 2 and 3, test with the harness's HARNESS_GPS.
- [ ] **A Bluetooth keyboard** with JS8, already the next item (below). v1.0.2 has
  no pairing screen: pair with `bluetoothctl` over the USB console.

VE7NHW's list for beta 5 (2026-10-02), with notes on where each item
touches the code. **Highest priorities:** time sync, high-SWR protection,
the two upstream merges, and station history. Items get ticked here and
moved into the README's "New in ..." as they land.

## Core priorities

- [x] **Time sync overhaul + automatic time sync**, behaving like desktop
  JS8Call's auto sync. Today Time Sync is a button (Settings) that sets a
  JS8-only drift from the stations heard (README "Time Sync").
  **Why the old one failed (user, 2026-10-04):** it needed 3 decodes in
  the last 2 minutes (heartbeats did count), which a normal band rarely
  gives. **Researched:** desktop JS8Call-improved 44fa092
  (processDecodeEvent.cpp, WideGraph.cpp: Automatic Time Drift = every
  non-duplicate Normal/Slow frame's drift into a cumulative moving average
  capped at 60, set at each DecodeFinished; while it starts, Normal decoded
  every second, CPU-heavy, auto-stop after 1 decode by default); the
  Android-port app 9996202f (Auto time sync = desktop's average, off by
  default; "sync clock to next decode", "sync clock to this signal", +-1 s
  / +-100 ms; no every-second search); js8core only gives each decode's
  drift and set_time_drift_ms (we use both). **Done (user's choices: Auto
  on to start, Normal + Slow as desktop, page 4's empty button, the
  search too):** `src/js8/timesync.{hpp,cpp}`: AutoTimeSync (desktop's
  average; frames after the DuplicateFilter, low-confidence ones left out;
  a search's find counts as one frame, a reset starts afresh) and
  TimeSearch (its own low-priority thread: the latest 15 s decoded every
  4 s with js8core's legacy_decode, no js8core patch; a -18 dB signal
  decodes from windows starting 2.5 s before to 2.25 s after it, unit
  test, so 4 s leaves no gap; first decode = the drift; 5 min at most).
  Receiver feeds both; js8_rx C API (set_auto_sync, auto_sync_restart,
  search_start/stop, on_auto_drift, on_search). Dialog: page 4's Time
  button (press Auto on/off, `js8_tsync_auto`; hold search / stop; label
  "Time: Auto -1.2s" / "Time: Searching", marked while searching); Auto's
  drift set only when it moved 50 ms or more (each change re-snaps the
  decode windows) and never while sending; a find while sending waits for
  TX to end; Settings keeps Reset time drift (first line); the old
  3-decode median (js8_sync_drift) removed. CPU (ARM build under qemu,
  ~1.5x faster than the radio): a search window 0.26 s on noise, 1.2 s
  with three stations, so about a tenth of a core while it waits. To do:
  cpulog on the radio during a search. Harness ONLY_DRIFT rewritten (it
  had tested nothing since Time Sync left page 3).
- [x] **High-SWR auto-shutdown modes**, for unattended stations
  (automatic replies, heartbeats, beacons). The radio already reports SWR
  during TX (`vswr` in `radio.c`).
- [x] **High-SWR beep.**
  **Both done in 2e152e6:** over 3:1 for half a second while keyed turns
  AUTO, heartbeats, HB ACK and auto CQ off (the message on the air carries
  on), three beeps once TX is done. Harness ONLY_SWR. On the user's card
  since 2026-10-04 (image CI 37183047679 = fe42a8d: waterfall four levels +
  SWR guard + APRS Echo test / More services); to be tried on the air.
- [x] **Update to the latest gdyuldin release** (done in beta 4.1: we're on
  v1.0.2; his ver_1.1 with the Bluetooth pairing screen isn't released)
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

- [x] **Station history** (user's design, 2026-10-04; named *History*).
  **Done (d8263d9).**
  - **What's kept:** every station you've exchanged messages with, either
    way, heartbeat ACKs included, **per band**, for good (all-time list
    and map show where you've reached on that band). Their INFO and
    STATUS answers, to you or to anyone, with times; the latest is shown.
    The text of every message between you and them, time-stamped, in
    QSOs (a new QSO after 30 min quiet or on another band, the log
    prompt's rule). Heartbeat-only exchanges are kept but **not listed**
    as QSOs (user's choice C). Their messages to others aren't kept.
  - **Where:** one SQLite file, `/mnt/js8_history.db` (desktop keeps its
    inbox in SQLite; per-station files break on `/` calls and crowd the
    FAT partition). Written by its own thread (`js8-hist`, nice 10), a
    few seconds' worth per transaction; health line once a minute.
  - **Screens (to come):** page 3's free slot = *Heard: Recent / All
    time* for the Stations view and the map (over the messages it opens
    the Stations view in All time); an MFK press on a station (list or
    map) opens its History page: grid, distance, first/last heard, latest
    INFO and STATUS with their age, then QSOs newest first (date, band,
    messages, logged), each opening to its text. No "Ask INFO?" line
    (user: not wanted). Mock-ups first.
  - **Steps:** 1 recording only, measured on the radio with x6100-cpulog
    (user: scrap the feature if it's CPU heavy); 2 the All time switch;
    3 the History page; 4 the existing QSO log and Inbox brought in.
  - [x] Step 1 code: `src/js8/history.{hpp,cpp}` + `js8_history.h`,
    unit tests `[history]`, harness ONLY_HISTORY. Grid taken from the
    Stations list when the exchange itself has none. The CPU measurement
    on the radio was skipped (user: a geomagnetic storm left nothing to
    hear; carry on without it).
  - [x] Step 2, All time: page 3's slot, *Show History* over the messages,
    *Heard: Recent / All time* in the Stations view and on the map
    (`ever_rows`, loaded on the press and band changes, kept up to date
    from new messages; no fading on the map). A history-only station has
    no offset (-1): `apply_hold()` leaves yours alone.
  - [x] Step 3, the History page (`hpage_open()`): a short MFK press
    (LV_EVENT_SHORT_CLICKED, skipped after a hold) in the Stations view
    or on the map. Shots from the harness sent to the user before the
    build.
  - [x] **Lines without the calls** (user asked): their text with no
    recipient joins their open QSO; a message with no sender (first frame
    missed) joins the open QSO whose station sent on that offset (within
    the speed's "same station" window); your free text joins the QSO with
    the selected station. Never starts a QSO. Also fixed: our free text
    went out as "MYCALL: GOOD COPY" and its first word was taken for a
    call (classify() now decides the recipient).
  - [x] **Settings > Clear station history...** (user asked): two presses
    within 5 s; every band.
  - ~~Step 4: bring in the existing QSO log and the Inbox~~: not wanted
    (user, 2026-10-04: "we're good"); the history starts with this build.
    Also declined: an "All time" marker on the status line (the button
    shows it).
- [x] A Sort button on the Stations list (SNR, last heard ...). **Done
  (user's choices):** page 3's second button in the Stations view (blank
  over the messages, the map's view button on the map): *Sort: Heard you*
  (as before: who heard you first, then the newest), *SNR* (strongest
  first), *Time* (newest first), *Distance* (farthest first, no grid
  last); remembered (`js8_st_sort`); the selected station stays selected.
  `js8_stations_sort()` (`src/js8/js8_ops.cpp`, unit test), harness
  ONLY_STSORT. The distance column is wider so 5-digit km fit.
- [x] QRZ light on the Stations list and the map, to see at once who is
  calling you. The map already has a QRZ line (calls that sent you a
  message while the map or Stations view was open). **Done (user,
  2026-10-04: "the exact same QRZ indicator on the station list"):** the
  map's line over the Stations view too (`st_qrz_label`, a child of
  wf_box under the status line like it, same text and colours from
  `map_qrz_show()`); hidden on the map and over the messages. Leaving
  the Stations view with Show now clears the list as Map > Messages
  does (it used to keep old callers). Harness ONLY_STQRZ.

## APRS and messaging

- [x] An APRS commands submenu: weather and other simple services
  (research in [feature-ideas.md](feature-ideas.md): MPAD is worldwide,
  WXBOT US only; answers come back through every relay station that
  heard you). **Done (user's layout):** Echo test first in APRS >, and
  More services > just before Close opens a list of MPAD (incl. Email my
  position, `posmsg`), WXBOT, WXNOW, WHO-IS and JOKE messages, each filled in (grid added where the service
  takes one), with Back and Close. Harness ONLY_APRSMORE.
- [x] APRS badge on the Stations list: `@` in the star's place for a
  station seen relaying APRS messages back over JS8 (two-way gateway).
  **Done 2026-10-04:** a station heard sending `@APRSIS MSG TO:<anyone>`
  (an Echo test's answer to us, an SMS to anyone) gets `@`; it stays
  (the row is still gold if they heard us). `Station::aprs_gate`
  (src/js8/stations.cpp), unit test, harness ONLY_ATGATE.
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

- [x] **Map: a count on stacked stations (user, 2026-10-04):** on a busy
  band many stations share one spot (every station of a grid square sits
  at its centre; gridless ones at their call area's), so a white tag in
  the CQ tag's shape on the bottom right corner says how many (user
  picked white from green / white / dark mock-ups). Marks within 2 px
  are one spot; counts what the map shows. Harness ONLY_MAPSTACK.

- [x] **Sort: QSO (user, 2026-10-04):** a fifth Sort choice, only the
  stations you've had a QSO with: a QSO in the History that isn't
  heartbeat ACKs only (`js8_history_had_qso()`, an in-memory set loaded at
  open, marked as messages arrive), or logged (worked_before, the green
  calls: the History started empty with this build). Newest first; the
  map follows. Harness ONLY_STSORT.
- [x] **Freq into Settings (user, 2026-10-04):** *Frequencies: JS8Call's /
  GhostNet / kHz* (SETTINGS_FREQ 110, second line) opens the same list;
  page 6's slot 4 is empty. Harness ONLY_FREQ/KEYS/MODE use Settings.
- [x] **Bug: a USB keyboard plugged in while running did nothing** (user,
  2026-10-04; lights on, no keys). R1CBU 1.0's keyboard.c looked for
  `/dev/input/by-path/*-kbd` once per udev USB event, before the input
  node exists (0.34's USB thread slept 0.5 s between events, which hid
  it). User chose the shared-code fix (B) over a JS8-only rescan:
  keyboard.c rechecks every 0.5 s for 5 s after any USB add/remove,
  reopens a re-plugged keyboard (st_rdev/st_ino), frees the glob.
  `tools/js8_ui_harness/kbd_hotplug_test` (in the Tests workflow's harness
  job) fails on the old code. Noted in docs/UPSTREAM_README.md as worth
  offering upstream (ask the user first).
- [x] **Bug: Heartbeat button on the first open after power-on** (user):
  dialog_construct() draws the page's buttons before construct_cb, which
  turns auto HB off; with auto HB saved on (radio switched off with it
  running) the button showed "HB auto: soon" until the page changed, and
  hb_tick only refreshes it while auto is on. construct_cb now redraws
  the page's buttons at its end. Harness ONLY_BOOTHB (the stub buttons
  now keep the text they were drawn with; failed on the old code).
- [x] **Buttons moved (user, 2026-10-04), to free a slot for the next
  feature:** *Decode: All speeds / My speed* left page 6 for a Settings
  line; **Hold** moved from page 3 into Decode's place on page 6; page 3's
  middle slot is empty. **Waterfall default is Light** (was Calm): the
  user found it the best on the air. A card that already saved a level
  keeps it.

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
- [x] A small ALC rework for low power (under 1 W) into an amplifier.
  **Done after beta 4.5 (2026-10-04, user's video: 0.3 W into the XPA125B
  swung 18-35 W, about every 2.5 s, ALC bouncing).** Cause: tx_player.c
  corrected the gain every 2048-sample block (~23/s) from one reading;
  power reads in 0.1 W steps (radio.c tx_power * 0.1), and our d1ef18f
  "relative" up rule (< 80 % and > 0.1 W short) fired on a true 0.28 W
  reading 0.2, then the ALC rule pulled down: a limit cycle. Upstream FT8
  only raises when > 0.5 W short (never at 0.3 W). `src/tx_level.{c,h}`:
  1 s averages; within a transmission only down (ALC > 0.5: half the old
  formula, at most 1.5 dB a window; or power > 1.25 x setting + 0.1 W);
  between transmissions up (frame average short with ALC < 0.25: half the
  shortfall, 2 dB at most; within one reading step: creep 0.3 dB) until
  the ALC starts to show. **tx_player.c is JS8's only since the 1.0.2
  port** (FT8 runs upstream's src/ft8/tx_worker.c); they share the learned
  `ft8_output_gain_offset`. Unit tests `[txlevel]`: a model radio (0.1 W
  steps, meter lag) where the old loop swings 16 dB in a frame and the new
  one holds within 0.8 dB over 54 lag/rounding/start cases. Video and
  frames: ~/Work/x6100/research/alc-video/. Released as beta 4.6. **Then (user on 4.6: the ALC
  never left 0.0 and the amp crept up a watt or two a transmission):**
  while the ALC reads zero the between-transmissions step is 1 dB (0.3 dB
  only once the ALC shows a little): at the setting by the third
  transmission from 1.5 dB short instead of the sixth. **Then (user: "is there a better way to do
  it live?", 1-2 transmissions):** a live one-way ramp replaced the
  between-transmission steps: half-second averages; while the ALC reads
  zero, up 0.5 dB a window when under half the setting, else 0.25 (1 / 0.5
  dB a second), at most 8 dB a transmission; held for the rest of the
  transmission once the ALC shows (>= 0.1) or the power reads the setting,
  or after an overdrive cut (ALC > 0.5: half the old formula, at most
  0.75 dB a window). Can't see-saw: it never goes back up after holding.
  Model: 1.5 dB short is at the setting within the first transmission
  (ALC 0.28), steady from the second; never more than 2 direction changes
  in any frame over 72 lag/rounding/start cases.
- GPS: see "GPS" in VE7NHW's list (2026-10-09) at the top (merged there).

## Bugs

- [x] **A custom frequency showed the wrong band** (user, 2026-10-05: a CB
  frequency said "JS8 10m"). Opening JS8 with a custom frequency saved,
  load_band(0) tuned the nearest preset first and its message named that
  preset; where_label() now says "JS8 Custom" for any custom frequency
  (status, message line, info rows), the open path repeats it after the
  custom tune, the Frequencies list keeps the kHz ("Now: JS8 Custom, 27245
  kHz"), and the History screens name an off-band frequency "Custom".

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

- [x] **Decoder threads (VE7NHW 2026-10-09, my recommendation: both before
  Ultra could decode by default). Done 2026-10-09:** js8core patch 15
  (Ultra's own decode thread `js8-decode-u`, `on_decode_thread_start`
  hook); `src/js8/cpu_fence.{hpp,cpp}` keeps both decode threads and the
  time search off the last core (core 3 on the radio; core 0 takes the
  interrupts), Ultra's at nice 5; the GUI may run anywhere. Receiver: an
  Ultra pass ends a cycle (AUTO's replies) only while the main thread
  isn't in a pass; Time: Auto applies only at the main pass's end; a
  second stats line "decode Ultra: ..." each minute. Unit test checks the
  threads, their allowed cores and nice in /proc. **To measure on the
  radio:** this build vs the baseline below, Ultra on, then a busy band.
  Original notes: (1) Ultra on its own decode thread, so its 4 s slots never wait
  behind a Normal/Slow pass (js8core: `legacy_decode()` runs the speeds one
  after another on one thread; each pass has its own copy of the audio,
  decoders are `thread_local`, FFTW plan creation is already behind
  `fftw_mutex`, so threads are safe). Later maybe one thread per speed,
  events passed on in today's order, at a lower priority than the GUI.
  (2) Keep the decoder threads off one core (`pthread_setaffinity_np`,
  set while JS8 is open), not core 0 (busiest: interrupts); leave the GUI
  free to run anywhere rather than pinning it. Baseline 2026-10-09
  (research/cpulog/2026-10-09-2345Z, 4 speeds, quiet band, dummy load):
  decoder avg 25 % of a core, at 95 %+ for 11 s in 10 min in 1-2 s bursts,
  longest pass 1.5 s, ~2 windows merged a minute; all four cores never
  over 90 % together; GUI main avg 24 %, never 70 %, 0 ms wait.
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
