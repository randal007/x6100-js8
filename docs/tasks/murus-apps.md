# Task: the Murus apps into OpenX6100, one pull request at a time

Brief for Claude Code cloud sessions, which VE7NHW (Randal) starts on
claude.ai/code, **one session per part** (the parts are below). Read
`CLAUDE.md` at the top of this repo first. Its rules hold here too,
especially 1 (nothing transmits on its own), 3 (name the layer) and 9
(check the code before claiming anything).

## What this is

Hany El Imam (1KO125, the Murus team) wrote five apps for the X6100 GUI:
**WeFax, NavTex/SITOR-B, SSTV (receive and transmit), DSC** and the
**Channel List / Broadcast Info** (an SWL channel memory and a station-name
pop-up on the main screen). He wants them in **OpenX6100/x6100_gui**. He
doesn't use GitHub, so Randal (VE7NHW, an OpenX6100 maintainer) sends the
PRs with Hany as the author. The agreement so far is in
[discussion #302](https://github.com/OpenX6100/x6100_gui/discussions/302):
one PR per app, WeFax first, NavTex last (Hany is writing his own decoder to
replace the GPL-3.0 fldigi code), DSC whenever Hany says it's ready, app
numbers 109 WeFax / 110 NavTex / 111 SSTV.

## Where things are (branches in randal007/x6100-js8)

| Branch | What |
|---|---|
| `murus-drops` | Hany's source **as received**, one commit per drop, on top of x6100_gui **v1.0.2** (the base he built on). `git diff v1.0.2 murus-drops` shows exactly what he added and changed. First drop: 1KO125 1.0.2 beta 4, 2026-10-15 (c0bf2ab). Never merged anywhere. |
| `ox-main` | A copy of **OpenX6100/x6100_gui main** (3b8fd33, 2026-10-14), the base for every PR branch. You can also add `https://github.com/OpenX6100/x6100_gui` as a remote and fetch a newer main. Say which commit you used. |

The `v1.0.2` tag is in this repo (`git fetch --tags` if missing).

**You produce:** one branch per PR, cut from `ox-main`, pushed to
randal007/x6100-js8 with the name given for each part below, plus a
**draft PR into `ox-main`** in randal007/x6100-js8 whose description is
the text for the real OpenX6100 PR. **Don't push or open anything in
OpenX6100 or anyone else's repo:** Randal's local session reviews your
branch, compile-checks it with the image's GCC 12, builds a test image,
tests it on the radio, then pushes it to OpenX6100 and opens the real PR.

## What Hany changed outside his own files (vs v1.0.2)

| File | Change | Goes in |
|---|---|---|
| `src/ui_list.c/.h`, `src/app_info.c/.h` | new: list widget and Info window (the apps' credits) used by WeFax, NavTex, SSTV, DSC | Part 1 |
| `src/recorder.c` | the recorder writes **WAV, mono, 12 kHz, 16-bit** (was MP3 at the capture rate), the format the apps' *Test* function plays back; also `mkdir` of the folder, `localtime_r` | Part 1 (changes every user's recordings: say so in the PR, ask Georgy) |
| `src/spectrum.c` | tuning markers for WeFax (1075/1925 Hz), NavTex and DSC (915/1085 Hz): includes each app's header and asks `dialog_*_is_active()` | Part 1 (see below) |
| `src/cfg/settings_manager.h`, `cfg_api.*`, `settings/settings_page_ui.cpp`, `src/dsp.cpp` | **S-meter offset** setting (-30..+30 dB, display and CAT) | Part 1b, its own PR |
| `src/dsp.cpp` | auto levels add `zoom_level_offset` (the noise floor climbed 3 dB per zoom step) | Part 1c, its own PR |
| `src/cfg/settings_manager.h` | `HF_MIN_FREQ` 500 kHz to 100 kHz | **drop:** already in OpenX6100 (#294) |
| `src/settings_types.h` | `ACTION_APP_WEFAX/NAVTEX/SSTV/DSC` | each app's own PR, with its number written out (see the comment there on ox-main) |
| `src/buttons.cpp` | app buttons, a 4th APP page | each app's own PR (see below) |
| `src/main_screen.c` | each app's `case ACTION_APP_*`; a front-panel key closes SSTV; **the Channel List**: station-name pop-up (`broadcast_overlay_*`), saving a channel, recalling channels with keys | the app's case in its own PR; the Channel List parts in Part 4 |
| `src/main.c` | `sstv_resources_init()` | Part 3 |
| `src/CMakeLists.txt` | each app's sources | each app's own PR |
| `rootfs/usr/share/x6100/` | `wefax_stations.txt` (WeFax), `sstv/` (SSTV), `broadcast.csv` (Channel List), `dsc_*.bin` (DSC), `COPYING` | with its app; `COPYING` see Licence |
| top level | `NOTICE-1KO125.md`, `COPYING`, `README-PORT.txt`, `vessels.csv` (DSC's source data) | not sent, except `vessels.csv` with DSC if its script needs it |

Check the table against `git diff v1.0.2 murus-drops` yourself before you
start (rule 9). If something doesn't fit, say so in the draft PR.

## Rules for every part

- **Hany's code as he wrote it**, in a commit authored by him:
  `git commit --author="Hany El Imam <info@murus.it>"` (the address on his
  GitHub account, TheMurusTeam), with the trailers
  `Co-authored-by: Randal VE7NHW <randal007@gmail.com>` and the
  Claude `Co-Authored-By` line from CLAUDE.md. **Anything you change to
  make it fit current OpenX6100 main** (conflicts, APIs that moved, our
  suggestions) goes in **separate commits after his**, so a reviewer sees
  his work and ours apart. The PR description lists our commits and why,
  with a link to each (CLAUDE.md: a PR that's mostly someone else's code
  shows our own lines).
- **Licence: don't change any header yet.** Hany's files say
  GPL-3.0-or-later; he said on #302 he means LGPL-2.1-or-later as they go
  in, and Randal asked him (2026-10-15) to confirm we may change the
  headers. Until he answers, the draft PR says "headers change to
  LGPL-2.1-or-later once Hany confirms on #302" under *Before merging*.
  Others' code keeps its licence (Nayuki QR: MIT; cJSON: MIT).
- **Nothing transmits by itself** when an app opens or the radio starts
  (rule 1). SSTV transmits: check it only keys when the user presses send,
  and that closing the app mid-picture drops PTT. Say what you found.
- **After any change to `src/main_screen.c`** run CLAUDE.md's break check
  (the awk line under *Build and test*). It must print nothing.
- **App numbers** written out in `settings_types.h`: WeFax 109, NavTex 110,
  SSTV 111; DSC the next free number on main when its turn comes. No new
  long-press entries (gdyuldin's rule, discussion #283).
- **APP menu and order (VE7NHW, 2026-10-15): Kris's apps go into
  OpenX6100 first** (WSPR #271, PSK31 #272, Olivia #274), so the Murus app
  PRs are opened there only after those are merged; your branches can be
  ready before that. The layout carries on from VE7NHW's test build
  `digimodes-test` (see its `src/buttons.cpp`), which he likes:
  page 3 = WiFi, Bluetooth, WSPR, JS8 (JS8 comes last, in JS8's own PR
  4/4); **page 4 = PSK31, Olivia, WeFax, SSTV**; **page 5 (new) = NavTex,
  DSC**. The Channel List has no APP button. If Kris's apps aren't on
  `ox-main` yet, put WeFax/SSTV where that layout puts them and say in the
  PR that it's rebased once his are merged. App numbers don't change with
  the position (they're IDs, not places).
- **Compile check:** you can't build the ARM image. Build what you can on
  the host (e.g. the files that don't need the radio's headers with
  `g++/gcc -fsyntax-only`, or the project's own test build if it works) and
  say exactly what was and wasn't checked. Randal's session runs the
  image's GCC 12 on it.
- **Don't touch JS8** (`src/js8/`, `dialog_js8.c`, `third-party/js8core`),
  and nothing in `docs/` except your part's lines in this file's *Status*.
- **The PR text** (draft PR description): what the app does in plain words
  for a ham, what it changes outside its own files and why, our commits,
  **a radio test list** (what Randal should try on the radio), and open
  questions for gdyuldin / Kris / Hany. Short and factual: mark what you
  read in the code as "from the code" and anything you suspect as a guess
  (Randal's posts go out under his name).
- Finish with a short report: branch, commits, what's checked, what's left.

## The parts

**Part 1: shared groundwork.** Branch `murus-groundwork`. `ui_list`,
`app_info`, the recorder in WAV, and the spectrum's tuning markers. For
the markers, **suggest a small API instead of `spectrum.c` including every
app's header**: e.g. `spectrum_set_markers(int from_hz, int to_hz)` /
`spectrum_clear_markers()`, which an app calls when it opens and closes (one
commit of ours on top of Hany's; Georgy asked for exactly this kind of
change on #300). Nothing visible to a user until an app uses it, except the
recorder's WAV. Also, as **two separate small branches** from `ox-main`:
**Part 1b** `murus-smeter-offset` (the S-meter offset setting) and
**Part 1c** `murus-zoom-levels` (the auto-level fix at zoom), each with its
own draft PR.

**Part 2: WeFax.** Branch `murus-wefax`, cut from `murus-groundwork` (it
needs it). `dialog_wefax*`, `src/wefax/`, `wefax_stations.txt`, its
CMake lines, button, number 109, its `case`, and its markers through Part
1's API.

**Part 3: SSTV (receive and transmit).** Branch `murus-sstv`, cut from
`murus-groundwork`. `dialog_sstv*`, `src/sstv/`, `sstv_resources.*` and
its `main.c` call, `rootfs/usr/share/x6100/sstv/`, number 111, the
main-screen key that closes it. Its transmit path uses `radio_set_modem()`
/ `radio_set_pwr()`: compare with how FT8 and RTTY transmit on main and
say how its drive level is set (discussion #303 is about one shared
transmit-level module).

**Part 4: Channel List / Broadcast Info.** Branch `murus-channels`, from
`ox-main` (it doesn't use the groundwork; check). `channels.*`,
`dialog_channels.*`, `broadcast_db.*`, `cJSON.*`, `broadcast.csv`, and its
main-screen parts. **This one changes the main screen itself**, so keep
its `main_screen.c` changes as small and as separate as possible, and list
every behaviour it adds to the main screen and the keys. Hany called it
"probably less relevant to R1CBU": the PR asks Georgy whether he wants it.

**Later, not now:** **NavTex/SITOR-B** (wait for Hany's own decoder, which
replaces the GPL-3.0 fldigi/Venturi code) and **DSC** (Hany is still
working on it). Each gets its own part here when its drop arrives.

## Status

Each session updates its own line here (on its branch is fine; Randal's
session copies it to main).

- Part 1 (`murus-groundwork`, `murus-smeter-offset`, `murus-zoom-levels`): not started
- Part 2 (`murus-wefax`): not started (needs Part 1)
- Part 3 (`murus-sstv`): not started (needs Part 1)
- Part 4 (`murus-channels`): not started
- NavTex, DSC: waiting for Hany
