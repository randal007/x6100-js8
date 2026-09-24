# X6100 JS8

Native [JS8](https://js8call.com) for the Xiegu X6100, running on the radio
itself with no PC, phone or tablet attached.

This is the X6100 LVGL firmware GUI with a JS8 app added alongside the
existing FT8, RTTY, WeFax and NavTex apps.

> **Status: early development. Receive-only is in progress; no transmit yet.**
> Don't flash this expecting a finished feature. See [Roadmap](#roadmap).

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
- [ ] `src/js8` RX library + host tests
- [ ] JS8 app on APP 3:3: waterfall, band activity, directed messages
- [ ] JS8 band presets (DB migration)
- [ ] CI image build with JS8 dependencies
- [ ] On-air RX validation against desktop JS8Call
- [ ] Transmit: heartbeat, CQ, directed messages, free text
- [ ] Fast / Turbo / Slow submodes, auto-reply, logging

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
