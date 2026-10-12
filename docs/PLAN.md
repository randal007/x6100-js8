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
- [ ] **Bluetooth keyboards pair every time** (2026-10-11, branch
  `openx6100-bluetooth`): on the beta 5.1 release candidate the window's
  pairing failed 4 times out of 4 with VE7NHW's F01 keyboard. btmon on the
  radio: the keyboard's SMP keys reached Linux 0.3 ms *before* the chip's
  Encryption Change event (events and data come over two USB endpoints,
  and WiFi shares the same USB device), so the kernel dropped them
  ("unexpected SMP command 0x06") and the pairing timed out. A race, not a
  code change: the same error on 10-10 with x6100-bt-pair, a retry
  "worked"; Kris's first-time pairing was luck. With btusb's
  force_poll_sync switched on by hand it paired and bonded first time.
  Three fixes: **A** kernel patch 9002 (poll sync on from probe, the hold
  at least one whole jiffy, the event before the worker), in
  `buildroot/patches/linux/` and copied in by *Build image*; **B** Scan
  takes turns, LE 3 s / classic 5 s twice (the chip hears no LE during an
  inquiry); **C** Connect on a keyboard waits for it by name, LE only (it
  takes a new address at every pairing-key press: D7, D8, ... DD).
  From a read-through of the whole Bluetooth code, four more: **1** a
  keyboard paired at a new address makes the window forget its old
  pairings under the same name (Connect on one hung a minute); **2**
  Connect during Scan ends the Scan at once; **3** the window's highlight
  follows the device, not the row number (the list re-sorts during a
  Scan); **4** WiFi off/on is noticed (a new hci0, not only a missing
  one) and Bluetooth set up again without a power cycle, and bt_down.sh
  is never run against a chip that lost power.
  Reported, not fixed by us (OpenX6100/x6100_gui#292): CAT over
  Bluetooth needs no pairing; visible and pairable to anyone from boot
  until the window is opened; the headset PTT button is accepted from any
  device. **Pull requests opened 2026-10-11:** AetherX6100Buildroot#5
  (A, the kernel patch) and x6100_gui#291 (B, C, 1-4, from branch
  bt-keyboard-pairing in OpenX6100's repo); both say the radio test of
  the image is still to come. Test image: *Build image* run 38205360614.
  **Next:** test image, radio test (pair several times with WiFi on the
  home network; headset and phone still found by Scan; keyboard still
  reconnects after power off/on), then pull requests to OpenX6100:
  AetherX6100Buildroot (A, with the btmon evidence) and x6100_gui (B + C,
  Kris tagged). Also for Kris: switching Bluetooth off and on with WiFi
  running leaves the chip deaf to LE during inquiry until a reboot.
- [ ] **Beta 5.1 (the Bluetooth window + the compound-call fix), on
  branch `openx6100-bluetooth`** (2026-10-11): OpenX6100's main merged in
  (Bluetooth window, FT8 country, WSJT-X UDP, Decoder text, SSB fix), APP
  page 3 = WiFi, Bluetooth, (free), JS8 last; one cty.dat (Big CTY,
  /usr/share/x6100/cty.dat, /mnt/cty.dat wins) for FT8 and JS8; our gpsd /
  uhid / pairings-backup build steps and `x6100-bt-pair` removed (all in
  OpenX6100's Buildroot now); version string, README and
  docs/releases/js8-beta5.1.md ready. **First test image had a bug from
  the merge:** a lost `break` made JS8's button open Bluetooth on top of a
  still-running JS8 (orphaned JS8: screen glitches, JS8 wouldn't reopen,
  typing didn't work). Fixed in f00dd74; a merge review found nothing
  else. **Next:** flash image run 38188374332 (the release candidate),
  check the full harness run 38189126031, radio test (Bluetooth window
  pairing + typing in Callsign and JS8, JS8 RX / TX / beeps, FT8), then
  PR into main, tag `js8-beta5.1`, release (docs/releases/CHECKLIST.md),
  a "please use beta 5.1" banner on beta 5's page, Facebook post. If
  typing still fails on the fixed build: the keyboard does send keys
  (event5, hid-apple driver after a hid-generic swap) and the GUI has it
  open: dig into keyboard.c / the group focus with the console.
- [x] **Compound callsigns (KG4UHM/6) packed and read as desktop**
  (2026-10-11, a3553bc, js8core local patch 16): js8core's pack_cmd kept
  2 bits of the SNR and shifted every command, so "SNR -03" went out as
  -31, our queries arrived as CMD / unknown, and desktop's heartbeat ACKs
  showed as "HW CPY?", "FB" or "RR" (the auto-reply answered a question
  never asked). Tests against desktop's own buildMessageFrames() built
  with Qt, both ways. **Still to do:** report it upstream to
  JS8Call-improved (docs/js8core-bug-reports.md #11), as #104 was; retest
  on the air with a "/number" station.
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
  licence question waits on gdyuldin in discussion 23. **1/4 opened
  2026-10-11: OpenX6100/x6100_gui#290**, approved by myshak1 (his
  review: test_js8_db to expect the last migration, since FT2 #288 adds
  migration 5: done in 8479839, JS8's stored numbers written out too);
  tested on VE7NHW's radio (FT8 QSO logged right); waiting for gdyuldin.
  #289 (ADIF SSB-written-as-AM) merged the same day. Next: 2/4 once #290 is merged and gdyuldin
  has answered the licence question. 2/4 carries local patch 16 (the
  compound-call fix).
- [ ] **OpenX6100 reviews:** approved #288 (FT2) and #274 (Olivia);
  on #271 (WSPR) asked for two fixes (the beacon period survives closing
  the window, so reopening resumes beaconing; the worker thread's
  `thread_running` race on a quick close). Approve when Kris pushes them. Before 4/4: a 4th APP
  page agreed with myshak1, and one cty.dat (JS8 reads #282's).

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
