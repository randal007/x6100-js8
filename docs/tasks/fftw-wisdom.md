# Task: FFTW wisdom for JS8 (no hang closing JS8 after opening it)

Written 2026-10-14 for a Claude Code cloud session; agreed with VE7NHW. Read
`CLAUDE.md` first (rules, how we work, code map), then `docs/PLAN.md` (the
item "JS8 hangs 5-13 s when closed soon after opening").

## The problem (measured)

On the radio, pressing APP a few seconds after opening JS8 hangs the GUI
for 5-13 s. Log on 2026-10-14: "decode pass took 12.2 s", then 13.1 s on
the next open; every later pass 0.5-1.5 s.

- Closing JS8 joins the decode threads, which only check for stop between
  passes (`third-party/js8core/src/engine/engine.cpp` ~1282).
- The first pass after each open builds the decoders: `legacy_decode()`
  keeps them `thread_local`, built on first use (our local patch 3), and
  the decode threads are new on every open.
- Their FFTW plans use `FFTW_ESTIMATE_PATIENT`, which without
  `FFTW_ESTIMATE` is a **measuring** planner. With no wisdom kept, the
  measuring is redone on every open: on a PC the first pass over all four
  main speeds takes 1.1 s and later ones 0.05 s (0.09 s with
  `FFTW_ESTIMATE`); the radio is ~10x slower, hence 12-13 s.
- Desktop JS8Call uses the same flag but keeps `js8call_wisdom.dat`
  (`fftwf_import_wisdom_from_filename` at start, `fftwf_export_wisdom_to_filename`
  at exit, `mainwindow.cpp`), so it measures once ever. Rule 2 (behave like
  desktop) points the same way.
- FFTW is used only by the decoder (`legacy_decoder.cpp`); nothing else in
  the GUI program uses it. Planner calls there hold `fftw_mutex`
  (`commons.h`), which is not thread-safe otherwise.
- We asked JS8Call-improved what they think (Android-port#109); no answer
  needed for this.

## What to build (VE7NHW's decisions)

1. **`/mnt/js8_wisdom.dat`** (the DATA partition, next to `js8_inbox.txt`
   etc.). Loaded before the decoder starts; saved after it has stopped,
   via `.tmp` + rename like JS8's other files. A missing or damaged file
   only means measuring again; never an error the user has to act on.
2. **When the file is missing** (first open of a new build, or deleted):
   measure **all five speeds, Ultra included**, whatever is switched on,
   up front at open, not inside the first decode pass; save the file; free
   the decoders of speeds that are switched off (keep patch 3's memory
   saving: normal opens build only the speeds switched on). A speed
   switched on later then needs no measuring.
3. **A one-time notice** in the JS8 window while that setup runs, e.g.
   "Setting up the decoder (one time only, about 15 s)". Pressing APP
   meanwhile **must not look like a freeze or a crash**: keep the notice up
   while closing, or hide the window at once and finish behind it. Decoding
   may wait until the setup is done (once ever).
4. Prefer keeping it in our JS8 library (`src/js8/`, e.g. `Receiver`) and
   the app (`src/dialog_js8.c`) rather than the engine. If an engine change
   is unavoidable, it is a numbered local patch in
   `third-party/js8core/UPSTREAM.md`. One idea that needs no engine change:
   a one-off `legacy_decode()` with every speed's bit set on a silent
   buffer, on a short-lived thread, builds every plan (its `thread_local`
   decoders go when the thread ends); then export the wisdom.

## Rules for this job

- Work on branch **`fftw-wisdom`** from `main`; commit and push as you go;
  end commit messages with the Co-Authored-By line.
- Nothing transmits on its own (rule 1). Keep changes inside JS8 (rule 3).
- **Anything visual: mock-ups first.** Draw 2-3 options for the notice on a
  real UI-harness screenshot at true pixel positions (800x480, 5" screen:
  text bigger and bolder than desktop), save them under
  `docs/mockups/fftw-wisdom/`, recommend one and implement that one;
  VE7NHW picks before anything merges.
- Tests: unit tests (`CLAUDE.md`, "Build and test": `test_js8`,
  `test_js8_db`, `test_qth`) pass; add tests for the wisdom file (written,
  read back, a damaged file ignored) and that the second build of the
  decoders after loading it is fast. The UI harness
  (`tools/js8_ui_harness/README.md`) for the notice and for closing during
  the setup; add a scenario.
- You can't reach the radio. Don't merge into `main` and don't build an SD
  image: open a pull request into `main` on randal007/x6100-js8 and stop.
  VE7NHW tests every build on the radio first.
- The PR description: plain words (VE7NHW is a ham, not a programmer),
  what changed, the mock-ups, the measured times before and after (PC),
  and anything left open.
