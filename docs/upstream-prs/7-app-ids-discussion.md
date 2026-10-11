# OpenX6100 discussion on app IDs

**Posted 2026-10-11 as https://github.com/OpenX6100/x6100_gui/discussions/283** (General; JS8 → 112 as proposed, VE7NHW sends the link to the Murus team on Facebook). Was:
General). Title:

**Numbers for new apps (and other values saved on the card), so our PRs don't clash**

---

Hi @gdyuldin, @myshak1, and the Murus / 1KO125 team,

Several new apps are on their way into x6100_gui: WSPR, PSK31, Olivia and
the Bluetooth window from Kris, WeFax, NavTex and SSTV from the Murus
team, and JS8 from me. Each app gets an `ACTION_APP_*` number in
`press_action_t` (`src/settings_types.h`), and that number is **saved on
the card** when someone puts an app on a key's long press. If two apps
share a number, a card from one build opens the wrong app in another, and
the PRs conflict with each other as they're merged.

**What's in use today**

| Number | Upstream main | Murus 1KO125 (betas 6-8, 1.0.2 beta 1) | X6100 JS8 (randal007) | Open PRs (myshak1) |
|---|---|---|---|---|
| 100-108 | RTTY … WiFi | same | same | same |
| 109 | — | WeFax | — | WSPR #271, PSK31 #272, Olivia #274, Bluetooth #280 (each appended after WiFi) |
| 110 | — | NavTex | — | |
| 111 | — | SSTV | JS8 | |

**A proposal**

| Number | App | Why |
|---|---|---|
| 109 | WeFax | Murus, in their releases the longest |
| 110 | NavTex | Murus |
| 111 | SSTV | Murus, since their beta 7 |
| 112 | JS8 | moved from 111; JS8 converts the key bindings on cards from its own betas once (it can tell them apart), so nobody else has to do anything |
| 113 | WSPR | #271 |
| 114 | PSK31 | #272 |
| 115 | Olivia | #274 |
| 116 | Bluetooth | #280 |

And a rule for after that: each app's number written out in the enum
(`ACTION_APP_WSPR = 113,`), a comment that the values are saved on cards,
new apps always take the next free number, and a number is never reused
or changed. Anyone opening a PR with a new app claims the next number in
this thread first.

**Two more lists saved on the card**, for the same reason:

- `qso_log_mode_t` (the mode stored with every QSO in `qso_log.db`):
  upstream ends at `MODE_RTTY` = 7. JS8 uses `MODE_JS8` = 8. New modes
  (PSK31, Olivia, ...) would take 9, 10, … in the same way.
- `cfg_digital_type_t` (the digital-mode frequency lists): upstream has FT8
  = 0 and FT4 = 1. JS8 uses `CFG_DIG_TYPE_JS8` = 2 and
  `CFG_DIG_TYPE_JS8_GHOSTNET` = 3 (JS8Call's preset frequencies and the
  GhostNet ones).

Database migrations aren't a problem: JS8 keeps its own records in its own
table and doesn't use upstream's migration numbers.

If this looks right (or with other numbers you prefer), I'll use them in
the JS8 PRs, and Kris can set his in his four PRs. The Murus team: does
this match your plans for WeFax, NavTex and SSTV on 1.0.2?

Thanks,
Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)
