Part 4 of 4, after #290, #301 (merged) and 3/4 (#307, open: this branch sits on it, so only this PR's own commits show here; it can only be merged after 3/4). This adds the JS8 window and its button in the APP menu. After this one, JS8 is in the firmware.

### What it adds

A JS8 app as on desktop JS8Call-improved. It shows the decoded messages over its own waterfall, with a map of the stations heard, an Inbox, relays, queries and APRS, and the QSO log. It sends at all four speeds. Under it are the library (3/4) and the engine (2/4). This PR has the screen, the buttons and keying the radio.

While it's open (from the code): RX filter 200–3000 Hz, TX filter 160–3000 Hz (radio only), NR / NB / notch off (radio only, settings kept), power capped at 5 W while keyed, USB-D. All of it is restored on close. The radio helpers for this came in #290.

**Nothing transmits when it opens.** From the code: `construct_cb()` (`dialog_js8.c`) switches AUTO, heartbeats, HB ACK and auto CQ off every time it opens. The only path to PTT is `tx_play()` → `tx_player_play()` (#306) → `radio_set_modem(true)`, and it runs only for a message queued with `js8_tx_send()`. With AUTO off, an answer is only offered, never sent (`AutoPolicy::decide()`, 3/4).

### Three commits

1. **The app and its modules, unchanged** from the x6100-js8 releases (Release Candidate 1.1): `dialog_js8.c/.h`, `js8_wf.*`, `js8_clock.*`, `fonts/js8_marks_24.c`. Transmitting goes through `tx_player` and `tx_level`, which are in main since #306, so this PR has no copy of them.
2. **The button, the case and the build** (shared code, below).
3. **The version JS8 sends**: `<MYVERSION>` and the default STATUS answer say which firmware answered. In OpenX6100 that is now `"OpenX6100 JS8 " VERSION` (`main.h`), so today **"OpenX6100 JS8 v1.0.2"**, and it follows each OpenX6100 release by itself. (The x6100-js8 releases send "X6100 JS8 <release>", so the two can be told apart on the air.) This is the only change to `dialog_js8.c`: one line and an `#include "main.h"`.

### Files

| File | Lines | What |
|---|---|---|
| `src/dialog_js8.c` | 9,432 | the app |
| `src/dialog_js8.h` | 24 | |
| `src/js8_wf.c/.h` | 204 + 50 | its waterfall, drawn on the display's lower plane, seen through a hole in the dialog |
| `src/js8_clock.c/.h` | 22 + 22 | the GPS clock and time drift |
| `src/fonts/js8_marks_24.c` | 142 | the end-of-message mark ♢ and the degree sign, which `sony_24` doesn't have |
| shared code (below) | +18 −2 | |

### Outside its own files (shared GUI code)

- **`src/buttons.cpp`** (APP menu): page 3 is WiFi, Bluetooth, an empty slot, JS8. JS8 is the last button, where testers know it. The empty slot is for WSPR (#271). Once WSPR, PSK31 and Olivia are in, I'll rebase this onto whatever layout they bring.
- **`src/settings_types.h`**: `ACTION_APP_JS8 = 112`, as agreed in #283. It's opened from the APP menu only, with no long-press entry, so the number is never stored.
- **`src/main_screen.c`**: the `#include`, the window's `case` (with its `break`) and the action next to the other apps'.
- **`src/CMakeLists.txt`**: the app's three `.c` files, and `JS8` (the library from 3/4) linked into `x6100_gui`.
- **`CMakeLists.txt`**: installs the base map `tools/map_data/js8_map.bin` (from 3/4) to `/usr/share/x6100/js8/`.

Transmit level: JS8 uses #306's `tx_player` as it is, with its one stored level for every digital mode (`cfg.ft8.output_gain_offset`), and passes `tx_player_base_gain_offset()` as its base.

From building it (below): the app needs nothing else from main. The radio helpers, keypad hold time, `js8_db` and the waterfall ring buffer came in #290, and every header it uses is main's as it is.

**One cty.dat:** JS8 reads `third-party/cty/cty.dat` from #282 (`/usr/share/x6100/cty.dat`, `/mnt/cty.dat` wins), the same file as FT8's countries. It has no copy of its own. We use a newer Big CTY (VER20260915) in x6100-js8; I can send it as a separate PR if you want it.

**Files on the SD card** (from the code, all on `/mnt`): `js8_texts.txt`, `js8_inbox.txt`, `js8_held.txt`, `js8_saved.txt`, `js8call_log.adi` (desktop's name), `js8_history.db`, `js8_wisdom.dat` (the decoder's FFT plans). The text files are rewritten via `.tmp` + rename, the ADIF log is appended to, and `js8_history.db` is SQLite. It reads `qso_log.db` (read-only) for new grids and countries on the map.

### Licences

Each file says its licence in its first lines (`SPDX-License-Identifier`), unchanged from the releases:

- **GPL-3.0-or-later**, following desktop JS8Call's code: `dialog_js8.c/.h`, `js8_wf.c/.h` (and `tests/test_js8.cpp`, from 3/4).
- **LGPL-2.1-or-later**, like the rest of the GUI: `js8_clock.*` and the shared-code lines above.
- **`fonts/js8_marks_24.c`** has no SPDX line, like the other font files. It's two glyphs converted with lv_font_conv from Adwaita Sans and Adwaita Mono (SIL Open Font License 1.1). Its header says so.

As the README's *License* section from #301 says, the GUI program with JS8 is GPL-3.0 as a whole.

### Testing

- **The image's GCC 12:** `js8_wf.c`, `js8_clock.c`, `main_screen.c`, `buttons.cpp` and the marks font compile with the image's toolchain on a PC (2026-10-18, this branch). `dialog_js8.c` needs gpsd's headers, which only the image build has: **test image [run 39139987043](https://github.com/randal007/x6100-js8/actions/runs/39139987043) (building)** (this branch with Buildroot master, which has #8).
- **The app cases** in `main_screen.c` each end in their `break` (checked with an awk script, since a merge once dropped one).
- **Compiled and run on a PC** in our UI harness (randal007/x6100-js8 `tools/js8_ui_harness`), 2026-10-17, with the same `dialog_js8.c` apart from the version line. It runs the real `dialog_js8.c` and `js8_wf.c` headless on stock LVGL 8.3.11, with only the radio and the main screen stubbed. Its main scenario passed (about 5 minutes, no sanitizer reports): it opened listening, with nothing keyed until the scenario sent a message, decoded all 7 of the band's messages with their end marks, sent a message and stopped one with ESC mid-frame, sent heartbeats on a free offset, changed band and closed with ESC.
- **Unit tests:** 3/4's workflow runs `test_js8`, `test_js8_db` and `test_tx_level` on this branch too.
- **On the radio:** not yet: I will try that test image on my radio and add the result here.

### Things to know, and questions

- **The knob callback:** this is built against main as it is, `rotary_cb(int32_t diff)`. If #300 (apps' `rotary_cb(diff, dt)`) goes in first, JS8 gets the second argument. Its own 30 ms click count can then give way to `dt`, as FT8's does in #300.
- **The APP page 3 layout** (WiFi, Bluetooth, WSPR, JS8): is that the order you want once Kris's apps are in? A 4th page if they don't fit?
- **The `add_subdirectory(js8)` line** stays where 3/4 put it (`src/CMakeLists.txt`). This PR only links the library.
- **The version string** (commit 3): "OpenX6100 JS8 v1.0.2" today. Fine as it is, or would you rather it say something else?

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
