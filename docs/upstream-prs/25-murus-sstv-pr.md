This PR adds Hany El Imam's (1KO125, the Murus team) **SSTV app**: it receives and transmits slow-scan TV pictures. It is one PR per app, as agreed on discussion #302.

It needs the groundwork PR, #309 (`ui_list` and the Info window), and comes after WeFax, #311, as agreed on #302: its base is `murus-wefax`, so only SSTV's own commits show here. It goes in after those two.

## What it does (from the code)

- **Auto RX:** listens for the VIS header, recognises the mode and draws the picture line by line as it arrives. A new VIS restarts the capture; the *Auto RX Restart* button turns that off.
- **Manual RX:** decodes in a mode you pick or that the app detects from the line syncs. It stops when the syncs stop, or after the full length of the mode.
- **Modes:** Robot 8 B/W, Robot 36, Robot 72, Martin M1/M2 and Scottie S1/S2 on receive and transmit. Scottie DX is receive only.
- Received pictures are saved as PNG in `/mnt/sstv`. A browser (*RX Pics Browser*) shows them and can delete them.
- **TX wizard:** *TX* opens a list of actions:
  - CQ
  - Reply to a callsign
  - Image only
  - Repeat the last picture sent
  - Set the target callsign

  Then you pick a picture from `/mnt/sstv/tx_pics`, a text template from `/mnt/sstv/tx_text` and text colours. The radio's callsign setting is written on the picture. A preview shows exactly what will go out. *Halt TX* stops a transmission.
- On first start, `sstv_resources_init()` copies the default pictures and templates from `/usr/share/x6100/sstv` to `/mnt/sstv`. It never overwrites the user's files.
- When it opens, it caps the power at 5 W, like FT8.

## Outside its own files

- `src/main.c`: one call, `sstv_resources_init()`, after `cfg_init()`. It only makes folders and copies files; no radio access.
- `src/settings_types.h`: `ACTION_APP_SSTV = 111`. There's no long-press entry for it in the settings (per #283).
- `src/buttons.cpp`: the SSTV button on a new **APP page 4**, in the last place. The layout agreed for the APP menu is page 4 = PSK31, Olivia, WeFax, SSTV. WeFax (#311) sits next to it on this branch; PSK31 and Olivia (Kris, #272/#274) aren't on main yet, so their two places are empty for now. Those PRs change the same lines, so whichever is merged second will show a conflict; I'll add a commit that puts them together.
- `src/main_screen.c` (**shared main-screen code**):
  - the app's `case` (two places);
  - **any front-panel key press, and any microphone key except FINP, closes SSTV's target-callsign keyboard** if it is open. Otherwise the call returns at once.

- `src/CMakeLists.txt`: the SSTV sources. libpng and libsndfile are already linked.
- `rootfs/usr/share/x6100/sstv/`: 4 default pictures and 6 text templates.

## Commits

| Commit | Author | What |
|---|---|---|
| 6f3fe9b | Hany El Imam | His SSTV files byte-identical to the 1KO125 1.0.2 beta 4 drop (`dialog_sstv*`, `src/sstv/`, `sstv_resources.*`, `rootfs/.../sstv/`). Also the SSTV lines of his changes to the shared files above, as he wrote them |
| e3ff694 | ours | `ACTION_APP_SSTV = 111` written out. His enum was implicitly numbered, which would make it 109 here (WeFax's number); `ACTION_APP_*` numbers are fixed IDs |
| e7c8959 | ours | Button moved to the agreed place: page 4, last button |
| ee08b06 | ours | **Power restored when SSTV closes.** It caps the power at 5 W on open but never set it back, so after closing SSTV the radio stayed at 5 W while the screen showed the setting. Now `destruct_cb()` restores it after the TX thread has stopped, as FT8's does |
| 5f66c00 | ours | Hany's files under **LGPL-2.1-or-later**, as he agreed on #302 (copyright lines kept, the GPLv3 7(b) term dropped) |
| 72f16be | ours | Merge of the WeFax branch: both add a line in the same places (page 4: WeFax, SSTV; numbers 109, 111; each window case with its own `break`) |

## Transmit: what was checked (from the code)

- **Nothing transmits on open or at start-up.** `construct_cb()` subscribes to audio, sets the power cap and starts a UI timer; `sstv_resources_init()` only copies files. The modem is keyed in one place only, `radio_set_modem(true)` in `tx_thread_cb()`. That thread is started only by `tx_start_final_rf_mode()`, whose only callers are list confirmations in the TX wizard:
  - choosing the colours (CQ and Reply);
  - choosing the picture (Image only);
  - choosing *Repeat last* (only after a successful TX in this session).

  Each needs the operator to press *TX* and then confirm one to three lists. The lists confirm on `LV_EVENT_PRESSED`, so the release of the same press can't confirm the next list.
- **Halt TX and closing the app mid-picture both drop PTT.** *Halt TX* sets `tx_abort`. `destruct_cb()` sets `tx_abort` and **joins the TX thread before anything else is torn down**. The thread checks `tx_abort` every 2048 samples (about 43 ms at 48 kHz), drains what PulseAudio already holds (`audio_play_wait()`), then `radio_set_modem(false)`.
  - Small gap (a guess at the impact: negligible): if Halt or Close comes in the few milliseconds between the encoder finishing and the keying, the modem is keyed and released at once, with no audio.
- **Drive level, compared with FT8 on main** (RTTY on main only receives, so there's nothing to compare there):
  - **FT8** (`ft8/tx_worker.c`):
    - starts from a fixed offset: -9.4 dB with the patched BASE, -16.4 + 10·log10(P) with the stock one, plus the learned `cfg.ft8.output_gain_offset`;
    - part of it goes on the ALSA play volume, the rest as digital gain;
    - after about 30 blocks, every telemetry reading cuts the level on ALC > 0.5 or raises it when power is 0.5 W under target;
    - the result is saved back to the settings database.
  - **SSTV, digital modes (USB-D/LSB-D), or any mode with the stock BASE:**
    - the ALSA play volume is fixed at -12 dB; the rest is digital gain;
    - it starts from the level saved in `/mnt/sstv/tx_level_db.txt` (level and power it was measured at, rescaled for power on the stock BASE), or -40 dB the first time;
    - it averages ALC and power over 1 s windows:
      - ALC > 1.0: cut by an estimate, up to 10 dB;
      - ALC 0.3–1.0: cut 0.5 dB;
      - it waits while the ALC is still falling;
      - otherwise it steers the power to 97 % of the setting (max 5 W), 0.7 × the error, up to 6 dB;
    - the average of the good readings is saved to that file. The play volume is restored afterwards.
  - **SSTV, USB/LSB with the patched BASE:** no automatic control. The level is the TX codec gain slider and the normal play volume.
  - For #306 (the shared transmit level, now on main): SSTV has its own loop and its own file on `/mnt`, not the settings database. It would be a natural user of a shared module.
- SSTV doesn't set the mode, the TX/RX filters or the DSP. It also doesn't lock the frequency or mode during TX (FT8 does). It transmits in whatever mode the radio is in.

## Checked so far

- `git diff murus-drops~1 murus-drops` (Hany's drop against its base, v1.0.2): for SSTV, besides the app's files and its `case`, `main_screen.c` gets the keyboard-closing key hook. His `sstv/` files and his main-screen code don't use the spectrum markers.
- **Host compile** (GCC 13, `-std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror=implicit-function-declaration`, `-O2 -c`), against this tree's LVGL submodule, X6100Control v0.16.1 headers, libpng, libsndfile, liquid-dsp, pulse and libnm headers:
  - `dialog_sstv.c` (with its four `.inc`), `sstv_resources.c`, `sstv/*.c`, `main.c` and `main_screen.c` compile; `buttons.cpp` compiles as C++.
  - Warnings in SSTV code (none affects behaviour):
    - three unused items: `tx_type_preview_cq`, `tx_target_selector_build`, `tx_draw_text`;
    - one `-Wformat-truncation` on a saved file name;
    - four `-Wmisleading-indentation` on one-line `if`s in `sstv/decoder.c`, which do what they look like.
  - `main_screen.c`'s two warnings are already on main.
- CLAUDE.md's `case ACTION_APP_*` break check on `main_screen.c` prints nothing.
- **The image's GCC 12:** `dialog_sstv.c`, `sstv_resources.c`, `main_screen.c` and `buttons.cpp` compile with the image's toolchain (2026-10-18), no errors.
- **On my radio:** a test build with all of Hany's apps. SSTV opened and worked in a short receive try; **transmitting hasn't been tested yet** (dummy load first, item 5 below).

## Radio test list

1. Boot: `/mnt/sstv/tx_pics` holds 4 PNGs and `/mnt/sstv/tx_text` 6 templates. Add your own file, reboot, check it's still there.
2. APP → page 4: SSTV is the last button. Open and close it a few times.
   - **Nothing keys** on open.
   - With power set above 5 W: "Power was limited to 5W" on open, and **the setting is back after closing** (our fix).
3. Auto RX on a known SSTV frequency (14.230 USB) or a recording played in:
   - the picture draws as it comes;
   - the PNG is in `/mnt/sstv` and in the RX browser.
4. Manual RX: once with Autodetect, once with a fixed mode.
5. **TX into a dummy load first**, USB-D, 5 W:
   - TX → CQ → picture → colours. It keys only after the last confirmation.
   - Power settles near the setting; ALC stays low.
   - It returns to Auto RX at the end.
   - Repeat in USB (patched BASE: level from the TX codec slider).
6. **Halt TX** mid-picture: PTT drops within a fraction of a second.
7. **Close the app mid-picture** (*Close SSTV*, or open another app): PTT drops and the power setting is restored.
8. TX → Set target callsign: the keyboard opens. A front-panel key (e.g. a band key) closes it; FINP confirms it.
9. RX stays fine with the main-screen recorder running (WAV, from #309).

## Open questions

- **Georgy (gdyuldin):**
  - Are you OK with the hook in `main_screen.c`'s key handlers that closes SSTV's callsign keyboard on any key? An alternative is a generic "close the open keyboard" in `textarea_window`, which would keep `main_screen.c` free of app names.
  - On main, `audio_play()` in `audio.c` has no `return` (it calls `audio_player_send()` and drops its result; it was the same in v1.0.2). SSTV is its only caller and checks `audio_play(...) < 0`. Should we fix it in a separate one-line PR?
- **Kris / #306:** the shared transmit level is on main now (#306). Should SSTV's level control move onto it later, with its saved level in the settings database instead of `/mnt/sstv/tx_level_db.txt`?
- **Hany:**
  - Should SSTV lock the frequency/mode during TX, like FT8?
  - Should the Test RX code and the unused functions stay in, or go before merging? Test RX is behind `SSTV_PAGE3_MENU 0`.
  - The Info text mentions a "Web Manager (http://&lt;radio IP&gt;/sstv)", which doesn't exist in OpenX6100.

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
