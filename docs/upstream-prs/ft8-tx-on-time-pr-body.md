**Why**
The FT8/FT4/FT2 transmitter was keyed by the decoder thread, after the slot's last decode. On a busy band that decode takes 0.5 to 1 s on the radio, so the radio was keyed late and the start of the message was left out (radio log 2026-11-12: 5 of 14 transmissions 0.5 to 1.5 s into the slot). Kris asked for the radio log under TX on #371; this is the fix for what it showed.

**What changes** (`src/dialog_ft8.c`, `src/ft8/tx_worker.c/.h`, small parts of `audio_worker.c/.h` and `worker.c`)
- **A TX thread of its own**, as WSJT-X: it wakes at each slot's start and every 100 ms (on CLOCK_MONOTONIC, so Time Sync stepping the clock doesn't make it miss a slot) and keys on time with the message there is. The decoder goes on with the slot just ended meanwhile.
- **A late decode swaps the message**, as WSJT-X does, but only while everything sent so far is the new message's too (the sync tones and any tones the two share), so the other station still gets it whole: FT8 up to about 1.7 s into the slot, FT4 about 0.7 s (PC test). After that the message going out goes on and the new one goes next time: on the PC a later swap wasn't decodable, a late start was.
- **Another frequency** (a move wider than the signal) stops the TX and starts the new one at once, cut as a late start. Smaller moves (a reply's frequency differs by a few Hz from decode to decode) just swap.
- **Stops** (TX off, CQ off, close, band or mode change) are counted: a transmission started just before one ends at its next part.
- Repeats count only a message that went out. `tx_msg` and `tx_time_slot` are under a lock (GUI, decoder and TX thread write them); sending never waits for that lock (the decoder holds it through a QSO save on the SD card). The callsign table is locked as in #371: the TX thread encodes while the decoder decodes.
- FT2's TX tick before the slot's last decode is gone: nothing transmits from the decoder thread now.
- A log line with the moment each TX is keyed ("FT8/FT4 TX keyed N s into the slot"), and "swapped" / "too late to swap" lines.

**Built on #393** (the late answer to a picked station): this PR's base is its branch, so only this change shows here. GitHub moves it onto main when #393 is merged. It also touches the same lines as #371 (`audio_worker.c`, `worker.c`); whichever goes in second, I'll merge main into it.

**Checked**
- **On the radio** (2026-11-23, VE7NHW, test package 10 = main + this + #393 + #371 + #380, FT8 on 40 m on a busy band, about 24 minutes): 21 transmissions. The 16 that weren't a late start after a pick were keyed 0.03 to 0.05 s into the slot; `/mnt/ptt.log` shows the key command 26 to 36 ms after the slot start every time. The other 5 were late starts after picks (#393), the missed part left out. Nothing wrong seen. No swap happened in that run, so the swap was tested on the PC only.
- PC test of the TX thread (the branch's `tx_worker.c` with a fake clock, ft8_lib's decoder; AddressSanitizer + UBSan): swaps decode up to the limits above, a late start decodes, keyed within 1 ms of the slot start with 8 busy threads, a 2 s clock step back doesn't lose a slot. Run again on this merged branch: all OK.
- Compiles for the radio (GCC 12), merged with today's main and #393's branch.
- Code review (`/code-review` high, 2026-11-15): 9 findings, 7 fixed in the second commit (stop race, lock wait while sending, frequency jitter, monotonic sleep, level ramp per block, mode-switch order, locked reads). Not changed: the waveform is rebuilt for a swap (about 1 ms on the PC), and the thread set-up repeats #371's pattern.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_011735W6YK3DU2b3ivecHksz
