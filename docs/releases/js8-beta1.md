**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is the R1CBU/1KO125 X6100 firmware with a JS8 app added next to FT8, RTTY, WeFax and NavTex.

Beta 1 has been in use on the air: heartbeats acknowledged, queries answered, QSOs and messages with desktop JS8Call stations, and Fast stations decoded alongside Normal.

### Install

1. Download `sdcard.js8-beta1.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

Updating from an earlier card? Writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`) to your PC first, and copy them back after the first start.

### What's in it

- Receive and send JS8 as desktop JS8Call does: same decoder, frames, timing, commands and replies
- All four speeds (Normal, Fast, Turbo, Slow) decoded at once; send at any of them
- One-press messages: CQ, heartbeat, HW CPY?, SNR?, GRID?, INFO?, STATUS?, HEARING?, AGN?, RR, 73
- Opt-in automatic replies, heartbeats and heartbeat acks, all off every time the app opens
- Inbox for `MSG`, sending messages, and holding messages for other stations (store and forward)
- ADIF log in desktop JS8Call's format, with a prompt when a QSO ends, and POTA/SOTA activation fields
- Alerts: a beep for messages, CQs or new stations, and your own alert words highlighted
- APRS through JS8 gateways: grid and GPS spots, POTA, SOTA, SMS, email, Winlink
- Time Sync from the decodes; a Stations view showing who heard you
- Long messages shown as they arrive; receive filter 200–3000 Hz while the app is open

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use it.

### Known issues

- Type messages in capitals (lowercase letters are ignored)
- ESC in the Reply keyboard leaves the app instead of just closing the keyboard
- Test beep may be silent
- The waterfall scrolls a little less smoothly than the main X6100 waterfall
- Changing band clears the Stations list
- Transmitting stops at 2500 Hz (decoding covers up to 3000 Hz)
- APRS: only the grid spot is confirmed on the air so far; POTA, SOTA, SMS, email and Winlink still need testing

Planned for beta 2: those fixes, sending up to 3000 Hz, HW CPY? and Clear swapped between pages 1 and 2, GhostNet frequencies, and a custom frequency option.

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
