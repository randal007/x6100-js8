# Plan: done

Items ticked in [PLAN.md](PLAN.md), moved here at each offload so the work
list stays short. Newest at the top. Beta 5's list is
[BETA5_PLAN.md](BETA5_PLAN.md).

## Moved 2026-10-14

- [x] **Ultra decoded once a slot, as desktop (2026-10-14; engine, Ultra
  only): PR #10, merged into main 2026-10-14 (VE7NHW: go with what was
  agreed upstream), branch deleted.** punk-kaos (Android-port's
  developer) on #108: our patch 14 made Ultra's first decode 2.528 s into
  the slot, before the signal can be complete (2.628 s). He was right:
  desktop (`turboOrUltra`) decodes Turbo and Ultra once at 79 symbols +
  start delay + 0.5 s (Ultra 3.128 s); patch 14 came from misreading it.
  Measured with generated signals (scratch experiment, 20 seeds a point):
  the early try decodes on-time signals but misses weak or late ones,
  which the retry finds 0.4 s after desktop; Turbo (his design) holds up
  the same way, sent to him as information only. Patch 14 withdrawn
  (UPSTREAM.md, bug list), reverted on #108 (5a108cf4, title/body updated,
  reply with the numbers). Unit tests pass (fast + speed sets). Image run 38513350311 (with it) on the card
  2026-10-14; the bench check moved to "To try on the radio".
- [x] **Code review of everything since beta 5.1 (2026-10-14, VE7NHW
  asked, after the recent crashes).** JS8 app (typing count thread,
  20-frame text size, held log fields), the test build's shared code
  (Bluetooth CAT, Bluetooth window, FT8 knob), Buildroot bt-security
  scripts, workflows: nothing that could crash. Our shared changes are
  off the main-dial path (the main waterfall doesn't use lv_waterfall),
  so the dial crash stays an OpenX6100 item. Two fixes: fe81d2a (an
  encoder exception refuses the message instead of ending the GUI) on
  main; fe3f68a on #297 (Bluetooth CAT off, not open to all, if
  BT_SECURITY can't be set; comment posted). Noted: our main lacks 4
  OpenX6100 commits (9556ce3 DNF drawn at the right place among them),
  for the next upstream merge. Unit tests here need the scratch-copy
  trick: this PC's GCC 16 has no static libasan (tests/CMakeLists.txt
  links -static-libasan; CI is fine).
- [x] **Beta 5.2 (2026-10-14, VE7NHW asked):** main after merging
  OpenX6100 main (eb8589d: Bluetooth security #297 + Buildroot #6 merged
  by VE7NHW 2026-10-14, headset PTT #296, 100 kHz, DNF) + typing-lag,
  20 frames, Log fields kept, encoder fix, Ultra once a slot. Not in it:
  the FT8 knob (#300, Georgy wants a rework). Notes
  docs/releases/js8-beta5.2.md; README updated. **Released 2026-10-14:**
  tag js8-beta5.2 (9cc80c7), *Build image* run 38617078227 on Buildroot
  master; page titled, notes, prerelease; sha256 f1209d01... checked
  (copy in releases/js8-beta5.2/); "please use 5.2" banner on 5.1.
  **Next:** Facebook post if VE7NHW wants one (beta 5 template); keep
  `bt-security-js8` and `wspr-test` (VE7NHW: WSPR build goes on the radio
  next; wspr-test is based on bt-security-js8); VE7NHW deletes the merged OpenX6100 branches
  (bt-security in both repos, js8-groundwork, adif-ssb-fix,
  bt-keyboard-pairing; Claude's auto mode can't).
- [x] **Bluetooth security, #292** (shared GUI code + Buildroot, not JS8).
  **Done 2026-10-14: #297 and Buildroot #6 merged by VE7NHW** (both
  approved), in beta 5.2.
  Kris did point 3 (headset PTT only from the headset's own link):
  **x6100_gui#296, approved by VE7NHW 2026-10-13**. Ours, points 1 + 2,
  **opened 2026-10-13: x6100_gui #297** (CAT RFCOMM BT_SECURITY_MEDIUM,
  branch `bt-security`, 1d5452e) and **AetherX6100Buildroot #6**
  (bt_start.sh hidden, not pairable, no bt-agent; branch `bt-security`,
  8a58909), reviewers gdyuldin + myshak1. `JustWorksRepairing = always`
  kept (VE7NHW; explained in #6). **Tested:** keyboard pair, forget,
  re-pair, reconnect; phone + FT8CN over Bluetooth CAT changing frequency
  with the keyboard connected; from the unpaired PC the radio is
  invisible to a scan and CAT gets "Connection refused". **Reviews
  2026-10-13:** #297 approved by myshak1; Buildroot #6 *changes
  requested* by myshak1 (he'd merge both together): (a)
  `JustWorksRepairing` back to the default (he missed our reason;
  `confirm` instead, so the window's agent decides, yes only during
  Scan: VE7NHW chose it); (b) radios that ran the old
  bt_start.sh keep "discoverable" in /var/lib/bluetooth and the
  `discoverable off` can run before bluetoothd has the adapter: retry and
  verify as bt_up.sh does, plus a non-zero DiscoverableTimeout /
  PairableTimeout as a safety net (the window sets its own timeout for
  Scan); (c) stale comments in bt_start.sh and #297's bt_ctl.c.
  **All three done 2026-10-13** (Buildroot d92cb37, x6100_gui 8fc9068:
  also PairableTimeout set on every Scan, since bluez restarts that timer
  only then); replies posted on #6 and #297. Test branch
  `bt-security-js8` rebuilt as typing-lag + main + #297 (b0d6183, no
  TEST-ONLY commit any more). **Kris approved #6 2026-10-13 19:57Z**
  ("approving both once your retest is in", he merges #6 + #297
  together); his non-blocking leftovers done in Buildroot 9a70644 (CAT
  Serial Port record added only once, stale udev remove rule dropped;
  the 20 s udev hold left as is). Test image: *Build image* run
  **38462204666** (`buildroot_ref=bt-security`; the earlier run
  38448410652 lacks 9a70644). **Radio retest 2026-10-13 passed** (on
  bt-ft2-test d94d92b): hidden at boot, keyboard, confirm re-pair
  refused outside / accepted inside Scan, phone finds and pairs, Scan
  twice, closes on time, FT8CN CAT, WiFi off/on, one Serial Port record.
  **Found during it, fixed in Buildroot af4c2ae:** the udev rule
  `KERNEL=="hci*"` also matched every connection (`hci0:<handle>`,
  DEVTYPE link), so bt_start.sh ran on each connect: it hid the radio a
  second after a phone connected during Scan (before #6: made it visible
  with a new bt-agent) and added a Serial Port record each time (the
  duplicates). Now `ENV{DEVTYPE}=="host"`; tried in place on the radio.
  Results posted on #6 and #297. New test image with af4c2ae built in
  (and the FT8 knob fix, no FT2): *Build image* run **38478388820**
  (bt-security-js8 3385886, `buildroot_ref=bt-security`), **on the card
  2026-10-13** (backup sd-backup-2026-10-13-162408). **Next:** check a
  phone connecting during Scan leaves the radio visible;
  Kris merges #6 + #297; when both are merged, delete `bt-security-js8`
  and add
  the release-note line "Bluetooth CAT: pair the phone once in the
  Bluetooth window". Phone apps: FT8TW (danleetw/FT8TW, on Google Play)
  is the FT8CN fork most use; FT8AF is newer, dev builds only.

## Moved 2026-10-13

- [x] **JS8 typing and logging, branch `typing-lag`** (**merged into main 2026-10-13, PR #9 = 08fc7f6; branch deleted**; 5 commits; all JS8 app / library, nothing shared). Radio-tested
  by VE7NHW 2026-10-13 on test image 61831d2 unless noted:
  - **Typing a long message froze the screen** (on the air 2026-10-12
    ~04:13Z: GUI stall 23 s; bogs past ~8 frames typing fast). Every key
    re-planned the whole message for the "N frames, N s" count. f192baf:
    counted 350 ms after the last key, as desktop (100 ms there).
    **Works: pretty smooth.**
  - **Messages up to 20 frames, as desktop** (the box stopped at 160
    characters, ~10 frames). 35f34de: box 500 characters (20 frames carry
    ~345-425) and JS8_RX_TEXT_LEN 256 -> 640 (received long messages,
    preview, TX bar, History, Inbox were all cut at 255). Unit test +
    harness. **Works**, but the count on a pause near 20 frames stopped the
    waterfall 1-2 s.
  - -> ab567f1: **the count on its own thread** (js8-count, nice 10, off
    the GUI's core like the decoders, newest request wins). Harness KEYS
    SAVED COMPOSE LOGHOLD pass. **Not radio-tested yet:** test image
    *Build image* run **38428904715** (branch bt-security-js8, fb8769c);
    check: pause near 20 frames, the waterfall keeps running; decodes
    unaffected while counting.
  - **Log fields held during a QSO** (bdaa212): Name, Note, a typed Grid
    kept per station when the Log popup is cancelled; the Log at 73 opens
    with them; forgotten on Save or 30 min after the QSO; memory only.
    Cancel no longer clears another station's ended-QSO reminder. Harness
    LOGHOLD; reviewed by a second session. **Works.**
  **On the card 2026-10-13** (fb8769c, backup sd-backup-2026-10-13-113101).
  **Radio-tested 2026-10-13: works perfectly** (VE7NHW: no waterfall
  stutter at 20 frames; the count fine up to 36-38 frames, where sending
  is refused as "too long", as desktop). **PR #9** opened, unit tests
  pass; merged 2026-10-13. README describes it all (Send…, Logging);
  *Coming next* ticked.
- [x] **FT2 to play with (VE7NHW, 2026-10-13; tried, branch deleted the same day), throwaway branch
  `bt-ft2-test`** = `bt-security-js8` + Kris's FT2 (OpenX6100 #288,
  myshak1:ft2 76d98ce), merge d94d92b (conflicts in digital_modes.h /
  qso_log.h: both numbers kept, JS8 2/3 + 8, FT2 4 + 9). *Build image*
  run **38462549776** (`buildroot_ref=bt-security`, Buildroot 9a70644),
  unit tests run 38462556518 pass. **On the card 2026-10-13** (d94d92b,
  backup sd-backup-2026-10-13-143217): also the Bluetooth retest image. Going back to a build without FT2 is safe: migration 5
  stays recorded (older builds skip versions above theirs), the FT2
  frequency rows are type 4 (ignored by FT8/FT4/JS8), a saved
  `ft8_protocol` 2 is clamped on load. Never merge `bt-ft2-test`; delete
  it once FT2 has been tried (FT2 comes in properly when OpenX6100
  merges #288).

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

- [x] GPS outside: JS8's grid from the fix, the top line, *Battery clock
  set from GPS time* once. **Works perfectly** (VE7NHW, 2026-10-12).

- [x] Pictures for the beta 5.1 Facebook post: not needed, VE7NHW had
  some (2026-10-12).
