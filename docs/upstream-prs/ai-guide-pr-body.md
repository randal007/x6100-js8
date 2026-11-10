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
| ~40 check scripts in CI | 2, the ones that bit us |
| Skills for repeat jobs (`/pr-review`, `/pr-land`, `/release-prep`...) | 3 small ones |
| Constitution, signed commits, three code-owner tiers, a triage bot, an assign-yourself rule, an MCP bridge to drive the running app | Left out: too heavy for a group our size |

🤖 Generated with [Claude Code](https://claude.com/claude-code)
