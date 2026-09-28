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

## Summary

| ID | Feature | Bug | Severity | Confidence |
|---|---|---|---|---|
| B-01 | F02, F04, F05 | Closing the app during a transmission crashes it, radio keyed until it restarts | high | confirmed (reproduced) |
| B-02 | F05 | Switching off with JS8 open skips its close: back on the JS8 dial, USB-D, 200–3000 Hz | low | confirmed |
| B-03 | F08 | Heartbeat offset chosen differently from desktop | low | confirmed |

## Batch 1: Transmitting and the radio

### B-01. Closing the app during a transmission crashes it — high, confirmed (reproduced)

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
