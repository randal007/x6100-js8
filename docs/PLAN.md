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
1. **Card:** test build `digimodes-test` **afd514e** (beta 5.2 + FFTW wisdom + WSPR/PSK31/Olivia/RTTY TX + FT8 knob + #304 zoom auto levels + #305 S-meter offset), run 38761835319, flashed 2026-10-15 17:25 local, backup **sd-backup-2026-10-15-172525**, test-only core dumps re-added in /usr/sbin/x6100_daemon (to /mnt/cores; none were caught on 24c9481).
2. **Next on the radio:** test #304 (zoom in/out with auto levels: floor stays put) and #305 (offset moves the meter), post on both PRs; the dial crash (FT8, spin fast; fetch /mnt/cores, read with builds/ci-38761835319/unstripped/); RTTY TX, WSPR marker, PSK31, Olivia.
3. **On main, not released:** FFTW wisdom (PR #11). JS8 into OpenX6100: 1/4 #290 merged; **2/4 #301 approved by Georgy** (2026-10-15; he wants `legal-info` as its own PR); **Kris's review asks** a Buildroot PR (boost + fftw-single in x6100_gui.mk, X6100_GUI_LICENSE still "GPLv2"), README: the *GUI program* is GPL-3.0, not the image, and CRCpp/Eigen licence files. Our turn; 3/4 next.
4. **Murus apps:** **Hany's full written OK on #302 (2026-10-15 16:52Z):** headers to LGPL-2.1-or-later, 7(b) dropped, unlabelled files LGPL too, order agreed. **Cloud sessions done 2026-10-15:** draft PRs here #12 groundwork, #13 Channel List, #14 WeFax, #15 SSTV (base `ox-main`); no GCC 12 check, image or radio test yet; their notes on branches claude/* (not merged). Open: #12 recorder MP3 -> WAV for everyone, #13 + #14 both take mic UP/DOWN, our commits authored "Claude".
   **Done 2026-10-16 (VE7NHW agreed):** #301: README says the GUI *program* is GPL-3.0 + CRC++/Eigen licence files (b040b8f, same files on main); Buildroot **OpenX6100/AetherX6100Buildroot#8** (`js8-deps`: boost + fftw-single, licence LGPL-2.1+ / GPL-3.0; js8core/LICENSE joins LICENSE_FILES once X6100_GUI_VERSION has it). Reply to Kris posted on #301; #295 approved by us; the 3.8 W was in JS8. After #8 merges: drop the "Enable JS8 dependencies" step in main.yml. #305: asked Georgy if ±10 dB is OK (range not changed yet). #300 reworked as Georgy asked (6050dd9: rotary_cb gets `dt`; Kris told his WSPR/PSK31/Olivia need the extra argument). #303: told them we measure first. **Test image digimodes-test 48d1269** (knob rework + TX level log every 0.5 s, "TX level:" lines in /tmp/x6100_log.txt): *Build image* run **38975177237**. **#304 merged by VE7NHW 2026-10-16** (radio test OK). Branches deleted 2026-10-16: psk31/olivia/ft2-merge-main (OpenX6100), wspr-test, wisdom-wspr-test, bt-security-js8 (ours).
   **Open with others (2026-10-16):** #304 approved by both: VE7NHW to merge. #305: Kris approved, Georgy doubts ±30 dB (S-meter is calibrated), Hany offers less: our reply. #303: Georgy + Kris agree on one module (one stored level + per-mode correction; Kris: PSK31 only, clamp the stored level, he moves his apps once we send tx_level as a PR); **VE7NHW not happy with tx_level yet: 3.8 W of 5 W, no amp** (look first). #300: rework as Georgy asks (pass the knob's `dt` to apps). Kris's #272/#274 have our waterfall suggestions; all his apps approved, not merged; he deleted + restored his branches 2026-10-16 19:16Z (slip); myshak1/x6100_gui#1-3 closed (he merged main himself): delete our `*-merge-main` branches. #295: Buildroot #7 fixed our point: approve.
5. **Cloud credits** ($100, expire 2026-11-04): Murus Parts 1-4, then a deep review of JS8 parts 3+4.

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
  patches are features, not sent. **Ultra on Turbo's schedule taken back
  out 2026-10-14** (see the Ultra item). Not built in their tree (Claude may not
  build downloaded code here); the new checks pass against our copy.
  **Merged 2026-10-14 15:02Z by punk-kaos** (the compound fix and the
  includes; the Ultra commit reverted there too). **Next:** retest on the
  air with a "/number" station.
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
  **2026-10-14: #290 merged (VE7NHW, both approved), 2/4 opened as #301.**
  3/4 also carries the FFTW wisdom fix (PR #11, on main since 2026-10-15).
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
- [ ] **One transmit-level (ALC) module for every digital app (2026-10-14,
  VE7NHW: needs fixing; PSK31 and Olivia put out less than JS8 with the
  XPA125B).** FT8's tx_worker corrects per chunk from single readings
  (saves its offset); PSK31/Olivia/WSPR copy it but only read FT8's offset,
  never save. Ours (tx_level.c, beta 4.6) averages, ramps, holds at the
  ALC's onset, saves. PSK31 reads lower partly by nature (average < peak).
  **Discussion opened: OpenX6100/x6100_gui#303** (text in
  [upstream-prs/15-tx-level-discussion.md](upstream-prs/15-tx-level-discussion.md)):
  offered tx_level/tx_player as a PR of their own before JS8 4/4.
  **Next:** their answer; a JS8-vs-Olivia wattmeter comparison if useful.
- [ ] **PSK31 / Olivia tried on the radio (digimodes-test, 2026-10-14):**
  comments posted on #272 and #274 (VE7NHW's findings): the waterfall
  keeps scrolling while keyed and shows blank static (not the band, not
  our TX; TX itself fine, full power; first comment wrongly said TX audio,
  corrected) (suggested: ignore capture while keyed, as JS8, pause or
  black rows); the scrolling flicker JS8 had (suggested, optional: our
  row averaging, avg += a*(row-avg), a 0.65/0.5/0.35). Waiting for Kris.
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
  d6377057 to retest: in digimodes-test (now on the card). Branches
  `wspr-test`, `wisdom-wspr-test` and `bt-security-js8` are superseded by
  digimodes-test: delete when VE7NHW agrees. Earlier: try it on the radio (he asked: keep the branch until
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
  to see what changed between drops. **2026-10-15:** Hany answered on #302
  (13:47Z): five apps (SSTV, WeFax, NavTex/SitorB, DSC new, Channel List /
  Broadcast Info), GPLv3 so far but **LGPL-2.1-or-later as they go in**,
  NavTex last (his own decoder replaces the fldigi/Venturi GPL code), app
  numbers fine. His 1.0.2 beta 4 tarball (sha256 3cfd5ac3...) is in
  `~/Work/x6100/reference/1ko125-1.0.2-beta4/` and on branch
  **`murus-drops`** (c0bf2ab, on top of v1.0.2, Hany as author);
  **`ox-main`** = OpenX6100 main 3b8fd33 for the cloud sessions. Our reply
  (discussioncomment-18951729, text in
  [upstream-prs/16-murus-licence-reply.md](upstream-prs/16-murus-licence-reply.md)):
  may we change his headers to LGPL in each PR (the 7(b) credit term then
  dropped), licence for the unlabelled Channel List / ui_list files, PR
  order. **Plan (VE7NHW 2026-10-15): cloud sessions build the PR branches**,
  brief [tasks/murus-apps.md](tasks/murus-apps.md): Part 1 shared
  groundwork (+ S-meter offset and zoom auto-level fix as two small PRs),
  2 WeFax, 3 SSTV, 4 Channel List; NavTex and DSC later. Each session
  pushes a branch + draft PR into `ox-main` here; this PC compile-checks,
  test image, radio test, then the real OpenX6100 PR. **Order and layout (VE7NHW 2026-10-15):**
  Kris's apps go in first; **S-meter offset and zoom auto-level fix
  sent straight away as #305 / #304** (branches s-meter-offset,
  zoom-auto-levels, Hany as author; radio test in digimodes-test afd514e,
  Build image run 38761835319). APP page 3 WiFi, BT, WSPR, JS8; page 4 PSK31,
  Olivia, WeFax, SSTV; page 5 NavTex, DSC (as digimodes-test); numbers
  unchanged. **Waiting:** Hany's
  OK on the headers; Randal starts the Part 1 session.
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
  run 38478388820. **Georgy 2026-10-14 (review comment):** rather change
  the API and reuse the main screen's speed (`dt`, main_screen.c ~931)
  than count clicks in dialog_ft8.c: **rework needed** (shared code).
  Not in beta 5.2; the test builds still carry 3385886. **Next:** rework
  as he asks; radio check of the current fix still open.
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

- [ ] **GUI crash while spinning the main dial fast** (again 2026-10-14
  22:16:19Z on digimodes-test bd3bfd7, FT8 window, 37 s after boot,
  10 Hz steps every ~50 ms around 14.0956 MHz; **this time the log ends
  in `realloc(): invalid pointer`**: heap corruption, likely two threads
  on the same LVGL memory). Core dumps switched on for the running GUI
  (prlimit + /proc core_pattern → /mnt/cores; both gone at reboot, only
  the first crash per GUI start). *Build image* now also uploads the
  unstripped GUI (9c9eecb, artifact `x6100_gui-unstripped.<ref>`);
  **Crashes cluster on a build's first boot** (VE7NHW's observation,
  checked against flash backups: 09-27 27 min after a flash, 10-03 1 h,
  10-13 7 min, 10-14 10 min; 10-02 4 days, at GUI start-up, also
  `realloc(): invalid pointer`); no new settings keys on first boot, so
  likely a thread race exposed by first-boot load. **Set up 2026-10-14:**
  digimodes-test 24c9481 (run 38651955343, artifact
  x6100_gui-unstripped.digimodes-test, copy in
  builds/ci-38651955343/unstripped/) flashed with a TEST-ONLY
  x6100_daemon that dumps every GUI crash to /mnt/cores. **Next:** after
  a crash, fetch /mnt/cores/core.* over the console, then
  `arm-linux-gdb unstripped/x6100_gui core` (sysroot: the image's
  rootfs) for the backtrace; OpenX6100 issue with it. Earlier: (VE7NHW,
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
