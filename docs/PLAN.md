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

**In short** (updated at each offload; details in the items below):
1. Card: test image 3385886 (run 38478388820: Bluetooth incl. udev fix af4c2ae + FT8 knob fix, no FT2), flashed 2026-10-13, backup sd-backup-2026-10-13-162408. To check: the FT8 knob (#300), a phone connecting during Scan leaves the radio visible.
2. Bluetooth security: radio retest passed 2026-10-13 (plus udev fix af4c2ae); #297 + Buildroot #6 wait for Kris to merge.
3. JS8 into OpenX6100: 1/4 (#290) waits for gdyuldin; licence settled.
4. Workflow clean-up: one status list, PLAN_DONE archive, buildroot_ref (done 2026-10-13); CLAUDE.local.md backup place to decide.
5. **First next session: prepare JS8 part 2/4 (`js8-engine`)**: our #290 nudge (2026-10-13) said it's ready to open once #290 merges, but the branch doesn't exist yet.
6. Waiting on others: #290 (gdyuldin, nudged 2026-10-13), #300 FT8 knob, Android-port#108 (js8core), discussion #299 answered. Unexplained TX at power-up: FT8CN suspected.

- [ ] **Workflow clean-up (VE7NHW agreed 2026-10-13, from Claude's
  feedback).** Working style: shorter sessions, one topic each, offload
  at the end of each.
  - [x] **One source of truth:** PLAN.md is the only "where are we"
    document. Claude's memory keeps lessons and pointers only (no copy
    of the status); README *Coming next* stays a short mirror.
  - [x] **Trim PLAN.md:** ticked items move to
    [PLAN_DONE.md](PLAN_DONE.md) at each offload; the summary above.
    Both in CLAUDE.md's offload checklist.
  - [x] **Build image has a `buildroot_ref` input** (default master; a
    release tag refuses anything else): a test image with an open
    Buildroot PR is `-f buildroot_ref=<branch>`, no TEST-ONLY commit.
  - [ ] **Back up `CLAUDE.local.md`** (not in git, only on this PC). **Not
    on GitHub, not even a gist** (VE7NHW, 2026-10-13): it has the radio's
    login, the card serial and Bluetooth address. Decide where together:
    options are a copy on one of the PC's other disks (two 5.5 TB drives
    sda/sdc are attached), a USB stick, or alongside sd-backups/.
- [ ] **Compound callsigns, leftovers** (the fix is in beta 5.1, see
  [PLAN_DONE.md](PLAN_DONE.md)). **Sent upstream 2026-10-13 (VE7NHW asked):
  JS8Call-improved/Android-port#108** (from randal007/Android-port,
  branch compound-cmd-desktop): the compound fix, Ultra on Turbo's
  schedule (patch 14) and the GCC includes (patch 1); the rest of our
  engine bug fixes were already in their master (#105); our other local
  patches are features, not sent. Not built in their tree (Claude may not
  build downloaded code here); the new checks pass against our copy.
  **Next:** their review; retest on the air with a "/number" station.
- [ ] **JS8 into OpenX6100**, four pull requests one at a time
  ([upstream-patches.md](upstream-patches.md#js8-into-openx6100-the-plan)):
  groundwork, engine (with the licence note), library + tests, app.
  **1/4 opened
  2026-10-11: OpenX6100/x6100_gui#290**, approved by myshak1 (his
  review: test_js8_db to expect the last migration, since FT2 #288 adds
  migration 5: done in 8479839, JS8's stored numbers written out too);
  tested on VE7NHW's radio (FT8 QSO logged right); waiting for gdyuldin.
  #289 (ADIF SSB-written-as-AM) merged the same day. **Licence settled 2026-10-13 (D#23):** gdyuldin agreed: every existing
  file stays LGPL-2.1-or-later, `third-party/js8core` with its own GPLv3
  LICENSE, an image with JS8 is GPLv3 as a whole, **JS8 in the default
  build**. He asks for a README / build-docs note on the image's licence
  and where its corresponding source is: **goes in 2/4** (our reply said
  so). The add-on idea is off. **Nudged on #290 2026-10-13 22:4xZ** (asked gdyuldin if anything is left; said 2/4 is ready to follow). **Next:** prepare 2/4 now, before #290 merges (js8-engine, with that note
  and local patch 16) once #290 is merged; 3/4 will carry
  JS8_RX_TEXT_LEN 640 (typing-lag).
- [ ] **Bluetooth security, #292** (shared GUI code + Buildroot, not JS8).
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
- [ ] **OpenX6100 reviews:** approved #288 (FT2), #274 (Olivia), #271
  (WSPR, 2026-10-12, after Kris's two fixes), #296 (headset PTT,
  2026-10-13). #293 and #294 merged by gdyuldin 2026-10-12 (he answered
  the lv_async_call question; **our #294 question, is TX refused below
  500 kHz (630 m / 2200 m), still unanswered**). #295 (repo links):
  changes requested by us, no answer yet. #284-#286 closed by Kris (in
  #271/#272/#274). Not reviewed by us: #287 (world map). #253 (FT8 CQ
  modifier broken, not ours): gdyuldin asked the reporter for params.db;
  VE7NHW: leave it to others. Before 4/4: a 4th APP page agreed with
  myshak1, and one cty.dat (JS8 reads #282's).
- [ ] **Idea, to rethink: phone digital modes (FT8CN) with audio over
  Bluetooth** (VE7NHW, 2026-10-13; shared GUI code, Kris's bt_audio.c,
  not JS8). The X6200 does it (two-way audio + CAT with FT8CN). Tried by
  hand on the console with VE7NHW's S24 Ultra, then put back:
  - The phone connects for media only (A2DP: its music *into* the radio).
    The radio does offer the HSP headset role; `pactl set-card-profile
    <card> headset_audio_gateway` opened the call link (8 kHz mono). The
    radio can't switch back to a2dp_source itself (the phone must).
  - module-bluetooth-policy's automatic loopbacks went phone -> phone,
    since module-switch-on-connect made the phone the default sink *and*
    source. Hand-made loopbacks (alsa_input -> bluez_sink, bluez_source
    -> alsa_output) fixed the routing.
  - **RX:** audio reached the phone, weak even at +18 dB, and FT8CN showed
    **no FT8 signals** (suspect: the phone's call processing, noise
    suppression on the SCO microphone path; not confirmed).
  - **TX:** no audio reached the transmitter: CAT PTT (CI-V 0x1C) calls
    `radio_set_ptt()`, which keys from the radio's mic/line input, not
    the codec; it would need `radio_set_modem()` while a phone audio link
    is up, as Kris's headset PTT does.
  - Also seen: the radio's SDP list has the Serial Port record ~8 times
    (bt_start.sh's `sdptool add SP` runs on every hci0 add); tell Kris.
  Before trying again: find out how the X6200 does it, and whether FT8CN
  can take its audio without the phone's call processing. Or WiFi audio.
  **Asked Kris 2026-10-13: OpenX6100/x6100_gui discussion #299** (Ideas).
  Kris (same day): not his plan (CAT over Bluetooth is gdyuldin's; his
  window is for HamLogBridge, WSJT-X UDP to online logs); TX looks easy
  (route the phone's A2DP to the codec, `radio_set_modem()` on CAT PTT
  while a phone audio link is up; he'd review); RX is the open question
  (Android's call processing on SCO; A2DP and SCO may fight); the X6200
  guide says Call Audio off, and its author saw RX go over the air, not
  Bluetooth; a btmon from an X6200 owner would settle it; fallback: WiFi
  with Icom's network protocol (FT8CN speaks it, IC-705), a big job.

- [ ] **Unexplained transmit at power-up (2026-10-13, VE7NHW)**: first
  boot of bt-ft2-test d94d92b (21:37 UTC): a waterfall "glitch" a couple
  of seconds after start, then TX (red light, zero power) until he
  restarted; not seen on the next boot. The GUI did not crash (no
  app_logs file; x6100_daemon saves one on every exit); the app log was
  lost at the reboot; the USB console wasn't connected. Two suspects:
  (1) FT8CN in the background on the phone, still in TX after our audio
  test, reconnected over Bluetooth CAT and keyed PTT (CI-V 0x1C keys
  from the mic input: zero power fits); (2) the FT8 window was opened on
  that boot in FT2 with ft8_auto = 1 (FT2 starts a pending TX before the
  last decode). **Next:** if it happens again, keep the USB console
  connected; consider a temporary boot script copying
  /tmp/x6100_log.txt to /mnt (VE7NHW to decide). Idea for gdyuldin
  (shared CAT code): ignore CAT PTT for a few seconds after start or
  after a Bluetooth CAT client connects; CAT PTT isn't logged at all.

- [ ] **FT8 window: the knob moves the TX frequency 1 Hz a click
  however fast it turns** (VE7NHW, 2026-10-13; shared code, OpenX6100's
  FT8 app). Same cause as JS8's 10fdf95 (1.0 hands over every click on
  its own, so the 5x/10x on a single diff never triggers). **PR
  OpenX6100/x6100_gui#300** (branch `ft8-knob-speed`, counts clicks per
  30 ms with lv_tick; reviewers gdyuldin + myshak1). Also in test image
  run 38478388820. **Next:** try it on the radio, report on #300.
- [ ] **Start-up glitch, old** (VE7NHW, 2026-10-13: seen on earlier
  versions too, "sometimes"): right after boot no waterfall and the
  S-meter at full scale, then relays click and it comes right. Guess: a
  race between the GUI's first settings and the base board starting
  (lost settings, re-sent later). Nothing in the app log, **but dmesg
  shows `i2c i2c-0: sendbytes: NAK bailout` / `error -110` each time the
  GUI starts** (26 s after boot, and again after a GUI restart): a write
  to the base board not acknowledged. Strong lead. **Next, in its
  own session:** time the GUI's first base-board messages over several
  boots; then an OpenX6100 issue for gdyuldin. Shared code, not JS8.

- [ ] **GUI crash while spinning the main dial fast** (VE7NHW,
  2026-10-13, "a couple of times"): /mnt/app_logs/x6100_log_2026-10-13_
  23-31-44.txt ends mid-line 64 s after start in a run of
  `on_vfo_freq_change` (14.084-14.106 MHz, a step every ~40 ms), no error
  printed (a hard crash, SIGSEGV-like); x6100_daemon restarted the GUI,
  which power-cycles the WiFi/BT chip (USB errors at the same second) and
  re-inits the base board. **The 2026-09-27 log ends the same way**
  (fast tuning around 14.09 MHz, old 0.34-based build): an old bug in
  shared GUI code, not ours. **Next, in its own session:** enable core
  dumps on the radio for a while (`ulimit -c unlimited` + a core_pattern
  on /mnt in x6100_daemon, temporary), reproduce by spinning the dial,
  read the backtrace on the PC against that build's unstripped binary;
  then an OpenX6100 issue/PR.

## To try on the radio (no programming)

- [ ] Ultra on a busy band. Busy bands fine on Fast (VE7NHW, 2026-10-12);
  Ultra needs a second Ultra station: try the two-radio bench (HL2 +
  desktop JS8Call).
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
