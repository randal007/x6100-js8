**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is the R1CBU/1KO125 X6100 firmware with a JS8 app added next to FT8, RTTY, WeFax and NavTex.

Beta 4 adds a map of the stations heard in GridTracker's style, decode marks on the waterfall, two-way SMS, messages sent exactly as desktop JS8Call sends them, automatic replies that follow desktop's rules, and the fixes from a full code review.

![The map](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta4/docs/screenshots/12_map.png)

### Install

1. Download `sdcard.js8-beta4.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

Updating from beta 3? Writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`) to your PC first, and copy them back after the first start. Beta 4 reads them as they are.

### New in beta 4

New:
- **Show Map** (Show Stations twice): the stations of this frequency on a world map in GridTracker's style. Grid squares (or the callsign's province, state or country without a grid), curved paths to who heard you, red while a message to or from you is on the air, new stations popping up with **NEW DXCC** / **NEW GRID** from your QSO log, CQ tags, **QRZ** (who called you while you were away), other stations' QSOs in grey, a dot running along the path as your message goes out, squares fading as stations go quiet. Your continent close-in, the world when DX is heard, or **Follow** the selected station
- **Decode marks** on the waterfall (Settings), as desktop's *Show decode attempts*: where the decoder is trying, and in yellow what it decoded
- **Two-way SMS:** replies relayed back over JS8 reach the Inbox (confirmed on the air); **Reply by SMS to @number** fills in the number; the gateway's receipt shows as "SMS {04} to @number delivered"; SMS, email and Winlink get their full 67 characters
- **Hold the MFK** on a station to lock it (`locked: CALL` in the TX bar)
- **Colours and marks as desktop's:** commands in yellow, the end mark `♢` on complete messages, your own messages red and messages to you blue; your own messages show ` ...` while going out, `♢` once sent, `(stopped 2/5)` if stopped
- Rows without a callsign marked by the green bar on the selected station's frequency
- **Stations view:** bearing, km or miles, stations heard through a relay listed *via* it
- **Settings:** km or miles, how long stations and messages stay listed, decode marks, an operator callsign (logged as `OPERATOR`), and Time Sync now / Reset time drift (moved from page 3, whose button is now the map's view button)
- **Heartbeats pause** for 10 minutes after anything you send yourself, then carry on by themselves (press HB to resume sooner)
- The TX bar is half see-through

Fixes:
- **Messages go out exactly as desktop JS8Call sends them**, checked frame by frame at all four speeds: desktop read "RR 73" as "RR 31", a message starting with a number lost it, relays to a call starting with a digit (2E0ABC) named the wrong station
- **Closing JS8 while it transmits is safe:** GEN, APP or another app mid-frame crashed the app with the radio still keyed
- Band keys wait until you stop sending; automatic answers still waiting are dropped after a band or frequency change
- **Automatic replies as desktop sends them:** answers queue instead of replacing each other, nothing automatic keys over a message to you still arriving, a popup no longer holds up an unattended station, HB ACKs at most every 55 minutes per station, heartbeats on desktop's schedule
- **Messages and settings safer on the SD card:** a full Inbox no longer loses new messages, files survive a power cut, an unreadable file is kept aside, a message that couldn't be saved isn't ACKed
- **Smoother screen, lighter work:** far less redrawing when idle, no decoded messages lost when the screen falls behind, a regular alerts as new once per band
- **Screen and keyboard:** VOL knob in every list, every JS8 character on the keyboard, a too-long message says so as you type, the Inbox lists all 200 and opens on the newest unread, info rows no longer vanish
- **Logging:** Log QSO takes the selected station; a QSO ends on 73, SK or RR73 among the last three words; ESC on the prompt means *not now*
- As desktop: a message to VE7NHW/P isn't to VE7NHW, a message to a CQ7 call isn't a CQ, grids only from heartbeats, CQs and GRID replies, alert words match next to punctuation
- Relayed messages keep their whole path for Reply; a RETRIEVE MSG notice that couldn't go out is tried again

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use it.

### Known issues

- Relays and store and forward follow desktop JS8Call's code but haven't been tried on the air with desktop stations yet: reports welcome
- APRS: the grid spot and two-way SMS are confirmed; POTA/SOTA spots, position messages, email and Winlink are untested through the gateways
- The map is new: not yet tried on a busy band; your own square is orange for now (a colour Setting is to come)
- Long messages not yet watched all the way to the end
- The main screen's waterfall behind JS8's buttons stands still (cosmetic)
- A power loss with JS8 open leaves the USB-D receive filter at 200–3000 Hz

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
