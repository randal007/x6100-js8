Title: I2C write errors to the BASE: re-send the registers at once, and stop a TX nobody asked for

Georgy, Kris,

Hany's 1KO125 1.0.2 beta 5 has a fix for something many of us have seen: the radio going into TX by itself while receiving. Before I make a PR from it, I'd like your view, since it's the GUI's link to the BASE.

**What we've seen**
- On my radio, 2026-10-13, a couple of seconds after power-up: TX (red light, zero power) until I restarted. Once, not seen again.
- On my radio, every time the GUI starts: `i2c i2c-0: sendbytes: NAK bailout` / `error -110` in dmesg, about 26 s after boot (and again after a GUI restart).
- Hany says users of every R1CBU-based firmware have reported phantom TX, and his logs show an electrical disturbance that corrupts the I2C link for an instant.

**What his beta 5 does** (from his code, `radio.c` + `diag_log.c`)
1. **Re-send at once:** after a write error to the BASE, the whole register block (the library's copy, so always what the GUI is using) is sent again straight away, then every 50 ms for the first second and every 500 ms after that, until it gets through. Today it waits for the 3-second refresh.
2. **After every un-key** the block is sent once more (150 ms later).
3. **TX nobody asked for:** for 10 s after a write error, if the BASE stays in TX for more than a second without a TX command from the GUI, the block (PTT off) is sent again. A microphone or VOX TX isn't stopped.
4. **How it sees an error:** the control library reports a failed write only with `perror("Can't write to i2c")`, so he routes the GUI's stderr through a pipe and reads it in a thread, and also reads `/dev/kmsg` for `i2c-0:` messages. Both write to `/mnt/i2c.log`.

The PTT log from the same work is already in #316 (who keyed the transmitter), so we'll have data either way.

**My questions**
- Would you rather **X6100Control return the write error** (e.g. `x6100_control_idle()` and the setters returning false, or an error counter we can read), so the GUI doesn't have to parse stderr or the kernel log? Then the re-send in 1-3 is small and self-contained.
- Are 1-3 OK with you as they are? 2 adds one block write per transmission, and 1 adds writes only while the link is failing.
- Any reason the register block could be wrong to re-send mid-TX (e.g. during ATU or SWR scan)?

If you agree, I'll make the PR (with Hany as author of his parts) and test it on my radio with the console attached, watching the NAK at start-up.

73, Randal VE7NHW
