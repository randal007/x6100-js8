# Drafts: PRs and feedback for upstream

Ready-to-send texts for the fixes we carry outside the JS8 app (the queue
and the background: [../upstream-patches.md](../upstream-patches.md)).
**Nothing here has been sent.** Each needs VE7NHW's OK, and the ones marked
*after a test* need that test on the radio first.

Each patch applies cleanly to gdyuldin's current code (checked
2026-10-10: AetherX6100Buildroot 07a8d25, x6100_gui main 02f5240), and the
GUI files compile for the radio (ARM) with it.

| # | To | Patch | Status |
|---|---|---|---|
| 1 | AetherX6100Buildroot | [1-gpsd-hotplug.patch](1-gpsd-hotplug.patch) | tested on the radio 2026-10-10: ready |
| 2 | AetherX6100Buildroot | [2-uhid-ble-keyboards.patch](2-uhid-ble-keyboards.patch) | after a test: an LE keyboard types |
| 3 | AetherX6100Buildroot | [3-bluetooth-pairings-backup.patch](3-bluetooth-pairings-backup.patch) | after a test: still paired after an update |
| 4 | x6100_gui | [4-gui-keyboards.patch](4-gui-keyboards.patch) | after a test: Bluetooth keyboard everywhere; first in the order agreed with gdyuldin |
| 5 | x6100_gui | none yet | idea offered 2026-10-06 (knob speed in dialog_rotary), no code written |
| 6 | gdyuldin (feedback) | none | send with 2 and 4 |

Sending a Buildroot PR needs a fork of gdyuldin/AetherX6100Buildroot under
randal007 (none yet): fork, branch, `git apply` the patch, commit, PR with
the text below. PR 4 goes from a branch on gdyuldin/x6100_gui's fork
(randal007/x6100-js8 is one) based on his `main`.

---

## 1. gpsd: start with a control socket, so a USB GPS plugged in after boot works

**To:** gdyuldin/AetherX6100Buildroot

> A USB GPS (u-blox, `/dev/ttyACM0`) only works if it's plugged in before
> the radio is switched on. Plugged in later, or after it drops off USB
> and comes back, gpsd never uses it until a reboot.
>
> The gpsd package's hotplug rule does fire (`gpsd.hotplug: add
> /dev/ttyACM0`), but it hands the device to gpsd with `gpsdctl` through
> gpsd's control socket, and S50gpsd starts gpsd without one (`-F`), so
> the request goes nowhere (`gpspipe -w` shows `"devices":[]`).
>
> This adds `-F /var/run/gpsd.sock` to S50gpsd's options. Nothing else
> changes for a GPS plugged in at boot.
>
> **Tested** on an X6100: boot without the GPS, plug it in: gpsd takes it
> and APP > GPS shows it.
>
> Randal VE7NHW

## 2. Kernel: uhid, so Bluetooth LE keyboards can type

**To:** gdyuldin/AetherX6100Buildroot

> A Bluetooth LE keyboard (HID over GATT) pairs, bonds and connects, but
> no key ever arrives. bluetoothd (built with HoG) passes an LE keyboard's
> keys to the kernel through `/dev/uhid`, and the kernel config has no
> uhid (only `BT_HIDP`, which is for classic Bluetooth keyboards).
>
> This adds `CONFIG_UHID=m` (HID is a module in this config, so uhid is
> one too) and loads it at the start of S40bluetoothd, so `/dev/uhid` is
> there before bluetoothd needs it.
>
> **Tested** on an X6100 with an LE keyboard: `/dev/uhid` present after
> boot, the keyboard shows up as an input device with a `kbd` handler once
> it connects, and types (with the x6100_gui PR).
> *(Draft: "and types" is still to confirm on the radio.)* (The GUI side, opening a Bluetooth keyboard, is a separate
> PR to x6100_gui.)
>
> Randal VE7NHW

## 3. Keep Bluetooth pairings across updates

**To:** gdyuldin/AetherX6100Buildroot

> Bluetooth pairings live in `/var/lib/bluetooth` on the rootfs, so every
> update drops them and each keyboard or headset has to be paired again.
> S04restore_backup_config already saves WiFi connections and SSH keys to
> DATA at shutdown and puts them back at boot, before bluetoothd starts.
> This adds `/var/lib/bluetooth` to that list.
>
> It's added as `$(ls -d /var/lib/bluetooth 2>/dev/null)`, so the backup
> still works if the folder doesn't exist: the script runs with `set -e`,
> and a missing path would otherwise fail the whole backup, WiFi included.
> As before, the save only happens on a normal shutdown.
>
> **Tested:** the backup with and without the folder (both succeed, the
> pairing files are in the archive when present); on the radio: pair a
> keyboard, switch off, update, the keyboard reconnects without pairing.
> *(Draft: the radio part is still to do; don't send before it.)*
>
> Randal VE7NHW

## 4. Keyboards: hot plug, no lost keys, no key leaking out of the text window, Bluetooth keyboards

**To:** gdyuldin/x6100_gui (the first PR in the order we agreed)

> Fixes for external keyboards, found while typing a lot in our JS8 app.
> They're in the shared keyboard code, so every text box gets them.
>
> - **A USB keyboard plugged in while the radio is on is picked up**
>   (`src/keyboard.c`). Since 1.0 the GUI looked for the keyboard once,
>   right at udev's USB "add", a moment before its `/dev/input` node and
>   by-path link exist, so it was never found. Now it keeps looking for a
>   few seconds after any device comes or goes, and an unplugged keyboard
>   that comes back is opened again.
> - **No more lost keystrokes when typing fast** (`src/kbd_rollover.{c,h}`).
>   Fast typing overlaps keys (the next goes down before the last is up);
>   LVGL's keypad only takes a new key after a release, so overlapped keys
>   were dropped ("HELLO" came out "HLL"). The rollover filter releases the
>   held key first, then presses the new one, and tracks keys by scancode
>   so a late release (or Shift let go first) can't stick.
> - **The key that closes the text window no longer repeats into the
>   screen behind it** (`src/textarea_window.c`). Closing deletes the
>   focused object, which resets LVGL's keypad state with a press time of
>   0, so the still-held key counted as a long press and repeated into
>   whatever got the focus next: one ESC could close a whole app, Enter
>   could press the next button. The key is now ignored until it's
>   released.
> - **Bluetooth keyboards** (`src/keyboard.c`, `src/usb_devices.cpp`,
>   `src/pubsub_ids.h`). A Bluetooth keyboard gets no `/dev/input/by-path`
>   link (it's a virtual uhid device) and no USB event, so it was never
>   opened; and it goes to sleep and comes back as a new node. The udev
>   monitor now also watches the `input` subsystem
>   (`MSG_INPUT_DEVICE_CHANGED`, a new message so the GPS code isn't
>   woken by it), and when there's no USB keyboard the search finds a
>   Bluetooth one in udev's database (`ID_INPUT_KEYBOARD=1`,
>   `ID_BUS=bluetooth`). A USB keyboard still comes first. (LE keyboards
>   also need uhid in the kernel: the AetherX6100Buildroot PR.)
>
> **Tested:** on the radio with a USB keyboard (fast typing, closing the
> text window) and a Bluetooth LE keyboard; on the PC with a small test
> of the hot plug logic (USB add/remove, the link arriving late, a
> Bluetooth keyboard connecting, sleeping, waking as a new node, USB
> first): we can add it to the repo if you'd like it.
> *(Draft: confirm on the radio before sending: a USB keyboard plugged in
> while running, and the Bluetooth keyboard typing in JS8, Callsign and
> QTH, after sleep and after a power cycle.)*
>
> Randal VE7NHW

**Before sending, also consider (not in the patch yet):** the Callsign box
(`src/dialog_callsign.c`) accepts capitals only, so lowercase typed on any
keyboard without Caps Lock is silently dropped; the JS8 app turns lowercase
into capitals instead. A small `LV_EVENT_INSERT` handler would do the same
there. Check on the radio first whether that's part of what VE7NHW saw.

## 5. Knob speed in dialog_rotary

Offered to gdyuldin on 2026-10-06 (the timestamp-based speed he added to
the main knob, for `dialog_rotary` too; then JS8's own 30 ms click
grouping can go). No code written yet; wait for his answer.

## 6. Feedback for the Bluetooth screen (ver_1.1)

**To:** gdyuldin (a comment or a message, not a PR)

> Hi Georgy, a few notes on the Bluetooth screen in ver_1.1 from getting
> a Bluetooth keyboard going on 1.0.2 over the console, in case they help:
>
> 1. **Keyboards that ask you to type a code** call the agent's
>    `DisplayPasskey` (or `DisplayPinCode`): the radio shows a number and
>    you type it on the keyboard. The agent in `src/bt.cpp` has
>    RequestPinCode / RequestPasskey / RequestConfirmation /
>    RequestAuthorization / AuthorizeService but not those two, so such a
>    keyboard would fail to pair.
> 2. **Trusted:** after pairing, setting the device's `Trusted` property
>    lets it reconnect by itself without an authorization request (we
>    didn't find this set in bt.cpp).
> 3. **Pairing window:** many keyboards accept a pairing only for a few
>    seconds after their pairing key is pressed; a late `Pair()` gets no
>    answer, and connecting without pairing leaves a connected-but-useless
>    device. Pairing the moment it shows up in discovery worked every time.
> 4. **The image's own agent:** `bt_start.sh` starts
>    `bt-agent --capability=NoInputNoOutput` and the adapter stays
>    discoverable with no timeout, so anyone nearby can pair with the radio
>    without being asked. It also cancelled our keyboard pairing until
>    another agent took over as default. Your agent registering as the
>    default probably covers the second part; the first might be worth a
>    look.
> 5. Two image changes we needed are in PRs to AetherX6100Buildroot: uhid
>    in the kernel (LE keyboards) and keeping `/var/lib/bluetooth` in the
>    DATA backup (pairings survive updates).
>
> Thanks for all the work on this!
>
> Randal VE7NHW
