Part (a) of the AI guide (#373), split off as Kris suggested: the unit tests
in CI and the merge checks. Nothing in the GUI program changes, except one
stale test (`test_cfg`, below). This one can go in on its own; #373 (the
guide itself) only refers to it.

## Every unit test runs on every PR

CI ran **3 of the 11** unit tests (JS8, its settings, the TX level). QTH,
ITU region, ADIF records, the digital TX queue, FT8 QSO, CI-V, the settings
store and the CW decoder were never run by CI.

- A second job runs those eight on Ubuntu 22.04, the only one with the
  libgpiod 1.6 that X6100Control's GPIO code uses. JS8 stays on 24.04, for
  Boost.
- The job builds ft8lib, SQLite 3.40.1 and liquid-dsp 1.4.0 at the versions
  the image uses, with sha256 checks on the downloads (the same sums as
  Buildroot's hash files). Ubuntu's own copies are too old: no RIGHT JOIN in
  SQLite, and no `LIQUID_OK` or `liquid_hann` in liquid-dsp.
- `tools/checks/check_tests_in_ci.py` fails a test that is added to
  `tests/CMakeLists.txt` but not run by a CI step. Kris's #374 (`test_olivia`)
  is the first one it would catch.
- **Found on the first run:** `test_cfg` still expected the FT8 protocol
  setting to stop at FT4. FT2 (76d98ce2) widened it to FT8..FT2, so the test
  was out of date, not the code (fixed here).

## Merge checks (`.github/workflows/checks.yml`, seconds)

Mistakes that happened in real merges, compiled fine and broke things later:

- `check_app_cases.py`: every `ACTION_APP_*` window case in `main_screen.c`
  ends in its `break;` (a lost one opened two apps at once), and a case with
  no code of its own fails too. It also **warns** (never fails) when an app's
  window doesn't say its name for the voice mode, so a blind operator knows
  what opened. Today's one warning is Channels.
- `check_build_list.py`: every `.c`/`.cpp` in `src/` is in
  `src/CMakeLists.txt` (`tx_level.c`/`tx_player.c` were dropped in
  #311/#312, and only the image build showed it). Three files are
  deliberately not built (`goertzel.c`, `mouse.c`, `mouse_cursor_icon.c`,
  unused since 2023), listed with the reason.
- `check_ratchets.py`: `/dev/rtc1` and i2c-0 only in the two places that
  write the clock once today (`js8_clock.c`, `settings_page_general.cpp`); a
  loop on that bus once glitched an amplifier's band voltage. The counts are
  in `ratchets.json` and may only go down; raising one is the reviewers'
  call. The colour ratchet is **left out** (Kris): status messages use LVGL
  recolour strings and `styles.c` has nothing named for them yet.

Checked: main passes; a copy without JS8's `break`, the old tree without
the two files, and a new `rtc1` call in an app are each caught. Kris ran
them on our open branches merged onto it: all pass.

## Two questions for @gdyuldin

- **Required checks:** should main require *Merge checks* and *Unit tests*
  to pass, plus one approval? That's a repository setting, your call.
- **`-Werror=implicit-fallthrough`** next to the existing
  `-Werror=implicit-function-declaration` would catch a missing `break` in
  every switch, not only the app cases. It changes the whole build (any
  intended fallthrough needs a mark first, and only an image build shows
  them), so I'd do it as its own PR if you want it.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_011735W4WxdGqVBkzHYAK1C2
