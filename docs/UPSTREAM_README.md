# X6100 LVGL GUI

This is part of an alternative firmware for X6100 using the LVGL library

## Installing

Open [Releases](https://github.com/gdyuldin/x6100_gui/releases/latest) page and download `sdcard.img` file (in Assets section). With balenaEtcher or Rufus
burn img file to microSD card. Insert card to the transceiver and boot it.

## Importing ADI log

Application could mark already worked callsign in the UI.
To load information about previous QSOs - copy your ADI log to the `DATA` partition and rename it to `incoming_log.adi`.
Application will import records to own log on the next boot and will rename `incoming_log.adi` to `incoming_log.adi.bak`.

*Note*: `DATA` partition will be created after first launch transceiver with inserted SD card.


## Exporting ADI log

Application stores FT8/FT4 QSOs to the `ft_log.adi` file on the `DATA` partition of SD card. This file might be used to load QSOs to online log.


## Building


* Clone repositories

```
mkdir x6100
cd x6100
git clone https://github.com/gdyuldin/AetherX6100Buildroot
git clone https://github.com/gdyuldin/x6100_gui
```

* Build buildroot

```
cd AetherX6100Buildroot
git submodule init
git submodule update
./br_config.sh
cd build
make
cd ../..
```

* Build app

```
cd x6100_gui
git submodule init
git submodule update
cd buildroot
./build.sh
```

## Merging upstream: numbers the JS8 fork has taken

Some things the JS8 fork added are numbered, and the numbers are stored on
the SD card's DATA partition, which survives reflashing. When merging
upstream (gdyuldin/x6100_gui), keep these as they are:

- **`params.db` migrations:** since the move to R1CBU 1.0 (2026-10-02) we
  follow **upstream's numbering exactly**: take `src/cfg/migrations.c` as
  upstream has it, new ones included, with no renumbering. JS8's two
  frequency lists are no longer migrations: `src/cfg/js8_db.c` inserts
  them (`INSERT OR IGNORE`) after upstream's migrations at every start.
  Betas 1–4 had used versions 4 and 5 for them, which upstream's own
  migration 4 (scaled settings to real numbers: `pwr` 4 = 0.4 W) needs;
  `js8_db_before_migrations()` puts such a card back to version 3 once
  (the `js8_db` table records it), so the conversion runs exactly once.
  `tests/test_js8_db.cpp` covers every kind of card; keep it passing.
- **`qso_log.db` mode** (`src/qso_log.h`): `MODE_JS8` = 8, appended after
  upstream's `MODE_RTTY` = 7, is stored in every JS8 QSO (worked-before
  marks, ADIF export). A mode upstream adds gets 9 or later; renumbering
  JS8 would mean updating the QSO database on every card.
- **Long-press actions** (`src/settings_types.h`): `ACTION_APP_JS8` = 111,
  stored in the long-press settings. 109 and 110 stay free for the Murus
  team's WeFax and NavTex (their fork, merged again once it's on 1.0);
  their SSTV, also 111 in their build, takes 112 here.
- **`digital_modes.type`** (`src/cfg/digital_modes.h`): `CFG_DIG_TYPE_JS8`
  = 2 and `_GHOSTNET` = 3, after upstream's FT8 and FT4.

(First written 2026-09-30, review D10; the migrations part rewritten for
R1CBU 1.0, see [upgrade-1.0.2](upgrade-1.0.2/README.md).)

## Upstream code we fixed (worth offering upstream)

- **`src/keyboard.c`, USB keyboard hot plug (2026-10-04).** On R1CBU 1.0
  a keyboard plugged in while the radio is on is never found: udev's USB
  "add" comes before the keyboard's `/dev/input/by-path/*-kbd` link
  exists, and 1.0's `usb_devices.cpp` passes the events on at once (0.34
  slept half a second between them), so the one look at each event finds
  nothing. Ours looks again every 0.5 s for 5 s after any USB add or
  remove, opens a keyboard that was unplugged and plugged back (new
  device node), and frees the `glob()` result upstream leaked on every
  look. Test: `tools/js8_ui_harness/kbd_hotplug_test.c` (fails on the old
  code). Not sent upstream yet: show the user first.
- `src/keyboard.c` also reads keys through `src/kbd_rollover.c` (beta 2),
  so keys pressed together aren't lost.
