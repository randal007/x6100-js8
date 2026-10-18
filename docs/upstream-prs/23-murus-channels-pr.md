Murus apps, after the groundwork PR (#309) but independent of it: this one doesn't use the groundwork and builds on its own (see [discussion #302](https://github.com/OpenX6100/x6100_gui/discussions/302)). Base: OpenX6100 main 9714584; it merges with today's main (49328dd) and with each of Kris's open app PRs (#271, #272, #274, #276) without conflicts.

## What it does

Written by **Hany El Imam** (1KO125, the Murus team). It gives SWLs a channel memory and a station-name pop-up, both worked from the hand microphone:

- **Channel List:** up to 300 named channels. Each one stores the frequency, mode, AGC, PRE and ATT. They are kept in `/mnt/channels.json` (plain JSON, readable and editable by hand). The first time any channel function runs, the list starts with 18 channels (NAVTEX 518/4209.5, Shannon VOLMET, DWD RTTY and WeFax, GYA WeFax). The window shows number, name, frequency and mode, and can add, delete, rename and reorder channels.
- **Broadcast Info:** a pop-up showing the stations scheduled **now** within ±2 kHz of the current frequency. For each one it shows the name, schedule (UTC), language, target, ITU country and days. The schedule is `broadcast.csv`, copied to `/mnt/broadcast.csv` on first start so the user can replace it. *Guess:* by its column layout this is the EiBi schedule (eibispace.de), A26 season (its "last logged" remarks go up to [0926]).

The Channel List has no APP button and no app number. It doesn't use the groundwork PR's pieces (`ui_list`, `app_info`, the spectrum markers), and no other app calls it (from the code).

## What it changes on the main screen and keys

Everything is in `src/main_screen.c` `main_screen_hkey_cb()` (microphone keys) plus one line in `main_screen()`. That's 30 lines against main. Front-panel keys are unchanged (the panel V/M key is still the memories).

| Key / moment | On main today | With this PR (from the code) |
|---|---|---|
| Mic **V/M** | nothing ("Unsupported key" in the log) | opens/closes the CHANNELS window |
| Mic **GENE** | nothing | Broadcast Info pop-up for 2 s. Pressed again while it is up: the next match (shown as "2/3"). With no match: "NO STATION SCHEDULED" |
| Mic **NW** | nothing | saves the current frequency/mode/AGC/PRE/ATT as a new channel, named after the station in the pop-up (or the first one scheduled now). Shows "CHANNEL SAVED" |
| Mic **UP / DOWN** (short) | **tunes one step up/down** | **recalls the previous/next channel** (UP = previous) and shows its name. Wraps around. Blocked by the frequency lock, like before. **The mic no longer tunes by a step** (see question 2) |
| Mic UP / DOWN (long) | band up/down | unchanged |
| Boot | | `broadcast_db_init()` loads `broadcast.csv` into RAM (copying it to `/mnt` the first time) |

**With an app open** (FT8, RTTY panel, ...): V/M opens CHANNELS as an *overlay* over the app. In that case it doesn't go through `dialog.c`. It saves the app's soft-key page, loads its own (a 100 ms timer takes the keys back if the app reloads its page), and moves the encoder, VOL knob and USB keyboard to its own LVGL group. On close it gives them all back. It closes itself when the app closes or another dialog starts (`MSG_DIALOG_START/STOP`). The GENE pop-up is on `lv_layer_top()`, above everything, and never takes input.

**In the CHANNELS window:**
- F1 Close, F2 Add, F3 Delete, F4 Edit (name, through `textarea_window`), F5 Drag → Drop.
- MFK: one row; VOL knob: 10 rows. The VOL knob doesn't change the volume while the window is open.
- MFK press: tune to the channel and close.
- USB keyboard: arrows, PgUp/PgDn, Home/End, Esc.

## Files

- Hany's: `src/channels.c/.h`, `src/dialog_channels.c/.h`, `src/broadcast_db.c/.h`, `rootfs/usr/share/x6100/broadcast.csv` (installed by the existing `install(DIRECTORY rootfs/ ...)`), plus his lines in `src/CMakeLists.txt` and `src/main_screen.c`.
- `src/cJSON.c/.h`: Dave Gamble's cJSON (MIT), unmodified. `cJSON.h` is identical to v1.7.19 and `cJSON.c` to cJSON's master today (checked with diff).

## Commits

1. 85d651b **Hany's code** (author Hany El Imam), taken from his drop "1KO125 1.0.2 beta 4" (2026-10-15), Channel List parts only. In `main_screen.c` the hooks into his other apps are left out; they go in those apps' own PRs:
   - in UP/DOWN: `dialog_wefax/dsc/navtex_mic_step()`
   - in V/M: `dialog_wefax/dsc/navtex_vm_toggle()`

   Otherwise his lines are unchanged.
2. 6337505 **ours:** moves the Broadcast Info pop-up and Save as channel (~265 lines Hany wrote inside `main_screen.c`) into `src/broadcast_overlay.c/.h`, so `main_screen.c` keeps only the key cases. The code is unchanged; two functions lose `static`.
3. 87c7375 **ours:** Hany's files under **LGPL-2.1-or-later**, as he agreed on #302 (copyright lines kept; the files that had no licence line get one).

**Not in this PR,** though they are in the same drop:
- the microphone **MODE** key (next/previous mode, `mic_mode_step()`): not part of the Channel List; it could be its own small PR
- the SSTV keyboard dismiss and the other apps' cases (their PRs)

## Checked (by Claude, on a PC)

- **Syntax check:** `gcc -fsyntax-only -Wall -Wextra` against LVGL 8.3.11 and X6100Control v0.16.1 headers. It passes for `main_screen.c`, `dialog_channels.c`, `channels.c`, `broadcast_db.c`, `broadcast_overlay.c` and `cJSON.c`. The only warnings are unused-parameter ones, all on lines already on main. **The image's GCC 12:** the same six files compile with the image's toolchain (2026-10-18), no errors.
- **`broadcast_db.c` on the real `broadcast.csv`:**
  - loads 9,442 entries in about 10 ms on the PC and holds **about 12 MB of heap** (entries are fixed ~700-byte records in an array doubled up to 16,384)
  - lookups work (5000 kHz → WWV, WWVH, BPM, HLA; 4625 → The Buzzer)
  - not timed on the radio
- **`channels.c`:**
  - with no file, it loads the defaults and writes them
  - load/save round-trip, move, delete and rename all work
  - a truncated `channels.json` fails to load and is left as it is
- **Nothing transmits** (rule 1): no PTT, modem or TX call in any of these files (from the code).
- **Break check** on `main_screen.c`'s `case ACTION_APP_*` blocks: prints nothing. This PR adds no app case.

- **On my radio:** a test build with all of Hany's apps. The Channel List opened and worked in a short try; I haven't been through the list below yet.

## Radio test list

1. **Boot:** time from power-on to the main screen, against a main build. `/mnt/broadcast.csv` appears.
2. **Mic V/M on the main screen:**
   - CHANNELS opens with 18 channels and `/mnt/channels.json` appears
   - the MFK and VOL knob scroll; VOL doesn't change the volume while the window is open
   - MFK press tunes (frequency, mode, AGC, PRE, ATT) and closes
   - V/M and F1 close it
3. **Add, Delete, Edit, Drag/Drop:**
   - Edit with the on-screen keyboard and with a USB keyboard
   - close and reopen: the changes stay; reboot: they stay
4. **Mic UP/DOWN:**
   - steps through the channels and shows each name
   - confirm it no longer tunes by a step (question 2)
   - long press still changes band
   - with the frequency lock on, nothing happens
5. **Mic GENE:**
   - on 5000, 9420 and 15000 kHz, and on an empty frequency
   - press again within 2 s to step through the matches
   - with FT8 open: the pop-up appears above it
6. **Mic NW:**
   - saves a channel named after the station
   - with CHANNELS open, the new row is selected
   - during Drag it answers "Finish Drag or Edit first"
7. **CHANNELS over an app:**
   - V/M with FT8 open, then with the RTTY panel open: CHANNELS opens over it
   - close it: the app's soft keys, MFK and VOL work as before
   - also close the app (APP/GEN key) while CHANNELS is still open
8. **Transmit:**
   - nothing keys up at boot, on open/close, or on recall
   - press mic UP during an FT8 transmit or with PTT held, and note what happens. *From the code* there is no TX check: it retunes during TX (the VFO knob on main also does that) and also changes mode/AGC/PRE/ATT.
9. **Front panel:** the V/M key and the memories work as before.

## Open questions

**For gdyuldin (Georgy):**
1. **Do you want this in OpenX6100 at all?** Hany called it "probably less relevant to R1CBU".
2. **Mic UP/DOWN:** today they tune by a step; here they recall channels instead, always, because the list is never empty after first use. Should we:
   - (a) keep Hany's behaviour,
   - (b) step through channels only while the CHANNELS window is open and tune by a step otherwise, or
   - (c) add a setting?
3. **Overlay over an open app:** `dialog_channels.c` bypasses `dialog.c` (its own soft-key page save/restore, a 100 ms page-guard timer, its own input group). Is that acceptable, or would you rather `dialog.c` supported a second dialog on top?
4. **Memory and boot:** `broadcast.csv` is loaded at every boot whether or not GENE/NW is ever used (~12 MB heap measured on the PC). Load it on the first GENE/NW instead, and/or store it more compactly?

**For Hany:**
5. **`broadcast.csv`:** where does it come from, under what terms, and who updates it each season? *Guess:* it is EiBi's A26 schedule, so it would go stale when B26 starts at the end of October. Also: once copied, `/mnt/broadcast.csv` is never refreshed by a newer image, only replaced by hand (from the code).
6. **Smaller points:**
   - `channels.json` is written in place, so a power cut mid-write leaves it unreadable. Write a `.tmp` file and rename it?
   - recalling from the window ignores the frequency lock (the mic keys respect it)
   - `dialog_channels_add_on_open()` and `broadcast_db_count()` are unused
   - the default channels are European; is that intended?

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
