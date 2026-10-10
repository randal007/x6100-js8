**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is the R1CBU/1KO125 X6100 firmware with a JS8 app added next to FT8, RTTY, WeFax and NavTex.

Beta 3 adds relays and store and forward that work the way desktop JS8Call's do, Time Sync as a drift, alert beeps you can hear, a smoother waterfall and a Settings list.

### Install

1. Download `sdcard.js8-beta3.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

Updating from beta 2? Writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`) to your PC first, and copy them back after the first start.

### New in beta 3

New:
- **Relays**, as desktop JS8Call does them: your radio passes relays on for others (`N0XYZ>W1ABC HELLO`), ACKs back along the path, and answers the questions and messages they carry the same way. **Relay via them…** and **Can they reach…?** (`QUERY CALL`) on the Query list
- **Store and forward**, checked line by line against desktop JS8Call: `YES MSG ID 3 +2`, `NEXT MSG ID`, group messages (`MSG TO:@GROUP`), `RETRIEVE MSG` notices, messages from APRS gateways, and desktop's Reply choices in the Inbox. **Fetch message #…** on the Query list
- **Settings…** (page 4, was Texts…): INFO, STATUS, Relay on/off and your groups
- **Time Sync** shifts only JS8's own timing (a drift, like desktop JS8Call) instead of the radio's clock; the drift is shown on top, hold to reset
- **Alert beeps through the speaker**
- **Auto CQ interval** set with the knob (1–30 min)
- **Shorter position beacons** with a message (4–5 frames instead of 6), with a live frame count
- **TX bar** shows the frame count first ("1/5 starts in 13 s", "sending 2/5")
- **Inbox** button turns green while you have unread messages
- **Stations lists** kept when JS8 is closed and reopened, until the radio is switched off
- CQ page first (pages 1 and 2 swapped); a hold is half a second

Fixes:
- Always **USB-D**: a custom frequency on another band (e.g. CB) brought up that band's last mode, often USB, with the mode keys locked
- **Smoother waterfall** for less work
- Noise reduction, noise blanker and notch filters off while JS8 is open (the radio applies them in USB-D too); yours come back when you leave
- TX filter 160–3000 Hz while JS8 is open, the radio's own default
- A heartbeat sent by hand restarts the HB timer
- Show in the Stations view goes back to the messages
- Spot form: one Frequency row
- Held messages: delivered only once sent in full; asked again, sent again at once

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use it.

### Known issues

- Relays and store and forward are new and not yet tried on the air with desktop stations: reports welcome
- APRS: the grid spot and SMS are confirmed; POTA/SOTA spots, position messages, email and Winlink are untested through the gateways
- Long messages not yet watched all the way to the end
- The main screen's waterfall behind JS8's buttons stands still (cosmetic)
- A power loss with JS8 open leaves the USB-D receive filter at 200–3000 Hz

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
