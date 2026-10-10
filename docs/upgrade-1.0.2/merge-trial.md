# Trial merge onto v1.0.2

How it was done (2026-10-02): a throwaway worktree at `v1.0.2` (416377c);
for each upstream file our commits changed (`git diff 20c4b77 main`, the
Murus import 20c4b77 being our base), a 3-way `git merge-file` of our
version and v1.0.2's against 20c4b77's. Our 496 new files were left out:
they don't exist upstream, so they can't conflict (whether they *compile*
against v1.0.2 is the [README](README.md)'s "glue" section).

Our base under the Murus import is gdyuldin v0.34.2 (18353bd, tag
`v0.34.2`); v1.0.2 is 151 commits later, 264 files changed
(+34,683 / −12,680). The lvgl submodule moves from 7d827a0 to ad032d8:
same LVGL 8.3 fork, three drawing fixes (gradient cache, non-NEON
blending).

## The 29 upstream files we changed

| File | Ours | Upstream since 0.34.2 | Trial | How to resolve |
|---|---|---|---|---|
| `.github/workflows/main.yml` | +36 −12 | +8 −12 | 3 conflicts | Keep ours (JS8 deps, Release step, comments); take their ccache action if it speeds builds |
| `src/CMakeLists.txt` | +5 −1 | +32 −17 | 3 conflicts | Upstream's file, then add `kbd_rollover.c`, `dialog_js8.c`, `tx_player.c`, `add_subdirectory(js8)`, link `JS8` |
| `src/main_screen.c` | +7 | +503 −444 | 3 conflicts | Upstream's file, then add `#include "dialog_js8.h"` and the `ACTION_APP_JS8` cases in `main_screen_start_app()` and `main_screen_action()` |
| `src/buttons.cpp` | +2 −1 | +108 −105 | 2 conflicts | Upstream's file, then `btn_js8` on an APP page (where: user decision 5) |
| `src/ft8/tx_worker.c` | +7 −91 | +35 −28 | 2 conflicts | **Take upstream's** (ours made FT8 share `tx_player`; FT8 goes back to upstream's own loop) |
| `src/ft8/tx_worker.h` | +1 −2 | +5 −1 | 1 conflict | Take upstream's (as above) |
| `src/dialog_ft8.c` | +2 −7 | +87 −76 | 1 conflict | Take upstream's (as above) |
| `src/gps.c` | +27 | +15 −7 | 2 conflicts | Take upstream's; JS8 uses `gps_get_snapshot()` instead of our `gps_last_fix()` |
| `src/gps.h` | +4 | +7 | 1 conflict | Take upstream's (as above) |
| `src/fonts/CMakeLists.txt` | +1 | +4 −6 | 1 conflict | Upstream's file, then add `js8_marks_24.c` |
| `src/radio.c` | +21 | +238 −171 | 1 conflict | Upstream's file, then re-add `radio_set_tx_filter`, `radio_set_rx_dsp_off` (new setting names for NR/NB/DNF), `radio_speaker_play` (new audio API) |
| `src/widgets/lv_waterfall.c` | +114 −10 | +7 −1 | 1 conflict | Both: ours (mirrored ring, `fill_rect`, row counter) plus upstream's alpha = 0xFF for the overlay plane, applied to our ring too |
| `CMakeLists.txt` | +3 | — | clean | (js8core subdirectory, map data install) |
| `README.md` | +844 −37 | — | clean | Ours (the manual) |
| `sql/digital_modes.csv` | +13 | — | clean | (JS8 and GhostNet rows for new cards) |
| `src/adif.c` | +9 | — | clean | (`MODE_JS8` ↔ `MFSK`/`JS8`) |
| `src/cfg/digital_modes.h` | +5 −2 | — | clean | (`CFG_DIG_TYPE_JS8`, `_GHOSTNET`) |
| `src/keyboard.c` | +29 −1 | — | clean | (USB keyboard rollover) |
| `src/keypad.c` | +6 −1 | +2 −2 | clean | (`keypad_set_long_time`) |
| `src/keypad.h` | +4 | — | clean | |
| `src/qso_log.h` | +1 | — | clean | (`MODE_JS8` = 8) |
| `src/radio.h` | +10 | +39 −6 | clean | (our three helpers' declarations) |
| `src/textarea_window.c` | +12 | +4 −4 | clean | (the held-ESC fix) |
| `src/widgets/lv_waterfall.h` | +14 −1 | — | clean | |
| `tests/CMakeLists.txt` | +8 | +10 −1 | clean | (`test_js8`) |
| `src/dialog_settings.cpp` | +1 | **moved** | — | v1.0.2 split it into `src/settings/`; " APP JS8 " goes in the long-press list in `settings_page_ui.cpp` |
| `src/params/params.h` | +23 | **removed** | — | Our 21 settings become a `js8` group in `src/cfg/settings_manager.h` + `cfg_api.h/.cpp`; `ACTION_APP_JS8` moves to `src/settings_types.h` (with 109/110 reserved) |
| `src/params/params.c` | +45 | **removed** | — | (as above: defaults and limits go in the `Parameter` declarations) |
| `src/params/migrations.c` | +58 | **moved** to `src/cfg/` | — | **Not** a plain move: see "The power trap" in the [README](README.md) |

18 conflict blocks in 12 files; all 18 are at places where we added a line
or two next to code upstream rewrote.

## The Murus layer being dropped

`git diff v0.34.2 20c4b77`: 47 files, +39,082 −15. WeFax
(`dialog_wefax.c`, `src/wefax/`), NavTex (`dialog_navtex.c`,
`src/navtex/`), the broadcast channel list (`channels.c`,
`dialog_channels.c`, `broadcast_db.c`, `rootfs/usr/share/x6100/broadcast.csv`,
`cJSON`), and their hooks in `main_screen.c` (+510), `spectrum.c` (+54),
`tx_info.c`, `buttons.cpp`, `params`, `audio.c`, `panel.cpp`,
`controls.cpp`, `styles.c`. Nothing of ours depends on any of it except
the app number (`ACTION_APP_JS8` after `ACTION_APP_NAVTEX`) and the JS8
button's place after theirs on the APP page.

## Functions the JS8 glue calls that v1.0.2 removed

From `arm-linux-nm -u` on `dialog_js8.o`, `tx_player.o` and
`kbd_rollover.o` built from `main`, checked against v1.0.2's sources:

| Gone | Was | Replacement |
|---|---|---|
| `params` (struct), `params_bool_set`, `params_uint8_set`, `params_uint16_set`, `params_float_set` | upstream | `cfg.<group>.<name>()` + `param_i_set/param_f_set/param_t_set` |
| `params_int32_set` | ours | (as above) |
| `cfg_fg_freq`, `cfg_cur_mode`, `cfg_cur_filter_low/high`, `cfg_pwr`, `cfg_tx_filter_low/high` | upstream globals | `cfg.cur.fg_freq()`, `cfg.cur.mode()`, `cfg.cur.filter_low/high()`, `cfg.pwr()`, … |
| `main_screen_lock_mode/freq/band/ab` | upstream | `lm_set_mode/freq/band/ab` (`lock_manager.h`) |
| `dsp_set_waterfall_enabled`, `dsp_set_spectrum_enabled` | upstream | `waterfall_set_enabled`, `spectrum_set_enabled` |
| `waterfall_style`, `bg_color`, `wf_palette` | upstream | `style.…` (`styles.h`), `style.wf_palette` |
| `dialog_t.audio_cb` | upstream | `dsp_audio_subscribe_float()` + `dsp_audio_set_active()` |
| `gps_last_fix` | ours | `gps_get_snapshot()` |
| `radio_set_tx_filter`, `radio_set_rx_dsp_off`, `radio_speaker_play` | ours | re-added in `radio.c` |
| `tx_player_play`, `tx_player_base_gain_offset` | ours | `tx_player.c` rewritten on `app_ports` and an audio player |
| `keypad_set_long_time` | ours | merges as is |

Everything else it calls (`msg_update_text_fmt`, `textarea_window_*`,
`buttons_*`, `dialog_init/destruct`, `qso_log_*`, `qth_*`, `radio_set_freq`,
`radio_set_modem`, `radio_set_pwr`, `audio_gain_db*`, `audio_set_play_vol`,
`x6100_control_get_base_ver`, the fonts, LVGL, liquid-dsp) is still there.
