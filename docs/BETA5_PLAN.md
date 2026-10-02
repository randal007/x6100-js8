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
- [ ] **Merge the Murus team's SSTV release** (custom-r1cbu
  [releases](https://github.com/TheMurusTeam/custom-r1cbu/releases)):
  the list said beta 7; **beta 8 came out 2026-10-02** (SSTV transmit
  added). See "Merge notes" below.
- [ ] **Update to the latest gdyuldin release**
  ([x6100_gui releases](https://github.com/gdyuldin/x6100_gui/releases)):
  the list said v1.0.1; **v1.0.2 came out 2026-10-02**. Cleaner UI with
  more room for the map. See "Merge notes".

## Heartbeat and CQ

- [ ] The heartbeat timer no longer pauses during CQ.
- [ ] Heartbeats pause only when someone actually answers the CQ.
- [ ] The heartbeat timer works like CQ's (hold = Auto).
- [ ] The TX bar (red rectangle) moves to the heartbeat's frequency when a
  heartbeat goes out.

These change decision D1 from the code review (heartbeats pause for
10 minutes after anything you send by hand, CQ included;
[docs/review](review/)).

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
- [ ] Remove ACKs from the Winlink settings for APRS (stops the
  double-ACK spam).
- [ ] Contact book for SMS (no typing phone numbers each time).
- [ ] Saved messages / macros, as desktop JS8Call has.

## Screen and radio

- [ ] The green receive bar is as wide as the speed the station was last
  heard at (today it is always Normal's width).
- [ ] A small ALC rework for low power (under 1 W) into an amplifier.
  The TX audio path (`tx_player.c`) is shared with the FT8 app.
- [ ] GPS time and location (USB GPS dongle ordered; testing when it
  arrives). The firmware already reads gpsd for the APRS beacon.

## Bugs

- [ ] **Can they reach...? (QUERY CALL)** only works when you type the `?`
  yourself after the call. Desktop sends `CALL QUERY CALL W1ABC?`: add
  the `?` for you.
- [ ] **An @ALLCALL QUERY CALL** ("can anyone reach W1ABC?") to send to
  everyone, as well as to one station.

## Carried over from beta 4

From the README's "Coming next" and the fix plan:

- [ ] On-air tests: relays and store and forward with desktop JS8Call
  stations, the other APRS gateways (POTA, SOTA, email, Winlink), a long
  message watched to the end, and the safe-TX checks into a dummy load
  (GEN or a band key in the middle of a frame).
- [ ] Performance: each part's CPU use on the radio, spread over its four
  cores.
- [ ] Show Map: a Setting for your own square's colour (orange for now),
  polish from use on the air ([MAP_PLAN.md](MAP_PLAN.md)).
- [ ] Low-priority leftovers in the [fix plan](review/fix-plan.md)
  ("Not now": I-02, I-04, I-10 with B-11).

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

The Murus team hasn't moved to 1.0 yet, so merging both means bringing
their SSTV app onto 1.0 ourselves.
