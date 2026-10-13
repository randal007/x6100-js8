# What we carry on top of upstream

Our image is gdyuldin's **x6100_gui** (the GUI, with JS8 added) built by
gdyuldin's **AetherX6100Buildroot** (the Linux image). Anything we change
outside our own JS8 code can be lost or clash when either moves on. **When
updating to a newer upstream, go through this list**, and send the fixes
that help everyone upstream as PRs, so they stop being ours to carry.

## PRs to send (the queue)

Ready-to-send texts and patches (checked against gdyuldin's current code):
[upstream-prs/](upstream-prs/README.md).

Everything here is outside our JS8 app. Tick it when the PR is merged and
drop our copy.

| # | To | What | Status |
|---|---|---|---|
| 1 | gdyuldin/AetherX6100Buildroot | gpsd with a control socket (`-F /var/run/gpsd.sock` in S50gpsd): USB GPS hotplug works (section 1) | **MERGED 2026-10-10: OpenX6100/AetherX6100Buildroot#2** (gdyuldin's repos moved to the OpenX6100 organization). Our CI step now skips itself (it checks for `-F` first); remove it when convenient |
| 2 | gdyuldin/x6100_gui | USB keyboard hot plug (`src/keyboard.c`), Bluetooth keyboards found and followed (`src/keyboard.c`, `src/usb_devices.cpp`, `src/pubsub_ids.h`), then the other keyboard fixes (section 2) | **MERGED 2026-10-10: OpenX6100/x6100_gui#281** (myshak1 approved and merged, 0e6662e; with the Callsign lowercase fix). Our copy goes when we next merge upstream |
| 3 | OpenX6100/x6100_gui | JS8 itself, in four PRs: see [JS8 into OpenX6100](#js8-into-openx6100-the-plan) below | planned (2026-10-11); first the app-ID discussion (OpenX6100/x6100_gui discussion 283). **Licence settled (discussion 23, gdyuldin 2026-10-13):** every existing file stays LGPL-2.1-or-later, the engine in `third-party/js8core` with its own GPLv3 LICENSE, an image with JS8 is GPLv3 as a whole; **JS8 in the default build** (no off switch needed). He asks for a short README / build-docs note on the image's licence and where the corresponding source is (Buildroot configs, `make legal-info`): **goes in 2/4**, as our reply said ([12-licence-reply.md](upstream-prs/12-licence-reply.md), then discussioncomment-18903656). 1/4 = #290, approved by myshak1, waiting for gdyuldin |
| 4 | JS8Call-improved/Android-port (js8core) | local patches 1-15, e.g. 14 (Ultra decoded on Turbo's schedule) and 15 (Ultra on its own decode thread, a thread-start hook) | listed in [UPSTREAM.md](../third-party/js8core/UPSTREAM.md); bugs already reported in issue #104 |
| 5 | gdyuldin/x6100_gui | `dialog_rotary` timestamp-based knob speed (as his main knob) | offered to gdyuldin 2026-10-06 |
| 6 | gdyuldin/AetherX6100Buildroot | uhid in the kernel (`CONFIG_UHID=m`) + `modprobe uhid` in S40bluetoothd: Bluetooth LE keyboards type (section 1) | **MERGED 2026-10-10: OpenX6100/AetherX6100Buildroot#3** (myshak1, f1447e0). Our CI step skips itself (it checks for `CONFIG_UHID` / `modprobe uhid` first); remove it when convenient |
| 7 | gdyuldin/AetherX6100Buildroot | Bluetooth pairings kept across updates: `/var/lib/bluetooth` in S04restore_backup_config's backup (section 1) | **not sent:** myshak1's **OpenX6100/AetherX6100Buildroot#4** (open, 2026-10-10) has `S39btstate` (pairings in /mnt/bluetooth.tar), more complete. **Drop our CI step when #4 is merged**, or both would save and restore the pairings |
| 8 | gdyuldin (feedback, not a PR) | for his `ver_1.1` Bluetooth screen: keyboards need the agent's `DisplayPasskey` / `DisplayPinCode` (missing: a keyboard asking you to type a code would fail), a paired keyboard marked Trusted, and pairing the moment a keyboard appears (many accept a pairing only for seconds); the image's `bt-agent --capability=NoInputNoOutput` + always discoverable accepts any pairing without asking, and cancelled ours | **sent 2026-10-10 as a comment on OpenX6100/x6100_gui#280; ALL addressed by myshak1 the same day (e38637e on that branch):** LE keyboards listed (filter `auto`, input devices only), passkey/PIN shown in the window, Pair() sent the moment the keyboard appears, discoverable and pairable only 3 min after Scan, agent re-claims default. He has no keyboard and asked us to test on his branch. **Tested 2026-10-11 on VE7NHW's radio: all worked first try; report + log on #280.** Into our build only once merged in OpenX6100 (VE7NHW: wait, "do it proper"; notes in PLAN.md) |
| 9 | OpenX6100/AetherX6100Buildroot | Kernel patch 9002: btusb keeps HCI events ahead of ACL data (poll sync on from probe, a whole jiffy's hold, the event before the worker), so BLE keyboards pair every time; btmon showed the SMP keys arriving before Encryption Change | **MERGED 2026-10-12: OpenX6100/AetherX6100Buildroot#5** (gdyuldin approved). Our copy and CI step removed the same day (7253349) |
| 10 | OpenX6100/x6100_gui | Bluetooth window: Scan in LE / classic turns, a keyboard waited for by name (LE only), old same-name pairings forgotten, Connect ends a Scan, highlight follows the device, WiFi off/on re-setup (`bt_dev.c`, `bt_ctl.c`, `dialog_bt.c`) | **MERGED 2026-10-12: OpenX6100/x6100_gui#291** (gdyuldin approved; from branch `bt-keyboard-pairing` in OpenX6100's repo). OpenX6100's three files are identical to ours |
| 11 | OpenX6100/x6100_gui (issue) | Bluetooth security: CAT RFCOMM channel 1 accepts unpaired connections (and can key PTT), visible and pairable from boot until the window is first opened (bt_start.sh + bt-agent), headset PTT button taken from any device | **issue #292 opened 2026-10-12.** myshak1 agreed with all three; split: **point 3 (headset PTT) is his: x6100_gui#296**, approved by VE7NHW 2026-10-13. **Points 1 + 2 are ours, opened 2026-10-13:** x6100_gui **#297** (CAT RFCOMM `BT_SECURITY_MEDIUM`, branch `bt-security`) and AetherX6100Buildroot **#6** (bt_start.sh hidden, not pairable, no bt-agent; `JustWorksRepairing` kept, branch `bt-security`), reviewers gdyuldin + myshak1. Tested on VE7NHW's radio: keyboard pair/forget/re-pair, FT8CN CAT from a paired phone with the keyboard connected, from an unpaired PC the radio is invisible and CAT refused |
| 12 | OpenX6100/x6100_gui (reviews) | #271 WSPR (Kris's two fixes in), #296 headset PTT | **approved by VE7NHW 2026-10-12 / 2026-10-13** |

## JS8 into OpenX6100: the plan

Agreed with VE7NHW 2026-10-11. VE7NHW is a maintainer of OpenX6100
(team *Maintainers*, with myshak1; gdyuldin is the owner), so the branches
go **in OpenX6100/x6100_gui itself** (no fork), one PR each, **reviewed
and merged by gdyuldin or myshak1**, never merged by us alone. One PR at
a time: the next once the previous is merged.

Sizes against OpenX6100 main (0e6662e, with #281), 2026-10-11:

| PR | Branch | What | Size |
|---|---|---|---|
| 1 | `js8-groundwork` | The changes to code other apps use: radio helpers (TX filter, RX DSP off, speaker play), keypad hold time, JS8's settings group and its `js8_db` (outside upstream's migrations), JS8 + GhostNet frequency rows, `MODE_JS8` in the QSO log and ADIF (+ upstream's SSB-written-as-AM fix), the digital-mode types, the lv_waterfall ring buffer (every app's waterfall cheaper; clear = opaque black). Nothing a user sees on its own; small enough to read every line. **Built 2026-10-11: branch in OpenX6100 at 74b6dfb (16 files, +422 −18); merges cleanly with each of myshak1's open PRs** | 16 files, +422 −18 |
| 2 | `js8-engine` | `third-party/js8core` (JS8Call-improved/Android-port + our patches, listed in its UPSTREAM.md). **The licence note goes here** (GPL-3.0 engine; combines with the GUI's LGPL-2.1-or-later, the image as a whole is then GPL-3.0; or JS8 as a build option off by default: their call) | 8,000 lines own code; 150,000 vendored (Eigen, CRCpp); 524,000 generated JSC tables |
| 3 | `js8-library` | `src/js8` (messages, inbox, relays, stations, history, ...) and its unit tests (`tests/test_js8*.cpp`), with a CI job to run them | ~10,000 + 4,700 lines |
| 4 | `js8-app` | `src/dialog_js8.c`, `src/js8_*` (waterfall, clock), JS8's TX modules `tx_player` / `tx_level` (new files only JS8 uses: moved here from PR 1 on 2026-10-11), the marks font, the map, the app button and number (`ACTION_APP_JS8` = 112, in `buttons.cpp`, `main_screen.c`, `settings_types.h`, `src/CMakeLists.txt`): after this one JS8 is in the firmware. **Before it:** (a) the APP menu: page 3 has room for 4 apps and WiFi, Bluetooth, WSPR, PSK31, Olivia and JS8 make 6, so a 4th APP page (buttons.cpp counts pages by itself; tried in a trial merge) and an agreed layout with myshak1; (b) one cty.dat: myshak1's #282 added `third-party/cty/cty.dat` (VER20240202, installed to /usr/share/x6100/cty.dat, /mnt/cty.dat overrides); JS8 should read that one (same override) and we offer our newer Big CTY (VER20260915; his dxcc.c parses it) | ~14,000 lines |

Why four: one PR would be unreviewable (gdyuldin asked for small PRs);
the map stays in PR 4 because it is woven through `dialog_js8.c` and a
map-less app would be a version never used on the air. Each PR builds and
works on its own and needs only the ones before it.

Not sent: the UI harness (`tools/js8_ui_harness`, 6,700 lines; later if
they want it), our docs and research, `x6100-bt-pair` (myshak1's Bluetooth
window replaces it).

**Before PR 1: app IDs (settled 2026-10-11).** `press_action_t` values are
saved on the card only when an app is offered as a long-press action. In
OpenX6100/x6100_gui discussion 283 (our post, text in
[upstream-prs/7-app-ids-discussion.md](upstream-prs/7-app-ids-discussion.md))
gdyuldin suggested appending new apps at the end and leaving them out of
the long-press list, so nothing is stored and nothing can clash. myshak1
took that for his apps (no long-press entries; explicit values kept anyway:
WSPR 113, PSK31 114, Olivia 115, Bluetooth 116), and so did we: JS8 opens
from the APP menu only since 2026-10-11 and `ACTION_APP_JS8` has no fixed
number. Other numbers stored on the card, for the same discussion: `MODE_JS8` = 8
in `qso_log.db` (upstream ends at RTTY = 7), digital-mode types
`CFG_DIG_TYPE_JS8` = 2 / `CFG_DIG_TYPE_JS8_GHOSTNET` = 3. JS8 doesn't use
upstream's migration numbers (its `js8_db` table marks what it did).

## 1. The Linux image (AetherX6100Buildroot), patched in our build

Our image build (`.github/workflows/main.yml`, *Build image*) checks out
AetherX6100Buildroot and edits it before building. These edits live **only
in that workflow**: a new Buildroot doesn't remove them, but a changed line
upstream can make one stop applying.

| Step in main.yml | What it changes | Why | Upstream? |
|---|---|---|---|
| *Patch GUI mk* | `x6100_gui.mk`: build our checkout instead of OpenX6100's repo (Buildroot checked out from OpenX6100/AetherX6100Buildroot since 2026-10-11) | builds our GUI | no (ours only) |
| *Enable JS8 dependencies* | `x6100_gui.mk` + `X6100_defconfig`: boost (headers) and fftw-single | js8core needs them | with the JS8 PR |
| ~~*Enable gpsd hotplug (control socket)*~~ | `S50gpsd`: gpsd with `-F` | a GPS plugged in after boot | **merged: AetherX6100Buildroot#2; step removed 2026-10-11** |
| ~~*Enable Bluetooth LE keyboards (uhid)*~~ | kernel `CONFIG_UHID=m`, `modprobe uhid` | LE keyboards type | **merged: AetherX6100Buildroot#3; step removed 2026-10-11** |
| ~~*Keep Bluetooth pairings across updates*~~ | `/var/lib/bluetooth` in S04's backup | pairings across updates | **replaced by myshak1's S39btstate (`/mnt/bluetooth.tar`), AetherX6100Buildroot#4; step removed 2026-10-11** (old cards' `backup.tgz` still holds a pairing; S39btstate then sees `/var/lib/bluetooth` and leaves it) |

**Bluetooth on this radio, for whoever picks this up:** the WiFi and
Bluetooth chip (RTL8723BU, on USB) is one part with one power pin: the
GUI's WiFi switch (APP > WiFi, `wifi_power_on()` in `src/wifi.cpp`)
powers both, so with WiFi off there's no `hci0`. v1.0.2 has no pairing
screen (gdyuldin's `ver_1.1` branch adds one): pair with `bluetoothctl`
over the console, as its own default agent (`agent KeyboardDisplay`,
`default-agent`): the image's `bt-agent --capability=NoInputNoOutput`
(started by `/usr/bin/bt_start.sh` from a udev rule) cancelled the
pairing. Pairings live in `/var/lib/bluetooth` on the rootfs, which a
re-flash replaces; our build adds them to `S04restore_backup_config`'s
backup to DATA (above). Pairing: `x6100-bt-pair` (our `rootfs/usr/bin`,
installed by the GUI's CMake), the same steps as a script.

The gpsd, uhid and pairings-backup steps **fail the build** if the line they change isn't
what they expect, so they can't be lost silently: check upstream's S50gpsd /
S40bluetoothd / S04restore_backup_config and adjust (or drop the step if
upstream took the fix).

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
