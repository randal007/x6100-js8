# Feature ideas, 2026-09-28

A brainstorm of features for the X6100 JS8 app: APRS services, camping
and off-grid use, messaging, desktop JS8Call parity, nets, fun. **Nothing
here is built or promised**; it's a list to pick from. Written by Claude
Code after checking which APRS services answered on 2026-09-28 (findu.com
message logs), reading desktop JS8Call-improved's inbound APRS relay code
(`JS8_Main/AprsInboundRelay.cpp`, `APRSISClient.cpp` at `d9c5051`), and
looking at what the radio firmware already offers.

## Top 5

1. **APRS Services menu + Contacts book**: builds on the SMS work
2. **Check-in button + smart GPS tracking**
3. **Saved messages with macros**: typing with the knob is slow
4. **Battery and SWR guards** for unattended automatic TX
5. **Relay-station badge + ECHO test**: know whether APRS works before
   relying on it

## What the research showed

- **Every APRS answer comes back as its own JS8 transmission.** A desktop
  JS8Call-improved (3.0.1+) station with inbound relay on passes on **any**
  APRS message to a call it heard recently (within its callsign aging), at
  high priority, as `@APRSIS MSG TO:CALL text DE SENDER`. It logs in to
  APRS-IS with `filter t/m` (all messages).
- **The relay station ACKs the APRS sender for us**, whether or not our
  radio decodes the relay. So services and SMS gateways treat it as
  delivered even if we missed it.
- **Several relay stations = duplicate answers.** Each one that heard us
  relays the same message (their de-duplication is per station), and the
  `{ID}` is stripped. The Inbox should drop repeats (same sender and text
  within a few minutes).
- **Some services send many messages.** Seen on 2026-09-28: REPEAT sent one
  user 7 messages in 3 s, WXNOW sent 10. Over JS8 that's minutes of
  airtime each, so prefill the one-line forms (`N 1`, `brief`, `top1`).
- **Most services use your last APRS position.** MPAD and WXBOT also
  take a grid in the message (`grid CN89xx`), so the app can fill it in and
  no beacon is needed first.
- **WXBOT is US National Weather Service only.** MPAD is worldwide (use it
  in Canada).
- **ANSRVR groups:** `CQ GROUP text` also joins you for 12 hours, and every
  group message is then relayed to you over JS8. Send `U GROUP` right
  after.
- **The radio already has, unused by JS8:**
  - supply voltage, battery voltage, % and charging (`radio.c`:
    `pack->vext`, `vbat`, `batcap`)
  - SWR, ALC and power during TX (`vswr`, `alc_level`, `tx_power`)
  - GPS through gpsd (`gps_last_fix()`, used only by the APRS beacon)
  - RHVoice text-to-speech (`voice.h`: `voice_say_text()`), the voice mode
    for blind operators

## APRS services that answered on 2026-09-28

| Service | What | Example message |
|---|---|---|
| MPAD | worldwide weather | `grid CN89 today` |
| | sunrise and sunset | `riseset` |
| | nearest repeater | `repeater 2m` |
| | next ISS pass | `satpass iss` |
| | nearest hospital, fuel, water ... | `osm hospital` |
| | email your position | `posmsg you@example.com` |
| | pager message (DAPNET) | `dapnet CALL text` |
| | magic 8-ball | `magic8ball` |
| WXBOT | US forecast | `grid CN89 brief` |
| WXNOW | nearest weather station | `N 1` |
| REPEAT | nearest repeaters | `N 1 2m` |
| WHO-IS | callsign lookup | `VE7ABC` |
| ECHO | echoes your text back: proves both directions work | `test` |
| WTSAPP | WhatsApp | `@number text` |
| EMAIL-2 / SMSGTE | email / SMS | (in the app already) |
| WLNK-1 | Winlink: list, read, send | `L`, `R1`; needs a login challenge answered each time |
| NTSGTE | radiograms into NTS | fields separated by `\`; `help` |
| APRS2SOTA / APSPOT | SOTA / POTA spots | (in the app already) |
| MAIL | leave a message for any APRS user | `@CALL text` |
| OTA | a live POTA-style activation game over APRS | `CQ`, `Chase CALL` |
| JOKE | jokes | `joke` |
| ANSRVR | groups, #APRSThursday (`HOTG`) | `CQ HOTG text`, then `U HOTG` |

The public registry (aprs.hemna.com) marked SMS, EMAIL-2 and APSPOT as
down, yet they had traffic that day; don't trust its health checks.

## APRS

- **Services menu:** pick a service, the one-line message comes prefilled
  (with your grid), the answer lands in the Inbox as from MPAD, WXBOT ...
  Duplicates from several relay stations dropped.
- **Test my APRS path:** send to ECHO; an echo back means sending and
  receiving both work right now.
- **Relay-station badge in Stations:** mark stations seen passing on
  `@APRSIS MSG TO:` relays, so you know whether answers can reach you on
  this band.
- **Contacts book:** names for SMS numbers, emails and calls ("SMS Home",
  "Email Mom") instead of typing digits with the knob. Kept on the SD card.
- **APRS status line and map objects** through `CMD`: a status such as
  `QRV 7.078 JS8`, or an object marking a camp, trailhead or water source
  on aprs.fi.
- **#APRSThursday button:** sends the check-in to ANSRVR `HOTG`, then
  leaves the group.
- **Delivery receipts:** show a gateway's `ACKnn` as "delivered" on the
  sent message (already on the to-do list).

## Camping and off-grid

- **inReach-style check-in:** one press sends "OK" + position to preset
  contacts (SMS, email, APRS, a JS8 group). Optional daily check-in at a
  set time, off by default.
- **Smart tracking:** a GPS beacon only after moving more than X km, plus
  a GPX track saved to the SD card.
- **Battery and SWR guards:** heartbeats, beacons and auto replies stop on
  low battery or high SWR (a camp wire that shifted, a wet antenna).
- **Supply voltage in STATUS and heartbeat replies** (like desktop's
  `<MYSTATUS>` macro).
- **Worked out on the radio, no internet:** sunrise, sunset and grey line
  for your grid; a band summary (how many stations each band heard in the
  last hour); an optional band scan that hops the JS8 frequencies to find
  activity.
- **GPS Time Sync** (already on the ideas list).

## Messaging

- **Saved messages with macros** (`<MYGRID4>`, `<SNR>`, `<CALL>`,
  `<MYSTATUS>`), as desktop has.
- **Callsign and word suggestions while typing** (the Android port just
  added callsign suggestions; desktop has JSC word suggestions).
- **Read messages aloud** with the radio's voice when voice mode is on:
  hands-free, and JS8 for blind operators.
- **Delivery ticks on sent messages:** queued → sent → ACKed / relayed
  by X.
- **AGN? for a cut-off message:** offer a resend request when a message
  arrived without its end mark (desktop JS8Call-improved issue #46 asks for
  this).
- **Conversation view for each station.**

## Desktop parity

- **JS8 60** (desktop 3.0.1): no heartbeats, not for MSG, CPU-heavy even on
  desktop; profile on the radio first.
- **Automatic time drift.**
- **Idle timeout:** automatic heartbeats off after N hours untouched.
- **Message count in QUERY MSGS and heartbeat replies** (desktop #191).
- **Frequency schedule** (e.g. a net frequency at 0200Z), with auto-reply
  settings per entry (desktop #226).

## Nets and emergencies

- **Net control mode:** roll call for an `@GROUP` net, check-in list,
  logged or emailed.
- **Radiogram form:** through NTSGTE, or to `@NTS` over JS8.

## Fun

- **Personal records:** best DX today, grids and countries worked (country
  from the callsign prefix with cty.dat, no internet).
- **Who hears me:** a list from the SNRs other stations reported for us.
- **OTA, JOKE, magic8ball** in the Services menu.

## With Wi-Fi at home

- **Be an APRS gateway and inbound relay** yourself, as NR4U is: these are
  rare, and every JS8 user nearby gains from one.
- **PSKReporter spots.**

## Sources

- APRS service registry: https://aprs.hemna.com/
- findu.com message logs: `http://www.findu.com/cgi-bin/msg.cgi?call=<SERVICE>`
- MPAD commands: https://github.com/joergschultzelutter/mpad/blob/master/docs/COMMANDS/ACTION_KEYWORDS.md
  and examples: https://github.com/joergschultzelutter/mpad/blob/master/docs/EXAMPLES.md
- APRS information services: https://how.aprs.works/aprs-information-services/
- WxBot: https://sites.google.com/site/ki6wjp/wxbot
- ANSRVR and #APRSThursday: https://cn86radio.substack.com/p/aprs-thursday
- JS8Call user guide: https://js8call.com/JS8Call-improved/d6/d14/md_docs_2user__guide_2JS8Call__User__Guide.html
- JS8Call-improved releases and issues #46, #191, #226: https://github.com/JS8Call-improved/JS8Call-improved
- Android port 1.0 release notes: https://github.com/JS8Call-improved/Android-port/releases
