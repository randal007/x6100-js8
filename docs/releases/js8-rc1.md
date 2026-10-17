**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed: the X6100 firmware (OpenX6100, gdyuldin's) with a JS8 app next to FT8 and RTTY.

**Release Candidate 1 is the JS8 that is going into the OpenX6100 firmware itself.** From here on it gets bug fixes only.

New since beta 5.2: **JS8 opens and closes without a pause**, its decoder setup kept on the SD card as desktop JS8Call does. Below that is **everything JS8 on the X6100 does**, from beta 1 to here, in one list.

> [!IMPORTANT]
> Writing the image replaces the whole SD card. **Copy your files off the DATA partition first** (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`, `bluetooth.tar`) and copy them back after the first start.

## New since beta 5.2

- **JS8 opens and closes without a pause.** Every time JS8 opened, the decoder's first pass set up its maths from scratch (FFTW timing test runs). That took **12-13 seconds on the radio**, and closing JS8 in the meantime hung until it was done. The setup is now saved on the SD card as `js8_wisdom.dat`, as desktop's `js8call_wisdom.dat`, so it runs **once**: the first time JS8 opens after a new image, with a yellow **Setting up the decoder** notice over the waterfall for about 20 seconds. After that JS8 decodes from the start and closes at once. Tested on the radio. Deleting the file is harmless: it's made again.
- **Licence notes:** the JS8 files that follow desktop JS8Call are marked GPL-3.0-or-later, as desktop's own are, and the engine's bundled libraries (CRC++, Eigen) carry their licence files. Nothing changes in use.

## Bug fixes since beta 5.2

- Closing JS8 right after opening it could hang for 10-15 seconds (above)

## Everything JS8 on the X6100 does

Built up from beta 1 to here. The [manual](https://github.com/randal007/x6100-js8#readme) explains each one.

### On the air, as desktop JS8Call

- **Receive and send JS8 exactly as desktop JS8Call-improved does:** the same decoder, frames, timing, commands and replies, checked frame by frame against desktop's own code, so desktop users see nothing unusual
- **Five speeds:** Slow, Normal, Fast and Turbo all decoded at once, plus **Ultra** (desktop's "JS8 60", 4 s slots) on its own decoder thread; send at any of them, and **hold Speed** to match the selected station
- **Long messages up to 20 frames**, as on desktop, and long messages from desktop stations come through whole
- **One-press messages:** CQ, heartbeat, HW CPY?, SNR?, GRID?, INFO?, STATUS?, HEARING?, AGN?, RR, 73, *Any messages?*, *Anyone have messages?*, *Can they reach...?* and *Can anyone reach...?*
- **Ten saved messages** of your own with desktop's macros (`<CALL>`, `<SNR>`, `<MYGRID4>` ...); macros also work in typed messages, INFO and STATUS
- **Automatic replies, heartbeats and heartbeat ACKs**, each a switch you turn on, following desktop's rules; nothing transmits by itself when the app opens
- **Heartbeat works like CQ:** press for one, hold for auto heartbeats every 5-30 minutes; they keep going while you call CQ and pause when someone calls you
- **Relays** through other stations, with the whole path kept for Reply
- **Inbox** for `MSG` messages, sending messages, and **holding messages for other stations** (store and forward); `QUERY MSG` answered as desktop does, brackets included
- **STATUS** starts as desktop's default (`IDLE <MYIDLE> VERSION <MYVERSION>`)

### Time

- **Automatic time sync from every station decoded**, heartbeats included (desktop's *Automatic Time Drift*); hold **Time** to search when the clock is too far off for anything to decode
- **USB GPS:** your grid from the fix, `GPS` in the top line, GPS time saved to the radio's battery clock, and JS8's timing kept steady when the clock is set

### The screen

- **A calm waterfall** with four levels of averaging, drawn straight onto the display's lower layer so it costs the radio very little
- **Decode marks** on the waterfall, as desktop's *Show decode attempts*
- **Desktop's colours and marks:** commands in yellow, `♢` when a message's last frame arrived, your own messages in red and messages to you in blue
- **Stations view:** who is on and who heard you, with bearing, km or miles, stations heard *via* a relay, a **QRZ** line naming who called you while you were away, `@` on APRS gateways, and **Sort** by heard you, SNR, time, distance or QSO
- **The map**, in GridTracker's style: grid squares, paths to who heard you, new stations popping up (**NEW DXCC** / **NEW GRID**), CQ tags, other stations' QSOs, a count where several share a square, your continent, the world, or **Follow** the selected station with its headings
- **Station History:** everyone you've exchanged messages with, per band, kept for good, with their INFO, STATUS and the text of every QSO; an all-time list and map per band
- **Hold the MFK** on a station to lock it while you scroll
- **The red and green bands** show where a heartbeat goes and how wide the selected station's speed is

### Logging and alerts

- **ADIF log** in desktop JS8Call's format, with a prompt when a QSO ends, POTA/SOTA activation fields, and an operator callsign of its own; **Log QSO keeps what you typed** during the QSO
- **Alerts:** a beep for messages, CQs or new stations, and alert words (calls or words you choose) highlighted in purple

### APRS through JS8 gateways

- **Grid and GPS position beacons** (with an optional message), **POTA and SOTA spots** on any frequency and mode
- **Two-way SMS** (with *Reply by SMS* and delivery receipts), **email** and **Winlink**
- **More services:** weather, sunrise, repeaters, ISS passes, nearest hospital, fuel or water, callsign lookup and more, each message filled in

### Frequencies and transmitting

- **JS8Call's, GhostNet's or your own frequencies** on the band keys; a custom frequency shows as "JS8 Custom"
- **A steady transmit level for an amplifier:** the drive ramps up within the first transmission and holds at the ALC's onset
- **High-SWR protection** for an unattended station: over 3:1 while sending switches every automatic sender off, with three beeps
- While JS8 is open: RX filter 200-3000 Hz, noise reduction, the noise blanker and the notches off, power capped at 5 W; all put back when JS8 closes

### Keyboards and the radio

- **USB and Bluetooth keyboards** everywhere you type, picked up when plugged in or woken up
- **The Bluetooth window** (from OpenX6100, Kris SP2KMO) to pair keyboards on the radio itself; Bluetooth locked down, with the radio hidden until you Scan and CAT over Bluetooth only for paired devices
- **The decoders stay off one processor core**, so the screen always has a core of its own
- **Health lines in the app log**, so a bug report shows whether the radio kept up
- **Built on the latest OpenX6100 firmware** (R1CBU / gdyuldin v1.0.2 and later), with its FT8, RTTY and other apps alongside

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
