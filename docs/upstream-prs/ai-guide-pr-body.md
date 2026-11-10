**Draft, for discussion.** Most of us use an AI assistant for this firmware
now, often hams first and programmers second. This PR writes down one way of
working with AI so the radio stays safe, reviews stay manageable, and every
AI tool reads the same rules. It's big because it's five things in one PR (so
it's one discussion); each one is its own commit, so you can read them one at
a time. Nothing in the GUI program itself changes.

Where it comes from: the JS8 port's daily routine with Claude Code (one
radio, every build tried on the air), shaped like
[AetherSDR's](https://github.com/aethersdr/AetherSDR) AI setup but much
smaller (see the comparison at the end).

## The five parts

### 1. `AGENTS.md` + `docs/working-with-ai.md` + PR template (30adf93f)

- **`AGENTS.md`**: the guide any AI tool reads by itself. `CLAUDE.md`,
  `GEMINI.md` and `.github/copilot-instructions.md` just point to it. It
  has:
  - **Safety on a live radio:** nothing transmits on its own, never poll
    i2c-0, hands off during flashes, back up DATA, ...
  - **What an AI may not decide alone:** looks, behaviour, defaults,
    structure, scope. It gives options with a recommendation; the person
    picks; the maintainers decide for the project.
  - **How a change gets in:** branch, test image, radio test, PR, review,
    and read the last comments before merging.
  - **Adding an app:** the five shared places, the name lines from #313, no
    empty APP buttons.
  - **Merging**, **build and test**, a **code map**, and **lessons from the
    code** (the two screen layers, LVGL 8.3 traps, the two PTT paths, ...).
- **`docs/working-with-ai.md`**: the same way of working for *people*: the
  loop (options → mock-up → test image → radio test → AI review → PR),
  keeping notes an AI can find, closing a session, keeping it honest, with
  the short stories behind the rules (#305, #311/#312, ...).
- **`.github/PULL_REQUEST_TEMPLATE.md`**: what it does, what the user sees,
  what it touches, which image was tested on a radio.
- README: a short *Working on the code* section linking these.
  `.gitignore`: `CLAUDE.local.md` / `AGENTS.local.md` for personal notes.

### 2. Merge checks on every PR (73fe5567)

Two mistakes that happened in real merges, compile fine and broke things
later, now caught in seconds:

- `tools/checks/check_app_cases.py`: every `ACTION_APP_*` window case in
  `main_screen.c` ends in its `break;` (a lost one opened two apps at once).
- `tools/checks/check_build_list.py`: every `.c`/`.cpp` in `src/` is in
  `src/CMakeLists.txt`, and with `--base` nothing the target branch builds
  was dropped (`tx_level.c`/`tx_player.c` were lost that way in #311/#312;
  only the image build showed it).
- `.github/workflows/checks.yml` runs both on every PR and push to main.
  Checked: main is clean; a copy without JS8's `break` and the old tree
  without the two files are both caught. Three files in `src/` are
  deliberately not built (`goertzel.c`, `mouse.c`, `mouse_cursor_icon.c`,
  R1CBU's, unused since 2023): listed in the script with the reason.

### 3. *Build image*: test images from any branch (ef34a68f)

Today *Build image* makes an image only for a tag's release (its upload step
is commented out). With this change:

- run by hand on **any branch**, it keeps the image as a **download on the
  run's page for 30 days**, so a change can be tried on a radio before its
  PR;
- `buildroot_ref`: build with an open AetherX6100Buildroot PR's branch (a
  tag still has to use master);
- it also keeps `x6100_gui-unstripped.<branch>`, the same GUI with its
  symbols, for reading a core dump from the radio.

Tag builds and the Release step are unchanged. This is the step we've used
in our fork since October for every test package.

### 4. `tools/radio/`: tools for a test radio (f8711c6f)

Helpers from the JS8 work, cleaned of one PC's paths, for a Linux PC on the
radio's DEV USB-C port:

| Tool | What it does |
|---|---|
| `x6100-console` | Logs the console in the background and runs commands. It refuses to type while the radio boots or shows the stock Xiegu login |
| `x6100-screenshot` | Saves the radio's screen as a PNG |
| `x6100-cpulog` | Logs each core and each GUI thread once a second, and prints a report. Never touches i2c |
| `x6100-flash` | Writes an image to the SD card and keeps DATA: backs it up, writes only the `.img` from a zip, checks DATA afterwards |
| `x6100-syncheck` | ARM compile check of single files |
| udev rule | Lets your user open the radio's serial ports |

Tested here: the console against a fake radio on a pseudo-terminal (commands
work; the stock-login guard refuses); the flash script's image checks on a
real image (layout and zip handling). The card part hasn't been run with the
new card detection yet; I'll run it up to the "Write?" question before this
leaves draft.

### 5. Skills: `/new-app`, `/pr-check`, `/close-session` (3d231a52)

Repeat jobs as checklists in `.claude/skills/`. They're Claude Code skills,
but plain Markdown that any tool or person can follow:
- **`/new-app`:** registering an app in the five shared places.
- **`/pr-check`:** before opening or merging a PR: the checks, conflicts
  with other open PRs, unanswered questions, tested or not, scope.
- **`/close-session`:** end a session with nothing left only in the chat.

## Added the same day: more from AetherSDR

### 6. Every unit test runs on every PR (dc5c0929, 187c8cc5, 63ebf760, 94cb6863)

CI ran **3 of the 11** unit tests (JS8, its settings, the TX level). QTH,
ITU region, ADIF records, the digital TX queue, FT8 QSO, CI-V, the
settings store and the CW decoder were never run by CI.

- A second job runs those eight on Ubuntu 22.04, the only one with the
  libgpiod 1.6 that X6100Control's GPIO code uses. JS8 stays on 24.04,
  for Boost.
- The job builds ft8lib, SQLite 3.40.1 and liquid-dsp 1.4.0 at the
  versions the image uses. Ubuntu's own copies are too old: no RIGHT JOIN
  in SQLite, and no `LIQUID_OK` or `liquid_hann` in liquid-dsp.
- `tools/checks/check_tests_in_ci.py` fails a test that is added to
  `tests/CMakeLists.txt` but not run by CI.
- **Found on the first run:** `test_cfg` still expected the FT8 protocol
  setting to stop at FT4. FT2 (76d98ce2) widened it to FT8..FT2, so the
  test was out of date, not the code; it's fixed in 94cb6863.
- Now all 11 pass (run 42917671229).

### 7. Ratchets (bb22b8b4)

AetherSDR's main trick: count a risky pattern per file, keep the counts in
the repo (`tools/checks/ratchets.json`), and let them only go down. Old code
is left alone, but new code can't add more.

- **i2c:** `/dev/rtc1` and i2c-0 only in the two places that write the
  clock once today (`js8_clock.c`, `settings_page_general.cpp`).
- **Colours:** there are 249 colours typed into 37 files outside
  `styles.c`. Freezing the count keeps themes possible later.
  **@gdyuldin, your call:** keep it, or drop it?

Raising a number is the reviewers' decision. Checked: a new `rtc1` call or
a new `lv_color_hex()` in an app fails the check.

### 8. Voice mode (63d1fd10)

With the radio's voice mode, each app says its window's name as it opens
(`voice_say_text_fmt`), so a blind operator knows what opened. The app
check now **warns** (never fails) when an app's case doesn't say its name,
like AetherSDR's accessibility check. Today's one warning is Channels: its
window opens without saying so. That's for its author to look at.

### 9. Memory in the CPU logger (063a2296)

`x6100-cpulog` now also records the GUI's own memory every second, and
the report gives its trend in MB per hour. Left running for hours, it
shows a slow leak. Checked on made-up data so far, not yet on a radio.

### 10. Bug report form (ed47be07)

The form now asks for:
- the App and BASE versions (SETTINGS > Info)
- which image
- what's plugged in
- how often it happens
- the run's log file from DATA's `app_logs` folder, and a screenshot

There's also a line for reports written with an AI: say what you saw,
and keep guesses apart. The discussions link now points to OpenX6100.

### A question, not code: required checks

AetherSDR doesn't let a PR merge until its checks pass and someone has
approved it. Should OpenX6100's main require **Merge checks** and **Unit
tests** to pass, plus one approval? That's a repository setting, so it's
Georgy's decision.

## What I'd like from you

1. **Do the rules match how you want the project run?** In particular
   *What an AI may not decide alone* and *How a change gets in*. Anything
   to add, cut or change?
2. **Parts 2–4 touch CI and add tools.** Fine to have here, or would you
   rather have some of them as separate PRs?
3. Anything in `AGENTS.md` that's wrong about the code, or missing (your
   own app's traps, Buildroot)?

## Compared with AetherSDR

| AetherSDR | Here |
|---|---|
| One `AGENTS.md`, pointers for each tool | Same |
| Written limits on what AI may change (looks, UX, defaults, architecture, scope) | Same, in plain words |
| AI-assisted PRs held to the same standard | Same: you answer for your PR |
| ~40 check scripts in CI | 4 (app cases, build list, tests in CI, ratchets), the ones that bit us |
| Skills for repeat jobs (`/pr-review`, `/pr-land`, `/release-prep`...) | 3 small ones |
| Ratchets (counts that may only go down) | i2c, typed-in colours |
| Accessibility check (warnings only) | Voice-mode warning for apps |
| Memory soak | GUI memory trend in `x6100-cpulog` |
| Bug form asking for version, radio, log | Same |
| Constitution, signed commits, three code-owner tiers, a triage bot, an assign-yourself rule, an MCP bridge to drive the running app | Left out: too heavy for a group our size |

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_011735W6NuFdQLZvSdEFoB4q
