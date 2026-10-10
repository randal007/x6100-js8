Fixes for external keyboards, found while typing a lot in our JS8 app on the X6100. They're all in the shared keyboard code, so every text box gets them. This is the first of the small PRs we talked about. The JS8 app itself comes later, separately.

**A USB keyboard plugged in while the radio is on is picked up** (`src/keyboard.c`)
Since 1.0 the GUI looked for the keyboard once, right at udev's USB "add". That's a moment before the keyboard's `/dev/input` node and by-path link exist, so it was never found. Now it keeps looking for a few seconds after any device comes or goes, and an unplugged keyboard that comes back is opened again.

**No more lost keystrokes when typing fast** (`src/kbd_rollover.{c,h}`)
Fast typing overlaps keys: the next goes down before the last is up. LVGL's keypad only takes a new key after a release, so overlapped keys were dropped ("HELLO" came out "HLL"). The rollover filter releases the held key first, then presses the new one, and tracks keys by scancode, so a late release (or Shift let go first) can't stick.

**The key that closes the text window no longer repeats into the screen behind it** (`src/textarea_window.c`)
Closing deletes the focused object, which resets LVGL's keypad state with a press time of 0. The still-held key then counted as a long press and repeated into whatever got the focus next: one ESC could close a whole app, and Enter could press the next button. The key is now ignored until it's released.

**Bluetooth keyboards** (`src/keyboard.c`, `src/usb_devices.cpp`, `src/pubsub_ids.h`)
A Bluetooth keyboard gets no `/dev/input/by-path` link (it's a virtual uhid device) and no USB event, so it was never opened. It also goes to sleep and comes back as a new node.
- The udev monitor now also watches the `input` subsystem and sends a new `MSG_INPUT_DEVICE_CHANGED`, so the GPS code isn't woken by it.
- When there's no USB keyboard, the search finds a Bluetooth one in udev's database (`ID_INPUT_KEYBOARD=1`, `ID_BUS=bluetooth`). A USB keyboard still comes first.
- LE keyboards also need uhid in the kernel: OpenX6100/AetherX6100Buildroot#3. Keyboard notes for the Bluetooth window are in #280.

**Callsign: lowercase typed as capitals** (`src/dialog_callsign.c`)
The box accepts capitals only, so on a keyboard without Caps Lock every letter was silently dropped. An `LV_EVENT_INSERT` handler turns lowercase into capitals. LVGL inserts the replacement as if typed, so the accepted list still applies.

**Tested**
- On the radio: a USB keyboard (fast typing, closing the text window), in our builds since September. A Bluetooth LE keyboard's keys reached the GUI.
- On the PC: a small test of the hot plug logic, covering:
  - USB add/remove, with the link arriving late
  - a Bluetooth keyboard connecting, sleeping, and waking as a new node
  - USB first, then back to Bluetooth when USB is unplugged

  I can add the test to the repo if you'd like it.
- A full image build with these files (our CI), and every changed file compiled against current `main`.
- Still to confirm on the radio: the Bluetooth keyboard being found by itself in every text box, after sleep and after a power cycle. I'll report back here.

Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)
