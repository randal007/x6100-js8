# Moving to gdyuldin v1.0.2: the study

Written 2026-10-02, before any porting starts. The user's direction:
**build on gdyuldin's x6100_gui v1.0.2 and drop the Murus team's fork
(1KO125: WeFax, NavTex, the channel list) for now**; the Murus team is
moving their fork to 1.0.2 themselves, so their apps come back after, once.
Reference: [JS8Call-improved](https://github.com/JS8Call-improved/JS8Call-improved)
for anything JS8 behaves like, as always.

Detail files:

- [merge-trial.md](merge-trial.md): every upstream file we changed, what a
  trial merge onto v1.0.2 gave, and how each one gets resolved
- [appendix-commits.md](appendix-commits.md): all 151 upstream commits
  from v0.34.2 to v1.0.2, grouped by area, with the files that meet our code

## In short

- **Most of our code moves over untouched.** Of the 525 files our 161
  commits added or changed, 496 are our own new files: the JS8 engine
  (`third-party/js8core`), the JS8 library (`src/js8/`, no GUI code at
  all), tests, tools, map data and docs. Only **29 upstream files** carry
  edits of ours, all small.
- **A trial merge onto v1.0.2:** 13 of the 29 merge cleanly, 12 have 18
  conflicting blocks between them, and 4 no longer exist (moved in
  v1.0.2). All 18 blocks are small and understood.
- **The real work is the JS8 app's glue to the radio's GUI**
  (`src/dialog_js8.c`, 7,415 lines, plus `tx_player.c` and
  `kbd_rollover.c`): v1.0.2 rebuilt the parts it leans on. 19 of the
  functions it calls are gone, and settings, receive audio, transmit
  audio, screen layout and styles all changed:

  | Area | 0.34.2 (today) | v1.0.2 |
  |---|---|---|
  | Settings | `params.js8_*` struct fields, `params_*_set()` | `cfg.<group>.<name>()` Parameter objects (C++ `cfg` library) |
  | Receive audio | the dialog's `audio_cb`, 44.1 kHz | `dsp_audio_subscribe_float(cb, rate)`: any rate we ask for (12 kHz for JS8) |
  | Transmit audio | `audio_play()` at 44.1 kHz | audio "players" through `app_ports`, 48 kHz (FT8 uses 6 kHz) |
  | Screen | one framebuffer | DRM, two planes: apps on a see-through, software-rotated LVGL overlay; the main spectrum and waterfall drawn directly underneath |
  | Layout | hard-coded sizes | `DIALOG_WIDTH` 796 × `DIALOG_HEIGHT` 345 at (2, 72): more room for the map |
  | Themes | Simple, Legacy | Simple, Black, Flat (Legacy removed) |
  | Styles | separate globals (`bg_color`, `waterfall_style` …) | one `style` struct (`style.wf_palette`, `style.btn.active` …) |
  | Locks | `main_screen_lock_*()` | `lm_set_*()` (lock_manager.h) |
  | Main waterfall off | `dsp_set_waterfall_enabled()` | `waterfall_set_enabled()`, `spectrum_set_enabled()` |
  | GPS | (ours) `gps_last_fix()` | `gps_get_snapshot()` + `MSG_GPS` |

- **Still LVGL 8.3:** v1.0.2 keeps the same LVGL fork (3 small drawing
  fixes on top), so our screen code doesn't need a new LVGL.
- **One safety trap, about transmit power** (below): a plain merge
  walks straight into it, and so does the rule we wrote down for it
  (in a rarer case). A one-time fix is proposed.
- **What you lose until the Murus team's 1.0.2 fork lands:** WeFax,
  NavTex and the broadcast channel list. Everything else in 1KO125 beta 6
  was the same R1CBU/R2RFE firmware.

## The power trap (must not be missed)

v1.0.2 stores decimal settings as real numbers. Its **migration 4**
converts old cards: `pwr` 4 → 0.4 W, `output_gain` (÷5), the per-band
`dac_offset` (÷10) and others. Our beta 4 already used migration numbers
**4 and 5** for the JS8 and GhostNet frequency lists, so every card that
ran beta 4 has "migration 5 done" stored.

v1.0.2 moved the file (`src/params/migrations.c` → `src/cfg/migrations.c`),
so a merge raises **no conflict**: taking upstream's file and adding ours
after it (5, 6) would make a beta-4 card skip the conversion. Your card's
`pwr` = 4 (0.4 W) would then read as **4 W**, and the TX gain and band
offsets would be wrong the same way, with the XPA125B in line.

A second trap sits inside the obvious fix. The `params` table's value
column is declared `INTEGER`, so after v1.0.2 has converted a card, a power
of 5.0 W is saved as the integer 5. Running the conversion a second time on
such a card (one that comes from stock R1CBU 1.0.x) would make it 0.5 W.
So the conversion must run exactly once per card, never twice, never
skipped.

**Recommended: realign with upstream's numbering once, and keep JS8's
frequency lists out of the numbered migrations from then on.**

- `src/cfg/migrations.c` = upstream's list exactly (0–4, and whatever
  they add later: merged as they come, no renumbering ever again).
- Before upstream's migrations run, one JS8 step looks at the card. If it
  has no `js8_db` table yet, the version is 4 or 5, and our JS8 frequency
  rows (`digital_modes` type 2) are there, the card ran one of our betas,
  which used 4 and 5 for those rows only. The step sets the version back
  to 3, so upstream's conversion runs. It then creates `js8_db`, so it
  never looks again.
- After upstream's migrations, the JS8 and GhostNet lists are inserted
  with `INSERT OR IGNORE` at every start (they don't need a number).

| Card | Version | JS8 step | Upstream runs | Result |
|---|---|---|---|---|
| New card | none | nothing to realign | 0–4 | lists added; nothing to convert (a new database has no power rows) |
| Our beta 1 | 4 + JS8 rows | back to 3 | 4 | **power converted** once |
| Our betas 2–4 | 5 + JS8 rows | back to 3 | 4 | **power converted** once |
| Stock R1CBU / 1KO125 0.34.x | 3 | nothing | 4 | converted once; lists added |
| Stock R1CBU 1.0.x | 4, no JS8 rows | nothing | none | already converted; lists added |
| Ours after the port, any later start | `js8_db` there | nothing | new ones only | nothing converted twice |

(The alternative, keeping ours at 4 and 5 and putting upstream's
conversion at 6 as [UPSTREAM_README.md](../UPSTREAM_README.md) says,
works for our cards but converts a card from stock 1.0.x a second time,
needs a guard for that, and needs every later upstream migration
renumbered. If the user takes the recommendation, that rule is rewritten.)

To check on the PC before flashing: run the migrations against copies of
the user's real `params.db` (in `sd-backups/`) and of a stock 1.0.2 one,
and compare `pwr`, `output_gain` and `dac_offset` before and after.

Going **back** to beta 4 after a 1.0.2 build has converted the card isn't
safe either way round: beta 4 reads those real numbers as integers.
Restore the DATA backup that `x6100-flash` makes before every flash.

## Other stored numbers

- **Long-press app number:** today `ACTION_APP_JS8` = 111, after
  1KO125's WeFax (109) and NavTex (110). v1.0.2 ends at WiFi (108). Keep
  JS8 at 111 by leaving 109 and 110 reserved (unused) for WeFax and
  NavTex, so cards with JS8 on a long press keep it, and the Murus apps
  get their own numbers back when they return. **Their beta 8 also gives
  SSTV 111**: when SSTV is merged it takes 112 here.
- **`MODE_JS8` = 8** in the QSO log: v1.0.2 added no modes, no clash.
- **`CFG_DIG_TYPE_JS8` = 2, `_GHOSTNET` = 3:** v1.0.2 added no types, no
  clash.
- **Setting names in `params.db`:** v1.0.2 still keeps settings in the
  `params` table by name. Declaring ours with the same names
  (`js8_speed`, `js8_hb` … all 21) carries every beta-4 value over.
- **Theme:** Legacy is gone; a card set to Legacy (1) comes up as Black.

## The JS8 app's glue, area by area

### Settings

Our 21 `js8_*` settings become a `js8` group in v1.0.2's `cfg`, declared
the way FT8's are: a `Parameter<int32_t> p_js8_speed{"js8_speed", …}` in
`settings_manager.h`, a `cfg_js8_refs_t` in `cfg_api.h` and its table in
`cfg_api.cpp`. In the app, `params.js8_speed.x` becomes
`param_i_get(cfg.js8.speed())` and `params_uint8_set(&params.js8_speed, v)`
becomes `param_i_set(cfg.js8.speed(), v)` (181 places in
`dialog_js8.c`). The shared ones move to their new names
(`cfg.callsign()`, `cfg.qth()`, `cfg.pwr()`, `cfg.cur.fg_freq()`,
`cfg.cur.filter_low()`, `cfg.theme()`, `cfg.audio.play_gain_db()` …).
Buttons that show a setting can now bind to it (`.subj`), as FT8's do.

Upstream files touched: `src/cfg/settings_manager.h`, `cfg_api.h`,
`cfg_api.cpp` (adds only; flag it as shared code).

### Receive audio

`dialog_t.audio_cb` is gone. FT8 now subscribes:
`dsp_audio_subscribe_float(audio_cb, rate)` once, then
`dsp_audio_set_active(id, true/false)` on open and close. The callback runs
on the DSP thread under a lock (it must not subscribe or unsubscribe).

For JS8: subscribe at 48 kHz and keep our own resampler (48 → 12 kHz is
an exact ÷4 and the decoder has only ever been tested behind ours), or ask
for 12 kHz directly and drop it. Recommended: 48 kHz + ours first, then
compare decodes on the same recording in the harness before switching.

### Transmit audio

`tx_player.c` is our copy of the old FT8 transmit loop (ALC gain
correction per block, power cap 5 W, retune by the offset). v1.0.2
rewrote FT8's loop the same way but through `app_ports` and an audio
player (`audio_player_send/wait`) at `AUDIO_PLAY_RATE / 8` = 6 kHz.

Recommended: **leave FT8 exactly as upstream has it** (today our
`ft8/tx_worker.c` and `dialog_ft8.c` call `tx_player`; drop that, which
also removes 3 of the 18 conflicts), and rewrite `tx_player.c` on the new
ports and player. JS8's synthesiser takes the player's rate. Our
`radio_speaker_play()` (the alert beep through the speaker) moves to the
same audio API.

### Screen

- **Two planes.** Apps draw on a see-through LVGL overlay that is rotated
  in software on every flush; the main screen's spectrum and waterfall
  are drawn directly on the plane below. A pixel with alpha 0 on the
  overlay is a hole: upstream had to set alpha 0xFF in `lv_waterfall`'s
  palette and buffer for this. Our own buffers must do the same: the
  mirrored waterfall ring (`lv_waterfall.c`), the map canvas
  (`map_render` writes ARGB) and the decode marks.
- **The JS8 waterfall's smoothness work was measured on the old path**
  (exact invalidation, opaque boxes, 15 rows/s). It has to be measured
  again on the radio: the software rotation adds work per redrawn area,
  and the see-through overlay may change what "opaque" saves.
- **The frozen main waterfall behind JS8's buttons** (a known issue) may
  disappear: the main waterfall now draws directly, below. To check.
- **Full screen:** FT8 now calls `waterfall_set_enabled(false)` and
  `spectrum_set_enabled(false)` while open; JS8 does the same (it used
  `dsp_set_*_enabled`).
- **Layout:** the dialog is 796 × 345 at (2, 72) in every theme. Our
  sizes (771-wide waterfall and list, the map's 793 × 345 / 788 × 339
  frames per theme, `MAP_X/Y/W/H` from `map_geometry()`, the corner
  images) are redone for it and for the three new themes; the map gets the
  extra room the user wanted.
- **Styles:** `bg_color` (15 uses), `waterfall_style`, `wf_palette` and
  the button styles move to the `style` struct.
- **Messages** (`msg_update_text_fmt`) now sit on LVGL's top layer: check
  they still show over the JS8 window where we expect them.

### Everything else the glue calls

- **Locks:** `main_screen_lock_mode/freq/band/ab()` →
  `lm_set_mode/freq/band/ab()`.
- **GPS:** our `gps_last_fix()` addition to `gps.c` goes; read
  `gps_get_snapshot()` instead (the position beacon, the log's grid).
- **Our radio helpers** (`radio_set_tx_filter`, `radio_set_rx_dsp_off`,
  `radio_speaker_play`): re-added to the new `radio.c` with the new
  setting names. Same X6100Control calls; Buildroot now ships X6100Control
  v0.16.3 (was v0.16.1), to check for changes in those calls.
- **Events:** `EVENT_BAND_UP/DOWN` are still there (now `lv_event_code_t`).
  `dialog_is_run()` is deprecated in favour of `MSG_DIALOG_START/STOP`.
- **Hold time:** `keypad_set_long_time()` (ours) merges cleanly.
- **Keyboard:** our USB keyboard rollover and the "ESC closed the whole
  app" fix in `keyboard.c` / `textarea_window.c` merge cleanly (shared
  code, bug fixes for every app).
- **Build:** v1.0.2 compiles the app with
  `-Werror=implicit-function-declaration` and `_GNU_SOURCE`; our C files
  must be clean under both.
- **App list and settings:** the JS8 button on the APP page
  (`buttons.cpp`), its case in `main_screen_start_app()` /
  `main_screen_action()`, and " APP JS8 " in the long-press list, which
  moved to `src/settings/settings_page_ui.cpp`.

## Tests and tools

- **Unit tests** (`tests/test_js8.cpp`, 112 fast cases): test only
  `src/js8/` and js8core, so they move over as they are.
- **The UI harness** (`tools/js8_ui_harness`) builds `dialog_js8.c`
  against stock LVGL 8.3 with stand-ins (`stubs.c`, `driver.c`) for the
  GUI. Those stand-ins follow the new APIs: settings (v1.0.2's `cfg` is
  its own library now, so the harness can link the real one with a
  database in its build folder instead of faking `params`), the audio
  subscription, the styles struct and the new dialog size. All 42
  scenarios then run again; their expected results change where the
  layout does (map positions, screenshots).
- **CI:** keep our *Build image* workflow (JS8's Boost and FFTW, the
  Release step); take upstream's newer ccache action if it speeds builds.
  Drop upstream's two new workflows: `cache_test.yml` runs on every pushed
  tag (each of our releases) and `cache_migrate.yml` is a one-off of
  theirs.
- **Buildroot:** our CI already clones AetherX6100Buildroot's newest
  commit, and since 2026-09-29 that's the 1.0 one (DRM, X6100Control
  v0.16.3, a new panel driver, Bluetooth): **beta 4 was built on it**
  and works. Nothing to change there for 1.0.2.

## Save point and how to go back

- `js8-beta4` (tag) and its release image stay as they are: flashing that
  image is always a way back.
- Before porting starts, tag `main` as it is then (`js8-0.34-final`, with
  the QUERY CALL fix) and do the port on a branch (`port-1.0.2`). `main`
  and the beta 4/5 work on it stay untouched until the port works on the
  radio; only then is it merged.
- Going back after a 1.0.2 build ran on a card: flash the old image **and
  restore the DATA backup** (see the power trap: the settings were
  converted).

## Proposed order of work

1. Save point: tag `js8-0.34-final`; branch `port-1.0.2` from v1.0.2.
2. Bring our files over (the 496 new ones as they are; the 29 shared ones
   by [merge-trial.md](merge-trial.md)), FT8 left as upstream's.
3. Migrations as above (upstream's list, the one-time realignment, the
   lists inserted at every start), with the PC check against copies of
   the real `params.db` and a stock 1.0.2 one.
4. Settings: the `js8` cfg group; the app moved onto it.
5. Audio: receive subscription, transmit player, the beep.
6. Screen: planes (alpha), full screen, locks, styles; then the layout
   for 796 × 345 and the three themes, map frames included.
7. Harness and CI green; unit tests unchanged.
8. Build image; on the radio, with the amp on standby and the dummy load:
   receive, decode, transmit power and ALC, the waterfall's smoothness and
   CPU, the map, every theme; then on the air.
9. Merge into `main`; beta 5 features continue on top. The Murus apps come
   back when their 1.0.2 fork is out (SSTV at 112, WeFax/NavTex at 109/110).

## Decisions for the user

1. **The power-trap fix:** realign with upstream's migration numbers once
   (recommended), or keep ours at 4–5 with upstream's conversion at 6 and a
   guard?
2. **Keep JS8 at 111** with 109/110 left free for WeFax/NavTex: agree?
3. **FT8 left exactly as upstream's**, JS8 with its own transmit loop:
   agree? (Our only FT8 change today is that it shares JS8's loop.)
4. **Receive audio at 48 kHz through our own resampler** first, 12 kHz from
   upstream's later if it decodes as well: agree?
5. **Where the JS8 button goes** on the APP pages now that WeFax/NavTex
   are out (today: page 2 after them).
