This is the first of the Murus team's PRs (Hany El Imam, 1KO125; see discussion #302). It contains the shared pieces that the WeFax, SSTV, NavTex and DSC apps build on. None of it shows on the radio until an app uses it, **except the recorder, which now writes WAV instead of MP3**.

Base: OpenX6100 main 9714584; it merges with today's main (49328dd) and with each of Kris's open app PRs (#271, #272, #274) without conflicts. With #276 (RTTY) the only clash is the source list in `src/CMakeLists.txt`.

## What it adds

- **`ui_list.c/.h`** (new): a scrollable list widget with the look of Hany's Channels window. It draws its own rows instead of using `lv_table`. MFK/arrows move one row (wrapping round), PgUp/PgDn and the VOL knob jump several rows, Home/End go to the ends, and an MFK press calls the app. The widget keeps no data: the app passes each visible row through a callback. (From the code.)
- **`app_info.c/.h`** (new): the apps' "Info" window, which opens over the app and shows a description, credits and legal notice. MFK scrolls, a press or *Close Info* closes, VOL still works, and reception continues underneath. It holds the texts for WeFax, NavTex, DSC and SSTV. (From the code.)
- **`recorder.c`**: the main-screen recorder now writes **WAV, mono, 12 kHz, 16-bit** (`/mnt/rec/REC_<date>_<time>.wav`) instead of MP3 at 48 kHz. This is the format that the apps' *Test* function plays back into their decoders. The audio comes from the same DSP float path the decoders use (DC blocker plus resampling to 12 kHz), not the raw 48 kHz capture. Other changes: it creates `/mnt/rec` if it is missing, uses `localtime_r`, clips loud peaks instead of letting them wrap round, deletes a file that failed to open, and ignores a second start or stop. (From the code.)
- **`spectrum.c/.h`**: tuning markers for an app, through a small API (our commit, below):
  - `spectrum_set_markers(from_hz, to_hz)` and `spectrum_clear_markers()`
  - the app calls them when it opens and closes
  - the lines are drawn like the RTTY markers, on the sideband's side
  - WeFax will set 1075/1925 Hz, NavTex and DSC 915/1085 Hz.

## Outside its own files

- `src/recorder.c`: **this changes every user's recordings** (see above and the questions below).
- `src/spectrum.c`, `src/spectrum.h`: the marker API. Nothing calls it yet.
- `src/CMakeLists.txt`: adds `ui_list.c app_info.c`.

## Commits

| Commit | Author | What |
|---|---|---|
| 569d3a0 | Hany El Imam | His files and changes exactly as received in the 1KO125 1.0.2 beta 4 drop. `ui_list.*`, `app_info.*` and `recorder.c` are byte-identical to it. `spectrum.c` is his hunk, which applies cleanly on main |
| 7a796b4 | ours | Replaces his `#include "dialog_wefax.h"` / `dialog_navtex.h` / `dialog_dsc.h` and the `dialog_*_is_active()` checks in `spectrum.c` with `spectrum_set_markers()` / `spectrum_clear_markers()`, so `spectrum.c` doesn't need to know about any app. The drawing is the same as his |
| ddf3f84 | ours | Hany's files under **LGPL-2.1-or-later**, as he agreed on #302 (copyright lines kept, the GPLv3 7(b) term dropped, `ui_list.*` given a header) |
| e574b53 | ours | The Info window's credits text: LGPL-2.1-or-later and a link to the source instead of the missing `/usr/share/x6100/COPYING`, "Originally from the 1KO125 firmware, part of OpenX6100"; Hany's credits unchanged (he agreed on #302) |

Hany's commit **does not build on its own**: his `spectrum.c` includes the apps' headers, which aren't in this PR. The next commit fixes that. Please merge with a merge commit (or squash), so no commit on main is left unbuildable.

## Checked so far

- `git diff v1.0.2 murus-drops` matches the brief's table for these files. `recorder.c` hasn't changed on main since v1.0.2, so his version replaces it unchanged. His drop also deletes `buildroot/*`, which is an artefact of his tree, so it is left out.
- `gcc -fsyntax-only -Wall -Wextra` on the host (GCC 13) against this tree's LVGL submodule, X6100Control v0.16.1 headers, libsndfile 1.2.0, liquid-dsp 1.4.0 and pulseaudio 16.1 headers:
  - `ui_list.c`, `app_info.c`, `recorder.c` and `spectrum.c` compile with no errors.
  - The only warnings are two `-Wsign-compare` in `spectrum.c`, which main's `spectrum.c` already has.
  - The three headers also compile from C++.
- The recorder was run on the host with real libsndfile and stubbed DSP/GUI calls. It produces `REC_….wav`, 1 channel, 12000 Hz, 16-bit, the expected frame count, and an over-range sine clipped to ±32767/-32768 rather than wrapped. A double start or stop does nothing.
- The new float subscription is one more of the 8 resampled slots. FT8, RTTY and CW use the same path. An exclusive raw subscriber (voice-message recording) stops the float subscribers as well as the raw ones, the same as before for the recorder.
- No transmit code. The only radio call is `radio_change_vol()` in the Info window. `main_screen.c` is untouched.
- **The image's GCC 12:** `ui_list.c`, `app_info.c`, `recorder.c` and `spectrum.c` compile with the image's toolchain (2026-10-18), no errors.
- **On my radio:** a test build with all of Hany's apps on top of this. The apps opened and worked in a short try; I haven't been through the test list below yet.

## Radio test list

1. Recorder (main screen): start, talk or listen for 30 s, then stop.
   - A `REC_….wav` appears in the Recorder window and plays back normally.
   - The spectrum's centre line turns red while recording.
2. Recorder with `/mnt/rec` missing (rename it first): starting it creates the folder.
3. Old `.mp3` recordings still show and play in the Recorder window.
4. Voice messages (still MP3) record and play as before.
5. Spectrum: nothing new should appear, since no app sets markers yet. RTTY markers are unchanged.
6. (Later, with the WeFax PR: the WeFax markers show while the app is open and go away when it closes.)

## Open questions

- **Georgy (gdyuldin):** are you OK with the recorder switching to WAV 12 kHz for everyone?
  - Files get bigger: 24 kB/s, about 86 MB per hour.
  - Audio is limited to 6 kHz, which is plenty for SSB.
  - The audio now passes through the DC blocker instead of being the raw capture.
  - It no longer needs LAME in libsndfile. Voice messages still use MP3.
  - Alternative, if you'd rather: keep MP3 for the main-screen recorder and add a WAV option.
- **Georgy:** is the marker API shape fine? It holds one pair, set and cleared by the app that owns the screen. Should RTTY move onto it later?
- **Hany (@TheMurusTeam):** the Info texts still mention SSTV's "Web Manager (http://<radio IP>/sstv)" and the fldigi/Venturi credit for NavTex (which goes away with your own decoder). We'll adjust those as each app comes in.

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
