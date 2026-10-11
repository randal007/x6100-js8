# Plan: after beta 5

**The one work list** (started 2026-10-11, after beta 5). Beta 5's list,
with how each item was done or decided, is [BETA5_PLAN.md](BETA5_PLAN.md).
The README's *Coming next* mirrors the top of this.

**How we work** (agreed 2026-10-11): small fixes and docs straight on
`main`; anything bigger on its own short-lived branch (a name without `/`,
e.g. `feature-cw-input`, so *Build image* can name its artifact), a test
image from it, into `main` by a pull request once it works on the radio,
then the branch is deleted. One or two such branches at a time at most.
Releases are tags on `main`. For OpenX6100: branch → pull request → review
by gdyuldin or myshak1, never merged by us alone.

## Now

- [x] **Kris's Bluetooth window** (OpenX6100/x6100_gui#280 +
  AetherX6100Buildroot#4): test with VE7NHW's LE keyboard and report on
  #280 with `/mnt/bt_pair.log`. Test image: *Build test image* run
  38106197476 (his branches merged with OpenX6100 main; no JS8 in it, so
  flash our build back after). To try: forget the old pairing (hold
  Forget), Scan, the keyboard listed as `[kbd]`, Connect after its pairing
  key, the passkey if asked, types in Callsign and FT8, hidden after 3
  minutes (a phone can't see the radio), still paired after a normal
  power-off and on. **Done 2026-10-11: everything worked first try**
  (paired in 9 s, hidden after 3 min, saved at power-off and reconnected);
  report with the log posted on #280. Still to do: **when Buildroot#4 is
  merged, drop our pairings-backup step from *Build image*.**
- [ ] **Kris's Bluetooth window into our build:** wait until he merges
  #280 (and AetherX6100Buildroot#4) in OpenX6100 (both approved by
  gdyuldin and VE7NHW 2026-10-11), then take it with
  OpenX6100's code (VE7NHW 2026-10-11: "do it proper"). A trial merge had
  three small conflicts (JS8's and Bluetooth's app button in
  settings_types.h, buttons.cpp, main_screen.c: keep both, Bluetooth
  number 116 as proposed in #283); it also brings headset audio, the
  phone serial link, FT8 over UDP and changes to the shared audio and
  volume code, so test JS8's receive, transmit and beeps on the radio
  with it. Then drop our pairings-backup step and `x6100-bt-pair`.
- [x] **App numbers** (OpenX6100/x6100_gui discussion 283): gdyuldin
  suggested leaving apps out of the long-press list so their numbers are
  never stored; myshak1 did that for his four apps. **Done 2026-10-11:**
  JS8 is no longer a long-press action (opens from the APP menu only),
  so there's no number to change and nothing to convert. A tester whose
  key was set to JS8 finds it does nothing: say so in the next release
  notes. Asked on #283 the same day (reply posted): JS8's two other
  stored numbers (`MODE_JS8` = 8, `CFG_DIG_TYPE_JS8` 2 / 3) stay as they
  are, so new log modes start at 9 and digital types at 4.
- [ ] **JS8 into OpenX6100**, four pull requests one at a time
  ([upstream-patches.md](upstream-patches.md#js8-into-openx6100-the-plan)):
  groundwork, engine (with the licence note), library + tests, app. The
  licence question waits on gdyuldin in discussion 23.

## To try on the radio (no programming)

- [ ] GPS outside: JS8's grid from the fix, the top line, *Battery clock
  set from GPS time* once.
- [ ] Ultra on a busy band.
- [ ] NR held off while JS8 is open: turn NR on over CAT (radio on the
  PC's USB) and see it stay off.
- [ ] POTA / SOTA spots through the gateways, when out portable anyway.
- [ ] Screenshots for the beta 5 page and the Facebook post: `GPS` in the
  top line, an Ultra row.

## Decided: not doing

So they don't come back up (VE7NHW, 2026-10-11):

- **CW input for JS8's text boxes** (Discussion #2, OE3WYC): out of scope,
  a lot of work for few users.
- **Show's blank rows:** seen once, never reproduced; look again only if
  someone sends screenshots.
- **The map's home colour Setting:** it stays orange.
- **Merging the Murus apps into this fork:** they come through OpenX6100.
- **Review leftovers** I-02, I-04 (transmit buffers, planning twice),
  I-10 / B-11 (re-decode after a stall), I-22 (WAV test mode, warnings):
  work fine; only if a real problem shows up.
- **Performance work** (a decoder thread per speed): measured, nothing
  worth it.
- **Hiding the dim decode marks:** nobody asked.
- **FT8's waterfall on the lower layer:** an idea for OpenX6100, not ours.
- **Map polish "from use":** too vague; new map wishes become their own
  items.
