# Ultrareview, 2026-10-10

Three cloud reviews of our own code (PRs #5, #6, #7 on randal007/x6100-js8,
review-only slices of `main`; the comments are on the PRs). Ten findings
after merging the duplicates (the `rtc_done` one was found twice). Each
was checked against `main` at `8e440ea`.

Verdicts and effort as in [fix-plan.md](fix-plan.md): **S** under an
hour, **M** a few hours.

## Packages, in order

### UR1: Received text can't crash the app (S) — done 2026-10-10

`parse_snr` (all three) takes a sign and 1-3 digits; `delivered_signature`
reads a NEXT MSG ID of up to 6 digits, a longer one as no next id
(desktop's `toInt()` gives 0). The realistic form on the air is free
text, `K2XYZ HI SNR +9999999999`: a plain `K2XYZ SNR +9999999999` goes
out as the SNR command `SNR +09 999999999`. Unit tests *a long number
after SNR is no report, and no crash* and the signature test's long id;
both threw `stoi` on the old code.

| # | Finding | Where | Verdict |
|---|---|---|---|
| R1 | A long digit run after `SNR` in a received frame (`MYCALL: SNR +9999999999`) makes `std::stoi` throw `out_of_range`; nothing catches it, so the app aborts. Anyone on the air can send it as free text. Same pattern in the `NEXT MSG ID` capture. `msg_id_arg()` / `msg_id_offered()` already cap the length at 6 digits. | `stations.cpp` `parse_snr`, `history.cpp` `parse_snr`, `qsolog.cpp` `parse_snr`, `inbox.cpp` `delivered_signature` | **Fix**: cap the digit run (SNR: 3 digits; message ID: 6, as the others), unit tests with a long run |

### UR2: Small fixes (S, all together) — done 2026-10-10

As in the verdicts. R4: `on_audio` makes no waterfall rows without `sg`
or `psd` (the decoder carries on). R3: INT exits 130, TERM 143. Harness
ONLY_GPS, ONLY_AUTOCQ, ONLY_STQRZ, ONLY_MAP and ONLY_WFTIME pass.

| # | Finding | Where | Verdict |
|---|---|---|---|
| R2 | `rtc_done` is set before `pthread_create()`; if the thread can't start, the battery clock isn't tried again this power-on and nothing is shown. (Found by two reviews.) | `dialog_js8.c` `rtc_tick` | **Fix, small**: set `rtc_done` only once the thread is running |
| R3 | Ctrl-C / SIGTERM runs `finish` but doesn't exit: the script goes back into its wait loop (up to 3 minutes) with the bluetoothctl session already closed. | `rootfs/usr/bin/x6100-bt-pair` | **Fix, small**: `trap 'finish; exit 130' INT TERM` |
| R4 | `psd = malloc(...)` not checked; `wf_emit_row()` writes through it. 32 KB at most, so unlikely, but the map's buffers next to it are checked. | `dialog_js8.c` `rx_start` | **Fix, small**: skip waterfall rows when `psd` (or `sg`) is NULL |
| R5 | `starts_with_call()` called twice in a row with the same arguments. | `dialog_js8.c` `tx_queue_at` | **Fix, small** |
| R6 | Comment says the @ ALLCALL cooldown is 15 min; it is 55 (`HB_ACK_REPEAT_MS`, decision D4). | `autoreply.hpp:62` | **Fix, small** |

### UR3: NR / NB / DNF stay off while JS8 is open (M, radio check) — done 2026-10-10 (B), radio check open

Option B (VE7NHW): JS8 subscribes to `cfg.dsp.nr/nb/dnf/dnf_auto` while
open (`dsp_watch[]`, `dsp_held_cb` → `radio_set_rx_dsp_off(true)`),
unsubscribed on close before the settings go back. Subscribers run in
the order they subscribed, so radio.c's (from start-up) sends the new
value and JS8's turns it off again right after; both on the thread that
changed the setting (CAT's), radio calls only. `radio.c` untouched.
Harness ONLY_DSPHOLD (each of the four turned on from another thread:
setting kept, radio off; back to the settings on close; not held after);
with the subscriptions taken out, the radio stayed on. Still to do at
the radio: NR on over CAT with JS8 open.

| # | Finding | Where | Verdict |
|---|---|---|---|
| R7 | JS8 turns NR, NB and DNF off on the radio once at open, without changing the saved settings. The settings stay wired to the radio (`radio_init` subscriptions), so changing one while JS8 is open (CAT, or a front-panel control if it reaches through) turns it back on for the rest of the session; JS8 decodes through it. | `radio.c` `radio_set_rx_dsp_off` and the `cfg.dsp.*` subscriptions | **Decide** (shared GUI code): (A) remember "held off" in `radio.c` and have the NR/NB/DNF/auto-notch subscribers send off while it's held; touches every app's DSP path, small but shared. (B) JS8 only: JS8 subscribes to the four settings itself and sends "off" again whenever one changes while it's open. **Recommend B.** Check at the radio: toggle NR over CAT with JS8 open. |

### UR4: Cleanups — decided 2026-10-10 (VE7NHW: as recommended)

R9 done: both functions and the `rows` counter that only fed them
removed (shared widget; FT8 calls none of them; the ring buffer stays).
R10 won't fix. R8 reviewed (below), to do before the JS8 code goes
upstream.

**R8 review:** 16 copies in 11 files: `upper()` x7 (assembler,
autoreply, callsign_place, classify, commands, directed, macros; all the
same), `words()` x4 + assembler's `split_ws()` (all the same: split on any
whitespace), `trim()` x4 in three kinds: spaces only (autoreply,
history), space/tab/CR/LF (assembler), any whitespace (callsign_place,
which reads cty.dat). Not copies, left alone: directed's `ltrim()`,
inbox's `split(char)`, alerts' `split(seps)`. The `trim()` kinds only
differ on tabs and line ends, which decoded text never has (the JS8
alphabet has none) and js8_texts.txt loses at load (`\r` cut); so one
`trim()` stripping any whitespace changes nothing on the air (a tab at
the end of an INFO typed on a PC would be trimmed, not sent as a space).
Plan: header-only `src/js8/strutil.hpp` (`upper`, `trim`, `words`), the
copies removed; unit tests + full harness run.

| # | Finding | Where | Verdict |
|---|---|---|---|
| R8 | `upper()` / `trim()` / `words()` copied into about 12 files under `src/js8`; the copies differ (`trim()` strips spaces in some, all whitespace in `callsign_place.cpp`). | `src/js8/*.cpp` | **Later** (M): one `strutil.hpp`; check each caller's whitespace before merging the `trim()`s |
| R9 | `lv_waterfall_get_rows()` / `lv_waterfall_fill_rect()` (ours, from 2402951) have no callers; `js8_wf.c` does the same. | `src/widgets/lv_waterfall.c/.h` | **Fix, small**: remove them (keeps the shared widget closer to upstream) |
| R10 | Hand-written Kaiser polyphase resampler instead of liquid-dsp's `resamp_rrrf`. | `src/js8/resampler.cpp` | **Won't fix**: it's tested (length, in-band tones, image rejection) and decodes on the air; swapping the DSP risks decode regressions for no user gain |
