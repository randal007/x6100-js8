# POTA and SOTA spots over JS8 → APRS (beta 2)

Research 2026-09-26, for the beta 2 item "POTA and SOTA spots with a
frequency and mode you choose".

## How a spot travels

1. The X6100 sends a JS8 message to the `@APRSIS` group:
   `@APRSIS CMD :<addressee padded to 9>:<text>`.
2. Any desktop JS8Call station that hears it, with "spot to APRS" on,
   forwards it to APRS-IS as a message **from your callsign** (JS8Call
   `spotAprsCmd` → `APRSISClient::enqueueThirdParty`; `CALL/7` becomes
   `CALL-7`, a plain call stays plain). Several gateways can forward the
   same message, so a spot may arrive 2-3 times (the gateways flag dupes).
3. A spotting gateway on APRS-IS reads it and posts the spot.
4. The gateway replies ("Spotted", "Dupe", errors) as an APRS message
   to your callsign. **Nothing carries that reply back over JS8**, so
   the radio never sees it. Check pota.app / SOTAwatch, or aprs.fi /
   findu.com messages for your call.

APRS text after the addressee: 67 characters max. JS8 can send every
character needed (`!` is in the varicode table).

## Gateways, checked live on findu.com (2026-09-26)

| Gateway | Addressee | Status | Format |
|---|---|---|---|
| APRS2SOTA (sotaspots.co.uk) | `APRS2SOTA` or `SOTA` | busy, replies `Spotted:` / `Dupe:` | `<summit> <freq> <mode> [call] [comment]` |
| APSPOT (VK2MES, apspot.radio) | `APSPOT` | working: `! POTA US-6233 14.081 DATA CQCQ` → `SUCCESSFULLY SPOTTED FOR US-6233 TO pota.app` (AD9BL-7, 02:08Z) | `! <POTA/SOTA/WWFF/SIOTA> <ref> <MHz> <mode> [comment]` |
| POTAGW | `POTAGW` | one message since August (`N4JAW US-7956 14074 FT8 NOW`), no reply seen; "down for a long time" in reports | `<call> <park> <kHz> <mode> [comment]` (JS8Spotter's format, what beta 1 sends) |
| `POTA` | `POTA` | a few tries, no replies | - |

### APRS2SOTA
- Needs registration: email the operator (name + callsign) via
  sotaspots.co.uk. "Accepts any SSID on the originator's callsign."
- Modes: `AM CW DATA DV FM SSB OTHER` (FT8 sent by PY2TDB was posted as DATA).
- Frequency in MHz (`14.078`); kHz also works in practice (`21303` → 21.303).
- Optional callsign before the comment (for `/P` etc.). The comment must
  not look like another callsign.

### APSPOT
- POTA: posts to **pota.app** (and ParksNPeaks) for each system where you
  are a valid user; a pota.app account is enough for pota.app.
- SOTA: posts to ParksNPeaks, which cross-posts to SOTAwatch: needs a
  ParksNPeaks account.
- Frequency: decimal MHz ("7.144", "7.1"; not "7.0").
- Modes: `SSB CW FM AM DATA`, plus `FT8` for POTA. `FT4` failed live.
- The word "test" anywhere: checked but NOT posted.
- `spots POTA` (or `spots pota cw`, `spots sota 5`) replies with recent
  spots, but as said above the reply can't reach us over JS8.

### POTA park numbers
Since 2024 POTA uses ISO prefixes: `US-` (was `K-`), `CA-` (was `VE-`).
Beta 1's APRS hint said `VE-1234`; the form now warns about K-/VE- refs.

## What the app does (user's choices, 2026-09-26)

- POTA → APSPOT only (POTAGW dropped: no replies seen):
  `@APRSIS CMD :APSPOT   :! POTA CA-1234 7.078 DATA JS8`.
- SOTA → APRS2SOTA (the user registers with it):
  `@APRSIS CMD :APRS2SOTA:VE7/LM-001 7.078 DATA VE7NHW JS8`.
- APRS > POTA spot / SOTA spot open a form (like Log QSO): Send spot,
  Park/Summit, Frequency (JS8 dial or the last typed, press to switch),
  Type a frequency..., Mode (DATA SSB CW FM AM + FT8 for POTA / DV for
  SOTA), Comment ("JS8" automatically when spotting the JS8 dial with
  none typed), Close. A preview line shows the message. Remembered in
  js8_texts.txt: POTA=, SOTA=, SPOTMODE=, SPOTHZ=, SPOTTYPED=, SPOTNOTE=.
- Spot frequency: kHz, or MHz below 1000 (VHF spots); 1.8 MHz-1.3 GHz.
  Sent as MHz with 3+ decimals.
- Harness: ONLY_APRS=1 (starts from an empty texts file).

Sources: apspot.radio/getting-started, zindello.com.au (APSPOT author),
sotaspots.co.uk/Aprs2Sota_Info.php, github.com/DanDawson/js8spotter
(js8spotter.py `aprs_pota`), JS8Call mainwindow.cpp / APRSISClient.cpp,
findu.com msg.cgi for APSPOT, APRS2SOTA, SOTA, POTAGW, POTA, docs.pota.app,
qrper.com (POTA prefix change).
