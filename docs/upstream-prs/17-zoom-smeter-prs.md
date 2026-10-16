PR bodies as sent 2026-10-15 (OpenX6100/x6100_gui). Commits authored by Hany El Imam (1KO125).

## #304 zoom-auto-levels

Small fix from Hany El Imam's 1KO125 firmware (Murus team), the first of his changes coming in, as agreed in #302. The commit is his, as he wrote it.

**What it fixes:** with the spectrum's auto levels on, the noise floor gets brighter the further you zoom in. Each zoom step narrows the bins, and `decimate_emit()` adds `zoom_level_offset` (3 dB per step) to the displayed values, but `update_noise_and_levels()` worked out `auto_min` / `auto_max` without it. So at each zoom step the floor moved up 3 dB on the display. This adds the same offset to the auto minimum (4 lines in `src/dsp.cpp`).

**Checked:** `src/dsp.cpp` is unchanged on main since v1.0.2, so it is exactly Hany's change; compiles with the image's GCC 12. Radio test with a test image (Build image run https://github.com/randal007/x6100-js8/actions/runs/38761835319) to follow here: zoom in and out with auto levels on, the floor should stay put.

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

## #305 s-meter-offset

A small setting from Hany El Imam's 1KO125 firmware (Murus team), as agreed in #302. The commit is his, as he wrote it.

**What it adds:** *S-meter offset* in Settings > Interface, right under *Meter label*: a slider from -30 to +30 dB (0 to start, so nothing changes until it's set). It's added to the S-meter reading and to its noise floor in `src/dsp.cpp`, so the bar, the numbers and the S-meter that CAT reports all move together. Useful for calibrating the meter against a known signal or another radio (related: #244, CAT S-meter vs the stock GUI).

**Files:** `src/dsp.cpp` (the offset applied), `src/cfg/settings_manager.h` + `cfg_api.*` (the `s_meter_offset` parameter, GLOBAL), `src/settings/settings_page_ui.cpp` (the slider). 31 lines.

**Checked:** fitted onto current main by hand (only the settings files had moved since v1.0.2, `dsp.cpp` is Hany's exactly); compiles with the image's GCC 12. Radio test with a test image (https://github.com/randal007/x6100-js8/actions/runs/38761835319) to follow here.

**Question:** a new GLOBAL parameter, no migration: fine as a plain new key, or do you want it handled another way?

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)
