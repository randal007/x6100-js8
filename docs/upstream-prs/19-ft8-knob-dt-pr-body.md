Turning the main knob to move the FT8/FT4 TX frequency crawls at 1 Hz a click, however fast you spin it.

`rotary_cb()` speeds up (5 Hz / 10 Hz a click) when a single `diff` is over 3. That worked while the knob was read every 30 ms and handed over as the sum of its clicks; now every click arrives on its own (`diff` = ±1), so the speed-up never triggers.

**Reworked as Georgy suggested:** the apps' rotary callback now gets the knob's `dt` (ms per click, from `rotary.cpp`), the same value `freq_shift()` uses on the main screen; the `// TODO: add dt support` in `main_screen_rotary_cb()` goes. FT8 speeds up from it: under 8 ms a click 5 Hz, 5 ms or less 10 Hz (the old 4-5 / 6+ clicks per 30 ms); `dt` 0 (unknown) stays 1 Hz.

- `dialog.h` / `dialog.c`: `dialog_rotary_cb_t` and `dialog_rotary()` take `uint16_t dt`.
- `main_screen.c`: passes `data->dt`.
- `dialog_ft8.c`: the only window with a rotary callback today.

Compiles with the image's GCC 12. Not yet tried on the radio: it goes into my next test image, I'll report here.

Side note, not changed here: `set_freq()` invalidates the whole finder (the whole waterfall) on every click; in JS8 invalidating only the band's old and new strip took a fast spin from about 39 to 9 ms/s of GUI time.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_017kh9JtieivDQgN5JvFUSZr
