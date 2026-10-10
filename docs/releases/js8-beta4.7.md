**JS8 on the Xiegu X6100, running on the radio itself.** This is R1CBU's X6100 firmware (gdyuldin v1.0.2) with a JS8 app added next to FT8 and RTTY.

**Beta 4.7 improves the transmit level fix from 4.6: please use it instead of 4.6 or 4.5.** Everything that's new in 4.5 (the station History, Time: Auto, saved messages, Sort: QSO, the map's count tags, high-SWR protection, the APRS Echo test and services, the calmer waterfall, the USB keyboard fix) is in here too: see the [beta 4.5 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.5).

### What changed

- **The transmit level now finds its place live, within the first transmission or two.** In 4.6 the level stayed steady during a transmission (no more see-saw), but at low power it only crept up a little after each one, so an amplifier behind the radio took many transmissions to come up to power. Now, while the ALC reads zero, the drive rises smoothly during the transmission (faster while it's far short, gently near the end), and **stops and holds** as soon as the ALC starts to show, which holds the power at your setting. It never turns back up within a transmission, so it can't see-saw; if the ALC shows overdrive it eases down a little. The level it finds is kept, so the next transmission starts right there, steady from the start.
- **A custom frequency says "JS8 Custom"** on the status line, the message line and the info rows. A CB frequency used to show the band of the nearest JS8 preset ("JS8 10m"). Settings → Frequencies still shows the frequency in kHz.

How the level control works, and why it can't see-saw: readings are averaged over half a second (the radio reads its power in 0.1 W steps, too coarse to act on one at a time); while the ALC reads 0.0 the drive goes up 1 dB a second when far short, 0.5 dB a second near the setting; at the first sign of ALC (0.1 or more) it holds for the rest of the transmission. JS8 only: the FT8 app has its own level control and is unchanged. Reports from the air are very welcome, especially with an amplifier.

### Install

1. Download `sdcard.js8-beta4.7.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

**Updating from beta 4.6, 4.5 or 4.1:** writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`) to your PC first, and copy them back after the first start. **Coming from beta 4 or earlier:** see the [beta 4.1 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.1).

### Known issues

As in [beta 4.5](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.5): WeFax, NavTex and the channel list are not included; a slight waterfall flicker while it scrolls (much reduced by *Waterfall: Light*); not yet tried on the air: the high-SWR protection, the APRS Echo test and services, POTA/SOTA spots and a Bluetooth keyboard. The `<MYVERSION>` macro says `X6100 JS8 beta 5`.

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions). The [manual](https://github.com/randal007/x6100-js8#readme) covers everything.

73 de VE7NHW
