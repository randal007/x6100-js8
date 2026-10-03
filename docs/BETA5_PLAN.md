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
- [ ] **High-SWR auto-shutdown modes**, for unattended stations
  (automatic replies, heartbeats, beacons). The radio already reports SWR
  during TX (`vswr` in `radio.c`).
- [ ] **High-SWR beep.**
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
- [ ] A Sort button on the Stations list (SNR, last heard ...).
- [ ] QRZ light on the Stations list and the map, to see at once who is
  calling you. The map already has a QRZ line (calls that sent you a
  message while the map or Stations view was open).

## APRS and messaging

- [ ] An APRS commands submenu: weather and other simple services
  (research in [feature-ideas.md](feature-ideas.md): MPAD is worldwide,
  WXBOT US only; answers come back through every relay station that
  heard you).
- [ ] **Winlink: no ACK request.** Winlink works on the air, but the
  WLNK-1 gateway answers with its own message anyway, so the `{nn}` we
  add to ask for a receipt brings a second ACK. Stop adding it for WLNK-1
  (`want_id` in `aprs_prepare()`, `src/dialog_js8.c`); SMS and email keep
  theirs.
- [ ] Contact book for SMS (no typing phone numbers each time).
- [ ] Saved messages / macros, as desktop JS8Call has.

## Screen and radio

- [x] The green receive bar is as wide as the speed the station was last
  heard at (today it is always Normal's width). **Done (port-1.0.2):** the
  Stations entry's speed, else the row's (selected_speed()); JS8 draws it
  itself (cursor_box), lv_finder (shared with FT8) untouched; hidden only
  where the red band covers it exactly. Harness ONLY_FINDER.
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

## Carried over from beta 4

On the air with beta 4 (VE7NHW, up to 2026-10-02): relays, messaging,
store and forward and Winlink all work; only QUERY CALL failed (above).

- [ ] On-air tests still to do: POTA and SOTA spots through the gateways.
- [ ] Show Map: a Setting for your own square's colour (orange for now),
  polish from use on the air ([MAP_PLAN.md](MAP_PLAN.md)).

## Low priority (after the features)

- [ ] Performance: each part's CPU use on the radio, spread over its four
  cores.
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
