**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is R1CBU's X6100 firmware (gdyuldin v1.0.2) with a JS8 app added next to FT8 and RTTY.

**Beta 5 gathers everything since beta 4.** New in this release: the **Ultra** speed, a **USB GPS** for the time and your grid, **Bluetooth keyboards**, *Anyone have messages?*, desktop's default STATUS, and fixes from a full code review. If you're coming from beta 4, you also get everything from betas 4.1 to 4.7: the latest R1CBU firmware underneath, the **station History**, **automatic time sync**, **saved messages**, high-SWR protection, a calmer waterfall and a transmit level that holds steady for an amplifier.

![WB8PLB's History page on the radio](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta5/docs/screenshots/16_history.png)

### Install

1. Download `sdcard.js8-beta5.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

**Updating from beta 4.1 to 4.7:** writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`) to your PC first, and copy them back after the first start.

**Updating from beta 4 or earlier:** the same, but R1CBU 1.0 converts the settings in `params.db` at its first start (power, TX gain and band offsets are stored differently), so **keep your copy**: to go back to beta 4, write its image and put those files back.

### New since beta 4.7

- **Ultra speed** (desktop JS8Call-improved's "JS8 60": 4-second slots, 250 Hz wide, `U` in the lists). Always decoded, as desktop does, on its own decoder so it never holds up the other speeds (about a tenth of one processor core on the radio). **Settings → Ultra on Speed button** puts it on the Speed button after Turbo; **hold Speed** on an Ultra station matches it either way
- **The decoders stay off one processor core**, so the screen and the waterfall always have a core of their own
- **A USB GPS** in the HOST port (a u-blox type that shows up as `/dev/ttyACM0`): the radio sets its clock from it, to about a millisecond; JS8 uses the GPS grid while there's a fix (heartbeats, CQ, distances, the map, APRS spots) and goes back to your saved QTH without one; `GPS` / `GPS no fix` in the top line; the time is saved to the radio's battery clock once, so it stays right after you unplug the GPS; and JS8's timing stays steady when the clock is set while it's open. Plugging the GPS in with the radio already on works now (before, it had to be in at power-on)
- **Bluetooth keyboards** type everywhere a USB one does: JS8, Callsign, QTH and the other text boxes, and they reconnect by themselves after sleep. Pair once with one command over the USB console (`x6100-bt-pair`, then press the keyboard's pairing key); the pairing is kept across updates. WiFi must be on (one switch powers WiFi and Bluetooth). A USB keyboard plugged in still comes first
- **Anyone have messages?** in the Query list sends `@ALLCALL QUERY MSGS`, with or without a station selected
- **STATUS starts as desktop's default**, `IDLE <MYIDLE> VERSION <MYVERSION>` (set once; a STATUS you clear stays empty)
- **Noise reduction, the noise blanker and the notches stay off** while JS8 is open, even if one is turned on (over CAT, for example); your settings come back when you leave

### Fixes since beta 4.7

- **A message with a very long number after `SNR`** (or in `NEXT MSG ID`) could close the app; anyone could send one as free text
- **Fetching a held message works with brackets:** `QUERY MSG [3]` or `[ID 3]`, as desktop's template leaves them, are answered like `QUERY MSG 3`
- **Lowercase in the Callsign box** is typed as capitals instead of being dropped
- **The APRS Echo test is gone:** relay stations don't bring ECHO's answer back over JS8. The `@` badge for two-way gateways still comes from any relayed APRS answer
- Smaller fixes from a full code review of the app (the battery clock is tried again if its write couldn't start, among others)

### Also new since beta 4 (betas 4.1 to 4.7)

- **The latest R1CBU firmware underneath** (gdyuldin v1.0.2): the Simple, Black and Flat themes, and JS8's waterfall, list and map laid out with more room
- **Station History:** every station you exchange messages with, kept for good, per band: their latest INFO and STATUS and the text of every QSO. Page 3's **Show History** lists them, on the map too; a short MFK press on a station opens its History page. **Settings → Clear station history** forgets it all
- **Time: Auto** (page 4): JS8's timing follows every station decoded, as desktop's *Automatic Time Drift*; **hold Time** to search when the clock is too far off to decode anything
- **Saved messages** (Query → Saved messages): ten of your own, one press to send, with desktop's macros (`<CALL>`, `<SNR>`, `<MYGRID4>` …), also in typed messages and your INFO/STATUS
- **Heartbeat works like CQ** (page 1): press for one, hold for auto heartbeats (5–30 minutes with the main knob), with a countdown. Heartbeats keep going while you call CQ and pause for 10 minutes when someone calls you or you send something yourself
- **The red band shows where a heartbeat goes** (a free spot at 500–999 Hz); **the green band is as wide as the selected station's speed**
- **The Stations view:** **Sort** by Heard you, SNR, Time, Distance or QSO; **QRZ** for who called you while you weren't looking; **`@`** for a station that passes APRS messages back over JS8
- **The map:** a count where several stations share a square
- **High-SWR protection:** over 3:1 while sending switches AUTO, heartbeats, HB ACK and auto CQ off, with three beeps
- **APRS More services:** weather, sunrise, repeaters, ISS passes, nearest hospital/fuel/water, email your position, callsign lookup and more
- **Can anyone reach…?** (`@ALLCALL QUERY CALL`) in the Query list
- **A steady transmit level for an amplifier:** while the ALC reads zero the drive rises smoothly within the transmission and holds as soon as the ALC shows, so the power is right within the first transmission or two, with no see-saw at low power
- **The waterfall** is drawn straight onto the display's lower layer (much less work for the radio), with **Sharp / Light / Medium / Calm** averaging in Settings
- **A custom frequency says "JS8 Custom"**; *Decode* and *Frequencies* are Settings lines now, **Hold** is on page 6
- **Health lines in the app log** (decoder load, missing audio, screen stalls, time changes) for bug reports
- **Fixes:** QUERY CALL adds the `?` desktop stations need to answer; a fast turn of the main knob moves 5 or 10 Hz a click without lagging the waterfall; a USB keyboard plugged in with the radio on is picked up; Winlink messages get one ACK, not two; distances of 10,000 km or more no longer wrap

### Known issues

- **WeFax, NavTex and the broadcast channel list are not included:** they came from the Murus team's (1KO125) fork, which is still on the older firmware
- **A slight flicker in the waterfall while it scrolls,** much reduced by *Waterfall: Light* (the default) or *Calm*; it doesn't affect decoding
- **Without a GPS, the radio's clock** gains a few seconds a week; Time: Auto follows it, but at 2.5 s or more off nothing decodes: hold **Time** to search, or set the clock in the radio's Settings
- **Not tried on the air yet:** the high-SWR protection, APRS More services, POTA/SOTA spots, position messages and email through the gateways, Ultra on a busy band, and the GPS grid and battery clock outside (the GPS clock itself works)
- **Bluetooth keyboards:** tested with one LE keyboard; pairing is over the console until R1CBU's next firmware brings a pairing screen. The pairing is saved when the radio is switched off normally: pulling the power skips that
- **Switching Show** (All / No HB / Directed) was once seen leaving blank rows; not reproduced since. Screenshots very welcome if you see it
- If the radio loses power (or you switch it off by holding POWER) with JS8 open, the USB-D receive filter stays at 200–3000 Hz; leaving the app first (ESC) puts yours back

### Coming next

- WeFax, NavTex and SSTV from the Murus team, once their fork is on R1CBU 1.0
- A Bluetooth pairing screen, with R1CBU's next firmware
- A Setting for the map's home colour

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use everything, including [Using a GPS](https://github.com/randal007/x6100-js8#using-a-gps) and [Using a Bluetooth keyboard](https://github.com/randal007/x6100-js8#using-a-bluetooth-keyboard).

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
