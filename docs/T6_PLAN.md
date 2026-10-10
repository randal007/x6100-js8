# T6: JS8 speeds (Normal, Fast, Turbo, Slow)

Researched 2026-09-25 against desktop JS8Call (`JS8Submode.cpp`,
`mainwindow.cpp`, `varicode.cpp`, `decodedtext.cpp`) and the vendored
js8core engine.

## The speeds

From desktop's `JS8Submode.cpp` (12 kHz, 79 symbols per frame):

| Speed | Letter | Varicode | Samples/symbol | Symbol | Tone spacing | Bandwidth | Slot | Start delay | Costas | rxThreshold | Decodes to |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Normal | N (A) | 0 | 1920 | 160 ms | 6.25 Hz | 50 Hz | 15 s | 500 ms | original | 10 Hz | −24 dB |
| Fast | F (B) | 1 | 1200 | 100 ms | 10 Hz | 80 Hz | 10 s | 200 ms | modified | 16 Hz | −22 dB |
| Turbo | T (C) | 2 | 600 | 50 ms | 20 Hz | 160 Hz | 6 s | 100 ms | modified | 32 Hz | −20 dB |
| Slow | S (E) | 4 | 3840 | 320 ms | 3.125 Hz | 25 Hz | 30 s | 500 ms | modified | 10 Hz | −28 dB |
| Ultra | (I) | 8 | 384 | | | 250 Hz | 4 s | 100 ms | modified | 50 Hz | disabled on desktop |

Every slot length divides 60 s, so a minute boundary (and every 30 s) is a
slot start for all of them.

## What desktop does

- **Receive:** the *multi-decoder* (on by default, `SubModeMultiDecode`)
  decodes every speed from the same audio, each on its own slot grid. Off,
  only the speed you transmit is decoded.
- **Transmit:** one current speed for everything you send, including
  automatic replies. There is no automatic switch to the other station's
  speed; the band/call activity menus offer "Jump to Fast speed" when the
  selected station was last heard on another one.
- **Heartbeats** only in Normal, Fast and Slow (`canCurrentModeSendHeartbeat`);
  none in Turbo. The heartbeat time is rounded to 15 s whatever the speed,
  and the sub-band search always uses 500–1000 Hz, 50 Hz wide.
- **Frames:** data (free text) frames are packed with `packDataMessage` in
  Normal and `packFastDataMessage` in every other speed. The receiver tries
  fast data when the frame's data bit is set, so this is invisible to users.
- **Display:** an optional speed column (N/F/T/S) in band and call
  activity; "on their frequency" (the directed view) uses the decode's own
  speed's rxThreshold; message buffers are matched within the frame's
  speed's rxThreshold.
- **Message buffers** close 60 s after the *latest* frame and are dropped
  after 90 s, measured from the latest frame, not the first.
- **Late start:** desktop may start a frame late (up to a fraction of the
  slot: 3/4 of the normal allowance in Fast, 1/2 in Turbo). We don't start
  late at all, at any speed: we wait for the next slot.

## What this app assumes today (all Normal)

- `dialog_js8.c` asks the receiver for Normal only.
- `tx.hpp/.cpp`: 15 s slot, 0.5 s start delay, 0.16 s symbols (6.25 Hz),
  original Costas, `build_message_frames(..., 0)`, max offset 2450 Hz,
  air time `(frames−1)·15 + 79·0.16`.
- `js8_tx.cpp` / the dialog: preview and send have no speed; the red TX
  band on the waterfall is 50 Hz; the knob clamps to 500–2450 Hz.
- `assembler.cpp`: buffers keyed by offset only, ±10 Hz, dropped 90 s after
  the **first** frame. That splits any message longer than 6 Normal frames
  and is the likely cause of fragments like `XHZ2TK5SP FB` seen on air.
- `testsignal.cpp`, the test WAV player and the loopback tests: 15 s slots.
- Directed view "on their frequency": fixed ±10 Hz.
- Stations: no speed recorded.

The engine (js8core) already schedules and decodes all speeds; the
Android port runs it with all of them on (`0x1F`). Each speed's decoder is
built on first use (our patch 4), roughly 7 MB each.

## Plan

1. **A speed table** (`src/js8/speeds.hpp`) with the numbers above, used by
   everything instead of 15 s / 50 Hz constants.
2. **Receive all speeds** by default (desktop's multi-decoder default),
   with a switch to decode only your own speed if the radio struggles.
   Rows get a speed letter (F, T or S; Normal unmarked), the Stations view
   a speed column. Directed view uses the decode's own rxThreshold.
3. **Assembler:** one buffer per (speed, offset), each speed's rxThreshold,
   close 60 s after the latest frame (desktop), so long messages and Slow
   frames (30 s apart) aren't cut.
4. **Transmit at a chosen speed:** `plan_message(..., speed)` (fast data
   frames, modified Costas), `synth_frame` with the speed's symbol length,
   slot timing and air time per speed, max offset 2500 Hz − bandwidth.
5. **UI:** page 6 *Speed: Normal/Fast/Turbo/Slow* (press cycles; hold
   matches the selected station's speed, desktop's "Jump to … speed");
   *Decode: All speeds / Mine*; TX bar and waterfall band show the speed and
   width; Reply warns when the station was heard on another speed.
   Heartbeats (button and HB) refuse Turbo, as on desktop.
6. **Tests:** a test band with mixed speeds decoded together; loopback of
   our own TX audio at every speed; DT is 0 for on-time signals at every
   speed (Time Sync); slot timing and air time per speed; the assembler's
   per-speed buffers and latest-frame timeout.
7. **On the radio:** CPU and memory with all speeds on, before and after.

## Status (2026-09-25): built, tested on the PC

Done as planned (steps 1–6); step 7 (the radio) is next. What testing
turned up along the way:

- **The engine dropped decode windows** while a decode was running
  (js8core `enqueue_decode()`), after marking them done. Invisible with
  Normal only; with several speeds, Turbo's once-a-second retries made
  Normal/Fast slots go missing. Fixed as js8core patch 8: ready windows wait
  (merged) behind the running decode, as desktop queues them.
- **Turbo decoded some frames twice** (its per-second retries), which made
  a stray one-frame message. The receiver now drops a frame seen again at
  the same speed, frame and offset within one slot.
- **The receiver always keeps schedules for all four speeds**; Decode: All /
  My speed only switches them on and off (`set_submodes`), so nothing is
  restarted while audio is flowing.
- **Changing speed restarts the HB interval**, so a heartbeat that fell due
  while in Turbo isn't sent the moment you leave it.

Tests: the speed table against desktop's numbers, slot timing, plans and
audio at every speed, per-speed assembly and duplicate filtering, and three
end-to-end runs through the real decoder: a band with all four speeds at
once (DT near 0 for every speed), our own TX audio at every speed, and Fast
switched on mid-run. UI harness `ONLY_SPEED=1`.

To check on the radio: CPU and memory with all speeds on (each speed's
decoder is built on first use: Slow ~14.5 MB, Fast 4.8, Turbo 2.9 on top of
Normal's 7.3), and decodes of real Fast/Turbo/Slow stations.
