# Reply on OpenX6100/x6100_gui discussion 283 (app IDs)

Posted 2026-10-11 as VE7NHW: https://github.com/OpenX6100/x6100_gui/discussions/283#discussioncomment-18864754
after gdyuldin suggested leaving apps out of the long-press list and
myshak1 did that for WSPR, PSK31, Olivia and Bluetooth (113-116).

---

Thanks Georgy, and thanks Kris for sorting the numbers out so quickly.

That makes sense to me. I've taken JS8 out of the long-press list too, so it opens from the APP menu only and its `press_action_t` number is never saved on the card. When JS8 comes in as a PR it'll just go at the end of the list, no fixed value needed. (Kris, the comment in your `settings_types.h` lists 112 for JS8: fine to leave or drop, it won't be stored either way.)

That leaves two values JS8 does save on the card, and I'd like to keep them as they are so testers' logs and settings carry over:
- `qso_log_mode_t`: `MODE_JS8` = 8, right after `MODE_RTTY` = 7. It's stored with every JS8 QSO in qso_log.db.
- `cfg_digital_type_t`: `CFG_DIG_TYPE_JS8` = 2 and `CFG_DIG_TYPE_JS8_GHOSTNET` = 3, after FT8 and FT4. They're stored in the digital_modes table.

If anyone adds a new log mode or digital type before the JS8 PRs land, could they start at 9 and 4? Then nothing clashes.

73, Randal VE7NHW
