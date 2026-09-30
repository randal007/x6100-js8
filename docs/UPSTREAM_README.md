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

Three things the JS8 fork added are numbered, and the numbers are stored on
the SD card's DATA partition, which survives reflashing. When merging
upstream (gdyuldin/x6100_gui), keep ours and give upstream's new ones the
next free numbers, or cards already in use get mixed up:

- **`params.db` migrations** (`src/params/migrations.c`): ours are 4
  (`_4_add_js8_presets`) and 5 (`_5_add_ghostnet_presets`), after
  upstream's 0–3. A card already at version 5 skips any migration 4 or 5
  upstream adds, so put upstream's after ours (6, 7 …). Ours only
  `INSERT OR IGNORE`, so running them again is harmless.
- **`qso_log.db` mode** (`src/qso_log.h`): `MODE_JS8` = 8, appended after
  upstream's `MODE_RTTY` = 7, is stored in every JS8 QSO (worked-before
  marks, ADIF export). A mode upstream adds gets 9 or later; renumbering
  JS8 would mean updating the QSO database on every card.
- **Long-press actions** (`src/params/params.h`): `ACTION_APP_JS8` is
  appended to `press_action_t` (after 1KO125's WeFax and NavTex) and its
  value is saved in the settings: keep it where it is.

(Decided 2026-09-30, review D10: write the rule down rather than renumber
now, which would need database surgery on existing cards.)
