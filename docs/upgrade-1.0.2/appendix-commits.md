# Appendix: every upstream commit, v0.34.2 → v1.0.2

151 commits by Georgy Dyuldin R2RFE and contributors, grouped by area. **Touches** lists the files among them that our JS8 code includes, calls or changed (the ones to read when porting); empty means it doesn't meet our code directly.

## Settings (params → cfg) (6 commits, 6 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 4fbec13 | 2026-09-10 | Update parameters app | 3 | `src/params` |
| 681b3e8 | 2026-09-10 | Remove outdated params | 2 | `src/params` |
| 2b577e4 | 2026-09-17 | dialog settings refactoring | 13 | `src/CMakeLists.txt`, `src/main_screen.c` |
| a279b14 | 2026-09-19 | Simplify cfg | 39 | `src/app_ports`, `src/buttons.h`, `src/cfg`, `src/main.c` |
| b46b7af | 2026-09-22 | Fix saving/loading quantized floats | 5 | `src/cfg` |
| f30d556 | 2026-09-30 | Fix issue with shared buf on param_t_get | 6 | `src/cfg`, `src/dialog_ft8.c` |

## Audio / DSP / spectrum (9 commits, 8 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 1864d61 | 2026-09-01 | Some DSP changes and optimisations | 2 | `src/dsp.cpp`, `src/dsp.h` |
| ad9dce4 | 2026-09-10 | Fixes for msg, lv_waterfall, minor fixes | 15 | `src/CMakeLists.txt`, `src/dialog_ft8.c`, `src/main_screen.c`, `src/msg.c`, `src/msg.h`, `src/radio.c` … |
| 1f8d88a | 2026-09-11 | Update audio interface | 12 | `src/audio.c`, `src/audio.h`, `src/dialog_ft8.c`, `src/dsp.cpp`, `src/dsp.h`, `src/ft8/tx_worker` |
| 012a9bc | 2026-09-14 | fixup! Switch to audio subscriptions | 1 |  |
| 592aadc | 2026-09-15 | Update audio level monitor | 1 | `src/audio.c` |
| 73ee8e8 | 2026-09-16 | Remove waterfall zoom on/off | 7 | `src/cfg`, `src/main_screen.c` |
| a620600 | 2026-09-17 | Move freq boundaries above the spectrum | 2 | `src/main_screen.c`, `src/styles.c` |
| 85c6e17 | 2026-09-29 | Update DSP noise offset | 1 | `src/dsp.cpp` |
| bdf4ef6 | 2026-09-29 | Add spectrum height | 18 | `src/cfg`, `src/main_screen.c`, `src/msg.c`, `src/msg.h`, `src/styles.c`, `src/styles.h` |

## Screen: DRM, planes, rendering (9 commits, 6 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| f85ebc1 | 2026-07-28 | Move waterfall rendering to thread (radio), optimize rendering | 1 |  |
| 3011a76 | 2026-08-22 | Switch to double buffering and NEON rotate | 2 | `src/main.c` |
| 6a55289 | 2026-08-26 | Switch to DRM | 7 | `src/main.c` |
| bb8ecd0 | 2026-08-26 | Switch for 2 lvgl displays for 2 planes | 4 | `src/main.c` |
| 6fe088c | 2026-08-27 | Switch spectrum to direct render | 7 | `src/main.c`, `src/main_screen.c`, `src/main_screen.h` |
| a9a69f0 | 2026-08-27 | Switch waterfall to direct render | 6 | `src/main.c`, `src/main_screen.c`, `src/styles.c` |
| 6f130f3 | 2026-08-27 | Draw waterfall middle line on primary plane | 1 |  |
| cffd821 | 2026-09-02 | Fix screenshot with DRM | 3 |  |
| 1027c88 | 2026-09-16 | Update lvgl submodule | 1 | `lvgl` |

## Look: themes, styles, layout (22 commits, 22 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 1c3b390 | 2026-07-27 | add black theme | 11 | `src/params`, `src/styles.c` |
| d4b2086 | 2026-07-27 | add adjustable spectrum color | 6 | `src/params`, `src/styles.c`, `src/styles.h` |
| a3ab8fc | 2026-07-27 | add adjustable meter color | 6 | `src/params`, `src/styles.c`, `src/styles.h` |
| 2cb8006 | 2026-07-27 | add adjustable swr color | 4 | `src/params`, `src/tx_info` |
| 156af16 | 2026-07-27 | add flat theme and change waterfall_line color for black and flat theme | 4 | `src/params`, `src/styles.c` |
| e9bf036 | 2026-09-04 | Add fonts | 3 | `src/fonts` |
| c69eca4 | 2026-09-05 | Migrate to skin scheme for base text color and spectrum | 17 | `src/dialog.c`, `src/main_screen.c`, `src/radio.c`, `src/styles.c`, `src/styles.h`, `src/textarea_window.c` … |
| 5dde420 | 2026-09-05 | Use muted text color for freq_info | 4 | `src/main_screen.c`, `src/styles.c`, `src/styles.h` |
| c1b2ce9 | 2026-09-05 | Update spectrum and s0meter colors | 5 | `src/styles.c`, `src/styles.h` |
| b9ee5da | 2026-09-07 | Update themes | 13 | `src/dialog_ft8.c`, `src/main_screen.c`, `src/styles.c`, `src/styles.h`, `src/textarea_window.c`, `src/tx_info` |
| aee9dc8 | 2026-09-10 | Update styles and layout | 11 | `src/main.c`, `src/main_screen.c`, `src/main_screen.h`, `src/msg.c`, `src/styles.c`, `src/styles.h` |
| 8976445 | 2026-09-10 | Remove legacy theme, add panel line count calculation | 4 | `src/params`, `src/styles.c` |
| 2cbae1a | 2026-09-10 | Update styles, fix knob info visibility | 13 | `src/dialog.c`, `src/main.c`, `src/main_screen.c`, `src/main_screen.h`, `src/pubsub_ids.h`, `src/scheduler` … |
| c29d0f6 | 2026-09-10 | Add use custom spectrum color toggle | 8 | `src/cfg`, `src/styles.c`, `src/styles.h` |
| 175c6ae | 2026-09-10 | Add screen invert | 17 | `src/CMakeLists.txt`, `src/cfg`, `src/display`, `src/events.c`, `src/keypad.c`, `src/main.c` … |
| 4bcf082 | 2026-09-16 | Remove UI dependencies from services | 15 | `src/events.c`, `src/events.h`, `src/gps.c`, `src/gps.h`, `src/pubsub_ids.h`, `src/radio.c` … |
| a3b81f3 | 2026-09-16 | Update panel and styles | 2 | `src/styles.c` |
| 66163f0 | 2026-09-16 | Move display_invert to display block | 6 | `src/audio.c`, `src/cfg`, `src/display` |
| dd2d7af | 2026-09-18 | Move palettes to separate files | 4 | `src/styles.c` |
| c52d718 | 2026-09-19 | Update UI | 15 | `src/cfg`, `src/dialog.c`, `src/dialog.h`, `src/main_screen.c`, `src/msg.c`, `src/msg.h` … |
| a1994eb | 2026-09-20 | Update TX info bg colors | 1 | `src/styles.c` |
| 4fc645d | 2026-09-29 | Update panel style and positioning | 2 | `src/main_screen.c`, `src/styles.c` |

## Structure: messages, subjects, ports (9 commits, 9 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 0960183 | 2026-08-22 | Switch ObserverDelayed to use it own queue | 6 | `src/cfg`, `src/main.c` |
| cf5e99b | 2026-09-01 | Switch for MSG for radio RX/TX and low battery | 16 | `src/events.c`, `src/events.h`, `src/main.c`, `src/main_screen.c`, `src/main_screen.h`, `src/pubsub_ids.h` … |
| 5812480 | 2026-09-04 | Add default user_data to Subject::subscribe_delayed_and_notify | 1 | `src/cfg` |
| 042710b | 2026-09-04 | Switch to subjects for display fg/bg freq | 10 | `src/fonts`, `src/main_screen.c`, `src/radio.c`, `src/radio.h`, `src/styles.c`, `src/styles.h` |
| 31905b9 | 2026-09-14 | Add cleanup Observers on Subject destructor | 4 | `src/cfg` |
| 9bcd213 | 2026-09-15 | Update voice msg recording meter | 4 | `src/dsp.cpp` |
| 65a1c2a | 2026-09-15 | Fix msg show on FT8 | 3 | `src/main_screen.c`, `src/msg.c`, `src/msg.h` |
| d9c81cc | 2026-09-16 | Use subscriptions instead of direct call for radio API | 4 | `src/radio.c`, `src/radio.h` |
| 5e687e0 | 2026-09-19 | Switch for lv_msg for dialog start/stop on main_screen | 1 | `src/main_screen.c` |

## CW decoder / tune (18 commits, 7 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 09bb145 | 2026-09-05 | [WIP] | 35 | `lvgl`, `src/CMakeLists.txt`, `src/buttons.cpp`, `src/buttons.h`, `src/dialog.c`, `src/dialog_ft8.c` … |
| a6aea62 | 2026-09-07 | Update CW tune position, some fixes | 5 | `src/dsp.cpp`, `src/events.c`, `src/main_screen.c`, `src/styles.c` |
| 1b8501e | 2026-09-16 | Move remains params to cfg | 66 | `src/CMakeLists.txt`, `src/audio.c`, `src/buttons.cpp`, `src/buttons.h`, `src/cfg`, `src/dialog_ft8.c` … |
| d34f309 | 2026-09-17 | Fix filter on CW | 2 | `src/cfg` |
| 995d1eb | 2026-09-20 | Update CW tune and fix audio subscription | 3 | `src/styles.c` |
| ada52d7 | 2026-09-21 | Add bayes CW decoder/detector | 15 |  |
| 24b0de0 | 2026-09-21 | Update CW bayes | 15 | `src/CMakeLists.txt`, `tests/CMakeLists.txt` |
| bed0b8b | 2026-09-21 | Update CW detector | 6 |  |
| 065c489 | 2026-09-22 | CW: Add 25 percentile and EMA for noise floor estimation | 5 |  |
| 42c9b43 | 2026-09-23 | Move FFT calculation to cw_bayes | 10 |  |
| 8336dca | 2026-09-23 | Perparation for heterodyne for CW detection | 4 |  |
| 21d2235 | 2026-09-23 | Add CW coherent tracker | 20 |  |
| 2825846 | 2026-09-29 | Refactor CW detector module | 20 |  |
| 9be9588 | 2026-09-29 | Rename CW decoder | 26 | `src/CMakeLists.txt`, `src/cfg`, `tests/CMakeLists.txt` |
| 0e68094 | 2026-09-29 | Fix wrong WPM detecting | 7 |  |
| 4c12a5e | 2026-09-29 | Add missed CW detector | 1 |  |
| c46edbe | 2026-09-29 | Add support of the CW peak to CW decoder | 11 |  |
| caa51ac | 2026-09-29 | make elem/letter K shorter to avoid combining letters | 1 |  |

## CAT / wfview / Bluetooth (29 commits, 21 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 7eba037 | 2026-08-18 | Merge branch '0.34' | 137 | `CMakeLists.txt`, `src/CMakeLists.txt`, `src/audio.c`, `src/buttons.cpp`, `src/buttons.h`, `src/cfg` … |
| ff3a5df | 2026-09-10 | Update CAT, add BT rfcomm support | 2 | `src/CMakeLists.txt` |
| 4a6594e | 2026-09-11 | Update CAT module | 2 |  |
| b06d912 | 2026-09-11 | Add tests for CAT | 9 | `src/CMakeLists.txt`, `src/main.c`, `tests/CMakeLists.txt` |
| c21e061 | 2026-09-12 | Add vfwiev lan support | 19 | `src/CMakeLists.txt`, `src/main.c`, `tests/CMakeLists.txt` |
| 6886aa3 | 2026-09-13 | Rewrite some CAT handlers, fix to_bcd_be bug | 6 |  |
| 8770d20 | 2026-09-13 | Update score seding for wfview | 14 | `src/cfg`, `src/dsp.cpp` |
| 6eb9234 | 2026-09-13 | Add wfview audio | 7 | `src/audio.c`, `src/audio.h`, `src/dsp.cpp`, `src/dsp.h` |
| ed268ae | 2026-09-14 | Update CI-V filter bw, move resampler to common | 6 | `src/dsp.cpp` |
| 432c9d6 | 2026-09-14 | Switch to audio subscriptions | 22 | `src/dialog.c`, `src/dialog.h`, `src/dialog_ft8.c`, `src/dsp.cpp`, `src/dsp.h` |
| 91f86fe | 2026-09-14 | Move CAT LAN to CAT submodule | 10 | `src/CMakeLists.txt`, `src/main.c`, `tests/CMakeLists.txt` |
| ff91816 | 2026-09-14 | Fix dsp DC filter, LAN audio | 2 | `src/dsp.cpp` |
| 99b4070 | 2026-09-14 | wfview spectrum auto min/max, update span parsing, zoom validation | 5 | `src/cfg`, `src/dsp.cpp` |
| 2c12cad | 2026-09-15 | Update cat mode support | 2 |  |
| adda9d0 | 2026-09-15 | Fix CAT filter BW | 1 |  |
| 78b8e12 | 2026-09-15 | Fix scope span width | 1 |  |
| 5a65e44 | 2026-09-15 | Move min/max logic to dsp | 7 | `src/dsp.cpp`, `src/main_screen.c` |
| 300f9e5 | 2026-09-15 | Remove global cfg_sm | 44 | `src/audio.c`, `src/buttons.cpp`, `src/cfg`, `src/dialog_ft8.c`, `src/display`, `src/dsp.cpp` … |
| bdd6950 | 2026-09-15 | Add ports for DI and services | 19 | `src/CMakeLists.txt`, `src/app_ports`, `src/dialog_ft8.c`, `src/ft8/tx_worker`, `src/main.c`, `src/ports` |
| cc298c0 | 2026-09-16 | Add subscription for DSP | 10 | `src/app_ports`, `src/dsp.cpp`, `src/dsp.h`, `src/ports` |
| cbccd44 | 2026-09-16 | Split libraries and extract UI parts | 13 | `src/CMakeLists.txt`, `src/dialog_ft8.c`, `src/util`, `tests/CMakeLists.txt` |
| c7156df | 2026-09-16 | Remove meter_get_raw_db | 8 | `src/app_ports`, `src/dsp.cpp`, `src/dsp.h`, `src/ports` |
| e6e70cf | 2026-09-17 | Single FFT on DSP | 15 | `src/app_ports`, `src/dialog_ft8.c`, `src/dsp.cpp`, `src/dsp.h`, `src/ports`, `src/util` |
| 4f6177e | 2026-09-17 | Add wfview scope speed support | 8 | `src/app_ports`, `src/dsp.cpp`, `src/dsp.h`, `src/ports` |
| 41fb86f | 2026-09-19 | Reorganize parameters | 34 | `src/audio.c`, `src/buttons.cpp`, `src/cfg`, `src/dialog_ft8.c`, `src/dsp.cpp`, `src/ft8/tx_worker` … |
| dc60d88 | 2026-09-30 | Fix cat 1A 06 set data mode | 1 |  |
| f2dadc3 | 2026-10-02 | Use wfview requested sampling rate for audio | 10 | `src/app_ports`, `src/dialog_ft8.c`, `src/dsp.cpp`, `src/dsp.h`, `src/ports` |
| b549512 | 2026-10-02 | Add CAT CW tone | 2 |  |
| f30309b | 2026-10-02 | Add CAT band get/set (ICOM format) | 1 |  |

## CI (14 commits, 13 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| 164ecdd | 2026-09-11 | Fix GH build action | 1 | `.github` |
| 90e9a58 | 2026-09-16 | Cache some parameters for spectrum | 1 |  |
| 86db0f0 | 2026-09-30 | Add migrate_cache action | 1 | `.github` |
| 3c1b81f | 2026-09-30 | Update ccache usage on GA | 1 | `.github` |
| 733323a | 2026-09-30 | Update cache migration | 1 | `.github` |
| f5dd6de | 2026-09-30 | Update ccache GA | 1 | `.github` |
| ccdfc21 | 2026-09-30 | Add cache test workflow for Buildroot | 1 | `.github` |
| 336529a | 2026-09-30 | Update ccache command to ignore errors | 1 | `.github` |
| d1485df | 2026-10-01 | Enhance cache workflow with SDK handling and ccache limit | 1 | `.github` |
| dfd015c | 2026-10-01 | Update cache workflow to use actions/cache v5 | 1 | `.github` |
| 22a97d8 | 2026-10-01 | Fix syntax for conditional expressions in YAML | 1 | `.github` |
| dfd92ab | 2026-10-01 | Update cache conditions in cache_test.yml | 1 | `.github` |
| 45a358f | 2026-10-01 | Add toolchain external prefix for ARM architecture | 1 | `.github` |
| b27bbe5 | 2026-10-01 | Clean up toolchain prefix settings in cache_test.yml | 1 | `.github` |

## Other (35 commits, 27 touch our side)

| Commit | Date | What | Files | Touches |
|---|---|---|---|---|
| a7f7033 | 2026-07-28 | Add 500 kHz lower limit for the frequency | 1 | `src/main_screen.c` |
| ae2af22 | 2026-07-28 | Switch to epoll for main encoder, change acceleration to exponential | 10 | `src/CMakeLists.txt`, `src/events.c`, `src/events.h`, `src/main.c`, `src/main_screen.c` |
| 153fa2e | 2026-08-18 | PR review changes | 8 | `src/params`, `src/styles.c`, `src/styles.h`, `src/tx_info` |
| 9216172 | 2026-08-18 | Merge branch 'pr/dl2zw/260' | 17 | `src/params`, `src/styles.c`, `src/styles.h`, `src/tx_info` |
| 4fefb30 | 2026-08-22 | Extract indicator bar to separated widget | 7 | `src/styles.c`, `src/tx_info` |
| eb196bc | 2026-09-02 | Rename atu config | 7 | `src/cfg` |
| 0df2f6d | 2026-09-02 | Add SIGINT handler | 4 | `src/globals.h`, `src/main.c` |
| 8e340dc | 2026-09-04 | Create lock manager | 8 | `src/CMakeLists.txt`, `src/dialog_ft8.c`, `src/lock_manager`, `src/main_screen.c`, `src/main_screen.h`, `src/pubsub_ids.h` |
| 192afa4 | 2026-09-04 | Add device_removed_sig_cb to wifi | 1 |  |
| 976e053 | 2026-09-04 | Move screen size to globals | 8 | `src/buttons.cpp`, `src/globals.h`, `src/main_screen.c`, `src/styles.c` |
| d5bd0bd | 2026-09-04 | Add gracefull stop | 3 | `src/main.c`, `src/main_screen.c`, `src/radio.c` |
| 65653e6 | 2026-09-04 | Some fixes | 7 | `src/params`, `src/radio.c` |
| 37f3894 | 2026-09-07 | Reset BB on init timeout | 1 | `src/radio.c` |
| ce415cf | 2026-09-10 | Fix wifi device_removed_sig_cb | 1 |  |
| 3bf5b8a | 2026-09-14 | Fix some app stop issues | 4 | `src/main.c`, `src/radio.c`, `src/radio.h`, `src/scheduler` |
| de70919 | 2026-09-14 | Fix knobs and usb devices memory leak | 4 | `src/main.c` |
| 11e829c | 2026-09-14 | Fix memory leakage issues | 14 | `src/buttons.cpp`, `src/cfg`, `src/dsp.cpp`, `src/main.c`, `src/msg.c` |
| 778a347 | 2026-09-16 | Fix power info | 8 | `src/CMakeLists.txt`, `src/cfg`, `src/radio.c`, `src/radio.h`, `src/settings_types.h` |
| 7ff3497 | 2026-09-16 | Fix implicit-function-declaration on C++ | 1 | `src/CMakeLists.txt` |
| 21c2b93 | 2026-09-16 | Fix low power logic | 2 | `src/main_screen.c`, `src/radio.c` |
| abcd325 | 2026-09-16 | Add _GNU_SOURCE | 2 | `src/CMakeLists.txt` |
| 075703e | 2026-09-16 | Some fixes | 2 |  |
| 215cadc | 2026-09-16 | Remove clock_set_* methods | 3 |  |
| 710a747 | 2026-09-17 | Split main_screen main_screen_keypad_cb | 1 | `src/main_screen.c` |
| 5faf619 | 2026-09-17 | Restore missed main_screen voice prompts | 1 | `src/main_screen.c` |
| 449b996 | 2026-09-17 | Fix dps frames collecting | 2 | `src/dsp.cpp`, `src/dsp.h` |
| 035529d | 2026-09-18 | Fix if_shift display handling | 4 | `src/main_screen.c` |
| 0b01da3 | 2026-09-19 | Migrate from C API for c++ files | 7 | `src/cfg`, `src/dsp.cpp` |
| da454b4 | 2026-09-29 | Use layer_top for messages | 5 | `src/main_screen.c`, `src/msg.c`, `src/msg.h` |
| 47b9e5f | 2026-09-30 | Bump version | 2 | `.github` |
| aca1ee0 | 2026-09-30 | Bump version | 1 |  |
| 62a5292 | 2026-10-01 | Fix sp mode | 2 | `src/radio.c` |
| 16ff2e9 | 2026-10-02 | Fix some issues | 3 |  |
| 195c01d | 2026-10-02 | Fix wrong freq indication below 1 MHz | 1 |  |
| 416377c | 2026-10-02 | Bump version | 1 |  |

