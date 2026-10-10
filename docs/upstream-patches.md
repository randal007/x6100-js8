# What we carry on top of upstream

Our image is gdyuldin's **x6100_gui** (the GUI, with JS8 added) built by
gdyuldin's **AetherX6100Buildroot** (the Linux image). Anything we change
outside our own JS8 code can be lost or clash when either moves on. **When
updating to a newer upstream, go through this list**, and send the fixes
that help everyone upstream as PRs, so they stop being ours to carry.

## PRs to send (the queue)

Everything here is outside our JS8 app. Tick it when the PR is merged and
drop our copy.

| # | To | What | Status |
|---|---|---|---|
| 1 | gdyuldin/AetherX6100Buildroot | gpsd with a control socket (`-F /var/run/gpsd.sock` in S50gpsd): USB GPS hotplug works (section 1) | ready; send after a test on the radio (boot without the GPS, plug it in) |
| 2 | gdyuldin/x6100_gui | USB keyboard hot plug (`src/keyboard.c`), Bluetooth keyboards found and followed (`src/keyboard.c`, `src/usb_devices.cpp`, `src/pubsub_ids.h`), then the other keyboard fixes (section 2) | agreed order with gdyuldin: first PR; waiting for his GitHub organization |
| 3 | gdyuldin/x6100_gui | js8core + `src/js8` + the JS8 app, map and extras | after 2; licence question (GPLv3 js8core) open with gdyuldin |
| 4 | JS8Call-improved/Android-port (js8core) | local patches 1-15, e.g. 14 (Ultra decoded on Turbo's schedule) and 15 (Ultra on its own decode thread, a thread-start hook) | listed in [UPSTREAM.md](../third-party/js8core/UPSTREAM.md); bugs already reported in issue #104 |
| 5 | gdyuldin/x6100_gui | `dialog_rotary` timestamp-based knob speed (as his main knob) | offered to gdyuldin 2026-10-06 |
| 6 | gdyuldin/AetherX6100Buildroot | uhid in the kernel (`CONFIG_UHID=m`) + `modprobe uhid` in S40bluetoothd: Bluetooth LE keyboards type (section 1) | ready after a test on the radio (pair an LE keyboard, `/dev/uhid` exists, keys reach the GUI) |

## 1. The Linux image (AetherX6100Buildroot), patched in our build

Our image build (`.github/workflows/main.yml`, *Build image*) checks out
AetherX6100Buildroot and edits it before building. These edits live **only
in that workflow**: a new Buildroot doesn't remove them, but a changed line
upstream can make one stop applying.

| Step in main.yml | What it changes | Why | Upstream? |
|---|---|---|---|
| *Patch GUI mk* | `x6100_gui.mk`: build our checkout instead of gdyuldin's repo | builds our GUI | no (ours only) |
| *Enable JS8 dependencies* | `x6100_gui.mk` + `X6100_defconfig`: boost (headers) and fftw-single | js8core needs them | with the JS8 PR |
| *Enable gpsd hotplug (control socket)* (2026-10-10) | `rootfs-overlay/etc/init.d/S50gpsd`: `DEVICES="-n -F /var/run/gpsd.sock /dev/ttyACM0"` | without `-F`, the gpsd package's hotplug rule (`gpsd.hotplug` → `gpsdctl add`) has no control socket to talk to, so a GPS plugged in after boot (or one that drops off USB and comes back) is never used until a reboot. Found on the radio: `logread` shows `gpsd.hotplug: add /dev/ttyACM0`, the socket refuses connections, `gpspipe` shows `"devices":[]` | **yes: PR to send** |
| *Enable Bluetooth LE keyboards (uhid)* (2026-10-10) | `board/X6100/linux/sun8i-r16-x6100_defconfig`: `CONFIG_UHID=m`; `rootfs-overlay/etc/init.d/S40bluetoothd`: `modprobe uhid` first thing in `start()`; the Build step fails if `uhid.ko` isn't in the image | an LE keyboard (HID over GATT, e.g. VE7NHW's F01-keyboard, Nordic chip) pairs, bonds and connects, but bluetoothd hands its keys to the kernel through `/dev/uhid`, and the kernel had no uhid (only `BT_HIDP`, for classic keyboards), so it never types. Found on the radio: no `/dev/uhid`, no input device after connecting; bluetoothd itself has HoG built in. HID is a module on this kernel, so uhid is one too | **yes: PR to send** |

**Bluetooth on this radio, for whoever picks this up:** the WiFi and
Bluetooth chip (RTL8723BU, on USB) is one part with one power pin: the
GUI's WiFi switch (APP > WiFi, `wifi_power_on()` in `src/wifi.cpp`)
powers both, so with WiFi off there's no `hci0`. v1.0.2 has no pairing
screen (gdyuldin's `ver_1.1` branch adds one): pair with `bluetoothctl`
over the console, as its own default agent (`agent KeyboardDisplay`,
`default-agent`): the image's `bt-agent --capability=NoInputNoOutput`
(started by `/usr/bin/bt_start.sh` from a udev rule) cancelled the
pairing. Pairings live in `/var/lib/bluetooth` on the rootfs, which a
re-flash replaces; `S04restore_backup_config` backs up WiFi connections
and SSH keys to DATA at shutdown, not Bluetooth (adding
`/var/lib/bluetooth` there would keep pairings: an idea, not done).

The gpsd and uhid steps **fail the build** if the line they change isn't
what they expect, so they can't be lost silently: check upstream's S50gpsd /
S40bluetoothd and adjust (or drop the step if upstream took the fix).

**PR to send (AetherX6100Buildroot):** "gpsd: start with a control socket
so USB GPS hotplug works": the one-line change above; the test: boot with
the GPS unplugged, plug it in, `gpspipe -w` lists the device and
`ntpq -n -p 127.0.0.1` reaches `.GPS.`.

**Not pinned:** the workflow takes AetherX6100Buildroot's newest commit at
every build, so upstream changes (e.g. 2026-10: Bluetooth profiles, a
display panel kernel patch, PulseAudio settings) go into our images
without us choosing them. Pinning a commit (`ref:` in the checkout step)
would make builds repeatable; then updating = moving the pin on purpose
and reading this list. Not decided yet.

## 2. Shared GUI code (x6100_gui outside the JS8 app)

`git diff --stat upstream-main..main -- . ':!src/js8' ':!src/dialog_js8.*'
':!third-party' ':!docs' ':!tests' ':!tools'` lists them (2026-10-10:
38 files). The main ones, by what they're for:

- **USB and Bluetooth keyboards** (PR candidates, first in the agreed
  order with gdyuldin): `src/keyboard.c` (hot plug; since 2026-10-10 also
  a Bluetooth keyboard: it has no by-path link, so it's found in udev's
  database, `ID_INPUT_KEYBOARD` + `ID_BUS=bluetooth`, a USB one still
  first), `src/usb_devices.cpp` + `src/pubsub_ids.h` (the udev monitor
  also watches `input`, as `MSG_INPUT_DEVICE_CHANGED`: a Bluetooth keyboard
  connecting, or waking from sleep as a new node, has no USB event),
  `src/kbd_rollover.{c,h}` (keys
  lost while another is down), `src/textarea_window.c` (a held key leaking
  into the next field), `src/keypad.{c,h}` (hold time while JS8 is open).
- **JS8's transmit level**: `src/tx_level.{c,h}`, `src/tx_player.{c,h}`
  (also used by FT8: same audio path).
- **Radio helpers**: `src/radio.{c,h}` (TX filter, speaker play, RX DSP
  off while JS8 is open).
- **Waterfall widget**: `src/widgets/lv_waterfall.{c,h}` (fill rect for
  decode marks, exact invalidation).
- **Settings and database**: `src/cfg/*` (JS8 params, js8_db),
  `src/settings_manager.h`, `sql/digital_modes.csv` (JS8 and GhostNet
  frequencies), `src/adif.c` / `src/qso_log.h` (MODE JS8).
- **Hooking JS8 in**: `src/main_screen.c`, `src/buttons.cpp`,
  `src/settings_types.h`, `src/CMakeLists.txt`, `CMakeLists.txt`.
- Also changed (check before a merge): `src/cat/civ_processor.cpp`,
  `src/cfg/db.cpp`, `src/settings/settings_page_ui.cpp`.

Upgrade notes from the 1.0.2 move: [upgrade-1.0.2/](upgrade-1.0.2/README.md).

## 3. The JS8 engine (js8core)

Local patches, one commit each, with what to send upstream:
[third-party/js8core/UPSTREAM.md](../third-party/js8core/UPSTREAM.md)
(15 as of 2026-10-10). Bugs reported to the JS8Call-improved team:
[js8core-bug-reports.md](js8core-bug-reports.md).
