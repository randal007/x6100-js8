**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed: the X6100 firmware (OpenX6100, gdyuldin's) with a JS8 app next to FT8 and RTTY.

Beta 5.1 brings a **Bluetooth window** to pair keyboards on the radio itself, and an important fix: **messages to and from stations with a "/" and a number in their call** (like KG4UHM/6) were packed and read differently from desktop JS8Call. Please update if you work JS8 on the air.

> [!IMPORTANT]
> Writing the image replaces the whole SD card. **Copy your files off the DATA partition first** (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`, `js8_history.db`, `bluetooth.tar`) and copy them back after the first start.

## Highlights

### Messages to and from "/number" calls fixed

Stations like **KG4UHM/6** (a "/" followed by a number) are sent in a special kind of frame, and the JS8 engine this app is built on packed its command and SNR differently from desktop JS8Call. So since beta 1:

- **Our replies to such a station went out wrong:** an SNR report of −03 arrived as −31, and HW CPY?, INFO?, STATUS?, RR and the rest arrived as other commands.
- **Their messages were read wrong:** a desktop station's heartbeat acknowledgement ("VE7NHW HEARTBEAT SNR −17") showed up as "HW CPY?", "FB" or "RR", and the automatic reply could answer a question that was never asked.

Both directions now match desktop JS8Call bit for bit, checked against desktop's own code. Calls like W1ABC/P (a letter after the "/") were never affected. Found on the air with KG4UHM/6; the fix is offered to the JS8Call-improved developers too.

### The Bluetooth window

**APP, page 3 → Bluetooth** (Kris SP2KMO's, from OpenX6100): press **Scan**, press the keyboard's pairing key, select it and press **Connect**. If the keyboard wants a number typed, the window shows it. Hold **Forget** to remove a pairing. The radio can only be paired with for 3 minutes after Scan; the rest of the time it's hidden. Pairings are kept across updates when you switch off normally. The console command `x6100-bt-pair` is gone. WiFi must be on (one switch powers WiFi and Bluetooth).

[Using a Bluetooth keyboard](https://github.com/randal007/x6100-js8#using-a-bluetooth-keyboard) in the manual.

### Also new, from OpenX6100's latest firmware

- **FT8/FT4 shows each station's country** next to the distance
- **FT8/FT4 sends WSJT-X-style UDP** for logging programs on the network
- **Settings → Interface → Decoder text:** Large or Compact text in FT8 and the decoder panel
- **One country file** for FT8 and JS8's map (AD1C's Big CTY, September 2026); a newer `cty.dat` on the DATA partition is used by both

## Bug fixes since beta 5

- Messages to and from "/number" calls: command and SNR packed and read as desktop does (above)
- The log export wrote SSB contacts as AM (our fix, merged in OpenX6100)
- CAT on the 6 m band (from OpenX6100)

## Changed

- **JS8 is no longer a long-press action** (Settings → Interface → *GEN / APP / KEY … long press*): open it from the APP menu. If one of your keys was set to JS8, it does nothing now until you pick something else for it.

## Updating

1. Copy the DATA partition's files to your PC (see the box at the top).
2. Download `sdcard.js8-beta5.1.img.zip` below and write it to the card with balenaEtcher or Rufus.
3. Start the radio once with the new card, then copy your files back.
4. APP → page 3 → **JS8**.

**From beta 4 or earlier:** see the [beta 5 notes](https://github.com/randal007/x6100-js8/releases/tag/js8-beta5) first.

## Known issues

- **WeFax, NavTex, SSTV and the broadcast channel list** aren't included. They're the Murus team's and are coming through **OpenX6100**, so every app ends up in one firmware
- **Bluetooth headphones while JS8 is open:** while you listen on headphones the radio's speaker is turned down, so JS8's alert beeps may not be heard. Not tried yet
- **A slight waterfall flicker** while it scrolls; *Waterfall: Light* (the default) or *Calm* in Settings make it much smaller. It doesn't affect decoding
- **Without a GPS** the radio's clock drifts a few seconds a week. Time: Auto follows it; if nothing decodes, hold **Time**
- **Not tried on the air yet:** high-SWR protection, APRS More services, POTA/SOTA spots, Ultra on a busy band, and the GPS grid outside
- If the power goes off while JS8 is open, the USB-D receive filter stays at 200–3000 Hz; leaving JS8 first (ESC) puts yours back

## Questions and bug reports

Bugs: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions). Everything is explained in the [manual](https://github.com/randal007/x6100-js8#readme).

73 de VE7NHW
