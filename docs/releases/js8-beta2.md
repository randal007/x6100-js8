**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is the R1CBU/1KO125 X6100 firmware with a JS8 app added next to FT8, RTTY, WeFax and NavTex.

Beta 2 fixes everything the first on-air QSOs with beta 1 turned up, and adds GhostNet and custom frequencies, a POTA/SOTA spot form, and position beacons with a message.

### Install

1. Download `sdcard.js8-beta2.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

Updating from beta 1? Writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`) to your PC first, and copy them back after the first start. The GhostNet frequencies are added to your settings automatically.

### New in beta 2

Fixes from the first QSOs:
- ESC closes only the text box, no longer the whole app
- USB keyboard: no lost letters; lowercase is typed as capitals
- Log QSO: Enter in a field only saves that field (Submit logs)
- The selected station stays selected (green bar, `selected:` in the TX bar), and Reply always goes to it
- The list follows new lines, and stays in view while you type a reply
- CQ switches heartbeats off; **hold CQ for auto CQ**, one a minute after each CQ ends, until someone answers
- Show *No HB* also hides heartbeat SNR reports
- Hold the page button to go back a page; Clear on page 1, HW CPY? on page 2

New:
- **Freq** (page 6): JS8Call's frequencies, **GhostNet's** (3.575, 7.107, 14.107 MHz), or **your own** typed in kHz
- **Send up to 3000 Hz** (the TX filter is set to 200–3000 Hz while JS8 is open, and put back when you leave)
- **POTA and SOTA spot form**: park or summit, the JS8 frequency or your SSB/CW one, mode and comment, all remembered. POTA spots now go through APSPOT (posts to pota.app with your pota.app account); SOTA through APRS2SOTA (registration needed)
- **Position beacons with a message**: Spot my grid / Spot GPS position ask for an optional message, e.g. `MADE IT TO CAMP`, shown with your position on aprs.fi. Just Enter sends the plain beacon
- Each frequency keeps its own Stations list

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use it.

### Known issues

- Test beep may be silent
- The waterfall scrolls a little less smoothly than the main X6100 waterfall
- A power loss with JS8 open leaves the USB receive filter at 200–3000 Hz
- APRS: only the plain grid spot is confirmed on the air so far; the spot form, position messages, SMS, email and Winlink need testing through the gateways. Gateway replies come back over APRS, never over JS8, so check pota.app, SOTAwatch or aprs.fi

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
