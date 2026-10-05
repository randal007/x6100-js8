**JS8 on the Xiegu X6100, running on the radio itself.** This is R1CBU's X6100 firmware (gdyuldin v1.0.2) with a JS8 app added next to FT8 and RTTY.

**Beta 4.6 is a quick fix to beta 4.5: please use it instead of 4.5.** Everything that's new in 4.5 (the station History, Time: Auto, saved messages, Sort: QSO, the map's count tags, high-SWR protection, the APRS Echo test and services, the calmer waterfall, the USB keyboard fix) is in here too: see the [beta 4.5 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.5).

### The fix: a steady transmit level at low power

With the power set low (0.3 W driving an amplifier), the transmit level see-sawed during a transmission: an XPA125B behind the radio swung between about 18 and 35 W every couple of seconds, and the ALC bounced with it.

Why: the radio reads its power in 0.1 W steps, so at 0.3 W a true 0.28 W reads as 0.2 W, "short". JS8's level loop acted on every single reading, about 23 times a second: up hard when it read short, then down hard when the ALC rose.

Now the readings are averaged over about a second; **within a transmission the drive only ever comes down** (if the ALC shows overdrive), in small steps, so the level stays steady; **between transmissions it comes up** a little if the last one ran short, until the ALC just starts to show, which holds the power at your setting. At very low power it can take a few transmissions to settle. It starts from the level you had, so watch your amplifier for the first couple of transmissions.

JS8 only: the FT8 app has its own level control and is unchanged. Tested against a model of the radio (0.1 W steps, meter delay, noise) where the old loop swung 16 dB within a frame and the new one stays within 0.8 dB; reports from the air are very welcome.

### Install

1. Download `sdcard.js8-beta4.6.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

**Updating from beta 4.5 or 4.1:** writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`) to your PC first, and copy them back after the first start. **Coming from beta 4 or earlier:** see the [beta 4.1 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.1).

### Known issues

As in [beta 4.5](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.5): WeFax, NavTex and the channel list are not included; a slight waterfall flicker while it scrolls (much reduced by *Waterfall: Light*); not yet tried on the air: the high-SWR protection, the APRS Echo test and services, POTA/SOTA spots and a Bluetooth keyboard. The `<MYVERSION>` macro says `X6100 JS8 beta 5`.

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions). The [manual](https://github.com/randal007/x6100-js8#readme) covers everything.

73 de VE7NHW
