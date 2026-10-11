**Opened 2026-10-11: OpenX6100/x6100_gui#290** (branch `js8-groundwork`, 3252bde on main cae2982), reviewers gdyuldin + myshak1. Text as sent:

---

This is the first of four pull requests that bring JS8 (JS8Call) into the firmware, as talked about in #23. The JS8 app has been running on 1.0.2 in test builds since early October (randal007/x6100-js8, betas 4.1 to 5), and testers use it on the air every day.

### The series

One at a time, each after the one before is merged:

| | What | Size |
|---|---|---|
| **1/4 (this one)** | The changes outside the JS8 app that it needs, and a test for its database part | 18 files, +540 −18 (106 of them the test) |
| 2/4 | `third-party/js8core`: the JS8 engine from JS8Call-improved (GPL-3.0, see below) | ~8,000 lines of its own code, plus the libraries it brings (Eigen, CRCpp) and generated tables |
| 3/4 | `src/js8`: messages, inbox, relays, stations, history, and their unit tests | ~10,000 lines + ~4,700 of tests |
| 4/4 | The app: `dialog_js8.c`, its waterfall, clock and map, and the JS8 button in the APP menu | ~14,000 lines |

### What this one does

It's the only one of the four that changes code other apps use, so it's kept small enough to read every line. On its own, nothing a user sees changes, apart from a lighter waterfall.

- **Settings:** a `js8` group (`cfg.js8.*`), with the names the test builds already stored, so testers keep their settings.
- **params.db:** JS8's and GhostNet's frequency rows (`digital_modes` types 2 and 3).
- **`src/cfg/js8_db.c`:** realigns a card from the early JS8 test builds (betas 1-4, on 0.34). They numbered their own migrations 4 and 5, so 1.0's migration 4 (scaled integers to real numbers) would be skipped and `pwr` 4 read as 4 W instead of 0.4 W. Such a card goes back to version 3 once, and a small `js8_db` table records it. On any other card it only creates that table. **`tests/test_js8_db.cpp`** takes every kind of card through it (new, stock 0.34, stock 1.0.x, JS8 test builds, run twice).
- **QSO log:** `MODE_JS8` = 8, written to ADIF as MFSK / JS8.
- **radio.c:** set the TX filter, and switch NR / NB / notch off, for the radio only (not saved), plus the speaker play mode for an app's beeps.
- **keypad:** an app can shorten the hold time while it's open.
- **lv_waterfall:** rows go into a ring buffer instead of moving the whole image for every new row, so every waterfall (FT8's, and the new apps') costs less per row. Clearing it now gives opaque black instead of see-through pixels, which on the overlay plane let whatever was underneath show through.

### Things I'd like your opinion on

- **The frequency rows** are added at every start with `INSERT OR IGNORE`, not as a numbered migration, so JS8 never takes a migration number someone else might want. If you'd rather have a numbered migration, I'm happy to change it.
- **Numbers stored on the card**, as in #283: `MODE_JS8` = 8, `CFG_DIG_TYPE_JS8` = 2, `CFG_DIG_TYPE_JS8_GHOSTNET` = 3. JS8's app number (112) comes with the app in 4/4. It opens from the APP menu only, not as a long-press action.

### Licence (for 2/4)

Everything in this PR is LGPL-2.1-or-later like the rest of the GUI. The engine in 2/4 is GPL-3.0, as JS8Call is. LGPL-2.1-or-later code can be used under GPL-3.0, so the two can be combined; the firmware image as a whole would then be GPL-3.0. The other way is JS8 as a build option, off by default. @gdyuldin, it would be good to have your decision in #23 before 2/4.

### Testing

- Builds into a full image: https://github.com/randal007/x6100-js8/actions/runs/38152673163 (on main d0d0509; OpenX6100 has no build on PRs, so I used our test-image workflow). #273 was merged while it ran: the branch is now on top of it, and the files both touch compile for the radio.
- `test_js8_db` passes (82 checks). `test_cfg` I couldn't run: it needs the ARM LVGL, so it doesn't build on a PC. The same settings code has run on testers' radios for weeks.
- Merged on its own with each open PR (#271, #272, #274, #275, #276, #280, #284, #285, #286, #289), it goes in cleanly with every one of them.
- The same code has been on testers' radios since beta 4.1 (2 October), including cards carried over from betas 1-4.

The SSB-written-as-AM bug in the ADIF export that I found on the way is its own PR: #289.

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)
