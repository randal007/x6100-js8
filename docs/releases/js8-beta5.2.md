**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed: the X6100 firmware (OpenX6100, gdyuldin's) with a JS8 app next to FT8 and RTTY.

Beta 5.2 makes **long messages** work the way they do on desktop: no more freezes while typing, up to 20 frames, and long messages from desktop stations no longer cut short. **Log QSO keeps what you typed** during a QSO, and **Bluetooth is locked down**: the radio stays hidden until you Scan, and CAT over Bluetooth only talks to paired devices.

> [!IMPORTANT]
> Writing the image replaces the whole SD card. **Copy your files off the DATA partition first** (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`, `bluetooth.tar`) and copy them back after the first start.

## Highlights

### Long messages, as on desktop

- **No more freezes typing a long message.** Every key re-counted the whole message's frames for the "N frames, N s" line, and past about 8 frames the screen stalled (once for 23 seconds on the air). The count now waits until you pause typing, as desktop does, and runs on its own thread.
- **Up to 20 frames.** The message box stopped at 160 characters (about 10 frames). Now only the 20-frame limit (5 minutes on the air) stops you, as on desktop.
- **Long messages from desktop stations come through whole.** The message list, the preview, History and the Inbox cut every message at 255 characters; a 20-frame message carries up to about 425.

### Log QSO keeps what you typed during the QSO

Typing a name mid-QSO used to mean saving halfway (two log entries) or losing it. Now a name, note or grid typed in the Log popup and then cancelled is **kept for that station**, and the Log at the end opens with it: one QSO, start to end. It's forgotten on Save, or 30 minutes after the QSO.

### Bluetooth locked down

From OpenX6100 (our x6100_gui#297 and AetherX6100Buildroot#6, and Kris SP2KMO's #296):

- **The radio is hidden at power-on.** It can only be found and paired with during the 3 minutes after **Scan** in the Bluetooth window. Before, it was visible to anyone nearby until the window had been opened once.
- **CAT over Bluetooth only talks to paired devices.** A phone app (FT8CN, FT8TW) now needs pairing once: press Scan in the Bluetooth window, then pair from the phone.
- **A Bluetooth headset's button only keys the radio from that headset.**

Keyboards already paired stay paired.

## Bug fixes since beta 5.1

- Typing a long message froze the screen (above)
- Long messages from desktop stations were cut at 255 characters (above)
- A message the encoder couldn't take ended the GUI program instead of being refused (found in a code review, never seen on the air)
- **Ultra is decoded once a slot, as desktop does** (3.1 s into the slot, when the signal is complete). Betas 5 and 5.1 tried it early and then every second, which desktop doesn't. Spotted by JS8Call-improved's developer
- From OpenX6100: the low tuning limit is 100 kHz (was 500 kHz), and DNF's notch is drawn at the right place

## Updating

1. Copy the DATA partition's files to your PC (see the box at the top).
2. Download `sdcard.js8-beta5.2.img.zip` below and write it to the card with balenaEtcher or Rufus.
3. Start the radio once with the new card, then copy your files back.
4. APP → page 3 → **JS8**.

**From beta 4 or earlier:** see the [beta 5 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta5) first.

## Known issues

- **Closing JS8 right after opening it can hang for 10-15 seconds.** The decoder's first pass after JS8 opens sets up its maths from scratch, which takes 12-13 s on the radio, and closing waits for it. Wait a few seconds after opening before closing. A fix that keeps the setup on the SD card is planned
- **Spinning the main dial very fast** has twice restarted the GUI program (the radio comes back by itself in a few seconds). Seen on older builds too; it's in OpenX6100's shared code, not JS8
- **Bluetooth headphones while JS8 is open:** while you listen on headphones the radio's speaker is turned down, so JS8's alert beeps may not be heard. Not tried yet
- **Connect on a keyboard's old entry** keeps the Bluetooth window busy for up to a minute; Connect the new entry instead (the old one goes by itself)
- **Not tried on the air yet:** high-SWR protection, APRS More services, POTA/SOTA spots, Ultra on a busy band, and the GPS grid outside

## Questions and bug reports

Bugs: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions). Everything is explained in the [manual](https://github.com/randal007/x6100-js8#readme).

73 de VE7NHW
