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
1. **Card:** since 10-28 06:00Z a private test build (d9ef184) whose app tree is `kris-apps-test3` f38b0d7 (kris-apps + the PSK31/Olivia VOL fix + the Channels button #328 + WeFax Post Proc. #329), so the kris-apps list can be tested on it as is; newest DATA backup **sd-backup-2026-10-27-225840**. **Found on it:** the Channel List crashed the GUI on an empty list (fix **#330**, below). The next private build carries the #330 fix.
2. **Releases:** v1.2.0 waits for Kris's tag (#326 approved). #324/#325 now approved by Kris and Georgy: VE7NHW asked Kris on #326 whether they go in before or after the tag. Ours: RC1.1 (`js8-rc1.1`). v1.3.0 = Kris's five apps + OpenVoiceCommand, Hany's WeFax update (#324/#325 + Post Proc. #329: VE7NHW wants WeFax done for a release soon), Channels button (#328), later NavTex/DSC.
3. **Next (VE7NHW):** flash kris-apps-test3 and test: WeFax Denoise Post Proc. (a `_pp.png` after 2-3 min, progress panel, full-screen view F2, F4 Original/Processed), VOL with Menu/Macros open in PSK31/Olivia/RTTY (only the list closes), Channels from APP page 5 and from a long press, plus the earlier list (RTTY TX, PSK31, Olivia, WSPR with VE7NHW and VE7NHW/P, FT2); results to Kris.
4. **Waiting on others:** Kris: v1.2.0 tag, the #324/#325 answer, myshak1#10-18; Hany: #328 (Channels button), **#330** (Channel List empty-list crash, also Georgy/Kris), #329 (Post Proc.), NavTex/DSC (his PRs or ours? asked in the reply email VE7NHW sends); Georgy: webserver v0.0.5 tag (then Buildroot #9 to the tag); EiBi's B26 schedule.
5. **VE7NHW, any time:** delete old branches (OpenX6100 `wefax-channels-merge`, `js8-groundwork`, `js8-engine`, `tx-level`, `js8-library`, `js8-app`, `murus-*`; ours `js8-library`, `js8-app`, `ox-murus-sstv-stacked`, `four-apps-test`, `claude/bold-albattani-x66yc5`, `claude/cool-goldberg-68nv8y`); pick where to back up CLAUDE.local.md.

- [ ] **Channel List crash on an empty list (VE7NHW, 2026-10-28): OpenX6100 #330** (branch `channels-empty-list`,
  worktree `~/Work/x6100/wt-channels-empty`). Pressing Channels (APP page 5) restarted the GUI every time; the V/M key
  runs the same function. Core dump from the radio (prlimit + core_pattern to /tmp/cores, header and stack read over the
  console): SIGSEGV at address 4 in `lv_table_set_row_cnt` <- `refresh_table` <- `build_window` <- `construct_cb`.
  With `/mnt/channels.json` empty the new table shrinks from 1 row to 0 and LVGL 8.3 frees cell 0, never set, without
  a NULL check (lv_table.c:228). Fix: one `lv_table_set_cell_value(table, 0, 0, " ")` in build_window(); reproduced and
  fixed on the PC (LVGL ad032d8, ASan). **Next:** review by Hany / Georgy / Kris; radio test with the next build. Note:
  the app_logs files stop at ~18 KB because x6100_gui's stdout is block-buffered and lost at a crash.
- [ ] **Hany's 1KO125 1.1-beta1 = the reference for his PRs (Hany's email, 2026-10-27; tag d65e605).** VE7NHW: get WeFax
  Post Proc. in now for a release soon. **OpenX6100 #329** (branch `wefax-postproc`, base `wefax-denoise` until #325 merges):
  Hany's 12 commits (neural-network Post Proc., model `rootfs/usr/share/x6100/wefax_pp.bin` 1.2 MB, full-screen fax view);
  training kit and NOTICE header lines left out; WeFax files otherwise identical to the tag. GCC 12 clean; PC test at 6 dB
  (`~/Pictures/wefax-postproc-pc-test.png`). **#328:** Channels moved to place 4 of APP page 5 (58828f3), matching his
  NavTex / DSC / FT8 CB places; VE7NHW: FT8 CB not coming in. Reply email to Hany drafted 10-27 (in the chat and clipboard; no licence questions, VE7NHW's wish): VE7NHW sends it.
  **Test image:** branch `kris-apps-test3` f38b0d7 (kris-apps-test2 + #328's move + #329), test-image.yml run
  **40572901929**; replaces kris-apps-test2. **Next:** VE7NHW flashes and tests the kris-apps list plus WeFax Post Proc.
- [ ] **OpenX6X00 (Guru-RF/OpenX6X00, Joeri Van Dooren ON6URE; not OpenX6100) and our JS8 code (checked 2026-10-27,
  VE7NHW asked).** Created 10-20. Credits R1CBU, R2RFE, Hany, Kris, randal007 (map, PSK/Olivia merge...), keeps R1CBU's
  copyright lines, JS8 files under GPL-3. **But** `src/js8_window.c` holds 41 of the long sentences of our `dialog_js8.c`
  and `src/js8/macros.cpp` is 95 % ours (code lines), both without "Copyright (c) 2026 Randal Warren VE7NHW"; the credits
  don't say the JS8 app is from our port; their README says every author's notice stays. Probably inherited from the
  code they forked (not confirmed). Facebook clarification drafted for VE7NHW (OpenX6100 ≠ OpenX6X00, Georgy R2RFE leads),
  without the licence point. **Next, if VE7NHW wants:** a friendly issue/message to Joeri (notice back in those files + a
  credit line), VE7NHW posts it.
- [ ] **Two finds from VE7NHW's kris-apps test (2026-10-27).**
  - **VOL press with Menu/Macros open closed the whole PSK31/Olivia app** (RTTY fine). Kris's shared `digi_menu.c`: the list
    is deleted while VOL is still down, LVGL resets the keypad and the held key repeats ESC into the window. Reproduced and
    fixed on the PC (LVGL 8.3 + this file, ASan): `lv_indev_wait_release()`. **PRs myshak1#16 (psk31), #17 (olivia), #18 (rtty).**
  - **Channel List only from the hand mic's V/M key.** VE7NHW: a Channels button on the APP page. **OpenX6100 #328** (branch
    `channels-app-button`): Channels on a new APP page 5 (NavTex/DSC fixed places beside it), Channels as a long-press
    action, number 117; for Hany to decide.
  - **Test image:** branch `kris-apps-test2` 4dec903 (kris-apps-test + both), test-image.yml run **40524047751**.
    **Next:** VE7NHW flashes it and checks VOL with Menu/Macros open in PSK31/Olivia/RTTY, and Channels from APP 5 and a long press.
- [ ] **v1.2.0 tag, then our fork up to it (2026-10-25).** All four app PRs merged, #326 (bump) approved.
  **Next:** Kris radio-tests main and tags v1.2.0; then bring our main up to OpenX6100's on its own branch
  (break check, build-list check, rerere check, test image), README *Coming next* and a v1.2.0 Facebook post
  (VE7NHW: a positive "free and open" post first).
- [ ] **Hany's apps, after v1.2.0 (2026-10-25).** **2026-10-27: Georgy approved #324 and #325 (both clean); VE7NHW asked Kris on #326 (in v1.2.0 or after its tag?) and Georgy on webserver #6 for a v0.0.5 tag. Webserver #6 merged by Georgy: Buildroot bump opened as AetherX6100Buildroot#9 (pins the merge commit; to the tag if Georgy makes v0.0.5).** All four first PRs merged (#309, #310, #311, **#312 merged 10-25**).
  Hany's own repo TheMurusTeam/x6100_gui `1ko125` (remote `murus`) now carries our changes (his comment, 10-22),
  so new work comes from there. **Opened 10-25 (VE7NHW: "add the PR for all the WeFax updates"):** **#324** shared
  app radio set-up (app_radio.c: USB-D locked, zoom 4x unsaved, DSP off while open; Hany author; own build line) and
  **#325** WeFax Denoise + Auto Slant 70 + USB-D/zoom + tests/wefax PC tools (base app-radio; ML training kit and
  .exe tools left out). PC test (our own chart through wefax_gen): at 4 dB Denoise missed the start, classic didn't;
  asked Hany on #325; **fixed by Hany 10-25 (5e8df85: START judged on the classic discriminator), on #325 as e4fa888; retest: start found at 8.5/6/4/3 dB.** Kris approved #324 and #325 (note: NR button turns NR back on while WeFax holds DSP off; Hany fixed it 10-25, 3a9adae, same code as JS8's dsp_held_cb: on #324 as 3af8902, merged into #325). Hany (email 10-25): NavTex/SITOR-B/DSC decoder ready (beats Venturi's in 95% of his regression set), SITOR-B text view to match RTTY's font setting first; may open his first PR himself. **Still to come:** NavTex (now LGPL, own SITOR-B receiver) and DSC (Nayuki QR code, MIT) on APP
  page 5 (a shared "page 5 on" commit as with page 4); webserver SSTV page (OpenX6100/x6100_webserver#6, needs
  Georgy's review, then a webserver tag and a Buildroot version bump); mic MODE key; FT8 CB (VE7NHW undecided);
  SSTV RX/TX radio test. Email to Hany about NavTex/DSC: drafted 10-25.
- [ ] **Kris's apps (WSPR #271, PSK31 #272, Olivia #274, RTTY #276, FT2 #288) ready for v1.3.0 (2026-10-25, VE7NHW: "fix all the issues, PRs for all").**
  All approved by Georgy. **Kris merged #311 (WeFax) 2026-10-25**; #312 waits for its retarget. Found and fixed, as PRs into
  Kris's branches: **myshak1#10** WSPR "plain call only" (a /P or /QRP call made every TX slot fail silently);
  **myshak1#11** main merged into rtty (CMakeLists + spectrum.h, digi_menu.c off RTTY's line);
  **myshak1#12** main merged into ft2 (worker.c includes, FT2 = 4 / 9 beside JS8's numbers; test_ft8_qso not run here);
  **myshak1#13, #14, #15**: tx_level.c / tx_player.c back in WSPR's, PSK31's and Olivia's build line (our 10-18
  app-places merges dropped them: merged as they were, each would have broken main's link). digi_menu.c listed
  twice when PSK31/Olivia merge: harmless (CMake builds it once, tried). myshak1#8/#9 (page 4 on) now redundant:
  main has it since #311. **Test image:** branch `kris-apps-test` cccd5e3 (main 24b5fa8 + SSTV + all the above;
  break check, build lists, 53 files GCC 12 clean), test-image.yml run **40221803013**.
  **Next:** VE7NHW flashes and tests RTTY TX (dummy load), PSK31, Olivia, WSPR (with VE7NHW and VE7NHW/P), FT2,
  VOL with Menu/Macros open in each; results to Kris.
- [ ] **EiBi B26 schedule into #310's broadcast.csv** when EiBi publishes it
  (sked-b26.csv 404 on 10-21 and 10-22; season starts 2026-10-25).
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
  **2026-10-16:** #295 approved by us (Buildroot #7 fixed our point); #253's reporter attached params.db (left to others).
  **2026-10-25: #326 (v1.2.0 bump) approved.**
  **2026-10-24: #323 approved** (Kris: FT8/FT4 with non-standard own calls + a /P, /QRP list in Callsign, fixes #322; GCC 12 clean, merges with our four). Notes for Kris: WSPR (#271) skips every TX silently with a call containing "/" (wants a blocked reason); digi_menu.c listed twice once #272/#274/#276 follow; ESC in the suffix list saves nothing without a word. JS8: VE7NHW: prefix + /QRP (over 9 letters) too rare to handle.
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

- [ ] **NR2 for the X6100 (base firmware + GUI), kept private for now**
  (VE7NHW, 2026-10-18; last worked on 2026-10-25). Status, next step and
  notes are in the private repo (NR2-NOTES.md, see CLAUDE.local.md), not
  here.

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
  **2026-10-16:** FT2 (suspect 2) is in all-apps-test, now on the card: console connected on the first boots.
- [ ] **Digital apps' list crash: VOL press with Menu/Macros open
  (VE7NHW, 2026-10-17, RTTY page 2 on all-apps-test; shared code, Kris's
  `digi_menu.c`).** `digi_menu_close()` took the list out of the group
  before deleting it, so LVGL's keypad sent CANCEL to freed memory
  (reproduced on the PC under AddressSanitizer). One-line fix sent as
  **myshak1/x6100_gui#4** into Kris's `rtty` (branch
  `digi-menu-esc-crash` in OpenX6100), noted on #276; the same file is in
  #272 / #274. On `all-apps-test` (89c2f4e, now in d91a675's image, run 39059910797). Also seen:
  the test-only core dumps in /mnt/cores came out **0 bytes** (three
  crashes 15:27-15:35Z), so that set-up doesn't work yet. **Next:** test
  image, VOL with each list open in RTTY / PSK31 / Olivia; Kris merges.
  **2026-10-18: Kris merged #4 into rtty** (checked it against LVGL: the ENTER path too) and put the same commit on #272 / #274. Left: the radio check.
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
  **2026-10-16:** nothing caught on afd514e (no cores, no new crash logs since 10-14). all-apps-test flashed with the same test-only core dumps (unstripped GUI in builds/ci-38979367716/unstripped/).
  **2026-10-17:** the three cores written that morning (RTTY menu crash) were **0 bytes**: the core-dump set-up doesn't work as it is (check /mnt free space, vfat file size, `ulimit -c` in x6100_daemon) before relying on it. d91a675 on the card has none (unstripped GUI in builds/ci-39059910797/unstripped/).

- [ ] **From Hany's 1.0.2 beta 5, outside the apps: each its own PR (VE7NHW 2026-10-20: "anything outside the app should be a separate PR").** Opened the same day, Hany as author:
  - **#316 ptt.log** (branch `ptt-log`): who keyed the transmitter (TX commands with their source, BASE TX on/off, "NOT COMMANDED"); his diag_log trimmed to the PTT part, radio.c +55, one-line source names in main_screen/cat/cat_lan/ft8 audio_worker/msg_voice. Fits *Unexplained transmit at power-up*.
  - **#315 waterfall noise floor** (branch `waterfall-noise-floor`): the app waterfalls' colours follow the audio's noise floor (FT8's background no longer changes colour slot by slot with the AGC); fitted onto #290's ring buffer; overrides set_min/max for every user of the widget (asked if it should be a per-app switch). Not the same as #304 (main waterfall zoom).
  - **OpenX6100/x6100_webserver#6 SSTV page** (from fork randal007/x6100_webserver, branch sstv-page; no push access there, reviewer request not allowed): upload/delete SSTV pictures and text templates, photo editor. OpenX6100's image already runs this web configurator; Hany added the page. Useful once #312 is in.
  - **I2C resend** (radio.c resync + stderr/kmsg readers): **discussion posted 2026-10-20: OpenX6100/x6100_gui#317** (VE7NHW OK'd; text in [upstream-prs/27-i2c-discussion.md](upstream-prs/27-i2c-discussion.md)): ask whether X6100Control should return write errors instead of parsing perror text; then a PR, radio-tested with the console (the NAK at GUI start).
  - **FT8 CB**: VE7NHW undecided, not now.

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
