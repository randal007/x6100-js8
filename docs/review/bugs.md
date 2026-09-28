# Bugs found in the feature review

Bugs found by reading the code feature by feature, following the check
sheet in [features.md](features.md). **No code was changed.** Started
2026-09-28 on `main` at `9391f80`; line numbers refer to that commit.

Findings from the earlier hunt ([bug-hunt-2026-09-28.md](../bug-hunt-2026-09-28.md),
`BH-n`) aren't repeated here.

- **Confidence:** *confirmed* (the failure path is clear in the code),
  *likely* (the code allows it; not reproduced), *possible* (worth a look).
- **Severity:** *high* (wrong frequency or power on the air, the radio
  left in a wrong state, messages or settings lost), *medium* (wrong
  behaviour you'd notice), *low* (cosmetic or rare).

## Suggested fix order

Combining this review with the earlier bug hunt (BH-n):

1. **B-01** (crash with the radio keyed when the app closes mid-frame),
   with its harness test (I-01).
2. **On the air:** BH-10 (band keys mid-transmission), B-07 (WSPR guard).
3. **Lost data:** BH-8, B-14, B-17, B-15, BH-16, with I-12 and I-13
   (one safe file writer and tests for damaged files).
4. **The unattended station, as desktop:** B-04, B-05, BH-7, BH-12,
   BH-1, BH-2, B-25.
5. **Smoothness:** I-14 (TX bar redraw), I-17 (station lists), I-15.
6. The rest, low severity, as convenient; B-16 and B-22 are quick.

## Summary

| ID | Feature | Bug | Severity | Confidence |
|---|---|---|---|---|
| B-01 | F02, F04, F05 | ~~Closing the app during a transmission crashes it, radio keyed until it restarts~~ **fixed in 2ed19a8** | high | confirmed (reproduced) |
| B-02 | F05 | Switching off with JS8 open skips its close: back on the JS8 dial, USB-D, 200–3000 Hz (won't fix: user's decision) | low | confirmed |
| B-03 | F08 | Heartbeat offset chosen differently from desktop | low | confirmed |
| B-04 | F11, F14 | Automatic replies go out while a message to us is still arriving | medium | confirmed |
| B-05 | F14 | Heartbeat ACKs every 15 min per station; desktop waits 55 | medium | confirmed |
| B-06 | F12 | Our 5-minute guard drops a repeated AGN? and relayed questions | low | confirmed |
| B-07 | F07, F11 | No WSPR guard band: can transmit on top of WSPR on 30 m (won't fix: user's decision) | low | confirmed |
| B-08 | F13, F15 | Heartbeat and auto CQ timing follow the older desktop | low | confirmed |
| B-09 | F15 | Holding CQ while sending: first CQ a whole interval later | low | confirmed |
| B-10 | F11 | An `@APRSIS MSG` without `TO:` is kept and ACKed | low | confirmed |
| B-11 | F22 | After a receiver stall, minute-old audio can decode again as new | low | possible |
| B-12 | F26, F30 | A message still arriving when JS8 closed can take over a new message's row | low | likely |
| B-13 | F28 | Any first word starting with "CQ" makes a message a CQ | low | confirmed |
| B-14 | F32, F33, F41 | ~~An Inbox file that can't be read is overwritten by the next save~~ **fixed in 4bfb8d6** | medium | confirmed |
| B-15 | F38, F41 | ~~`js8_texts.txt` is rewritten in place: a power cut can wipe the settings~~ **fixed in 4bfb8d6** | low | confirmed |
| B-16 | F40, F63 | Without a callsign the keyboard never opens, and leaves a stale edit mode behind | low | confirmed |
| B-17 | F34, F41 | ~~A message that couldn't be saved is still ACKed~~ **fixed in 4bfb8d6** | low | confirmed |
| B-18 | F12, F62 | QUERY CALL about a station heard only through a relay gets no answer | low | confirmed |
| B-19 | F56 | The VOL knob does nothing while most popups are open | low | confirmed |
| B-20 | F60, F83 | "New station" alerts again for a station that dropped off the list | low | likely |
| B-21 | F66 | AGN? repeats what we last queued, not what went out | low | confirmed |
| B-22 | F63 | The keyboard refuses some characters JS8 can send ($, %, [ and others) | low | confirmed |
| B-23 | F63, F72 | A message that's too long freezes the frame count instead of saying so | low | confirmed |
| B-24 | F74, F76 | A QSO left unlogged stays first in Log QSO for good | low | confirmed |
| B-25 | F88 | A stalled screen can silently drop decoded messages (64-item scheduler queue) | low | possible |
| B-26 | F83 | Alert words miss a word with punctuation attached ("SOTA,") | low | confirmed |
| B-27 | F99 | `x6100-flash` with no argument writes an old build (18ebf06) | low | confirmed |

## Batch 1: Transmitting and the radio

### B-01. Closing the app during a transmission crashes it — high, confirmed (reproduced)

**Fixed in 2ed19a8:** `js8_tx_destroy()` stops and joins the TX thread
before resetting the pointer; unit test and harness `ONLY_TXSAFE`.

**Where:** `src/js8/js8_tx.cpp:119-123` (`js8_tx_destroy`), `:115-117`
(`js8_tx_stopping`); `src/dialog_js8.c:1462-1465` (`tx_abort_check`),
`:1561-1565` (`tx_stop_all`), `:2249` (`destruct_cb`);
`src/main_screen.c:530` (`apps_disable`).

**What goes wrong:** `js8_tx_destroy()` calls `t->tx.reset()`.
`std::unique_ptr::reset()` sets the pointer to null *first* and only then
runs `~Transmitter()`, which stops and joins the TX thread. While a frame
is on the air, that thread is in `tx_player_play()`, which calls
`tx_abort_check()` every 2048 samples (~46 ms). That calls
`js8_tx_stopping(tx)`: the global `tx` is still set, but `t->tx` is now
null, so `t->tx->stopping()` reads through a null pointer and the app
dies.

**When:** anything that closes the app while a frame is keyed: the GEN,
APP, KEY, DFN or DFL keys (`apps_disable()` → `dialog_destruct()`), or
starting another app. ESC is safe (the first ESC only stops TX). A frame is
keyed about 85% of the time while a message is going out at Normal speed.

**On the radio:** the app dies with the modem keyed. Nothing unkeys it
until `x6100_daemon` has restarted the app and `x6100_control_init()`
rewrites every base register from zero: PTT (and the amp's PTT line) stays
on for the restart time, a few seconds, not measured. `destruct_cb` never
runs, so the radio also comes back on the JS8 dial in USB-D with the
200–3000 Hz filter, and the pre-JS8 memory slot isn't restored (as in
B-02). The app log from the crash goes to `app_logs`.

**Reproduced:** a scratchpad program linked against the harness's
`libJS8.a` did what the app does (a `play` callback polling
`js8_tx_stopping(tx)`, then `js8_tx_destroy(tx)` while keyed). ASan:
`SEGV on unknown address 0x79 ... in Transmitter::stopping() tx.hpp:132,
js8_tx_stopping js8_tx.cpp:116`. The harness never closes the app while a
frame is keyed (its TX stub polls `abort_check` too, so it would catch
this: see I-01).

<details><summary>The reproduction (build against the harness's ASan libraries)</summary>

```c
/* gcc -g -fsanitize=address,undefined -std=gnu11 -I$REPO/src -c t.c
 * g++ -g -fsanitize=address,undefined t.o $B/js8/libJS8.a
 *     $B/js8core/libjs8core.a -lfftw3f -lpthread -o t
 * with B=$REPO/tools/js8_ui_harness/build */
#include "js8/js8_tx.h"
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
static js8_tx_t *tx;          /* the dialog's global */
static atomic_bool keyed;
static bool play(int16_t *s, unsigned n, int index, int count, void *ctx) {
    atomic_store(&keyed, true);
    for (int k = 0; k < 300; k++) {             /* tx_player: a part ~46 ms */
        if (js8_tx_stopping(tx)) return false;  /* tx_abort_check */
        usleep(43000);
    }
    return true;
}
int main(void) {
    js8_tx_cb_t cb = {.play = play};
    tx = js8_tx_create(48000, 1325, &cb);
    char err[64] = "";
    js8_tx_send(tx, "VE7NHW", "CN89", "CQ CQ CQ", 1500, JS8_SPEED_TURBO, err, sizeof err);
    while (!atomic_load(&keyed)) usleep(10000);
    js8_tx_destroy(tx);                         /* tx_stop_all() */
    puts("survived");                           /* not reached today */
}
```

</details>

**Fix:** in `js8_tx_destroy()`, stop and join *before* the pointer goes
(e.g. `t->tx->stop(); t->tx->join();` with a small `join()` method, then
`reset()`). Better still, give `tx_abort_check` the transmitter through its
`ctx` instead of the global `tx`, which the TX thread reads while the UI
thread writes it (a data race even with the order fixed).

### B-02. Switching off with JS8 open skips its close — low, confirmed

**Where:** `src/radio.c` `radio_poweroff()` (hold POWER): it flushes the
settings and powers off without `dialog_destruct()`.

**What goes wrong:** the app's close never runs, so the saved settings
keep what JS8 set: the JS8 dial, USB-D, the digital-mode (USB-D/LSB-D)
receive filter at 200–3000 Hz, and the pre-JS8 memory slot
(`mem_save(MEM_BACKUP_ID)`) isn't loaded back. After switching on, the
radio is on 7.078 USB-D instead of where you were before opening JS8. The
README's known issue only mentions the filter and says "loses power",
but holding POWER is the normal way to switch off. The FT8 app behaves the
same (upstream code), and TX and RX filters are radio-only, so nothing
else is left changed.

**Fix:** README wording at least. Or have `radio_poweroff()` call
`dialog_destruct()` before flushing (an upstream-level change that fixes
FT8 too).

### B-03. Heartbeat offset chosen differently from desktop — low, confirmed

**Where:** `src/dialog_js8.c:2627-2638` (`free_hb_offset`),
`src/js8/commands.cpp:67-81` (`find_free_offset`).

**What goes wrong:** desktop (`UI_Constructor::sendHeartbeat` /
`isFreqOffsetFree`, JS8Call-improved `mainwindow.cpp:3408-3450, 3997`):
- sends the heartbeat on **your own offset** when it's at or below
  1000 Hz; ours always picks a random spot in 500–1000 Hz;
- counts your own offset, and the offsets of stations you're talking to,
  as free;
- looks at all band activity per offset, including frames without a
  callsign. Ours only looks at the Stations list (one offset per station
  with a callsign), so a busy call-less offset can be picked.

Small, but the user wants desktop behaviour.

**Fix:** port the two missing rules; feed the offsets of all recent
decodes, not only stations.

## Batch 2: Automatic sending

Compared with desktop JS8Call-improved at `d9c50510` (two commits after the
`e3d7a3b` used before): `JS8_Mainwindow/processCommandActivity.cpp`,
`pushNotificationHandler.cpp`, `JS8_UI/mainwindow.cpp`,
`JS8_Main/TxLoop.cpp`.

### B-04. Automatic replies go out while a message to us is still arriving — medium, confirmed

**Where:** `src/dialog_js8.c:2949-2970` (`auto_send`), `:3099-3132`
(`hb_tick`), `:3068-3081` (`push_tick`): they wait only for our own TX,
the keyboard and popups.

**What goes wrong:** we can't receive while we transmit (`audio_cb`
drops the audio while keyed). Desktop won't answer anyone while a
multi-frame command addressed to us (`MSG`, `MSG TO:`, `QUERY`...) is
still coming in (`processCommandActivity.cpp:1133-1139`,
`hasExistingMessageBufferToMe`), and sends no heartbeat ACK while *any*
such command is still coming in (`:636-641`, and `canEnableHBReplies()`,
`mainwindow.cpp:5118-5122`). We do. Example: N0XYZ sends us a 4-frame
`MSG`; after its first frame W1ABC's heartbeat decodes and, with HB ACK
on, we key in the next slot. We miss frames 2–4, so the message never
completes: nothing reaches the Inbox, no ACK goes back, and desktop N0XYZ
shows it as not delivered. The same with an `SNR?` or a relay from
someone else in the middle.

**Fix:** hold automatic replies (don't drop them, as desktop does: put
them off) while the assembler has an open partial addressed to our call,
and HB ACKs while it has any open multi-frame command. The partial rows
(`find_partial`) already know this.

### B-05. Heartbeat ACKs every 15 min per station; desktop waits 55 — medium, confirmed

**Where:** `src/js8/autoreply.hpp:107` (`HB_ACK_REPEAT_MS = 15 min`),
`AutoPolicy::decide()` (`autoreply.cpp:349-352`).

**What goes wrong:** desktop answers anything sent to `@ALLCALL` or
`@HB`, heartbeats included, at most once per station per **55 minutes**
(`processCommandActivity.cpp:296-307`, bumped by the HB ACK at
`:710-716`), and keeps that in a database (`HBBlockingDB`) so a restart
doesn't reset it. Ours allows one every 15 minutes, and forgets on power
off. A station heartbeating every 10 minutes gets an ACK from us every
15–20 minutes instead of about once an hour: up to four times desktop's
traffic from an unattended station. (Listed as "55-min ACK suppression"
among the desktop features we lack; it changes what we put on the air, so
it's here too.)

**Fix:** 55 minutes; optionally save the times with the held messages so
they survive a restart.

### B-06. Our 5-minute guard drops a repeated AGN? and relayed questions — low, confirmed

**Where:** `AutoPolicy::decide()` (`autoreply.cpp:357-358`), keyed by
`r.to + "|" + r.command`; `make()` (`:82-86`) sets `r.to` to the station
we heard.

**What goes wrong:** desktop has no such guard (it's ours, against
answering the same query twice). It also blocks things desktop answers:
- `AGN?` asked twice within 5 minutes (they missed our repeat too) gets
  nothing the second time;
- for a relayed question `r.to` is the **relaying** station, so two
  different stations asking `SNR?` through the same relay within 5
  minutes: the second gets no answer.

**Fix:** key the guard by the real asker (the relay path's first call),
and leave `AGN?` out of it.

### B-07. No WSPR guard band: can transmit on top of WSPR on 30 m — low, confirmed

**Where:** `tx_queue_at()` / `tx_player_play()`: no frequency check.

**What goes wrong:** desktop refuses to transmit, and cancels its CQ and
HB loops, when the on-air frequency is within 10 139.900–10 140.320 kHz,
the WSPR band (`mainwindow.cpp:2267-2280`). The 30 m preset (10 130 kHz)
is well clear, but a custom dial (Freq > Custom kHz) of e.g. 10 138 kHz
with a 2 kHz offset puts JS8 on top of WSPR, including automatic replies
and heartbeats.

**Fix:** the same check in `tx_queue_at()` (dial + offset, and the
signal's width) with a message, and stop auto CQ/HB there.

### B-08. Heartbeat and auto CQ timing follow the older desktop — low, confirmed

**Where:** `next_heartbeat_ms()` (`autoreply.cpp:367-376`), `hb_tick()`;
auto CQ in `cq_hold_cb()` / `ui_tx_done()` (`dialog_js8.c:1529-1533`).

**What goes wrong:** desktop now runs both through `TxLoop`
(`JS8_Main/TxLoop.cpp`): one transmission every N minutes on a fixed
schedule aligned to the speed's slots, the first one N minutes after
switching on, and nothing restarts it after other transmissions.
- Heartbeats: ours use the old `scheduleHeartbeat()` rule (next 15 s
  boundary + 1 s + N min, one slot later 25% of the time).
- Auto CQ: ours sends one at once, then counts N minutes from the end of
  **every** transmission (`ui_tx_done` restarts it after an automatic
  reply or a heartbeat too), so on a busy frequency CQs come further
  apart than asked. The "from the end" part was the user's choice
  (beta 2), so check before changing it.

**Fix:** port `TxLoop`'s schedule, if wanted.

### B-09. Holding CQ while sending: first CQ a whole interval later — low, confirmed

**Where:** `src/dialog_js8.c:2582-2588` (`cq_hold_cb`).

**What goes wrong:** the comment says "Busy sending: the first CQ right
after", but the code sets `auto_cq_next_ms = now + interval`, and
`ui_tx_done()` then moves it to *the end of the message* + interval. So
the first automatic CQ comes a whole interval after the message ends, not
right after it.

**Fix:** when busy, set `auto_cq_next_ms` so the first CQ follows the
message (e.g. a flag `ui_tx_done` honours), or fix the comment.

### B-10. An `@APRSIS MSG` without `TO:` is kept and ACKed — low, confirmed

**Where:** `src/js8/autoreply.cpp:194-205`.

**What goes wrong:** `@APRSIS` counts as one of our groups (as on
desktop). For `MSG` to `@APRSIS`, desktop only takes the gateway's form
`MSG TO:CALL text` and otherwise ignores it (`processCommandActivity.cpp:758-803`,
`continue`). Ours falls through to the ordinary `MSG` branch when the
`TO:` pattern doesn't match: the text goes into our Inbox and, with AUTO
on, we transmit an ACK to whoever sent it. Someone sending
`@APRSIS MSG hello` by mistake would get an ACK from us.

**Fix:** after the `@APRSIS` check, return nothing when the pattern
doesn't match.

## Batch 3: Receiving and decoding

The receive path matches desktop where it was checked (speed table,
assembler grouping and timeout, checksums, relay and command parsing) and
has good unit tests. Three small findings.

### B-11. After a receiver stall, minute-old audio can decode again as new — low, possible

**Where:** `src/js8/receiver.cpp:175-180` (`worker_loop`).

**What goes wrong:** if the worker falls more than 5 s behind, it throws
the backlog away and sets `aligned_ = false`, so the next buffer only
realigns the engine's 60 s ring (`request_realign()`, local patch 3). The
gap-fill comment a few lines below (`:32-35`) explains why that's wrong
for a gap: the ring positions skipped over still hold audio from a minute
earlier, which "decodes again as new". The TX gap is filled with silence;
this one isn't. The old frames are past the duplicate filter's 30 s
memory, so an old `MSG` could be ACKed again, an old query answered again,
an old CQ alerted again. It needs a 5 s stall of the worker thread, which
nothing normally causes (it does no file I/O), so it's unlikely.

**Fix:** keep the discarded length and push that much silence (up to the
ring), as the gap fill does, instead of a bare realign.

### B-12. A message still arriving when JS8 closed can take over a new message's row — low, likely

**Where:** `src/dialog_js8.c:683-690` (`find_partial`), `:751-760`
(`add_message`); `src/js8/assembler.hpp:87` (`next_id_ = 1`).

**What goes wrong:** partial rows are found again by the assembler's
`msg_id`. Each `Receiver` numbers from 1, but the message history lives
as long as the radio is on (JS8 closed and reopened included). A message
whose last frame never came before JS8 closed (or before a retune, which
clears the assembler) stays in the history as a partial; after reopening,
the new receiver's message with the same number updates *that* old slot:
the new text appears in the old row, at the old time and position, instead
of at the bottom. Such partials also keep their "growing" state forever.

**Fix:** a process-wide message counter (so ids never repeat while the
radio is on), and mark leftover partials as final on retune and close.

### B-13. Any first word starting with "CQ" makes a message a CQ — low, confirmed

**Where:** `src/js8/classify.cpp:67`
(`starts_with(first, "CQ")`).

**What goes wrong:** Portugal's special-event calls use the CQ prefix
(CQ0–CQ9, e.g. CQ7ABC). A directed message to such a station
(`CT1ABC: CQ7ABC HELLO`) is classified as a CQ: the CQ alert beeps, and
anything else that treats CQs specially (Stations marks, the Directed
filter) gets it wrong. Desktop decides CQ from the frame type.

**Fix:** `first == "CQ"` (the renderer already turns CQ frames into
`@ALLCALL CQ ...`, which the second test catches).

## Batch 4: Inbox, saved data, settings

### B-14. An Inbox file that can't be read is overwritten by the next save — medium, confirmed

**Fixed in 4bfb8d6** (package 2).

**Where:** `src/js8/js8_ops.cpp:525-531` (`js8_inbox_open`), `:614-620`
(`js8_held_open`); `src/js8/inbox.cpp:75`, `:185`.

**What goes wrong:** `Inbox::load()` returns false when the file exists
but can't be opened (`access()` finds it, `ifstream` doesn't: an SD read
error, too many open files, a damaged directory entry). Both open
functions ignore that and carry on with an empty list. The next change
(a new message, marking one read, a delivery, a RETRIEVE MSG notice)
calls `save()`, which writes the one-message list to `.tmp` and renames it
over the real file: every earlier message is gone. The same for held
messages (`js8_held.txt`), which are other people's traffic.

**Fix:** remember that the load failed and don't save over the file (say
so on screen: "Inbox file can't be read"), or move the unreadable file
aside (`js8_inbox.txt.bad`) before the first save.

### B-15. `js8_texts.txt` is rewritten in place: a power cut can wipe the settings — low, confirmed

**Fixed in 4bfb8d6** (package 2).

**Where:** `src/dialog_js8.c:3242-3254` (`save_texts`).

**What goes wrong:** `fopen(..., "w")` truncates the file, then writes it,
with no temporary file, no `fsync` and no check of `fprintf`/`fclose`.
A power cut or a full card during a save leaves it empty or cut short, and
INFO, STATUS, groups, alert words, the operator call, the POTA/SOTA refs
and the spot form are all lost (`load_texts()` just finds nothing). The
Inbox files next to it are written safely (temporary file, `fsync`,
`rename`).

**Fix:** the same write-then-rename as the Inbox (see I-12), plus
BH-16's fallback to the `.tmp` file on load.

### B-16. Without a callsign the keyboard never opens, and leaves a stale edit mode behind — low, confirmed

**Where:** `src/dialog_js8.c:1874-1879` (`compose_open`); callers that
set `edit_target` first: `texts_item_cb` (`:3344`), `freq_item_cb`
(`:5229`), the log, alerts, spot and beacon forms; `construct_cb` never
resets it.

**What goes wrong:** `compose_open()` refuses with "Set your callsign
first" when there's no callsign, for every use of the keyboard, including
the ones that never transmit: a custom frequency, alert words, INFO,
STATUS, groups, the log fields. Receiving works without a callsign, so a
new user can be listening and still unable to type a frequency.

Worse, the caller has already set `edit_target`, and nothing resets it
(not the refusal, not closing and reopening the app). After setting the
callsign, **Send...** or **Reply** opens the keyboard in the leftover
mode: e.g. INFO's editor (64 characters, INFO placeholder), and Enter
saves the typed message as INFO instead of sending it; or the frequency
editor, and Enter retunes.

**Fix:** reset `edit_target` in `construct_cb` and whenever
`compose_open()` refuses; require the callsign only when sending
(`tx_queue_at()` already refuses without one).

### B-17. A message that couldn't be saved is still ACKed — low, confirmed

**Fixed in 4bfb8d6** (package 2).

**Where:** `src/js8/js8_ops.cpp:239-261` (`keep`), `:309-326`
(`js8_process`).

**What goes wrong:** when the Inbox (or held-message) file can't be
written (card full, read-only after a FAT error, DATA partition missing),
`keep()` sets `id = -1` and the screen says "can't save", but the ACK
`process()` built still goes out. The sender sees the message as
delivered; it's only in memory here and is lost at power-off.

**Fix:** drop the ACK when the store failed, so the sender's station
tries again later.

## Batch 6: Selecting, navigating, Stations

### B-18. QUERY CALL about a station heard only through a relay gets no answer — low, confirmed

**Where:** `src/js8/autoreply.cpp:250-264` (`QUERY CALL`).

**What goes wrong:** a station we only heard through a relay is listed
with SNR −64 (desktop's value). `desktop_snr(-64)` is empty (outside
−60..+60), and our code then `break`s: no answer. Desktop builds
`QString("%1 (%2)").arg(formatSNR(cd.snr)).arg(since(...)).trimmed()`,
i.e. `"(5m)"`, and answers `N0XYZ YES (5m)` (`processCommandActivity.cpp:1100-1113`).

**Fix:** answer without the SNR when it's out of range, as desktop does.

### B-19. The VOL knob does nothing while most popups are open — low, confirmed

**Where:** the popups' key callbacks: `texts_key_cb`, `aprs_key_cb`,
`log_key_cb`, `inbox_key_cb`, `alerts_key_cb`, `freq_key_cb`,
`spot_key_cb`.

**What goes wrong:** the VOL knob is a keypad (`main.c:90-94`): its turns
arrive as `KEY_VOL_*` key events at whatever has the focus. The message
list (`key_cb`) and the Query list (`query_key_cb`) pass them to
`radio_change_vol()`; the other seven popups only handle ESC and the
arrows, so the volume can't be changed while reading the Inbox, the Log
form, Settings and so on.

**Fix:** one shared popup key handler that also does the volume (fits
with I-05's single popup table).

### B-20. "New station" alerts again for a station that dropped off the list — low, likely

**Where:** `src/dialog_js8.c:696` (`process_message`), `:3969-3979`
(`find_station`), `:4899-4903` (`alert_check`).

**What goes wrong:** "New station (not in log)" means "not in the
Stations list right now, and not in the log for this band". Stations
leave the list an hour after they were last heard (the default), so a
regular who isn't in the log beeps as new each time they come back after
an hour. Also, `find_station()` only looks at the first 200 stations of
the sorted list: past that (a busy band with "Stations kept: 6 hours" or
"always"), stations already heard count as new.

**Fix:** keep a separate "heard since power-on" set of calls (or
per-dial), and look the call up by key (see I-17).

## Batch 7: Sending by hand

### B-21. AGN? repeats what we last queued, not what went out — low, confirmed

**Where:** `src/dialog_js8.c:1682` (`tx_queue_at` sets `last_tx_text`
when a message is queued); `src/js8/autoreply.cpp:187-191`.

**What goes wrong:** desktop's `m_lastTxMessage` is built from the frames
actually sent, as they're sent (`mainwindow.cpp:3380-3385`), and cleared
when the band changes (`:2728`). Ours is whatever was last *queued*: a
message stopped before its first frame, or one sent on the previous band,
is what an `AGN?` gets.

**Fix:** set `last_tx_text` from the transmitter's `on_tx_done` (the text
sent, or the frames that went out when it was stopped), and clear it in
`retuned()`.

### B-22. The keyboard refuses characters JS8 can send — low, confirmed

**Where:** `src/dialog_js8.c:1886` (`lv_textarea_set_accepted_chars`).

**What goes wrong:** `is_sendable_char()` takes all printable ASCII
(`tx.cpp:73-78`, and the tests check they all round-trip), and desktop's
text boxes take any non-control character
(`Configuration.cpp:207`, `[^\x00-\x1F]*`). The compose box's list
leaves out ten of them: dollar, percent, less-than, both square
brackets, caret, vertical bar, tilde, backslash and backtick. So "COSTS $5" or "50%" can't be typed
into a message or an SMS, and a Reply prefilled with one of them loses it
silently (the filter also applies to prefilled text).

**Fix:** accept every character `js8_tx_sendable_char()` accepts.

### B-23. A message that's too long freezes the frame count instead of saying so — low, confirmed

**Where:** `src/dialog_js8.c:1748-1755` (`compose_changed_cb`).

**What goes wrong:** the live "N frames, S s" line is only updated while
the preview is OK. Past 20 frames (`TX_MAX_FRAMES`) the plan fails with
"too long: 21 frames (max 20)", and the line just keeps showing the last
good count; you only find out on Enter. Rare with 160 characters of
ordinary text, easier with many escaped punctuation characters.

**Fix:** show `pv.error` when the preview fails.

## Batch 9: Alerts, time, app life cycle, firmware hooks

### B-24. A QSO left unlogged stays first in Log QSO for good — low, confirmed

**Where:** `src/dialog_js8.c:4274-4288` (`log_offer` sets `log_pending`),
`:4067-4073` (only Save and the Cancel item clear it), `:4264`
(`log_cb` takes it before the selected station).

**What goes wrong:** when a QSO ends, its call is remembered as
`log_pending`. Closing the prompt with ESC (`log_key_cb` → `log_close`),
or with the log prompt off, or by closing the app, leaves it set, and
nothing expires it. From then on **Log QSO** always opens that old QSO,
whichever station you've selected since, until you either save it or pick
Cancel inside the popup.

**Fix:** clear it on ESC too, and prefer the selected station when it
isn't the pending one (or expire it with the QSO tracker's timeout).

### B-25. A stalled screen can silently drop decoded messages — low, possible

**Where:** `src/scheduler.cpp:26-37` (firmware code: `QUEUE_MAX_SIZE 64`,
"Scheduler queue overflow" and the item is dropped); JS8's users:
`on_message` (every frame's partial and every final message),
`on_cycle_done`, `on_tx_status`, `on_tx_done` and `wf_emit_row`
(15 waterfall rows a second).

**What goes wrong:** everything the worker threads hand to the LVGL
thread goes through one 64-item queue, and JS8 alone puts about 15 items
a second in it before any decodes. If the LVGL thread stalls for a few
seconds (a Stations rebuild with 200 database lookups, BH-17; an Inbox
`fsync`; a full redraw) while a slot's decodes arrive, the queue
overflows and items are dropped with only a log line. A dropped final
message is simply gone: no row, no Inbox, no ACK. A dropped `on_tx_done`
skips `deliver_end` (the held message stays held) and the auto CQ timer
restart. A dropped waterfall row leaks its `malloc`'d buffer.

**Fix:** give the waterfall rows their own ring (the dialog already has
one, `wf_queue`, fed through the scheduler), so the shared queue carries
only a few items per slot; and don't let `scheduler_put()` drop silently
for JS8's message and TX events (a bigger queue, or a JS8-side queue the
UI drains).

### B-26. Alert words miss a word with punctuation attached — low, confirmed

**Where:** `src/js8/alerts.cpp:48-57` (`alert_word_hit` splits only on
`: > space tab`).

**What goes wrong:** "SOTA," "(POTA)" or "VE7ABC?" don't match the alert
words SOTA, POTA, VE7ABC (the last one only if it's also the sender).
The row isn't purple and nothing beeps.

**Fix:** strip leading and trailing punctuation from each token before
comparing (keep `@` and `/`).

## Batch 10: Engine, build, tests, tools, docs

### B-27. `x6100-flash` with no argument writes an old build — low, confirmed

**Where:** `~/Work/bin/x6100-flash:13` (`RUN="${1:-36383576566}"`),
outside the repo.

**What goes wrong:** the default is the beta 4 test build of 18ebf06, the
one on the card today. Once the next build exists, running the script
without its run id (e.g. from memory, or when Claude isn't available,
which is what the script is for) quietly puts the old build back. It
still backs up DATA and asks before writing, so nothing is lost, but the
radio ends up on the wrong firmware.

**Fix:** require the run id, or default to the newest successful *Build
image* run on `main` and show its commit before asking (see I-24).
