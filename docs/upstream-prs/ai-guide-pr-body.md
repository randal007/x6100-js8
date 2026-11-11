**Draft, for discussion.** Most of us use an AI assistant for this firmware
now, often hams first and programmers second. This PR writes down one way of
working with AI so the radio stays safe, reviews stay manageable, and every
AI tool reads the same rules. Nothing in the GUI program itself changes.

Where it comes from: the JS8 port's daily routine with Claude Code (one
radio, every build tried on the air), shaped like
[AetherSDR's](https://github.com/aethersdr/AetherSDR) AI setup but much
smaller (see the comparison at the end).

## Split into four (Kris, 2026-11-11)

This started as one PR. As Kris suggested, the parts that aren't up for
discussion now have their own PRs and can go in on their own:

| | PR | What |
|---|---|---|
| (a) | #377 | Every unit test in CI (11, was 3) + merge checks: app cases end in their `break`, every source file is built, i2c only where it is today |
| (b) | #378 | *Build image*: test images from any branch, `buildroot_ref`, the GUI with symbols |
| (c) | **this one** | `AGENTS.md`, `docs/working-with-ai.md`, skills, PR and bug templates |
| (d) | #379 | `tools/radio/`: console, screenshot, CPU and memory log, SD card flash that keeps DATA |

**Until (a), (b) and (d) are merged, this PR's *Files changed* tab still
shows their files** (they are the same commits, the same content). Once
they're in, only the files below are left. The guide describes those parts,
so it goes in last.

## What's here

### `AGENTS.md` + `docs/working-with-ai.md` + PR template

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
  - **Merging**, **build and test**, a **code map**, **lessons from the
    code** (the two screen layers, LVGL 8.3 traps, the two PTT paths, ...)
    and **licences**.
- **`docs/working-with-ai.md`**: the same way of working for *people*: the
  loop (options → mock-up → test image → radio test → AI review → PR),
  keeping notes an AI can find, closing a session, keeping it honest, with
  the short stories behind the rules (#305, #311/#312, ...).
- **`.github/PULL_REQUEST_TEMPLATE.md`**: what it does, what the user sees,
  what it touches, which image was tested on a radio.
- README: a short *Working on the code* section linking these.
  `.gitignore`: `CLAUDE.local.md` / `AGENTS.local.md` for personal notes.

### Skills: `/new-app`, `/pr-check`, `/close-session`

Repeat jobs as checklists in `.claude/skills/`. They're Claude Code skills,
but plain Markdown that any tool or person can follow:
- **`/new-app`:** registering an app in the five shared places.
- **`/pr-check`:** before opening or merging a PR: the checks, conflicts
  with other open PRs, unanswered questions, tested or not, scope.
- **`/close-session`:** end a session with nothing left only in the chat.

### Bug report form

It now asks for the App and BASE versions (SETTINGS > Info), which image,
what's plugged in, how often it happens, the run's log file from DATA's
`app_logs` folder and a screenshot. A line for reports written with an AI:
say what you saw, and keep guesses apart. The discussions link points to
OpenX6100.

## Changed after Kris's review (11-11)

- **Licences:** most of the GUI's own files are LGPL-2.1-or-later, several
  apps are GPL-3.0-or-later (WSPR, Olivia, JTTY, JS8, ...); every file keeps
  its licence. (It said all LGPL.)
- **Matching the reference program:** doing what WSJT-X or JS8Call does is
  fine anywhere; code copied from them (GPL-3.0) only goes into a GPL-3.0
  file, with credit.
- **Build image outside OpenX6100:** run it in your own fork, after turning
  on Actions there.
- **tools/radio is Linux only** (udev, udisks); a note for Windows and WSL
  users.
- **The colour ratchet is gone** (it would have failed every new app's
  status messages; named message colours first, if we want themes).

## What I'd like from you

1. **Do the rules match how you want the project run?** In particular
   *What an AI may not decide alone* and *How a change gets in*. Anything
   to add, cut or change?
2. Anything in `AGENTS.md` that's wrong about the code, or missing (your
   own app's traps, Buildroot)?

## Compared with AetherSDR

| AetherSDR | Here |
|---|---|
| One `AGENTS.md`, pointers for each tool | Same |
| Written limits on what AI may change (looks, UX, defaults, architecture, scope) | Same, in plain words |
| AI-assisted PRs held to the same standard | Same: you answer for your PR |
| ~40 check scripts in CI | 4 (app cases, build list, tests in CI, i2c), the ones that bit us (#377) |
| Skills for repeat jobs (`/pr-review`, `/pr-land`, `/release-prep`...) | 3 small ones |
| Ratchets (counts that may only go down) | i2c (#377) |
| Accessibility check (warnings only) | Voice-mode warning for apps (#377) |
| Memory soak | GUI memory trend in `x6100-cpulog` (#379) |
| Bug form asking for version, radio, log | Same |
| Constitution, signed commits, three code-owner tiers, a triage bot, an assign-yourself rule, an MCP bridge to drive the running app | Left out: too heavy for a group our size |

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_011735W4WxdGqVBkzHYAK1C2
