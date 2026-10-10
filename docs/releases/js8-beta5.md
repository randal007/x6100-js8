**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed: R1CBU's X6100 firmware (gdyuldin v1.0.2) with a JS8 app next to FT8 and RTTY.

Beta 5 adds the **Ultra** speed, a **USB GPS** for the time and your grid, and **Bluetooth keyboards**, plus fixes from a full review of the app's code.

> [!IMPORTANT]
> Writing the image replaces the whole SD card. **Copy your files off the DATA partition first** (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`) and copy them back after the first start. Coming from **beta 4 or earlier**? See *Updating* below.

## Highlights

### Ultra speed

Desktop JS8Call-improved's fastest speed ("JS8 60": 4-second slots, 250 Hz wide) is always decoded, as desktop does, and shows as `U` in the lists. It runs on its own decoder, so it never holds up the other speeds (about a tenth of one processor core). To send at Ultra, turn on **Settings → Ultra on Speed button**, or **hold Speed** on an Ultra station to match it.

<!-- screenshot: an Ultra (U) row in the list -->

### USB GPS

Plug a USB GPS into the HOST port, before or after switching on:

- **The clock** is set from the GPS, to about a millisecond, and saved to the radio's battery clock, so it stays right after you unplug the GPS
- **Your grid** comes from the GPS while it has a fix (heartbeats, CQ, distances, the map, APRS spots); without one it goes back to your saved QTH
- **`GPS`** in the top line while there's a fix

<!-- screenshot: GPS in the top line -->

[Using a GPS](https://github.com/randal007/x6100-js8#using-a-gps) in the manual.

### Bluetooth keyboards

A Bluetooth keyboard now types everywhere a USB one does: JS8, Callsign, QTH and the rest. Pair it once with `x6100-bt-pair` over the radio's USB console and it reconnects by itself, even after sleep or an update. WiFi must be on (one switch powers WiFi and Bluetooth).

[Using a Bluetooth keyboard](https://github.com/randal007/x6100-js8#using-a-bluetooth-keyboard) in the manual.

### Also new

- **Anyone have messages?** in the Query list asks everyone (`@ALLCALL QUERY MSGS`)
- **STATUS starts as desktop's default:** `IDLE <MYIDLE> VERSION <MYVERSION>`
- **Noise reduction, the noise blanker and the notches stay off** while JS8 is open, even if one is turned on over CAT
- **The decoders leave one processor core free**, so the screen and waterfall always keep up

## Fixes

- A received message with a very long number after `SNR` could close the app
- `QUERY MSG [3]` (desktop's template leaves the brackets in) is now answered like `QUERY MSG 3`
- Lowercase typed in the Callsign box comes out as capitals instead of being dropped
- The APRS Echo test is gone: relay stations don't bring its answer back over JS8
- Smaller fixes from the code review

<details>
<summary><b>Coming from beta 4? Everything from betas 4.1 to 4.7 is in here too</b></summary>

**Underneath**
- The latest R1CBU firmware (gdyuldin v1.0.2): Simple, Black and Flat themes, and more room for the waterfall, list and map

**Operating**
- **Station History:** everyone you exchange messages with, kept for good, per band: their INFO, STATUS and every QSO's text. Page 3 → **Show History**; a short MFK press on a station opens its page
- **Time: Auto** follows every station decoded, as desktop's *Automatic Time Drift*; **hold Time** to search when the clock is far off
- **Saved messages** (Query → Saved messages): ten of your own with desktop's macros (`<CALL>`, `<SNR>`, `<MYGRID4>` …)
- **Heartbeat works like CQ:** press for one, hold for auto (5–30 min); it pauses for 10 minutes when someone calls you or you send something
- **Can anyone reach…?** (`@ALLCALL QUERY CALL`) in the Query list

**Screen and map**
- The red band shows where a heartbeat will go; the green band is as wide as the selected station's speed
- Stations view: **Sort** (Heard you, SNR, Time, Distance, QSO), **QRZ** for who called you, **`@`** for two-way APRS gateways
- A count on the map where several stations share a square
- The waterfall is drawn straight onto the display's lower layer, with **Sharp / Light / Medium / Calm** smoothing in Settings
- A custom frequency shows as "JS8 Custom"

**Transmitting**
- A steady transmit level for an amplifier: right within the first transmission or two, no see-saw at low power
- High-SWR protection: over 3:1 turns automatic sending off, with three beeps

**APRS**
- More services: weather, sunrise, repeaters, ISS passes, nearest hospital/fuel/water, email your position, callsign lookup

**Fixes**
- QUERY CALL adds the `?` desktop stations need; a fast turn of the main knob moves 5 or 10 Hz a click; a USB keyboard plugged in with the radio on works; Winlink gets one ACK, not two; distances over 10,000 km no longer wrap

</details>

## Updating

1. Copy the DATA partition's files to your PC (see the box at the top).
2. Download `sdcard.js8-beta5.img.zip` below and write it to the card with balenaEtcher or Rufus.
3. Start the radio once with the new card, then copy your files back.
4. APP → page 3 → **JS8**.

**From beta 4 or earlier:** the newer R1CBU firmware converts `params.db` at its first start (power, TX gain and band offsets are stored differently). Keep your copy: to go back to beta 4, write its image and put those files back.

## Known issues

- **WeFax, NavTex and the broadcast channel list** aren't included yet: they come from the Murus team's fork, which is still on the older firmware
- **A slight waterfall flicker** while it scrolls; *Waterfall: Light* (the default) or *Calm* in Settings make it much smaller. It doesn't affect decoding
- **Without a GPS** the radio's clock drifts a few seconds a week. Time: Auto follows it; if nothing decodes, hold **Time**
- **Bluetooth pairing** is done over the console until R1CBU's next firmware adds a pairing screen. It's saved for the next update when you switch the radio off normally (pulling the power skips that); keeping it across an update hasn't been tried yet
- **Not tried on the air yet:** high-SWR protection, APRS More services, POTA/SOTA spots, Ultra on a busy band, and the GPS grid outside
- If the power goes off while JS8 is open, the USB-D receive filter stays at 200–3000 Hz; leaving JS8 first (ESC) puts yours back

## Questions and bug reports

Bugs: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions). Everything is explained in the [manual](https://github.com/randal007/x6100-js8#readme).

73 de VE7NHW
