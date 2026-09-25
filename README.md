# X6100 JS8

Native [JS8](https://js8call.com) for the Xiegu X6100, running on the radio
itself with no PC, phone or tablet attached.

This is the X6100 LVGL firmware GUI with a JS8 app added alongside the
existing FT8, RTTY, WeFax and NavTex apps.

> **Status: receive and manual transmit, not yet tested on a radio.** The
> JS8 app decodes JS8 Normal-mode traffic and can send replies, free text
> and CQs. Every part has been tested on a PC (see [Testing](#testing)), but
> none of it on real hardware or real signals yet. **Test transmit into a
> dummy load first.** There is no auto-reply and no automatic heartbeat:
> it only transmits when you press a button.

![JS8 app, all messages](docs/screenshots/js8_04_all.png)

*The real JS8 app code running headless on a PC with a simulated band:
a CQ (green), heartbeats (grey), messages to this station (red) and an
@ALLCALL message (blue). See [tools/js8_ui_harness](tools/js8_ui_harness).*

## Using it

APP → page 3 → **JS8**. The radio tunes the nearest JS8 frequency (the band
keys step through JS8Call's standard dial frequencies) and starts decoding.

| Page | Button | Does |
|---|---|---|
| 1 | Show: No HB / Directed / All | Filter the list. *Directed* shows messages to your callsign (set in APP → Callsign) or to @groups. |
| 1 | **Reply** | Opens the keyboard with the selected station's call filled in, e.g. `N0XYZ `. Type the rest (`SNR?`, `HELLO …`) and press Enter. |
| 1 | **Send…** | Opens the keyboard empty: `@ALLCALL …`, a call and a message, or free text (your call is added). |
| 1 | **Stop TX** | Unkeys at once and drops the rest of the message. ESC does the same; the next ESC closes the app. |
| 2 | **CQ** | Sends `CQ CQ CQ <grid>`. |
| 2 | **Heartbeat** | Sends one heartbeat (`CALL: HEARTBEAT FN42`) at a free spot in the 500–1000 Hz heartbeat sub-band. Your chat offset doesn't move. |
| 2 | **Query >** | One-press messages to the selected station: SNR?, Send SNR (how you hear them), GRID?, My grid, INFO?, STATUS?, HEARING?, AGN?, RR, 73. |
| 2 | Clear | Clear the list, the station list and any half-received messages. |
| 3 | Time Sync | Snap the clock to the nearest 15 s. JS8 needs the clock within about ±1 s of UTC. |
| 3 | Test WAV | Decode `/mnt/js8_test.wav` instead of the radio audio: see [test-audio](test-audio). |
| 3 | **Hold: On/Off** | On (default): replies go out on your own offset. Off: Reply and Query move your offset to the station's first. |
| 3 | **Show Stations / Messages** | Switch the list to one row per station, like desktop JS8Call's Call Activity. |
| 4 | **AUTO: Off/On** | Answer SNR?, GRID?, INFO?, STATUS?, HEARING? and AGN? sent to your call. Off (default): the answer is offered on Reply instead. |
| 4 | **HB: Off/N min** | Send heartbeats every N minutes. Switching it on lets the main knob set 5–30 min; press HB again (or wait 8 s) to finish, press once more to turn it off. Holding HB changes the interval without switching. |
| 4 | **HB ACK** | Answer others' heartbeats with how you hear them. Acts only while AUTO and HB are on, as on desktop. |
| 4 | **Texts…** | Edit what AUTO sends for INFO? and STATUS? (kept in `/mnt/js8_texts.txt`). |

**Who heard you:** in the Stations view, stations that have heard you are
marked `*` and listed first. That covers anyone who acknowledged your
heartbeat or sent you a message. When they reported your signal (e.g. a
heartbeat ack `YOU HEARTBEAT SNR -08`) the view shows "heard you −08" and
how long ago. The other columns are time since last heard, their SNR here,
their grid and your distance to them. Stations drop off after an hour; a
band change clears the list.

![Stations view: * marks stations that heard you](docs/screenshots/js8_11_stations.png)

**Transmitting:** the **main tuning knob** sets your TX offset (the red band
on the waterfall, 500–2450 Hz, remembered). The dial frequency stays
locked. The TX bar under the waterfall shows the offset and then the
countdown ("starts in 9 s (1/3)"). It turns red while keyed, and the
waterfall gets a red border. Sent messages appear in the list in blue.
Power is capped at 5 W, as in the FT8 app, and restored when you leave.

![Replying: TX bar keyed, sent message in blue](docs/screenshots/js8_08_tx_keying.png)

**Automatic replies and heartbeats** are off until you switch them on, each
on its own, like desktop JS8Call. A message to you (other than a heartbeat
ack) turns HB and HB ACK off again, so heartbeats don't cut into a QSO.
After an hour without touching the radio, automatic transmissions pause
until you press something. The status line always shows what's on.

![AUTO, HB and HB ACK on: heartbeat queued in the sub-band](docs/screenshots/js8_15_auto_hb.png)

Rows show UTC time, SNR, audio offset and the message. The MFK moves through
the list and marks the selected station's offset with a green line on the
waterfall. Tapping a row also shows its callsign and SNR. Multi-frame messages appear
once their last frame arrives. Buffered commands such as `MSG` have their
checksum verified and removed, as in desktop JS8Call.

## Lineage

| Layer | Project |
|-------|---------|
| Firmware GUI | [gdyuldin/x6100_gui](https://github.com/gdyuldin/x6100_gui) v0.34.2 (R1CBU firmware, originally by Oleg Belousov R1CBU, maintained by Georgy Dyuldin R2RFE) |
| SWL additions | [TheMurusTeam/custom-r1cbu](https://github.com/TheMurusTeam/custom-r1cbu) 0.34.2 beta 6 by Hany El Imam 1KO125: WeFax, NavTex, channel list, broadcast info |
| JS8 engine | `core/` from [JS8Call-improved/Android-port](https://github.com/JS8Call-improved/Android-port), a Qt-free C++ port of [JS8Call](https://github.com/js8call/js8call) by Jordan Sherer KN4CRD and contributors |

Git history keeps these layers apart. The upstream x6100_gui history is
intact up to tag `v0.34.2`. Commit `20c4b77` imports the 1KO125 source,
which is published only inside that project's release tarballs. Everything
after that commit is the JS8 work.

## How it fits together

```
radio audio 44.1 kHz ─► dsp.cpp ÷4 ─► 11025 Hz float ─► dialog_js8 audio_cb
                                                          │
                          src/js8 (no LVGL, host-testable)│
                          ┌───────────────────────────────▼──┐
                          │ resample 11025 → 12000 Hz        │
                          │ js8core engine: 60 s UTC ring,   │
                          │   slot scheduling, decode thread │
                          │ frame → text, multi-frame join   │
                          └──────────────┬───────────────────┘
                                         ▼
                        dialog_js8.c: waterfall + band activity
```

JS8 Normal uses the same waveform as FT8: 79 8-FSK symbols of 0.16 s at
6.25 Hz spacing, 15 s slots, and the same Costas sync. It differs in the
error-correcting code (LDPC 174,87 with CRC-12 instead of 174,91 with
CRC-14), in the absence of Gray coding, and in its message layer. See
[docs/RESEARCH.md](docs/RESEARCH.md) for the full comparison and for why
the Android port's core was chosen over porting desktop JS8Call.

## Roadmap

- [x] Research: firmware, JS8 protocol, existing ports
- [x] Vendor js8core with small documented patches ([UPSTREAM.md](third-party/js8core/UPSTREAM.md))
- [x] `src/js8` RX library + host tests (x86 with ASan/UBSan, and ARM under qemu)
- [x] JS8 app on APP 3:3: waterfall, message list, filters, test mode
- [x] JS8 band presets (DB migration 4)
- [x] Headless UI harness and test-audio generator
- [x] CI image build with JS8 dependencies (GitHub Actions, manual dispatch)
- [ ] First boot on a radio: WAV test mode, then on-air RX against desktop JS8Call
- [ ] Decode timing on the Cortex-A7 on a busy band
- [x] Transmit, manual: reply, directed messages, free text, CQ, stop ([plan](docs/TX_PLAN.md), phases T1–T2)
- [ ] Transmit on air: dummy load, then PSK Reporter spots and a desktop JS8Call QSO
- [x] Heartbeat, query shortcuts, Hold offset, Stations view with "heard you" (T3)
- [x] Opt-in auto-reply, heartbeat interval, heartbeat acks, idle watchdog (T4)
- [ ] Logging and an inbox for MSG (T5)
- [ ] Fast / Turbo / Slow submodes

## Testing

| What | How |
|---|---|
| Library unit + end-to-end tests | `tests/test_js8.cpp` (Catch2): resampler image rejection, rendering and assembly of frames made by JS8Call's encoder, the Android port's assembly tests, checksums, clock realign and gap fill, auto-reply rules, and 11025 Hz audio → decoded message. `[.slow]` adds real-time WAV test mode. |
| The real UI on a PC | [tools/js8_ui_harness](tools/js8_ui_harness): the actual dialog on stock LVGL with a simulated busy band, in real time, under ASan/UBSan. |
| On the radio, without RF | [test-audio](test-audio) and the Test WAV button. |
| Radio compiler | Everything JS8 compiles cleanly with the image's GCC 12.3 (Cortex-A7, NEON) against Boost 1.80. |

## Building

The SD card image is built by GitHub Actions (`.github/workflows/main.yml`)
on each pushed tag, using
[AetherX6100Buildroot](https://github.com/gdyuldin/AetherX6100Buildroot).
Upstream build instructions are in
[docs/UPSTREAM_README.md](docs/UPSTREAM_README.md).

## License

The GUI is LGPL-2.1-or-later (`LICENSE`). The vendored js8core is GPLv3
(`third-party/js8core/LICENSE`), so a firmware image that includes it is
distributed under GPLv3.
