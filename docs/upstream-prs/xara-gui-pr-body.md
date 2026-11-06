**Draft, experimental.** The groundwork for XARA, so we can start working on it here. Needs OpenX6100/AetherX6100Buildroot#10 (the xara package); without it the app just says "xara is not installed".

## What it is

XARA runs **xara** (github.com/x0rro/xara, MIT, by x0rro), a VARA HF compatible modem, as its own program on the radio. A phone app connects to it over WiFi: **WoAD** (Android Winlink client) on the VARA ports 8300/8301, **APRSdroid** on the KISS port 8100 (also over Bluetooth, RFCOMM channel 3, paired phones only). The GUI holds no VARA code: it starts the modem, keys the radio when xara asks, and shows what it is doing.

**VARA is EA5HVK's modem; xara is x0rro's own, independent program.** That's said in the app (Settings list) and at the top of `dialog_xara.c`. **Free speed levels only**: the package never builds xara's `registered` feature.

## Where

APP page 5, after JTTY. `ACTION_APP_XARA = 119`, APP menu only (not a long-press action, never stored). Each shared line sits under its own `/* XARA */` line (#313), so it merges in any order with other apps.

## The app

- **Screen:** link state, the other station, level and speed, S/N, phone connections, and JS8's waterfall with xara's passband as two orange lines (our own TX rows get a red edge).
- **Buttons:** BW 500 / 2300 / 2750, APRS (40 m 7.083.5 / 30 m 10.148.2, USB-D, BW 500), Monitor (xara-monitor: decodes VARA between other stations, never transmits), Settings..., Restart modem.
- **Settings** (a list like JS8's): Busy detector, Record (a FLAC of what it hears, sends and PTT, in /mnt/xara-rec, newest 10 kept; **off by default**), Auto answer, PTT lead, TX level (MFK knob, also while sending), send and receive buffers.

## Nothing transmits on its own

- Plain xara answers every call to its callsign from the moment it starts. The package's xara has an **answer gate**: no call is answered until the app opens it, which needs Settings > Auto answer ON **and** a phone app connected. **Auto answer is off every time XARA opens.**
- PTT goes through `radio_set_modem()` (the codec path, as FT8 and JS8), never longer than 35 s, and drops if xara exits or the link breaks. The first ESC stops TX and drops the link, the second closes (as JS8).
- While open: USB-D, RX filter around xara's passband, TX filter 100-3000 Hz, NR/NB/notches off, power capped at 5 W. All put back on close.

## Tested

On my radio with private test builds of this app (2026-10-25 to 11-02), on a bench: X6100 on a dummy load, HL2 with real VARA HF on the PC, Pat as the Winlink client. APRS frames decoded over the air; Winlink P2P links worked both ways; the last build (the same xara as the package, 919f5dd) moved 60 kB in one session. Those test builds also had VARA's registered speeds (I hold a VARA licence), so the fastest results don't carry over: **this build is free speeds only**. This PR's image: test image run https://github.com/randal007/x6100-js8/actions/runs/42272617883 (building now; both branches on OpenX6100's current main).

## Still to do (why it's a draft)

- Lock the VARA and KISS ports to the phone (they're open on all interfaces while XARA is open).
- TX level at very low power into an amplifier (the level is set by hand for now).
- Move to x0rro's release when his libxara (0.1.4) has the X6100 fixes.
- A manual section.
- Radio test of this exact image.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
