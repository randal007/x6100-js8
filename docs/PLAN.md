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
1. Card: **WSPR test image 1de85e5** (run 38606840476: wspr-test = 12d374c + main + Kris's WSPR #271), flashed 2026-10-14, backup sd-backup-2026-10-14-121313. To check: WSPR RX, then a beacon on the dummy load; also still open from 12d374c: the FT8 knob (#300), a phone connecting during Scan leaves the radio visible, Ultra on the bench.
1b. **JS8 close hang** (new 2026-10-14): FFTW measured on every open; plan: saved wisdom + one-time notice (item below). Investigating only, no code yet; asked JS8Call-improved on Android-port#109.
2. Bluetooth security: #297 + Buildroot #6 merged 2026-10-14 (VE7NHW); **beta 5.2** released from main 2026-10-14.
3. JS8 into OpenX6100: 1/4 (#290) merged 2026-10-14; **2/4 = #301** (js8-engine) opened, waits for gdyuldin + myshak1.
4. Workflow clean-up: one status list, PLAN_DONE archive, buildroot_ref (done 2026-10-13); CLAUDE.local.md backup place to decide.
5. JS8 part 2/4 done (#301); 3/4 (`js8-library`) once #301 merges, with JS8_RX_TEXT_LEN 640 and the FFTW wisdom fix if ready.
6. Helping Kris: conflict-fix PRs myshak1/x6100_gui#1-#3 (PSK31, Olivia, FT2). Next test image: `digimodes-test` (5.2 + FFTW wisdom #11 + WSPR d637705 + PSK31 + Olivia + FT8 knob), run 38637209733.
7. Waiting on others: #290 (gdyuldin, nudged 2026-10-13), #300 FT8 knob, Android-port#108 (js8core), discussion #299 answered. Unexplained TX at power-up: FT8CN suspected.

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
- [ ] **JS8 hangs 5-13 s when closed soon after opening: FFTW measured on
  every open (2026-10-14, VE7NHW; JS8 library, not the engine's code).**
  Seen on 12d374c: APP pressed a few seconds after opening JS8, the GUI
  stuck until the first decode pass ended ("decode pass took 12.2 s", then
  13.1 s on the next open; scheduler queue overflow meanwhile; later passes
  0.5-1.5 s). **Cause:** closing joins the decode threads, which stop only
  between passes (engine.cpp ~1282), and the first pass after each open
  builds the decoders (thread_local in legacy_decode, new threads each
  open), whose FFTW plans use FFTW_ESTIMATE_PATIENT: without FFTW_ESTIMATE
  that is a *measuring* planner. Desktop uses the same flag but keeps
  `js8call_wisdom.dat` (fftwf_import_wisdom at start, export at exit,
  mainwindow.cpp 669/1945), so it measures once ever; js8core and the
  Android app keep no wisdom. **Measured on the PC** (scratch, one window,
  all four main speeds): first pass 1.1 s, later 0.05 s; with
  FFTW_ESTIMATE the first pass is 0.09 s; the radio is ~10x slower, which
  fits 12-13 s. Leading silence in the window makes no difference
  (checked, ruled out). No wisdom file on the radio. **Plan (VE7NHW
  2026-10-14, no code yet):** `js8_wisdom.dat` on /mnt, loaded before the
  decoder starts and saved after it stops (.tmp + rename), as desktop.
  When the file is missing (first open of a new build): build the decoders
  up front, not inside the first pass, with a small notice in the JS8
  window ("Setting up the decoder, one time only, about 15 s") until done.
  **The setup measures all five speeds, Ultra included (VE7NHW
  2026-10-14),** whatever is switched on, then frees the ones that are
  off, so the file covers any speed switched on later; normal opens build
  only the speeds switched on (keeps patch 3's memory saving). Messages
  in the first slot or two of that one-time setup can be missed;
  **pressing APP meanwhile must not look like a freeze or crash** (keep
  the notice up while closing, or hide the window at once and finish
  behind it). Mock-up first. Check who else in the GUI program uses FFTW
  (its planner isn't thread-safe). Branch e.g. `fftw-wisdom`. **Also:**
  asked JS8Call-improved what they think: **Android-port#109**
  (punk-kaos 2026-10-14 15:04Z: makes sense in js8core, he had planned it
  but not got to it; so a js8core version could be offered to them)
  (posted 2026-10-14, VE7NHW OK'd the text; "thinking of", not "doing").
  Note: our local patch 3 (decoders built lazily per thread) is what puts
  the measuring inside the first pass.
  **Done by the cloud session: PR #11** (branch
  claude/fftw-wisdom-task-fwoz5u; notice option C picked by VE7NHW;
  reviewed here, nothing wrong). **But:** the radio's second open
  (12.2 s, then 13.1 s) was in the same GUI run, where FFTW already had
  the wisdom in memory (PC: 1.1 s, then 0.07 s): a second cause may exist.
  Test branch `wisdom-wspr-test` (main + #11 + WSPR d637705 + FT8 knob +
  f50ef2c logging every open's first pass); its image run 38633091601 was
  cancelled for **`digimodes-test`** = that + Kris's PSK31 (#272) and
  Olivia (#274), from our conflict fixes, on a new **APP page 4** (page 3
  is full: WiFi, Bluetooth, WSPR, JS8; test-only layout, the real one is
  for Kris), *Build image* run **38637209733**.
  **Next:** flash when VE7NHW says; time the one-time setup, APP during
  it, then reopen twice and read "first decode pass since JS8 opened".
  Earlier: **To a cloud session (VE7NHW 2026-10-14, using the Claude Code cloud
  credits, which expire 2026-11-04):** brief in
  [tasks/fftw-wisdom.md](tasks/fftw-wisdom.md); it opens a PR on branch
  `fftw-wisdom`, then image + radio test here. Then the same credits for a
  deep bug review of JS8 parts 3 and 4 (library + app) before they go to
  OpenX6100. FFTW is used only by the decoder (checked): no other planner
  in the GUI program to clash with.
  Side note: the main decode lane is 25-38 % busy on the radio with a few
  windows waiting (rx_all on, 40 m): a separate question.
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
- [ ] **Compound callsigns, leftovers** (the fix is in beta 5.1, see
  [PLAN_DONE.md](PLAN_DONE.md)). **Sent upstream 2026-10-13 (VE7NHW asked):
  JS8Call-improved/Android-port#108** (from randal007/Android-port,
  branch compound-cmd-desktop): the compound fix, Ultra on Turbo's
  schedule (patch 14) and the GCC includes (patch 1); the rest of our
  engine bug fixes were already in their master (#105); our other local
  patches are features, not sent. **Ultra on Turbo's schedule taken back
  out 2026-10-14** (see the Ultra item). Not built in their tree (Claude may not
  build downloaded code here); the new checks pass against our copy.
  **Next:** their review; retest on the air with a "/number" station.
- [ ] **JS8 into OpenX6100**, four pull requests one at a time
  ([upstream-patches.md](upstream-patches.md#js8-into-openx6100-the-plan)):
  groundwork, engine (with the licence note), library + tests, app.
  **1/4 OpenX6100/x6100_gui#290: merged 2026-10-14 by VE7NHW** (approved by
  both gdyuldin and myshak1; VE7NHW: our own fully approved PR, we merge
  it; Claude's auto mode can't merge in OpenX6100, so VE7NHW presses it).
  **2/4 opened 2026-10-14: OpenX6100/x6100_gui#301** (branch `js8-engine`,
  35cbb7f, reviewers gdyuldin + myshak1): three commits, js8core as
  published (1f8d390a), our patches, build + README *License* (GUI files
  LGPL, js8core GPL-3.0, image GPL-3.0, corresponding source incl.
  Buildroot `legal-info`); engine identical to main and to the radio's
  12d374c; compiles with the image's GCC 12, no warnings. Asked gdyuldin
  who adds `legal-info` to *Build image*. Text in
  [upstream-prs/13-js8-engine-pr.md](upstream-prs/13-js8-engine-pr.md).
  1/4 history: opened 2026-10-11, approved by myshak1 (his
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
- [ ] **Helping Kris: PSK31, Olivia, FT2 conflicts (2026-10-14, VE7NHW
  asked).** #272 and #274 conflicted with main since the Bluetooth window
  (both add an app on APP page 3), #288 since our #290 (JS8 and FT2 in the
  same lists). Resolved each against OpenX6100 main (keep both; the
  Bluetooth case's `break` restored, a plain keep-both drops it; FT2
  numbers as agreed: dig type FT2 4, MODE_FT2 9), syntax-checked with GCC
  12, and offered as PRs **into his own branches**: myshak1/x6100_gui#1
  (psk31), #2 (olivia), #3 (ft2), from OpenX6100 branches
  `psk31-merge-main`, `olivia-merge-main`, `ft2-merge-main` (delete those
  once he has merged or closed them). Told him page 3 only holds four
  apps, so the third of WSPR/PSK31/Olivia (and JS8 in 4/4) needs a 4th
  APP page, his call. WSPR #271 and RTTY #276: approved and clean, waiting
  for Kris to merge.
- [ ] **WSPR test build (2026-10-14, VE7NHW wants to try WSPR):** branch
  `wspr-test` = bt-security-js8 (12d374c, on the card) + main + Kris's
  #271 (91c0292): WSPR in APP page 3's free slot, JS8 stays last. *Build
  image* run **38606840476** (`buildroot_ref=bt-security`), built OK, **on
  the card 2026-10-14** (backup sd-backup-2026-10-14-121313). **Next:**
  **Tried 2026-10-14 (VE7NHW):** RX decodes; a beacon was sent and VE7NHW
  saw it on PSK Reporter. Findings posted on #271 (comment
  6177106854): the red bar is the 200 Hz receive window, not TX (TX is a
  ~6 Hz signal at Center, unmarked); the receive window follows Center
  (from the code: Center 1600 = 1500-1700, misses 1400-1500); suggested
  WSJT-X's way (decode 1400-1600 always, thin TX marker). Waiting for
  Kris. **Kris answered the same evening (#271, 20:22Z): agreed on both,
  changed it as WSJT-X does in d6377057** (decodes 1400-1600 always, edges
  outlined; "TX freq" button/knob moves a thin ~6 Hz red marker; moving it
  no longer rebuilds the receive chain). VE7NHW's beacon reached Perth, AU
  (40 W, amp left on, 33' EFRW). **Next:** a new wspr-test image with
  d6377057 to retest, if VE7NHW wants. Earlier: try it on the radio (he asked: keep the branch until
  then; same JS8/Bluetooth code as 5.2 + WSPR + FT8 knob, says "beta 5.1"
  on the air); try WSPR RX, then a beacon on the dummy load
  (it opens in RX only); report on #271. Delete the branch after.
- [ ] **Murus apps (WeFax, NavTex, SSTV) into OpenX6100, for Hany
  (2026-10-14, VE7NHW).** Hany doesn't use GitHub: he hands VE7NHW a
  tarball once an app is done, we make the PRs (Hany as author, VE7NHW
  co-author), one per app, WeFax first. **Discussion opened:
  OpenX6100/x6100_gui#302** (text in
  [upstream-prs/14-murus-apps-discussion.md](upstream-prs/14-murus-apps-discussion.md));
  VE7NHW sends Hany the link on Facebook. Open points: Hany's own files
  carry no licence notice (needs his written OK, LGPL-2.1-or-later
  asked); NavTex has fldigi / Franco Venturi GPL-3.0 code (to
  third-party/); app numbers 109-111 already reserved (#283); a 4th/5th
  APP page with Kris. Tip: keep each tarball Hany sends in git (scratch)
  to see what changed between drops. **Waiting:** answers on #302,
  Hany's "version 1".
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

- [ ] Ultra still decodes after #10 (once a slot, 3.128 s in): two-radio
  bench, test build 12d374c is on the card.
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
