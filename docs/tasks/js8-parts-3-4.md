# Task: JS8 into OpenX6100, pull requests 3/4 and 4/4

Brief for Claude Code cloud sessions, which VE7NHW (Randal) starts on
claude.ai/code, **one session per part**: Part 3 first, Part 4 once Part 3's
session has pushed its branch. Read `CLAUDE.md` at the top of this repo
first. Its rules hold here too, especially 1 (nothing transmits on its own),
2 (JS8 behaves exactly like desktop JS8Call), 3 (name the layer) and 9
(check the code before claiming anything).

## What this is

JS8 goes into **OpenX6100/x6100_gui** (the community firmware this repo
forked) in four pull requests, the plan in
[docs/upstream-patches.md](../upstream-patches.md#js8-into-openx6100-the-plan)
(read its table and the *Licences* paragraph under it):

1. `js8-groundwork`: OpenX6100 #290, **merged** 2026-10-14.
2. `js8-engine`: OpenX6100 #301 (`third-party/js8core` + README licence
   note), approved by gdyuldin, Kris's three review points done
   (b040b8f); waiting for Kris and for AetherX6100Buildroot#8 (Boost and
   FFTW for the image). Not merged yet.
3. **`js8-library`** (this brief, Part 3): `src/js8` and its unit tests.
4. **`js8-app`** (Part 4): the JS8 window; after it JS8 is in the firmware.

**This repo's `main` is the source of truth**: JS8 as it runs on Randal's
radio (beta 5.2 + the FFTW wisdom fix). The PRs carry those files
**unchanged**, except where OpenX6100 main differs and a file must fit it:
each such change goes in its own commit, explained in the PR.

## Where things are (branches in randal007/x6100-js8)

| Branch | What |
|---|---|
| `main` | Our firmware. Besides JS8 it has other work (keyboards, Bluetooth, CI) that does **not** go in these PRs: take only the files named below. |
| `ox-js8-engine` | A copy of OpenX6100's `js8-engine` (PR 2/4, b040b8f) = OpenX6100 main 3b8fd33 + js8core. **The base for Part 3.** You may add `https://github.com/OpenX6100/x6100_gui` as a remote to compare with a newer main; say which commit you used. |

**You produce:** a branch per part, pushed to randal007/x6100-js8, plus a
**draft PR** in randal007/x6100-js8 whose description is the text for the
real OpenX6100 PR: Part 3 into `ox-js8-engine`, Part 4 into `js8-library`.
**Don't push or open anything in OpenX6100 or anyone else's repo:**
Randal's local session reviews the branch, compile-checks it with the
image's GCC 12, builds a test image and opens the real PR once 2/4 is
merged.

## Rules for both parts

- **Commits authored by Randal**: `--author="Randal VE7NHW <randal007@gmail.com>"`,
  with the Claude `Co-Authored-By` line from CLAUDE.md.
- **Licences (VE7NHW, 2026-10-16):** our JS8 files that follow desktop
  JS8Call's code are GPL-3.0-or-later, the rest LGPL-2.1-or-later. Keep each
  file's header as it is on `main`; the PR text lists which files are which
  (main's README *License* section says it). `tests/test_js8_db.cpp` got its
  LGPL line on our main only: carry it in Part 3.
- **Stored numbers don't move:** `MODE_JS8` = 8, `CFG_DIG_TYPE_JS8` 2 /
  `_GHOSTNET` 3 (already in OpenX6100 since #290), JS8's settings in
  `js8_db` (outside upstream's migrations). FT2 (#288, not merged) takes dig
  type 4 and migration 5: don't use either.
- **Nothing transmits by itself** (rule 1): Part 4 opens in receive; check
  it in the code and say what you found.
- **Compile and test what you can on the host.** The unit tests build on a
  PC (CI's `tests.yml` shows how). Say exactly what was and wasn't checked.
  You can't build the ARM image; `dialog_js8.c` compiles only in the UI
  harness (`tools/js8_ui_harness`, see its README) or the image.
- **The PR text:** what it adds in plain words, file list with sizes, what
  it changes outside its own files and why, the licences, how it was tested,
  open questions for gdyuldin / Kris. Mark what you read in the code as
  "from the code", guesses as guesses (posts go out under Randal's name).
  Previous PR texts to match in tone: docs/upstream-prs/9-js8-1-groundwork-pr-body.md,
  13-js8-engine-pr.md.
- Nothing in `docs/` except your part's line in *Status* below.
- Finish with a short report: branch, commits, what's checked, what's left.

## The parts

**Part 3: `js8-library`.** From `ox-js8-engine`. `src/js8/` (all of it,
including `wisdom.cpp`, the FFTW wisdom fix, and `JS8_RX_TEXT_LEN` 640, the
typing-lag fix), `tests/test_js8.cpp`, `tests/test_js8_db.cpp` (only its
LGPL line is new), the test support it needs (`tests/CMakeLists.txt`
entries, `JS8TestSignal` and friends) and a CI job that runs
`test_js8 "~[slow]"` and `test_js8_db` (OpenX6100 has its own workflows:
fit theirs, don't copy our whole `tests.yml`). It must build on its own,
with nothing in the firmware calling it yet: check whether
`src/CMakeLists.txt` should build `src/js8` in this PR or leave it to Part 4,
and say which. `test_js8` reads `third-party/cty/cty.dat` (already in
OpenX6100, #282) and `tools/map_data/js8_map.bin` (the map, Part 4):
either bring the map data in Part 3 or leave its tests for Part 4, and say
which. Size on main: ~10,000 lines + ~4,800 of tests.

**Part 4: `js8-app`.** From Part 3's `js8-library`. `src/dialog_js8.c/.h`,
`src/js8_wf.*`, `src/js8_clock.*`, `src/tx_player.*` and `src/tx_level.*`
(new files only JS8 uses; FT8 keeps its own on OpenX6100), the marks font
`src/fonts/js8_marks_24.c`, the map data and its install line, the app
button and case (`buttons.cpp`, `main_screen.c`, `settings_types.h`
`ACTION_APP_JS8` = 112, `src/CMakeLists.txt`), and what else `dialog_js8.c`
needs from main (find it by building in the harness; list each shared-code
change with its layer). Things to know:
- **APP menu:** page 3 is WiFi, Bluetooth, (WSPR), JS8, with JS8 last.
  Kris's WSPR/PSK31/Olivia aren't merged yet; put JS8 where that layout
  puts it and say it's rebased once his are in.
- **One cty.dat:** JS8 should read `third-party/cty/cty.dat` from #282
  (`/usr/share/x6100/cty.dat`, `/mnt/cty.dat` overrides), not a copy of its
  own. Our newer Big CTY (VER20260915) can be offered as a separate change.
- **The knob callback may change:** OpenX6100 #300 (open) gives apps'
  `rotary_cb` a second argument `uint16_t dt`. Build against main as it is;
  mention it in the PR.
- **Transmit level:** `tx_level` will become a shared module for every
  digital app (discussion #303) after a fix Randal is testing (the drive
  stops at 3.8 W of 5 W with the ALC at 0). Carry it as on main, and say so.
- **Part 3 left out the drive-level tests** (its commit b626f2a: −207 lines
  in `tests/test_js8.cpp`, the `#include "tx_level.h"` and one line in
  `tests/CMakeLists.txt`) because `tx_level.c` comes here: **put them back
  in Part 4**, as on main, and run them.
- Part 3's PR text asks whether `add_subdirectory(js8)` belongs in 3/4 or
  4/4; it's in 3/4 for now: leave it there.
- Not sent: the UI harness, docs, research, `x6100-bt-pair`.
Size on main: ~14,000 lines.

## Status

(each session adds one line for its part: branch, draft PR, what's checked,
what's waiting)
- **Part 3 (2026-10-16):** branch `js8-library` (3fcba74, 5 commits on ox-js8-engine b040b8f, authored VE7NHW), draft PR randal007/x6100-js8#16 into `ox-js8-engine` with the OpenX6100 text. `src/js8` and `test_js8_db.cpp` as on main 5f86ed0; map data (`tools/map_data`) brought here so the map tests run; `test_js8.cpp` minus the drive-level tests (they need `src/tx_level.c`: back in Part 4); `add_subdirectory(js8)` in `src/CMakeLists.txt` (compiled in the image, linked in Part 4); new `.github/workflows/tests.yml` (test_js8 "~[slow]" + test_js8_db). Checked on the host (GCC 13.3, ASan/UBSan): test_js8 "~[slow]" 129 passed + 2 skipped (root), "[slow]" 8 passed, test_js8_db passed. Not checked: GCC 12 / ARM image build (Randal's session), the PR's CI run. Waiting: 2/4 (#301) merged, then Randal opens the real PR.
  **Checked on Randal's PC 2026-10-16: every `src/js8` and `tools/map_data` file identical to main; all 29 `src/js8/*.cpp` compile with the image's GCC 12; CI run 38983749378: test_js8 131 test cases / 39,553 assertions and test_js8_db pass.**
