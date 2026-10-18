This is the second of the Murus team's PRs (Hany El Imam, 1KO125; see discussion #302). It adds **WeFax**, a receiver for HF weather fax (radiofax) broadcasts such as DWD, NOAA, JMH and Kyodo. It only receives: there is no transmit code in it.

**It builds on the groundwork PR, #309** (the list widget, the Info window and the spectrum marker API), so this PR's base is `murus-groundwork` and only WeFax's own commits show; it goes in after #309.

## What it does (from the code)

- **APP → page 4 → WeFax** opens a window over the main screen. It listens to the receiver audio at 12 kHz. Hany's decoder is set for 120 LPM, IOC 576, 1809 pixels per line, centre 1500 Hz and shift 850 Hz.
- **Manual RX / Auto RX:**
  - *Manual RX* draws lines as they come.
  - *Auto RX* waits for the APT start tone and phasing, then draws the fax. At the stop tone it saves the picture as PNG and waits for the next fax.
  - *Clear* wipes the picture.
- **Freq.:**
  - opens the station list (from weatherfax.com, about 1,500 lines) with each station's schedule in UTC
  - *Tune* sets **USB** and dial = frequency − 1.5 kHz
  - the button then shows the station's call, or the frequency in kHz once the VFO moves.
- **Live adjustments:**
  - *Align* (MFK moves the picture sideways) and *Tilt* (slant).
  - *View Size* switches between a small view in the panel and a big view.
  - *Save and Clear* writes a PNG and stops the decoder.
- **Fax Browser:** lists the saved PNGs. You can open one, fix its align or tilt, and *Done* writes it back.
- **Auto Slant:** remembers the tilt for each station in `/mnt/wefax/slant.txt`.
- **Info:** the credits window from the groundwork PR.
- **Files on the card:**
  - `/mnt/wefax/wefax-YYYYMMDD-HHMMSS.png` (local time) and `/mnt/wefax/slant.txt`
  - the station list is `/usr/share/x6100/wefax_stations.txt`; a copy at `/mnt/wefax_stations.txt` overrides it.
- **Spectrum:** while WeFax is open, two tuning markers show at 1075 and 1925 Hz. They go away when it closes.
- **Mic keys** (our commit 33ccf7e, from Hany's main-screen code). While WeFax is open:
  - mic UP/DOWN (short press) step to the previous or next station in the list
  - mic V/M opens or closes the list.
- **Not changed:** it changes no filter, DSP or power setting. The VOL knob still sets the volume inside the window.

## Outside its own files

| File | Change | Layer |
|---|---|---|
| `src/settings_types.h` | `ACTION_APP_WEFAX = 109` | shared |
| `src/buttons.cpp` | new APP page 4 with the WeFax button. Page labels change from 1:3…3:3 to 1:4…4:4 | shared |
| `src/main_screen.c` | `#include "dialog_wefax.h"`, its case in `main_screen_start_app()` and `main_screen_action()`, mic UP/DOWN and V/M hooks | shared (main screen) |
| `src/CMakeLists.txt` | `dialog_wefax.c wefax/decoder.c wefax/stations.c` | build |
| `rootfs/usr/share/x6100/wefax_stations.txt` | station list | data |

No new long-press entries. The break check on `main_screen.c` prints nothing.

**APP layout:** this follows VE7NHW's test layout:

- page 3 = WiFi, Bluetooth, WSPR, JS8
- page 4 = PSK31, Olivia, WeFax, SSTV
- page 5 = NavTex, DSC.

Kris's WSPR, PSK31 and Olivia (#271/#272/#274) aren't on main yet, so for now page 4 is *empty, empty, WeFax*, keeping PSK31's and Olivia's places. Those PRs and this one change the same lines (the button list, the app numbers, the window cases), so whichever is merged second will show a conflict; I'll add a commit that puts them together.

## Commits

| Commit | Author | What |
|---|---|---|
| 5d9bc2b | Hany El Imam | His WeFax files byte-identical to the 1KO125 1.0.2 beta 4 drop (`dialog_wefax*`, `src/wefax/`, `wefax_stations.txt`). Also his lines in the shared files, where he put them: the WeFax button on APP page 3, `ACTION_APP_WEFAX` after `ACTION_APP_WIFI`, his case, his CMake sources |
| 9b80de0 | ours | `ACTION_APP_WEFAX = 109` written out (same value; agreed on #302) |
| 32c6e21 | ours | WeFax button moved to its place in the APP layout (new page 4) |
| fd06b3a | ours | Markers through `spectrum_set_markers(1075, 1925)` on open and `spectrum_clear_markers()` on close. They replace his `dialog_wefax_is_active()` check in `spectrum.c` |
| 33ccf7e | ours | His mic UP/DOWN and V/M hooks for WeFax alone (his also cover DSC, NavTex and the Channel List). With WeFax closed, or with no station file, mic UP/DOWN does `freq_shift()` as before. It is a separate commit so it can be dropped or replaced by a generic hook |
| 1777792 | ours | `audio_cb(size_t n, …)` to match `audio_float_cb_t`. On the radio's 32-bit ARM it is the same type; on a 64-bit host the old signature is an incompatible pointer |
| b953082 | ours | Hany's files under **LGPL-2.1-or-later**, as he agreed on #302 (copyright lines kept, the GPLv3 7(b) term dropped) |

## Checked so far

- `git diff <v1.0.2> murus-drops` shows exactly the WeFax files and lines listed above (base: the drop's parent, 416377c, "Bump version" = v1.0.2).
- **Host compile (GCC 13, `-std=gnu11 -c -O2 -Wall -Werror=implicit-function-declaration`).** Built against this tree's LVGL submodule, X6100Control v0.16.3 headers (with a stub `api.h`, which is generated at build time), and liquid-dsp, pulseaudio and alsa headers. Results:
  - `dialog_wefax.c` (with its two `.inc`), `wefax/decoder.c`, `wefax/stations.c`, `main_screen.c` and `spectrum.c` compile with no errors. `buttons.cpp` compiles with no errors under `g++ -std=gnu++17`.
  - Warnings in Hany's code: six unused variables in `decoder.c` (diagnostics), one `-Wformat-truncation` in `dialog_wefax_slant.inc` (truncation is intended), and **`test_cb` defined but not used** (see questions).
  - The warnings in `main_screen.c` and `buttons.cpp` are ones main already has.
- **Nothing transmits:** WeFax has no PTT, modem, TX or power call. Its only radio calls are `radio_change_vol()`, plus setting the mode and frequency when you tune a station.
- **Teardown:** closing calls `dsp_audio_set_active(id, false)` before freeing the decoder. That call takes the DSP audio mutex, so no audio callback is running when the decoder is freed. A save in progress works on its own copy of the picture in its own thread.
- **Memory (from the code):**
  - a 750×320 preview canvas held all the time (≈940 KB at 32-bit colour; in BSS, so it uses RAM only once WeFax has opened)
  - a full-resolution buffer of up to 2400×1809 bytes (≈4.3 MB), allocated on first use and freed when WeFax closes.
- **The image's GCC 12:** `dialog_wefax.c`, `main_screen.c` and `buttons.cpp` compile with the image's toolchain (2026-10-18), no errors.
- **On my radio:** a test build with all of Hany's apps. WeFax opened and worked in a short try; I haven't been through the list below yet.

## Radio test list

1. **Open and close WeFax** (APP → 4:4 → WeFax), several times.
   - Nothing keys up.
   - The 1075/1925 Hz markers appear on the spectrum and go away on close.
   - APP page labels read 1:4…4:4.
2. **Freq. → station list:**
   - MFK scrolls, VOL scrolls fast, a VOL press shows the schedule.
   - *Tune* sets USB and dial = frequency − 1.5 kHz, and the button shows the call.
   - Turning the VFO turns the button back to kHz.
3. **Mic keys with WeFax open:** UP/DOWN step through stations, V/M opens and closes the list. With the frequency locked (SPCH), UP/DOWN do nothing. **With WeFax closed:** UP/DOWN step the frequency as before; long press still changes band.
4. **Manual RX on a live fax** (e.g. DWD 7880 kHz, NOAA, or Kyodo):
   - the picture draws
   - *Align* and *Tilt* work with the MFK
   - *View Size* toggles
   - *Save and Clear* writes `/mnt/wefax/wefax-….png`.
5. **Auto RX through a whole transmission:** it starts on the APT tone, saves at the stop tone and waits for the next one.
6. **Fax Browser:** open a saved PNG, *Fix Align* / *Fix Tilt*, then *Done*. The file is rewritten.
7. **Auto Slant on/off,** then reopen WeFax: the setting and the tilt per station are kept (`/mnt/wefax/slant.txt`).
8. **Close WeFax** in the middle of a fax, during a save, and with the list or Info open: no crash, and the markers are gone.
9. **Open another app (FT8, RTTY) after WeFax:** no WeFax markers left, and the audio is fine.
10. **Memory:** `free` before and after a long Auto RX session.

## Open questions

- **Georgy (gdyuldin):** the mic UP/DOWN and V/M hooks in `main_screen.c` (e66f2dc). Would you rather have a generic hook where an open dialog gets mic keys first? NavTex and DSC would want the same.
- **Georgy:** OK with a 4th APP page?
- **Hany:**
  - *Test* (`test_cb`, plays `/mnt/wefax/test.wav` into the decoder) is in the code but on no button in this drop. Should it get a button (FAX 4:4 has a free slot), or be removed?
  - Saved fax names use local time while the schedules are in UTC. Is that intended?
  - Your decoder uses 1500 Hz centre and 850 Hz shift (1075/1925 Hz), and tunes dial = frequency − 1.5 kHz. Many programs instead use 1900 Hz centre, ±400 Hz, dial = frequency − 1.9 kHz. That's a guess from general practice, not something we tested, so just checking it's deliberate.
- **Overlap with the Channel List, #310:** that one changes mic UP/DOWN to recall channels and adds a V/M action. Whichever is merged second gets a commit from me that combines the two hooks (WeFax first, then the channels, as in Hany's drop).

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
