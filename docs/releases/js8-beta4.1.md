**JS8 on the Xiegu X6100, running on the radio itself.** No PC, phone or tablet needed. This is R1CBU's X6100 firmware (gdyuldin v1.0.2) with a JS8 app added next to FT8 and RTTY.

Beta 4.1 moves JS8 onto the latest R1CBU firmware, and brings a Heartbeat button that works like CQ, a red band that shows where a heartbeat goes, a green band as wide as the station's speed, and QUERY CALL fixed.

![JS8 on R1CBU 1.0](https://raw.githubusercontent.com/randal007/x6100-js8/js8-beta4.1/docs/screenshots/12_map.png)

### Install

1. Download `sdcard.js8-beta4.1.img.zip` below.
2. Write it to a microSD card with balenaEtcher or Rufus.
3. Put the card in the radio and switch on. Then APP → page 3 → **JS8**.

**Updating from beta 4:** writing the image replaces the whole card. Copy the DATA partition's files (`params.db`, `qso_log.db`, `*.adi`, `js8_*.txt`) to your PC first, and copy them back after the first start. R1CBU 1.0 converts the settings in `params.db` at its first start (power, TX gain and band offsets are stored differently), so **keep that copy**: to go back to beta 4, write its image and put those files back.

### New in beta 4.1

New:
- **The latest R1CBU firmware underneath** (gdyuldin v1.0.2): a cleaner screen, the Simple, Black and Flat themes, and JS8's waterfall, list and map laid out for it with more room
- **Heartbeat works like CQ** (page 1): press for one, **hold for auto heartbeats** (one now, the main knob sets 5–30 minutes); press again to stop. The button counts down to the next one. Page 4's old HB button is gone
- **Heartbeats keep going while you call CQ** (by hand or auto) and pause when someone calls you or you send something yourself; they carry on 10 minutes later
- **The red band shows where a heartbeat goes:** heartbeats and HB ACKs pick a free spot at 500–999 Hz; the band moves there while one waits and goes out, then back
- **The green band is as wide as the selected station's speed** (Slow 25, Normal 50, Fast 80, Turbo 160 Hz)
- **Can anyone reach…?** in the Query list (`@ALLCALL QUERY CALL`), also with no station selected

Fixes:
- **Can they reach…? (QUERY CALL)** adds the `?` for you, as desktop JS8Call sends it (without it, desktop stations didn't answer)
- The main knob: a fast turn moves 5 or 10 Hz a click, and turning it doesn't make the waterfall lag

See the [manual](https://github.com/randal007/x6100-js8#readme) for how to use it.

### Known issues

- **WeFax, NavTex and the broadcast channel list are not in beta 4.1:** they came from the Murus team's (1KO125) fork, still on the older firmware; they come back once their fork is on R1CBU 1.0
- A very faint stutter in JS8's waterfall now and then (R1CBU 1.0 shows frames at the panel's refresh); being measured, doesn't affect decoding
- Keep the radio's clock right (it gains a few seconds a week; at 2–3 s off, decodes go missing): radio Settings → General, or JS8's Time Sync
- Winlink messages get two ACKs; a later beta drops the extra one. POTA/SOTA spots, position messages and email are untested through the gateways
- A power loss with JS8 open leaves the USB-D receive filter at 200–3000 Hz

### Reporting

Bugs and problems: please open an [issue](https://github.com/randal007/x6100-js8/issues) with the release, band and speed, and what happened. Feature requests and ideas: [Discussions](https://github.com/randal007/x6100-js8/discussions).

73 de VE7NHW
