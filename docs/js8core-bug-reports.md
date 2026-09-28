# js8core: bugs to report upstream

Bugs found in **js8core**, the JS8 engine from
[JS8Call-improved/Android-port](https://github.com/JS8Call-improved/Android-port)
(`core/`), while building the X6100 JS8 app. We use a copy at commit
`1f8d390a` in `third-party/js8core/`, fixed locally; this file is written to
be sent to that team. The technical notes and the exact code changes are in
[`third-party/js8core/UPSTREAM.md`](../third-party/js8core/UPSTREAM.md)
(patch numbers below), one commit per fix (`git log -- third-party/js8core`).

Desktop JS8Call-improved's own code (`JS8_Main/Varicode.cpp`) is the
reference: js8core is a port of it, and a station running js8core has to be
understood by desktop stations.

**When a new bug turns up in js8core: fix it in `third-party/js8core`, add it
to `UPSTREAM.md`, and add it here.**

**Reported:** bugs 1–4 (with the others listed) in
[Android-port#104](https://github.com/JS8Call-improved/Android-port/issues/104),
2026-09-28.

## Bugs

| # | Bug | Who it hits | Status here |
|---|---|---|---|
| 1 | A directed message's number taken after any command | desktop stations reading our messages | fixed (patch 11) |
| 2 | No space stripped after non-buffered commands | desktop stations (double spaces) | fixed (patch 11) |
| 3 | Normal-speed data frames never use Huffman | airtime (extra frames) | fixed (patch 11) |
| 4 | Checksum added to `@APRSIS MSG` / `MSG TO:` | desktop stations / APRS gateways | fixed (patch 11) |
| 5 | `is_valid_callsign()` accepts ordinary words | free text sent without our call | fixed (patch 6) |
| 6 | `format_snr()` unlike desktop's | display | fixed (patch 7) |
| 7 | Ready decode windows dropped while a decode runs | missed decodes | fixed (patch 8) |
| 8 | Decoder arrays read before being written | nothing decodes (heap builds) | fixed (patch 5) |
| 9 | `compute_drift_estimate()` one start delay short | time sync | worked around (patch 10) |
| 10 | Missing standard includes | builds with GCC / libstdc++ | fixed (patch 1) |

### 1. A directed message's number is taken after any command

`pack_directed_message()`'s pattern (`kDirectedRe`) takes
`\s*[+-]?\d{1,3}` after **any** command as the frame's number field.
Desktop's `optional_num_pattern` is `(?<=SNR)\s?[-+]?(?:3[01]|[0-2]?[0-9])`:
only right after SNR, and only 0–31.

- `N0XYZ RR 73`: sent as RR with a number; desktop shows **"RR 31"**.
- `N0XYZ MSG 73 GOOD DAY`: the 73 is lost.
- `N0XYZ>2E0ABC HELLO` (a relay), `N0XYZ MSG TO:9A1XYZ ...`,
  `N0XYZ QUERY CALL 4X1ABC?`: the first digit of the callsign goes into the
  number, so desktop reads **"E0ABC"**, "A1XYZ", "X1ABC": the relay or
  stored message goes to the wrong station.

Fix: take the number only when the command ends in SNR, with desktop's
number pattern (`std::regex` has no lookbehind), and consume only that.

### 2. No space stripped after non-buffered commands

Desktop's `isCommandBuffered()` is
`directed_cmds.contains(cmd) && (cmd.contains(" ") || buffered_cmds.contains(...))`:
in `buildMessageFrames()` every command with a space (" RR", " YES",
" HEARTBEAT SNR" ...) counts, so the text after it is left-stripped
("RR 73" sends "73"). js8core's `is_command_buffered()` only has the
buffered set, so it sends " 73": desktop shows "RR  73", and the extra
character can cost a frame. Fix: the same rule in `build_message_frames()`.

### 3. Normal-speed data frames never use Huffman

Desktop's `packDataMessage()` packs the text both with its Huffman table
(`packHuffMessage()`, prefix `10`) and with JSC (`11`) and keeps whichever
fits more characters (JSC on a tie). js8core's `pack_data_message()` only
uses JSC. Words JSC doesn't know (callsigns) then take more frames than on
desktop: `N0XYZ QUERY CALL W1ABC?` was 3 frames, desktop 2. Fix: the same
choice, with js8core's existing `huff_encode()` / `default_huff_table()`.

### 4. A checksum on `@APRSIS MSG` / `MSG TO:`

Desktop's `buildMessageFrames()` skips the checksum for `MSG` and `MSG TO:`
to `@APRSIS` (`skipAprsChecksum`); js8core adds one. Fix: the same skip.
Note for desktop too: its receiver (`processBufferedActivity()`) still
requires a checksum on every `MSG` / `MSG TO:`, so an APRS reply relayed
by `AprsInboundRelay` ("@APRSIS MSG to:CALL text DE SENDER", no checksum)
fails the check at a desktop station. We accept these without a checksum.

### 5. `is_valid_callsign()` accepts ordinary words

A simplified rewrite accepted almost any short word ("JUST", "HELLO",
"THE") as a callsign. Desktop requires a letter-digit pair, or a real
compound call or @group. Effect: `build_message_frames()` took the first
word of free text for a recipient, so forced identification never
happened and free text went out without the sender's callsign. Fix: a
straight port of desktop's `isValidCallsign()` / `isValidCompoundCallsign()`.

### 6. `format_snr()` unlike desktop's

Printed "-8" and "+5" where desktop's `Varicode::formatSNR()` prints "-08"
and "+05", and didn't return empty outside -60..+60.

### 7. Ready decode windows dropped while a decode runs

`enqueue_decode()` dropped windows that became ready while a decode was
running, so with several speeds decoding some slots were never decoded.
Desktop queues them. Fix: merge them into the next decode instead.

### 8. Decoder arrays read before being written

`DecodeMode` reads some arrays before writing them; upstream gets away with
it only because its decoders are `static` (zeroed). On the heap (e.g. to
save memory) a GCC -O2/-O3 build could decode nothing at all. Fix: `{}`
initialisers on every array member.

### 9. `compute_drift_estimate()` one start delay short

It takes the decode window's start minus the submode's start delay plus
`xdt`, so it comes out one start delay short (500 ms Normal and Slow, 200
Fast, 100 Turbo): signals 1.5 s late give -1000. We added
`events::Decoded::capture_drift_ms` and compute the drift from it instead.

### 10. Missing standard includes

`varicode.hpp` uses `std::function` and `jsc.cpp` uses `std::strncmp`
without `<functional>` / `<cstring>`; fine with Android's libc++, not with
GCC's libstdc++.

## How we checked against desktop

Desktop's `Varicode.cpp` and `JS8_JSC` build on their own with Qt 6
(minus the `BuildMessageFramesThread` class). We fed 50 typical messages
(queries, QSO lines, relays, `MSG` / `MSG TO:`, `QUERY` commands, HB acks,
CQs, groups, APRS) through desktop's `buildMessageFrames()` and js8core's
`build_message_frames()` at all four speeds. After fixes 1–4 every frame
matches bit for bit; `tests/test_js8.cpp` ("[desktop]") keeps 24 of them
as a regression test.

## Also changed (features, not bugs)

Patches 2, 3, 4 and 9 in `UPSTREAM.md`: a switch to turn off the spectrum
thread, `request_realign()`, lazily built per-speed decoders (73 → 44 MB
peak), and a settable decode range and QSO offset. Useful on small
devices; offered if the team wants them.
