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
