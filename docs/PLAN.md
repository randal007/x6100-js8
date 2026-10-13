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
- [x] **Bluetooth keyboards pair every time** (2026-10-11, branch
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
  **Radio test 2026-10-12: works** (flashed c2eec93, backup
  sd-backup-2026-10-12-000900): 5-6 pairings in a row with WiFi on,
  typing, Connect during Scan, reconnect after restart without opening the
  window, WiFi off/on; results posted on #291 and Buildroot #5. (Seen then:
  Connect on a keyboard's old pairing tied the worker up ~1 min; closed
  2026-10-12, see the Bluetooth security item.) **Next:** reviews of #291 / Buildroot #5; then
  beta 5.1 (below) with this in. **Released in beta 5.1** (2026-10-12).
  #291 and Buildroot #5 approved by gdyuldin; **Buildroot #5 merged**
  2026-10-12 (our copy of patch 9002 and its CI step dropped); **#291
  merged** 2026-10-12 (OpenX6100's Bluetooth files now identical to ours).
  Still to do: answers on #292 (Kris answered 2026-10-12: see below).
  **Next:** test image, radio test (pair several times with WiFi on the
  home network; headset and phone still found by Scan; keyboard still
  reconnects after power off/on), then pull requests to OpenX6100:
  AetherX6100Buildroot (A, with the btmon evidence) and x6100_gui (B + C,
  Kris tagged). Also for Kris: switching Bluetooth off and on with WiFi
  running leaves the chip deaf to LE during inquiry until a reboot.
- [x] **Beta 5.1 release, last steps** (done 2026-10-12: titled, notes, prerelease, banner on beta 5, sha256 e7ea6dc2 checked, local copy in releases/js8-beta5.1) (tag `js8-beta5.1` on 02c8682,
  *Build image* run 38218854490 started 2026-10-12 07:38 UTC): once it
  has published, `gh release edit js8-beta5.1 --title "X6100 JS8 beta
  5.1" --notes-file docs/releases/js8-beta5.1.md --prerelease`, a
  "Please use beta 5.1" banner on beta 5's page and in
  docs/releases/js8-beta5.md (beta 5 has the /number bug), check the
  asset's sha256 against GitHub's digest, then tell VE7NHW; the Facebook
  post is ready in ~/Pictures/x6100-js8-beta5.1-facebook/post.txt.
- [x] **Beta 5.1 (the Bluetooth window + the compound-call fix), on
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
  compound-call fix). **gdyuldin answered on D#23 (2026-10-12):** fine
  with GPL v3 himself but would need every author to agree; he suggests
  JS8 as a standalone add-on instead (a .so the user downloads onto the
  SD card and an init.d script installs at boot, or from a separate
  GitHub repo). VE7NHW read it as a misunderstanding (gdyuldin thought
  the whole project would have to become GPL v3). **Reply posted
  2026-10-12** ([text](upstream-prs/12-licence-reply.md),
  discussioncomment-18887731): every GUI file stays LGPL-2.1-or-later,
  only `third-party/js8core` is GPL v3, and only an image built with JS8
  is GPL v3 as a whole (LGPL 2.1 section 3 allows it without asking the
  authors); or JS8 as a build option off by default (`WITH_JS8`, not
  written yet: would go in 2/4). Asked him: always built in, or an
  option? **Waiting for gdyuldin.** Fallback if he still says no: his
  add-on idea (JS8 as one GPL add-on from our repo, a small "load an
  add-on from the card" hook in OpenX6100).
- [ ] **Typing a long message froze the screen** (VE7NHW on the air,
  2026-10-12 ~04:13Z: GUI stall 23 s, scheduler queue overflow; bogs
  past ~8 frames when typing fast). Cause: every key re-planned the whole
  message for the "N frames, N s" count (PC, ASan: 4 ms at 20 chars, 26 ms
  at 140; many times that on the radio). Fix on branch `typing-lag`
  (c344509's twin on bt-security-js8): counted 350 ms after the last key,
  as desktop (100 ms there). Harness KEYS + SAVED pass. Test image (with
  the Bluetooth changes) run 38346620195. Radio test: type a long message
  fast, no freeze, count appears after a pause; then PR into main.
- [ ] **Bluetooth security, #292 points 1 + 2** (2026-10-12; Kris takes
  3, the headset PTT, in bt_audio.c). Branch `bt-security` in both
  OpenX6100 repos: x6100_gui 1d5452e (CAT's RFCOMM socket
  BT_SECURITY_MEDIUM: bonded devices only, shared GUI code) and
  AetherX6100Buildroot 8a58909 (bt_start.sh: hidden, not pairable, no
  bt-agent at boot). Test image with JS8: our branch `bt-security-js8`
  (main + the CAT commit + a TEST-ONLY Buildroot ref in main.yml: never
  merge it), *Build image* run 38305954241. Radio test: FT8CN CAT from
  the phone (already paired; then a fresh pairing via Scan), an unpaired
  device refused, keyboard and headset reconnect, WiFi off/on, radio
  hidden in a phone's scan. Then the two PRs (linked, "Fixes part of
  #292"), release-note line "Bluetooth CAT: pair the phone once in the
  Bluetooth window", delete `bt-security-js8`. `JustWorksRepairing =
  always` (Kris's 9d5af79) **stays on** (VE7NHW, 2026-10-12; Kris had
  suggested the default and our #292 reply said so): pairing is only
  possible during the window's 3-minute Scan now, and it saves the
  "wrong PIN" trap. Say so in the Buildroot PR.
  The two leftovers from the keyboard tests are closed (2026-10-12):
  Bluetooth off/on with WiFi on works now (VE7NHW); Connect on an old
  keyboard entry waiting ~1 min was a pre-#291 entry (forget_old_keyboards
  removes them now), and ~1 min (2 x 30 s CONN_TIMEOUT_MS) for a device
  that's off is normal Bluetooth: nothing to do.
- [ ] **OpenX6100 reviews:** approved #288 (FT2) and #274 (Olivia);
  #271 (WSPR): Kris pushed both fixes asked for (beacon off on close,
  worker always joined), **approved 2026-10-12**. Before 4/4: a 4th APP
  page agreed with myshak1, and one cty.dat (JS8 reads #282's).
  2026-10-12, gdyuldin's three: approved #293 (DNF mark drawn at the auto
  flag; asked why lv_async_call) and #294 (100 kHz lower limit; asked
  whether TX is refused below 500 kHz, 630 m / 2200 m), changes requested
  on #295 (repo links: Buildroot's x6100_gui.mk still says gdyuldin, so
  its sed would match nothing; suggested `[^/]*`, which our own CI uses
  since bf39b8c; plus leftover links). Not reviewed by us: #287 (world
  map, YunongDai2005) and Kris's #284-#286 (#285/#286 say "apply with"
  themselves: ask which PR they mean).

## To try on the radio (no programming)

- [x] GPS outside: JS8's grid from the fix, the top line, *Battery clock
  set from GPS time* once. **Works perfectly** (VE7NHW, 2026-10-12).
- [ ] Ultra on a busy band. Busy bands fine on Fast (VE7NHW, 2026-10-12);
  Ultra needs a second Ultra station: try the two-radio bench (HL2 +
  desktop JS8Call).
- [ ] NR held off while JS8 is open: turn NR on over CAT (radio on the
  PC's USB) and see it stay off.
- [ ] POTA / SOTA spots through the gateways, when out portable anyway.
- [ ] Screenshots for the beta 5 page and the Facebook post: `GPS` in the
  top line, an Ultra row.
- [x] Pictures for the beta 5.1 Facebook post: not needed, VE7NHW had
  some (2026-10-12).

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
