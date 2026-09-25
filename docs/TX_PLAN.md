# JS8 transmit: plan and UI design

Decided for T3 (2026-09-25): query set as listed below; heartbeats carry
a 4-character grid; Hold offset defaults to On; add a Stations view like
desktop's Call Activity, where ★ marks stations that heard you.

Decided for T4 (2026-09-24): AUTO, HB and HB ACK are separate switches, all
off by default; a QSO turns HB and HB ACK off until you turn them back on;
the HB interval is 5–30 min in 1 min steps; INFO/STATUS are edited from
page 4; with AUTO off, answers are offered on Reply, as on desktop.

Status: **T1–T4 done and on the air** (2026-09-24): heartbeats acked by
KK6WVY, N7EAL and KN6OEH, INFO? answered by KN6OEH. Since then: DT-based
Time Sync, an APRS page (@APRSIS grid/POTA/SOTA/SMS/email/Winlink), and an
ALC fix for low power. T5 (logging, MSG inbox) is next.

## Principles

1. **Nothing transmits unless you asked.** Every transmission comes from a
   button you pressed, apart from auto-reply and heartbeats. Those are off
   by default, are separate switches, and always show on screen while on.
2. **Stop is always one press away**, and ESC, PTT or leaving the app stop
   TX immediately.
3. **Reuse what already works on this radio**: the FT8 app's TX path (PTT,
   ALC loop, power cap, TX-frequency shift) and its on-screen keyboard.
4. **Behave on air like desktop JS8Call.** Same frame building, timing,
   heartbeat sub-band and command replies, so JS8Call users see nothing
   unusual.

## What already exists

| Piece | Where | Notes |
|---|---|---|
| Frame building from text | js8core `build_message_frames()` | Same code as desktop JS8Call; handles directed, compound, data and checksummed commands |
| Frame → tones | js8core `legacy_encode()` | Tested round-trip against our decoder |
| TX playback with PTT and ALC | `src/ft8/tx_worker.c` | Plays int16 audio via `audio_play()`, keys with `radio_set_modem()`, corrects gain from ALC/power readback, caps at 5 W, shifts the radio so the tone sits at 1325 Hz audio |
| Text entry | `textarea_window_*` | On-screen keyboard, plus USB keyboards |
| Command vocabulary and auto-reply rules | Android port (`Js8Commands.kt`, `JS8EngineService.kt`) | Port to C++ like the RX side |

js8core also has a modulator and a TX scheduler inside its engine, built
around a pull-style audio output. The radio's audio is push-style
(`audio_play()` blocks), so we'll build TX waveforms ourselves, like the
FT8 app does, rather than use the engine's audio output.

## Waveform: GFSK, BT = 3

Desktop JS8Call sends plain continuous-phase FSK. The FT8 app uses Gaussian
FSK. Measured with our decoder (80 trials per point, true SNR in 2500 Hz,
single signal in AWGN):

| | power outside the 70 Hz channel | −18 dB | −19 dB | −20 dB |
|---|---|---|---|---|
| CPFSK (desktop JS8Call) | −27.2 dB | 74/80 | 49/80 | 11/80 |
| GFSK BT = 2 (FT8 app) | −34.7 dB | 68/80 | 44/80 | 11/80 |
| **GFSK BT = 3** | **−32.6 dB** | **76/80** | **49/80** | **16/80** |
| GFSK BT = 4 | −31.2 dB | 76/80 | 48/80 | 13/80 |

BT = 3 decodes as well as desktop's CPFSK and puts 5.4 dB less energy on
neighbouring signals. The generator in `src/ft8/gfsk.c` already takes BT as
a parameter.

## Timing

- JS8 has no even/odd slots. A message starts 0.5 s after the next 15 s
  boundary. A request made in the first 0.5 s of a slot still makes that
  slot, the same rule as js8core's modulator.
- Multi-frame messages use **consecutive** slots: key for 12.64 s, drop to
  RX for ~2.4 s, repeat.
- RX audio is not fed to the decoder while keyed, as in the FT8 app. The
  receiver's clock guard re-snaps the decode ring afterwards (already
  implemented and tested).
- Our own frames are added to the list as TX rows, like the FT8 app does.

## Frequency (audio offset)

- **Main tuning knob** moves the TX offset marker, as in the FT8 app, which
  already uses it for TX frequency. The dial frequency stays locked. The MFK
  keeps scrolling the list.
- **Hold TX offset**, default on: replying to someone doesn't move your
  offset to theirs, so you keep your own clear spot. Turning it off makes
  Reply jump to the other station's offset.
- Heartbeats go at a random offset in the 500–1000 Hz heartbeat sub-band,
  never at your chat offset.
- Offsets are limited to 500–2450 Hz, so the whole 50 Hz signal stays
  below 2500 Hz, the top of the range receivers search, and inside the
  radio's TX filter.

## Screen

Same layout as today, plus a one-line TX bar between the waterfall strip and
the list, visible only once TX is enabled:

```
┌──────────────────────────────────────────────────────────────┐
│ waterfall ░░▓░░░░░░░░█▌TX░░░░░░░░░░    JS8 20m 14:02:31Z     │
├──────────────────────────────────────────────────────────────┤
│ TX 1450 Hz │ → K2XYZ │ "K2XYZ SNR?"      │ ● keying 1/1  4 s │  ← TX bar
├──────────────────────────────────────────────────────────────┤
│ 14:01:45 -12 1320  N0XYZ: K2XYZ HELLO FROM THE X6100 TEST    │  red: to me
│ 14:02:00 -07  910  VE3KP: @ALLCALL CQ CQ CQ FN03             │  green: CQ
│ 14:02:15   TX 1450  K2XYZ: N0XYZ SNR?                        │  blue: sent
└──────────────────────────────────────────────────────────────┘
```

TX bar states: `idle`, `queued — starts in 9 s`,
`keying 2/3 — 11 s`, `auto-reply queued: SNR -12 to N0XYZ`.
While keyed, the waterfall shows a red border and the Stop button is
highlighted.

## Buttons

Five per page (the firmware's limit). Page 1 stays receive-focused:

| Page | 1 | 2 | 3 | 4 | 5 |
|---|---|---|---|---|---|
| **JS8 1:3** | page | Show: … | **Reply** | **Send…** | **Stop TX** |
| **JS8 2:3** | page | CQ | Heartbeat | Query ▸ | Clear |
| **JS8 3:3** | page | Time Sync | Auto-reply: Off | HB: Off/30/60 min | Test WAV |

- **Reply**: opens the keyboard pre-filled with `CALL ` for the selected
  row's station. You type the rest, then OK queues it.
- **Send…**: opens the keyboard empty, for CQ text, @ALLCALL, @groups or
  anything else.
- **Query ▸** opens a short list for the selected station: `SNR?`,
  `GRID?`, `INFO?`, `STATUS?`, `HEARING?`, `AGN?`, then `73`, `ACK`,
  `RR`. One press queues it, e.g. `K2XYZ SNR?`.
- **CQ**: queues `CQ CQ CQ <grid>` at your offset.
- **Heartbeat**: queues one heartbeat now (sub-band offset).
- **Stop TX**: clears the queue and unkeys at once. ESC in the app does the
  same, before closing.

The keyboard only accepts characters JS8 can send (taken from js8core's
alphabet), shows how many frames the text needs and roughly how long it
takes ("3 frames, 45 s"), and warns above ~6 frames.

## Auto-reply and heartbeats (off by default)

Three switches on page 4, each off by default, as in desktop JS8Call:

- **AUTO** answers queries to your call:

  | Heard | Reply (queued at your offset) |
  |---|---|
  | `YOU SNR?` / `YOU ?` | `THEM SNR -12` |
  | `YOU GRID?` | `THEM GRID FN42AB` |
  | `YOU INFO?` / `STATUS?` | `THEM INFO …` / `THEM STATUS …`, if you set the text |
  | `YOU HEARING?` | up to 4 recently heard calls |
  | `YOU AGN?` | your last transmission again |

  With AUTO off, the answer is *offered* instead: select the station and
  press Reply, and the keyboard opens with it filled in.
- **HB** sends a heartbeat every 5–30 min (switch it on, turn the knob, press
  again). Timing is desktop's: the next slot + 1 s + interval, one slot
  later a quarter of the time.
- **HB ACK** answers others' heartbeats with `THEM HEARTBEAT SNR -12` in
  the heartbeat sub-band. As on desktop it acts only while AUTO and HB are
  both on.

Guards: never to yourself, a group or @ALLCALL, nothing from low-confidence
decodes, the same answer to the same station at most every 5 min (heartbeat
acks: 15 min), and replies that arrive while you type or transmit wait for
their turn. Any message to you other than a heartbeat ack starts a QSO,
which turns HB and HB ACK off until you turn them back on. Automatic
transmissions pause after 60 min without a key press (desktop's idle
watchdog). The status line shows **AUTO**, **HB 30m next hh:mm**, **ACK**,
or **AUTO/HB PAUSED (idle)**.

`YOU MSG …` with a good checksum → `ACK` needs somewhere to keep the
message, so it moved to T5 with the inbox.

Regulatory note: whether unattended replies are allowed, and on which
frequencies, depends on your licence and country. That's why auto-reply is
opt-in and always visible.

## Safety limits

- **Power cap 5 W** while in JS8, as in the FT8 app (radio setting
  restored on exit). JS8 keys up to ~85 % of the time during long messages.
- **TX watchdog**: auto-reply and heartbeats pause after 60 min without a
  key press (desktop JS8Call's default).
- **Message length**: warn above 6 frames and refuse above 20 (5 min of TX).
- **Callsign and grid required** before anything can be sent (APP → Callsign,
  APP → QTH).
- High SWR or ALC faults from the radio stop TX, reusing what the radio
  already reports.

## Logging

A **Log QSO** action on a row writes ADIF with `MODE=MFSK SUBMODE=JS8` to
`/mnt/js8_log.adi` and to the radio's QSO database. The database's mode
enum needs a JS8 entry; that's appended, like `ACTION_APP_JS8`.

## Phases

| | Scope | Testable without a radio? |
|---|---|---|
| **T1** ✅ | `src/js8` transmitter: text → frames → GFSK audio at the radio rate, slot scheduling, queue, abort. Loopback tests: TX audio into our RX decodes. Frame/time estimates for the keyboard. | Yes, fully |
| **T2** ✅ | Radio glue: split the FT8 app's TX player into a shared "play with PTT and ALC" routine; JS8 uses it. Reply, Send, CQ, Stop, TX bar, TX rows, TX offset on the main knob. | Mostly (harness), then dummy load |
| **T3** ✅ | Query list, single heartbeat, Hold offset, Stations view (★ = heard you), messages to you always shown | Harness + on air |
| **T4** ✅ | Auto-reply, HB interval, HB acks, watchdog | Library tests + harness, then on air |
| **T5** | Logging; an inbox for stored MSG (with MSG ACK) and QUERY MSGS | |

## Checking it on air

1. Dummy load first: key a CQ and confirm power and ALC behave like FT8.
2. Antenna, heartbeat at 5 W: check <https://pskreporter.info> for JS8 spots
   of your call. JS8Call stations report what they decode.
3. A directed `SNR?` to a nearby JS8Call station, or to a second receiver
   such as a WebSDR or KiwiSDR running desktop JS8Call.
