Discussion for OpenX6100/x6100_gui (Ideas), posted 2026-10-14 as https://github.com/OpenX6100/x6100_gui/discussions/303 (VE7NHW asked).

Title: Transmit level (ALC) for the digital apps: one shared, amplifier-friendly way

---

Hi @gdyuldin, @myshak1,

Testing PSK31 (#272) and Olivia (#274) on my X6100 with an amplifier (XPA125B), I noticed they put out noticeably less power than JS8 at the same power setting. Looking at the code, every digital app sets its transmit level a little differently, and I think it's worth getting this right in one place, because with an amplifier on the radio the drive has to be steady and land at the set power.

**What each app does today**

- **FT8** ([`ft8/tx_worker.c`](https://github.com/OpenX6100/x6100_gui/blob/main/src/ft8/tx_worker.c#L33)): starts from the learned `ft8_output_gain_offset`; after ~1 s, every audio chunk applies a correction from *one* power/ALC reading (0.4 of it); goes up only while the power is more than 0.5 W short; saves the learned offset at the end.
- **PSK31, Olivia, WSPR** (`psk_tx.c`, `olivia_tx.c`, `wspr_app.c` in the PRs): copies of that scheme. They read FT8's saved offset, but **don't save their own**, so every transmission starts again from FT8's last value.
- **JS8** ([`tx_level.c`](https://github.com/randal007/x6100-js8/blob/d19d272971f837ea4ddecd2be115a71d042f780d/src/tx_level.c), used by [`tx_player.c`](https://github.com/randal007/x6100-js8/blob/d19d272971f837ea4ddecd2be115a71d042f780d/src/tx_player.c)): we had trouble keeping the XPA125B's drive steady, so we rewrote it in beta 4.6:
  - waits ~1.3 s for the PA and ALC to settle, then decides from **½-second averages** of power and ALC, not single readings;
  - **ramps up gently** while the ALC reads zero (1 dB/s when far short, 0.5 dB/s near), at most 8 dB in one transmission;
  - **holds** as soon as the ALC starts working or the power reaches the setting, so it sits at the ALC's onset instead of hunting;
  - backs off in limited steps if overdriven (ALC high) or well over the setting;
  - **saves what it learned**, so the next transmission starts at the right level.

  The result is a steady power that lands on the setting, which is what the amplifier needs. **On the air:** it has been in daily use on my station for the last couple of weeks. I drive the XPA125B with the X6100 set to just 0.3-0.4 W, all the time, and the ALC code holds that low level steady, transmission after transmission. That low-power range is exactly where the old scheme see-sawed for us.

**One thing that isn't the code:** PSK31's envelope rises and falls at every phase change, so its *average* power on a wattmeter is lower than a single steady tone (FT8, JS8, Olivia, WSPR) at the same peak. Some of PSK31's lower reading is the mode itself. Olivia, though, sends one tone at a time and should reach the same power as JS8.

**Suggestion:** one shared transmit-level module that every digital app uses (FT8, WSPR, PSK31, Olivia, RTTY, JS8), so they all behave the same with an amplifier and each mode's learned level is kept. `tx_level.c` / `tx_player.c` are small (~160 lines), have no JS8 in them, and are already planned to come in with JS8 4/4. I could also send them earlier as a PR of their own, so your apps could switch to them now.

What do you think? Happy to compare readings on my bench (MFJ wattmeter, dummy load, XPA125B) if that helps.

73, Randal VE7NHW
