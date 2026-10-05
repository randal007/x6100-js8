**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is R1CBU's X6100 firmware (gdyuldin v1.0.2) with a JS8 app added next to FT8 and RTTY.

Beta 4.5 brings a **station History**, **automatic time sync**, **saved messages with macros**, a smarter Stations view and map, **high-SWR protection**, more **APRS** services, a lighter and calmer **waterfall**, and a USB keyboard that works when you plug it in with the radio on.

![WB8PLB's History page on the radio](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta4.5/docs/screenshots/16_history.png)

### Install

1. Download `sdcard.js8-beta4.5.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

**Updating from beta 4.1:** writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`) to your PC first, and copy them back after the first start; beta 4.5 reads them as they are. The History is a new file (`js8_history.db`): copy it off too when you update next time. **Coming from beta 4 or earlier:** see the [beta 4.1 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta4.1) (R1CBU 1.0 converts `params.db` at its first start: keep your copy).

### New in beta 4.5

- **Station History:** every station you exchange messages with (heartbeat ACKs included) is kept for good, per band: their latest **INFO** and **STATUS** with how old they are, and the **text of every QSO**, each message with its time. Page 3's **Show History** lists them all on this band, and on the map; a short **MFK press** on any station (Stations view or map) opens its History page, and a QSO opens to its messages. Text without callsigns in a long QSO joins it too. **Settings → Clear station history** forgets it all (two presses)
- **Sort** in the Stations view: Heard you, SNR, Time, Distance, or **QSO** (only the stations you've had a QSO with; the map follows)
- **`@` in the Stations view** for a station that passes APRS messages back over JS8 (the one that brings your Echo test's answer: a two-way gateway)
- **QRZ** in the Stations view, as on the map: who called you while you weren't reading the messages
- **A count on the map** where several stations share one square
- **Time: Auto** (page 4): JS8's timing follows every station decoded, heartbeats included, as desktop JS8Call's *Automatic Time Drift*; **hold Time** to search when the clock is too far off for anything to decode
- **Saved messages** (Query → Saved messages): ten of your own, one press to send, with desktop JS8Call's macros (`<CALL>`, `<SNR>`, `<MYGRID4>` …), also in typed messages and your INFO/STATUS
- **High-SWR protection:** over 3:1 while sending switches AUTO, heartbeats, HB ACK and auto CQ off, with three beeps
- **APRS:** an **Echo test** (an answer in your Inbox proves both directions work) and a **More services** list: weather, sunrise, repeaters, ISS passes, nearest hospital/fuel/water, email your position, callsign lookup and more
- **The waterfall:** drawn straight onto the display's lower layer (the app's screen work fell from about 79 % to 29 % of one core), and **Sharp / Light / Medium / Calm** averaging in Settings (*Light*, the default, halves the slight flicker as it scrolls)
- **Buttons moved:** *Decode* and *Frequencies* (JS8Call's / GhostNet / custom kHz) are Settings lines now; **Hold** is on page 6; page 3 has **Show History**
- **Health lines in the app log** (`app_logs`): how busy the decoder was, and any missing audio, screen stalls or time changes, for bug reports

### Fixes

- **A USB keyboard plugged in while the radio is on** wasn't picked up (since beta 4.1's move to R1CBU 1.0); now it is, and unplugging and plugging it back works (the radio's shared keyboard code, so everywhere)
- The **Heartbeat button** on the first JS8 open after switching off with auto heartbeats on showed *HB auto: soon* until the page changed
- A faint **waterfall stutter** while the MFK steps through the list
- **Winlink** messages no longer get a second ACK
- Distances of 10,000 km or more no longer wrap in the Stations view

### Known issues

- **WeFax, NavTex and the broadcast channel list are not included** (the Murus team's fork is still on the older firmware)
- A slight waterfall flicker while it scrolls, much reduced by *Waterfall: Light/Medium/Calm*; it doesn't affect decoding
- **Not tried on the air yet:** the high-SWR protection, the APRS Echo test and More services, POTA/SOTA spots, position messages and email through the gateways, a **Bluetooth keyboard**
- The History starts empty with this release (earlier QSOs aren't brought in)
- A power loss with JS8 open leaves the USB-D receive filter at 200–3000 Hz
- The `<MYVERSION>` macro says `X6100 JS8 beta 5` in this release (it was built before the name was settled); it only goes out if you use it in a message, INFO or STATUS

### Coming next

- **GPS** time and location from a USB GPS
- **A Bluetooth keyboard** tried with JS8

### On the air

A 40 m QSO with WB8PLB, kept in the History (12 messages, logged), and opened to read:

![WB8PLB's History page](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta4.5/docs/screenshots/17_history_qso.png)

The map on 40 m: blue paths to the stations that heard VE7NHW, white count tags where several share a square:

![The map with count tags](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta4.5/docs/screenshots/18_map_counts.png)

The World view on 20 m with DX heard (HB9BV in Switzerland selected) and the band's other QSOs as grey lines:

![The World view](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta4.5/docs/screenshots/19_map_world.png)

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use everything.

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
