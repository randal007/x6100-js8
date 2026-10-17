**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed: the X6100 firmware (OpenX6100, gdyuldin's) with a JS8 app next to FT8 and RTTY.

**Release Candidate 1 is the JS8 that is going into the OpenX6100 firmware itself.** From here on it gets bug fixes only.

New since beta 5.2: **JS8 opens and closes without a pause**. The decoder's setup is now kept on the SD card, as desktop JS8Call does, so it runs once instead of every time JS8 opens. Everything else is as in [beta 5.2](https://github.com/randal007/x6100-js8/releases/tag/js8-beta5.2).

> [!IMPORTANT]
> Writing the image replaces the whole SD card. **Copy your files off the DATA partition first** (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`, `bluetooth.tar`) and copy them back after the first start.

## Highlights

### JS8 opens and closes without a pause

- Every time JS8 opened, the decoder's first pass set up its maths from scratch (FFTW timing test runs). That took **12-13 seconds on the radio**, and closing JS8 in the meantime hung until it was done.
- The setup is now saved on the SD card as `js8_wisdom.dat`, the same idea as desktop's `js8call_wisdom.dat`. It runs **once**: the first time JS8 opens after a new image, with a yellow **Setting up the decoder** notice over the waterfall for about 20 seconds.
- After that, JS8 decodes from the start every time and closes at once. Tested on the radio.
- Deleting `js8_wisdom.dat` is harmless: it's made again at the next open.

### Licence notes

The JS8 files that follow desktop JS8Call are now marked GPL-3.0-or-later, as desktop's own are, and the engine's bundled libraries (CRC++, Eigen) carry their licence files. Nothing changes in use.

## Bug fixes since beta 5.2

- Closing JS8 right after opening it could hang for 10-15 seconds (above)

## Updating

1. Copy the DATA partition's files to your PC (see the box at the top).
2. Download `sdcard.js8-rc1.img.zip` below and write it to the card with balenaEtcher or Rufus.
3. Start the radio once with the new card, then copy your files back.
4. APP → page 3 → **JS8**. The first open shows *Setting up the decoder* for about 20 seconds, once.

**From beta 4 or earlier:** see the [beta 5 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta5) first.

## Known issues

- **The first open after a new image** shows *Setting up the decoder* for about 20 seconds, and messages in that first slot or two can be missed. Once only
- **Transmit power can stop short of the setting:** without the amplifier, set to 5 W, one JS8 transmission stayed at about 3.8 W with the ALC at 0. Being measured; with the XPA125B it's steady
- **Spinning the main dial very fast** has twice restarted the GUI program (the radio comes back by itself in a few seconds). Seen on older builds too; it's in OpenX6100's shared code, not JS8
- **Bluetooth headphones while JS8 is open:** while you listen on headphones the radio's speaker is turned down, so JS8's alert beeps may not be heard. Not tried yet
- **Connect on a keyboard's old entry** keeps the Bluetooth window busy for up to a minute; Connect the new entry instead (the old one goes by itself)
- **Not tried on the air yet:** high-SWR protection, APRS More services, POTA/SOTA spots, Ultra on a busy band, and the GPS grid outside

## Questions and bug reports

Bugs: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions). Everything is explained in the [manual](https://github.com/randal007/x6100-js8#readme).

73 de VE7NHW
