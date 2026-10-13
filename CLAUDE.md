# X6100 JS8: start here

Read this before doing anything in this repository. It holds what doesn't
change much: the project, the rules, how we work, where things are. What's
going on right now is in **[docs/PLAN.md](docs/PLAN.md) ("Now")** and
`git log`: read both before reporting status or starting work.

## The project

Native JS8 (JS8Call's mode) for the Xiegu X6100, built into the radio's
firmware GUI. Repository **randal007/x6100-js8**, a fork of the community
firmware **OpenX6100/x6100_gui** (remote `upstream`; it was
gdyuldin/x6100_gui, moved to the OpenX6100 org 2026-10-10). Releases are
prerelease betas (`js8-beta5`, ...) with an SD card image on GitHub.

- The **README is the user manual**. Issues = bug reports, Discussions =
  feature requests.
- The owner and tester is **Randal, VE7NHW** (grid CN89), who tests every
  build on the radio. How to work with Randal is in `CLAUDE.local.md`.
- Randal is a maintainer at OpenX6100 too. JS8 is going upstream there in four
  pull requests ([docs/upstream-patches.md](docs/upstream-patches.md)).

## Rules (each one learned the hard way)

1. **Nothing transmits on its own** when an app opens or starts. Ever.
2. **JS8 must behave exactly like desktop JS8Call-improved.** Before
   changing behaviour, read desktop's source (clone it into a scratch
   folder) and do what it does. Our frames must be bit-identical to
   desktop's; tests check this.
3. **Keep changes inside JS8.** Shared GUI code (main screen, buttons,
   keyboard, power) is OpenX6100's: flag any change there, prefer a
   JS8-only fix, and name the layer when explaining.
4. **Never poll i2c-0** (`/dev/rtc1` or anything else on it) in a loop: it
   is the GUI's control bus to the base board and once glitched the
   XPA125B amplifier's band voltage. One write is fine.
5. **Hands off during flashes and reboots.** If the radio's console shows
   a reboot, a login prompt you didn't expect, or the stock
   `XIEGU-x6100 login:`, send nothing (not even Enter) and ask.
6. **The radio is live hardware** with an amplifier in line. Tell Randal
   before any command that touches hardware; only with the radio idle.
   Don't re-break a working app to test tooling.
7. **Only flash the SD card when Randal says so.** The DATA partition
   (Randal's inbox, logs, settings) is always backed up first and kept.
8. **Decided is decided:** docs/PLAN.md "Decided: not doing" lists ideas
   that were dropped. Don't bring them back up.
9. **Never claim done without checking the code**, and say where each
   status claim comes from (PLAN.md, git log, a CI run).

## How we work

- **Small fixes and docs: straight on `main`.** Anything bigger (a feature,
  a refactor, anything to try on the radio first): a short-lived branch
  with a plain name without `/` (*Build image* names its artifact after
  it), a test image, radio test, then a PR into `main` (self-merge OK) and
  delete the branch. One or two branches at a time; **always tell Randal
  which branch you're on.**
- **docs/PLAN.md is the one work list** and the only place for "where
  are we" (not Claude's memory, which keeps lessons only). Tick items in
  the same commit as the work. README *Coming next* mirrors its top.
- **Commit and push every change.** End commit messages with the
  Co-Authored-By line.
- **OpenX6100 (upstream):** always branch → PR → review by gdyuldin or
  myshak1, never merged by us alone. After merging upstream into ours,
  check every `case ACTION_APP_*` block in `src/main_screen.c` (a lost
  `break` once opened two apps at once) and stored numbers
  ([docs/UPSTREAM_README.md](docs/UPSTREAM_README.md)).
- **js8core bugs** (the engine, `third-party/js8core`) go in
  [docs/js8core-bug-reports.md](docs/js8core-bug-reports.md) and
  `third-party/js8core/UPSTREAM.md` (our local patches, numbered).
- **Releases:** follow [docs/releases/CHECKLIST.md](docs/releases/CHECKLIST.md).
  Step 1 is the version string `JS8_APP_VERSION` at the top of
  `src/dialog_js8.c`: it goes out on the air in every STATUS answer.
- **End of a session:** when Randal says **"offload"**, "closing down
  the session" or similar, run the checklist in
  [Closing a session](#closing-a-session-offload) below, all of it,
  without asking each step.

## Closing a session ("offload")

Randal's word for it is **offload** (or "closing down the session"). Do
every step, then report. Nothing may live only in this conversation.

1. **Running work:** check every CI run, background job and test started
   this session. Finish what can be finished now (e.g. a release whose
   build just published: its CHECKLIST steps). What can't, goes into
   docs/PLAN.md as a "Next" with the run ID and the exact command.
2. **docs/PLAN.md:** tick what's done, update each item touched this
   session (state, decisions Randal made, what's waiting on whom), and
   new items for anything found or promised. "Decided: not doing" for
   anything Randal dropped. Move ticked items (whole, with any leftover
   moved to a new open item) to the top of docs/PLAN_DONE.md, and
   rewrite the five-line *In short* at the top of *Now*.
3. **README (the manual):** the summary at the top, "New in ...",
   "Known issues in ..." (drop the ones no longer true), "Coming next"
   (mirrors PLAN.md), and any section whose behaviour changed.
4. **Other docs:** release notes in docs/releases/ if a release is in
   progress; docs/upstream-patches.md for every OpenX6100 PR or issue
   opened, merged or answered; docs/js8core-bug-reports.md and
   third-party/js8core/UPSTREAM.md for engine changes.
5. **GitHub:** everything committed and pushed (`git status` clean on
   every branch used); results posted on our open PRs and issues;
   branches merged or deleted as agreed. Say which branch is current.
6. **The radio:** say what's on the SD card (build, commit) and the
   newest DATA backup folder; nothing left running on it that matters
   (a btmon in /tmp dies at the next restart, fine).
7. **Claude's memory** (on this PC): any new lesson or preference of
   Randal's, and a short session entry in the history log. No copy of
   the status (that's PLAN.md); keep the MEMORY.md index line current.
8. **Report to Randal** in plain words: what was done, what's still
   running or waiting (and on whom), and anything he has to do himself
   (e.g. a merge or a post Claude isn't allowed to make).

## Code map

| Where | What |
|---|---|
| `src/dialog_js8.c` | The JS8 app (~9,000 lines, C, LVGL 8.3). `JS8_APP_VERSION` at the top |
| `src/js8/` | JS8 library (C++): receiver, decode threads, messages; text helpers in `strutil.hpp` |
| `third-party/js8core/` | The engine: JS8Call-improved's Android port + our local patches (UPSTREAM.md) |
| `src/js8_wf.c` | JS8's waterfall, drawn on the lower DRM plane |
| `src/js8_clock.c` | GPS clock, time drift |
| `src/tx_player.c`, `src/tx_level.c` | Shared transmit audio and level |
| `src/radio.c` | Radio helpers (TX/RX filter, DSP off, speaker) |
| `src/cfg/js8_db.c` | JS8 settings (`cfg.js8.*`), outside upstream's migrations |
| `src/keyboard.c`, `src/kbd_rollover.c` | Keyboards (USB and Bluetooth) |
| `src/main_screen.c` | App registrations (`ACTION_APP_*`): shared code |
| `tests/test_js8.cpp`, `test_js8_db.cpp`, `test_qth.cpp` | Unit tests (Catch2) |
| `tools/js8_ui_harness/` | The real JS8 app headless on a PC, scenarios in real time |
| `.github/workflows/` | `main.yml` = *Build image*, `test-image.yml` = other branches' images, `tests.yml` = tests |

While JS8 is open it sets the RX filter 200–3000 Hz (restored on close),
the TX filter 160–3000, NR/NB/DNF off, power capped at 5 W. Files on the
DATA partition: `js8call_log.adi`, `js8_inbox.txt`, `js8_held.txt`,
`js8_texts.txt`, `js8_saved.txt`, `js8_history.db` (written via `.tmp` +
rename).

## Build and test

- **The image** is built only on GitHub (about an hour):
  `gh workflow run 366471886 -R randal007/x6100-js8 --ref <branch or tag>`.
  To try an open OpenX6100 Buildroot PR with it, add
  `-f buildroot_ref=<its branch or commit>` (never on a release tag: it
  refuses anything but master).
  `dialog_js8.c` can't be compiled for ARM locally; the harness build is
  the real compile check.
- **Unit tests** (as CI does):
  `cmake -B build_test -DENABLE_TESTING=YES && cmake --build build_test -j --target test_js8 test_qth`,
  then `build_test/tests/test_js8 "~[slow]"` (fast ones) and
  `build_test/tests/test_qth`. Catch2 splits test names at commas:
  escape `\,`.
- **UI harness:** see [tools/js8_ui_harness/README.md](tools/js8_ui_harness/README.md).
  `ONLY_<NAME>=1` runs one scenario; audio is fed in real time, so a full
  run takes ~45 min even split across 4 build dirs. Full CI run:
  `gh workflow run tests.yml -R randal007/x6100-js8 --ref <branch> -f harness=true`.
- **Getting a CI image:** `gh run download` hangs; use the artifact API
  (`gh api repos/randal007/x6100-js8/actions/runs/<id>/artifacts`, then
  `.../actions/artifacts/<artifact id>/zip`).

## Lessons worth knowing before touching the code

- LVGL 8.3: an indev reset turns a held key into a long press in the next
  focus; the keypad drops a press while another key is down; an encoder
  never sends LONG_PRESSED to an editable object (use
  LONG_PRESSED_REPEAT); `lv_img` never covers and invalidate adds 5 px
  (whole-dialog redraws: use an opaque box and exact invalidate).
- Waterfall rows are paced by CLOCK_MONOTONIC µs, never an lv_timer
  period. Randal is sensitive to any flicker or stutter.
- The base board loops app audio played on the speaker back into capture:
  JS8 is fed silence then (beep_guard).
- GitHub forks can have a tag and a branch with the same name: pass SHAs.
- Two ways to key the transmitter: `radio_set_ptt()` (CAT PTT, CI-V 0x1C)
  takes the radio's mic/line input; `radio_set_modem()` takes the codec,
  where app audio goes (FT8, JS8, voice messages, the BT headset).
- Bluetooth state on the radio: `/mnt/bt_*.log` survive a reboot; to see
  who changes the adapter, `btmon -w` + `dbus-monitor --system` on the
  console. A udev `KERNEL=="hci*"` rule also matches every connection
  (`hci0:<handle>`): use `ENV{DEVTYPE}=="host"`.

## More

[docs/README.md](docs/README.md) indexes every plan, review and research
note. Machine-specific things (the radio's USB console, flashing the card,
local folders) are in `CLAUDE.local.md` on Randal's PC.
