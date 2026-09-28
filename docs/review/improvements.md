# Improvements found in the feature review

Ways to make the code faster, lighter, simpler, safer or better tested,
found by reading it feature by feature (check sheet:
[features.md](features.md)). These aren't bugs: the app works as it is.
**No code was changed.** Started 2026-09-28 on `main` at `9391f80`; line
numbers refer to that commit.

- **Kind:** *efficiency* (CPU, memory, SD card writes), *simplify*
  (less or clearer code), *robustness* (safer against a future change or
  an odd input), *tests* (a gap in the unit tests or the harness).
- **Worth:** *high* (clear win, small change), *medium*, *low* (nice to
  have).

## Summary

| ID | Feature | Improvement | Kind | Worth |
|---|---|---|---|---|
| I-01 | F02, F04 | Test closing the app while a frame is keyed | tests | high |
| I-02 | F02 | Synthesise TX audio without 16 MB of temporary buffers | efficiency | medium |
| I-03 | F10 | Keep the JS8 presets out of upstream's migration numbers | robustness | medium |
| I-04 | F03 | Plan each message once, not twice | efficiency | low |
| I-05 | F05, F89 | One list of popups instead of three | simplify | low |
| I-06 | F01 | Save the learned TX gain once per message, not per frame | efficiency | low |
| I-07 | F11–F20 | A desktop-parity test table for the auto-reply rules | tests | medium |
| I-08 | F11, F20 | Decide once, when the reply is actually sent | simplify | low |
| I-09 | F11 | Prune the auto-reply rate-limit map | efficiency | low |
| I-10 | F22 | Test the receiver's stall path | tests | low |
| I-11 | F31 | Size the groups setting for ten groups | robustness | low |
| I-12 | F32, F33, F38 | One safe "write the file" helper for all three data files | simplify | medium |
| I-13 | F32, F33, F38 | Tests for damaged, unreadable and half-written data files | tests | medium |
| I-14 | F51 | Restyle the TX bar and the waterfall frame only when they change | efficiency | high |
| I-15 | F45, F46, F59 | Work out each row's colours and fields once, not on every redraw | efficiency | medium |
| I-16 | F42, F43 | Cheaper waterfall rows: no malloc/qsort per row, direct pixel writes | efficiency | low |
| I-17 | F58, F62 | Forget expired stations; look one up without copying the list | efficiency | medium |
| I-18 | F63, F67, F40 | One "from a popup into the keyboard" helper | simplify | medium |
| I-19 | F75 | Keep `MODE_JS8`'s number clear of upstream's | robustness | low |

## Batch 1: Transmitting and the radio

### I-01. Test closing the app while a frame is keyed — tests, high

`tools/js8_ui_harness` closes the app with popups open (`[gen]`, `[log]`,
`[inbox]`) but never during a transmission, which is how B-01 went
unseen. Its `tx_player_play` stub already polls `abort_check`, so a
scenario "queue a message, wait for `[radio] PTT on`, `dialog_destruct()`"
would crash under ASan today. Also a unit test: `js8_tx_destroy()` while
`play` polls `js8_tx_stopping()`.

### I-02. Synthesise TX audio without 16 MB of temporary buffers — efficiency, medium

`synth_frame()` (`src/js8/tx.cpp:138-175`) builds the whole frame's
phase steps as `double`s, then a `float` waveform, and the play callback
(`js8_tx.cpp:63-65`) copies it again to `int16_t`. At 44.1 kHz a Slow
frame (1.11 M samples) needs ~9.1 MB + 4.5 MB + 2.2 MB, Normal half that,
and every sample does a `std::fmod`. It runs on the TX thread between
frames (about 1 s of slack at Turbo). The Gaussian pulse spans only three
symbols, so the phase step can be computed per sample from a 3-symbol
window, the phase kept in range with a subtraction, and `int16_t`
written directly: one 2.2 MB buffer (or none, synthesising part by part as
`tx_player_play` plays), same waveform bit for bit apart from rounding.

### I-03. Keep the JS8 presets out of upstream's migration numbers — robustness, medium

`src/params/migrations.c` adds `_4_add_js8_presets` and
`_5_add_ghostnet_presets` after upstream's 0–3. `params.db` lives on the
DATA partition (`/mnt`), which survives reflashing. When upstream adds its
own migration 4 and 5, a database already at version 5 skips them: after
we merge upstream (unless we renumber carefully), and whenever someone
goes back to an upstream image with the same card. The inserts are
idempotent (`INSERT OR IGNORE` + `UNIQUE(freq, type)`), so they can simply
run at every start outside the version sequence, or keep their own version
table.

### I-04. Plan each message once, not twice — efficiency, low

`tx_queue_at()` (`src/dialog_js8.c:1666-1676`) calls `js8_tx_preview()`
and then `js8_tx_send()`, which runs `plan_message()` again: two frame
builds and two decode-backs on the LVGL thread for every message. Have
`js8_tx_send()` return the preview, frame count and seconds (or take the
plan).

### I-05. One list of popups instead of three — simplify, low

`any_popup()` (`:3965`), `close_popups()` (`:2752`) and `destruct_cb()`
(`:2263-2294`) each name all eight popups. A popup missing from one of
them is exactly the "GEN with the Query list open crashed the app" bug
fixed earlier. One table of `{lv_obj_t **list, close_fn}` used by all
three keeps them in step.

### I-06. Save the learned TX gain once per message, not per frame — efficiency, low

`tx_player_play()` (`src/tx_player.c:114`) calls `params_float_set()`
after every frame; the params thread (`params.c:447`, every 100 ms) then
writes `params.db` on the SD card. A 20-frame message writes it 20
times. Save when the message ends, or only when the value moved by more
than ~0.1 dB. Shared with FT8, so a change here touches both apps.

## Batch 2: Automatic sending

### I-07. A desktop-parity test table for the auto-reply rules — tests, medium

`tests/test_js8.cpp` checks our own expectations of `process()`. The
frame encoder was made bit-for-bit with desktop by building desktop's code
and comparing (patch 11); `processCommandActivity()` is too tied to Qt for
that, but a table of (incoming text, switches, held messages) → (desktop's
reply, what it stores) written from reading desktop, one row per branch
(SNR?, INFO?, HEARING?, relays with and without a command, MSG / MSG TO:
/ QUERY / QUERY MSGS / QUERY CALL, @ALLCALL and group forms, the 55-min
cooldown, B-04's open-buffer rule), would catch drift both ways when
desktop changes. Plus a harness scenario for B-04: a heartbeat arriving
between the frames of a message to us.

### I-08. Decide once, when the reply is actually sent — simplify, low

`AutoPolicy::decide()` runs when the message is decoded, and `auto_send()`
checks the switches again because they "may have changed while it
waited"; the Turbo rule for HB ACKs is only in `auto_send()`, and the
rate-limit record (`js8_auto_sent`) only happens if it's queued. Keeping
the decoded reply and calling `decide()` once at send time would put all
the rules in one place (and is where B-04's hold-off would go too).

### I-09. Prune the auto-reply rate-limit map — efficiency, low

`AutoPolicy::last_sent_` (`autoreply.hpp:128`) gets a key per station and
command answered and never drops one; `autop` lives until power-off. A
relay or heartbeat station running for days keeps every station it ever
ACKed. Tiny per entry, but pruning entries older than the longest window
(55 min once B-05 is fixed) at each `sent()` keeps it bounded.

## Batch 3: Receiving and decoding

### I-10. Test the receiver's stall path — tests, low

`tests/test_js8.cpp` covers the realign (`:466`) and the TX gap fill
(`:483`) but not the "worker fell more than 5 s behind" branch
(`receiver.cpp:175-180`), where B-11 lives. A test feeding 6 s of audio
in one burst and checking that no frame from before the burst decodes
again would pin it down.

### I-11. Size the groups setting for ten groups — robustness, low

`js8_groups_normalise()` keeps up to 10 groups, but `groups_text` is 96
characters (`dialog_js8.c:294`): ten long group names (up to about 10
characters each, plus spaces) don't fit, and `copy_str()` cuts the last
one mid-name, which then silently matches nothing (or the wrong group).
Either a bigger buffer or stop at the last group that fits whole.

## Batch 4: Inbox, saved data, settings

### I-12. One safe "write the file" helper for all three data files — simplify, medium

`Inbox::save()` and `HeldMessages::save()` (`inbox.cpp:102-120`,
`:214-233`) are the same code twice (header, `.tmp`, `fsync`, `rename`,
clean-up), and `save_texts()` doesn't use it at all (B-15). One helper,
`write_file_atomically(path, lines)`, with the directory `fsync` and a
load-side `.tmp` fallback (BH-16), would fix B-15 and BH-16 in one place.
The two `load()`s also share their line splitting and `try`/`catch`
parsing.

### I-13. Tests for damaged, unreadable and half-written data files — tests, medium

`tests/test_js8.cpp` checks that files from the last release load
(`:2500`) and that the Inbox survives a reload (`:1538`). Nothing checks
what happens when a file exists but can't be read (B-14), when only the
`.tmp` survived (BH-16), or when `js8_texts.txt` is cut short (B-15). All
three are easy to set up in a temporary directory.

## Batch 5: The screen

No new bugs here (BH-18, info rows vanishing on a rebuild, is still
there). Three ways to draw less.

### I-14. Restyle the TX bar and the waterfall frame only when they change — efficiency, high

`tx_timer_cb()` calls `update_tx_bar()` every 250 ms, idle or not
(`src/dialog_js8.c:1567-1579`), and `update_tx_bar()` always sets the TX
bar's background colour, its text, and the waterfall's border width and
colour (`:1590-1636`). In LVGL 8.3 every style setter ends in
`lv_obj_refresh_style()` → `lv_obj_invalidate()`, changed or not
(`lvgl/src/core/lv_obj_style.c:270-276`, `:167-173`), and
`lv_obj_area_is_visible()` grows the area by 5 px for the object *and* for
its parent (`lv_obj_pos.c:896`, `:908`, `lv_obj_get_transformed_area()`
→ `lv_area_increase(area, 5, 5)`). For the waterfall that is exactly the
path 72b134e removed for waterfall rows: the area spills past the opaque
`wf_box`, so LVGL redraws from the dialog background (the 1 MB
`dialog.bin`, read and blended line by line) — now four times a second,
all the time JS8 is open. `lv_label_set_text()` also re-lays out the TX
bar each time.

**Improvement:** keep the last state, text and colours; call the setters
only when they change (the frame changes only when keying starts and
stops). Worth measuring first: the harness's `wf_bench` (`main.cpp:272`)
calls `lv_refr_now()` in a tight loop and never runs LVGL timers, so it
can't see this; a case that also runs `lv_timer_handler()` would.
Likely rather than confirmed: read in the code, not measured.

### I-15. Work out each row's colours and fields once, not on every redraw — efficiency, medium

The list is see-through over the waterfall, so every waterfall row
(15 a second) redraws every visible list row. For each one,
`table_draw_cb()` runs `row_command()` → `js8_command_span()`
(`js8_ops.cpp:133-157`: a `std::string` copy, `parse_directed()` with
more copies), then `draw_recoloured()` measures the text and draws it a
second time; in the Stations view `table_draw_end_cb()` calls
`station_fields()` (age, distance, bearing, formatted) per row per frame.
The command span only depends on the message: compute it once in
`add_message()` and keep it with the history slot; the station fields
change once per `rebuild_station_rows()` (every 5 s). The README's
"solid (not see-through) list" idea goes further: with an opaque list only
the 55-pixel strip above it needs redrawing per row.

### I-16. Cheaper waterfall rows: no malloc/qsort per row, direct pixel writes — efficiency, low

On the receiver thread, `wf_emit_row()` (`dialog_js8.c:1323-1360`)
allocates two buffers per row and `qsort`s all 771 values to find the
30th percentile: a preallocated buffer and a selection
(`nth_element`-style quickselect, O(n)) do the same. In the widget,
`lv_waterfall_add_data_with_ts()` paints each pixel through
`lv_img_buf_set_px_color()` (format checks per call); writing the row as a
`lv_color_t` array is simpler. `line_buf` (`lv_waterfall.c:90`) is
allocated and never read (already so upstream).

## Batch 6: Selecting, navigating, Stations

### I-17. Forget expired stations; look one up without copying the list — efficiency, medium

`StationList` (`src/js8/stations.cpp`) never erases anything: expired
stations are only filtered out when read (`sorted()`, `:85-97`), and the
lists live while the radio is on. Every read copies and `stable_sort`s all
of them into `js8_station_t`s (`js8_ops.cpp:159-180`), and that happens
several times per decoded message on the LVGL thread: `find_station()`
(which lists 200 stations to find one), `heard_stations()` in
`handle_incoming()`, `free_hb_offset()` for every heartbeat and HB ACK,
and `rebuild_station_rows()` every 5 s in the Stations view. After days
on a busy band that's thousands of entries copied and sorted per message.
Erase entries past the expiry in `add()` (or every few minutes), and add a
`js8_stations_find(call)` that looks the key up directly. It also fixes
the 200-station blind spot in B-20.

## Batch 7: Sending by hand

### I-18. One "from a popup into the keyboard" helper — simplify, medium

The same six steps are written out in `texts_item_cb`, `query_msg_cb`,
`freq_item_cb`, the Inbox items (`inbox_leave` + `msg_compose`), the log,
alerts, spot and beacon forms: take the list's buttons out of the group,
`lv_obj_del_async` the list, NULL its pointer, set `edit_target`,
`compose_open(prefill)`, `lv_group_set_editing(true)`, then a hint. Each
copy has to get the order right (memory: "lists opening the keyboard
remove their buttons from the group first"), and none of them undoes
`edit_target` when `compose_open()` refuses (B-16). One
`popup_to_keyboard(list, target, prefill, hint)` would hold the rule and
the reset in one place.

## Batch 8: Logging and APRS

No new bugs: the QSO tracker, the ADIF record, the Log popup, the APRS
formats (message IDs, position ambiguity, the POTA and SOTA gateways'
formats) and the Maidenhead conversion all read correctly. Still there
from the bug hunt: BH-6 (grids from any grid-shaped word), BH-12 (the log
prompt holds up automatic TX), BH-13 ("73" anywhere ends the QSO), and the
APRS length check counting the `{NN}` ID. Correction to the bug hunt's
"typed log grid" item: a typed grid *is* upper-cased (the keyboard turns
lowercase into capitals as it's typed), but it still isn't checked, so
"HOME" can reach the log.

### I-19. Keep `MODE_JS8`'s number clear of upstream's — robustness, low

`qso_log.db` (on the DATA partition, kept across reflashing) stores the
mode as an integer (`qso_log.c:188`, `mode INT NOT NULL`), and we appended
`MODE_JS8` to `qso_log_mode_t` (`qso_log.h:40`), so it's 8. Upstream's
enum stops at `MODE_RTTY` = 7; the next mode they add will also be 8.
Merging upstream then needs `MODE_JS8` kept at 8 and theirs moved, and a
card used with an upstream image would show our JS8 QSOs as their new mode
(worked-before marks, ADIF export). Same kind of risk as I-03: a fixed,
high value (e.g. 100) for `MODE_JS8` now, before more records are written,
avoids it; the few existing records would need a one-off update.
