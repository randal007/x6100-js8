Part 3 of 4, after #290 and #301 (both merged). This adds `src/js8`, the JS8 library that sits between the engine (js8core, 2/4) and the app (4/4), with its unit tests and a workflow that runs them. Nothing in the firmware calls it yet, so nothing changes on the radio.

### What it is

Everything JS8 does that isn't drawing the screen: it runs js8core's decoders on the radio's audio, puts frames back together into messages, and does what desktop JS8Call does with them (directed messages and commands, automatic replies, the inbox and relays, heartbeats, the QSO log, alerts and macros, time sync from decodes). For transmitting it turns text into frames and audio samples; keying the radio is the app's job (4/4). From the code: no file in `src/js8` calls LVGL or the radio (`radio_*`, X6100Control); the app talks to it through the C headers (`js8_rx.h`, `js8_tx.h`, `js8_ops.h`, `js8_history.h`, `js8_map.h`, `js8_wisdom.h`, `js8_speed.h`), the rest is C++ in `namespace x6100::js8`, built as C++20 like js8core. js8core is linked `PRIVATE`, so whatever links the library keeps its own C++ standard.

### Four commits

1. **`src/js8`, unchanged** from the x6100-js8 releases (63 files, 10,463 lines).
2. **The tests and the map data, unchanged**: `tests/test_js8.cpp`, the licence line for `tests/test_js8_db.cpp`, `tests/CMakeLists.txt`, `tools/map_data`.
3. **`add_subdirectory(js8)` in `src/CMakeLists.txt`**, so the image build compiles the library with GCC 12 already now. Nothing links it until 4/4, so `x6100_gui` is unchanged.
4. **`.github/workflows/tests.yml`**: builds and runs `test_js8`, `test_js8_db` and `test_tx_level` (#306's, which had no workflow yet) on a PC.

Commits 1 and 2 are the releases' files as they are (randal007/x6100-js8 `main`, Release Candidate 1.1); 3 and 4 are the only lines written for this PR.

### Files

| Part | Files | Lines |
|---|---|---|
| Receive: `receiver`, `js8_rx`, `resampler`, `render`, `classify`, `assembler`, `directed`, `timesync`, `cpu_fence`, `wisdom`, `js8_wisdom.h`, `speeds`, `js8_speed.h` | 24 | 3,352 |
| Transmit: `tx`, `js8_tx`, `wav`, `testsignal` (test signals, for the tests only) | 8 | 929 |
| Operating: `js8_ops` (the app's C interface), `commands`, `autoreply`, `inbox`, `stations`, `alerts`, `macros`, `qsolog`, `datafile`, `strutil.hpp` | 19 | 3,555 |
| Station history (SQLite): `history`, `js8_history.h` | 3 | 1,137 |
| Map and places: `geo`, `callsign_place`, `js8_map`, `map_render` | 8 | 1,448 |
| `src/js8/CMakeLists.txt` (libraries `JS8` and `JS8TestSignal`) | 1 | 42 |
| `tests/test_js8.cpp` | 1 | 4,621 |
| `tests/test_js8_db.cpp` (licence line only), `tests/CMakeLists.txt` | 2 | +2, +8 |
| `tools/map_data`: `js8_map.bin` (1.0 MB), `make_map_data.py`, `map_bench.cpp`, `README.md`, `.gitignore` | 5 | 238 + the .bin |
| `src/CMakeLists.txt`, `.github/workflows/tests.yml` | 2 | +1, +45 |

### Outside its own files

- **`src/CMakeLists.txt`**: one line, `add_subdirectory(js8)`. The library and `JS8TestSignal` are then compiled in the image build but not linked into `x6100_gui`. If you'd rather the image build not compile anything until the app is there, I'll move the line to 4/4.
- **`tests/CMakeLists.txt`**: `test_js8` (it brings in `third-party/js8core` and `src/js8` for the test build). `test_js8_db` is there since #290.
- **`.github/workflows/tests.yml`**: new. There is no test workflow on OpenX6100 yet, so I kept it to the two JS8 test binaries and #306's `test_tx_level` on Ubuntu 24.04 (js8core needs Boost ≥ 1.78). It runs on pull requests, pushes to main and by hand.
- **`tools/map_data`**: the base map the library's `map_render.cpp` reads (Natural Earth, public domain, 110m and 50m), with the script that makes it, so the map tests run here. Installing it into the image (`/usr/share/x6100/js8/js8_map.bin`) comes with the app in 4/4.

Nothing else changes: no shared code, no settings, no stored numbers.

### Licences

Each file says its licence in its first lines (`SPDX-License-Identifier`), unchanged from the releases:

- **GPL-3.0-or-later**, the files that follow desktop JS8Call's code (46 in `src/js8`): `alerts`, `assembler`, `autoreply`, `commands`, `cpu_fence`, `datafile`, `directed`, `history`, `inbox`, `js8_ops`, `js8_rx`, `js8_speed.h`, `js8_tx`, `js8_wisdom.h`, `macros`, `qsolog`, `receiver`, `render`, `speeds`, `stations`, `testsignal`, `timesync`, `tx`, `wisdom`, and `tests/test_js8.cpp`.
- **LGPL-2.1-or-later**, like the rest of the GUI, the files with nothing from desktop JS8Call (16 in `src/js8`): `callsign_place`, `classify`, `geo`, `js8_history.h`, `js8_map`, `map_render`, `resampler`, `strutil.hpp`, `wav`, and `tests/test_js8_db.cpp` (its SPDX line is the only change to that file).

That is the split the README's *License* section from 2/4 allows: the GUI program with JS8 is GPL-3.0 as a whole.

### Testing

- **Unit tests on a PC** (2026-10-16, the same files; Ubuntu 24.04, GCC 13.3, ASan/UBSan, as in the workflow): `test_js8 "~[slow]"` (55 s): 131 test cases, 129 passed, 2 skipped (the "unreadable file" checks skip themselves when run as root, as here; GitHub's runners don't run them as root), 39,537 assertions. `test_js8 "[slow]"` (the 3 `[slow]` and the 5 hidden `[.slow]` decode tests, left out of the workflow, 11.5 minutes under the sanitizers): 8 test cases, all passed (97 assertions). `test_js8_db`: passed (112 assertions). Against `third-party/cty/cty.dat` as it is on main (VER20240202).
- **Frames:** the `[desktop]` tests in `test_js8` compare our frames, bit for bit, with frames made by desktop JS8Call-improved's own `Varicode::buildMessageFrames()` (built with Qt), plain and compound calls, and read desktop's back.
- **The image's GCC 12:** all 29 `src/js8/*.cpp` compile with the image's toolchain, no errors (checked 2026-10-18 on this branch).
- The same files are what runs on testers' radios in the x6100-js8 releases (Release Candidate 1.1), on the air every day. The workflow runs on this PR, so its result is below.

### Things I'd like your opinion on

- **The `add_subdirectory(js8)` line** now (compile check in the image build) or in 4/4 with the app?
- **The workflow**: a new `tests.yml` with the JS8 tests and `test_tx_level`. Happy to rename it, or to add `test_qth` and others to it in a separate PR. (`test_cfg` and `test_ft8_qso` didn't build on a PC when we tried in x6100-js8: they link the LVGL fork, which is NEON-only, or X6100Control, which wants libgpiod 1.x.)
- **`tools/map_data/js8_map.bin`** is a 1 MB binary in git, made by `make_map_data.py` from a pinned Natural Earth commit, so it can be rebuilt and compared. Its README mentions a plan document (`docs/MAP_PLAN.md`) that is only in x6100-js8; I can drop that line.

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
