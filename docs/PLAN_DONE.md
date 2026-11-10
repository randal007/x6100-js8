# Plan: done

Items ticked in [PLAN.md](PLAN.md), moved here at each offload so the work
list stays short. Newest at the top. Beta 5's list is
[BETA5_PLAN.md](BETA5_PLAN.md).

## Moved 2026-11-09

- [x] **Test image 11-09 (built, flashed 11-10 13:54, radio-tested 11-10: FT8 20 min CQ): run 42687294904 (42685729338 cancelled: had the slot bug below), branch `test-package-5` c1a2f2e8, name ft8-late, worktree ~/Work/x6100/wt-test-5** = OpenX6100 main a5ced9c6 + #371 + #351 + #367 + ft8-psk-line-connected + ft8-late-start + js8-tx-waterfall + #349. Not in it, unlike the card: XARA (frozen). test-package-4-wf no longer merges with main. Merge checks OK; PC 10+7 and 14+5 decodes. **Next:** flash on VE7NHW's word.

- [x] **Shared waterfall for FT8/FT4/FT2 and WSPR, with our TX shown as XARA's (VE7NHW, 2026-11-08: "the FT8 waterfall sucks").**
  New `src/app_wf.c` (shared GUI, OpenX6100 branch `shared-waterfall` 4af756f0, worktree ~/Work/x6100/wt-shared-waterfall):
  XARA's waterfall made general = JS8's (12 kHz own audio, 15 rows/s by the clock, noise-floor colours, "Light" calm,
  lower DRM plane via js8_wf.c) + TX rows from the playback monitor with a red edge and "TX" tag. FT8 across its RX
  filter (TX shifted by tx_freq - 1325, FT8_TX_AUDIO_HZ), WSPR across 375 Hz (sliding FFT, 1.46 Hz bins). JS8 untouched.
  ARM compile checks OK; not yet run on the radio. **Test image:** branch `test-package-4-wf` (randal007/x6100-js8,
  93585a2d = test package 4 + shared-waterfall), run 42439469764 (name waterfall); with the FT8 contest fix and extra passes: 1b03d32a, run **42444841698** (built OK 11-08 04:45Z, downloaded to builds/ci-42444841698). **Next:** flash on VE7NHW's word,
  compare FT8/WSPR waterfalls, watch TX rows (FT8 at the finder, WSPR at its marker); CPU check; then PR + tell Kris (WSPR).
- [x] **FT8/FT4/FT2 and WSPR: NR/NB/notches off while open, as JS8 (VE7NHW 2026-11-08).** Done in OpenX6100 branch `ft-rx-dsp-off`
  (worktree ~/Work/x6100/wt-ft-dsp-off), merged into test-package-4-wf; ARM compile check OK; **not in the image on the card**; in run 42506841968.
  Same as JS8: radio only, settings kept, off again if turned on meanwhile, back on close. **RX filter (VE7NHW 11-08: set it, not super tight because of the roll-off), 6e70d30d + 2b88a57f on the same branch:** FT8 100-3100 Hz on the radio, app shows/decodes/TX-limits 200-3000; WSPR 1000-2000 around its 1400-1600; own filter back on close. Margins are a judgement, roll-off not measured: if the waterfall edges dim on the radio, widen the constants FTX_FILTER_*, WSPR_FILTER_*. TX filter not changed. Both apps already set USB-D.
  **AGC:** JS8 doesn't touch it. Proposed: radio test on one band, AGC Slow vs Off, count decodes, then decide.
- [x] **Radio test 11-08, first result: FT8/FT4 extra passes too slow on the radio.** "0 more decoded (of 14), 2523 ms / 2773 ms, stopped at the deadline": 14 subtractions use the whole 2.5 s, so no search runs (PC 110-135 ms). They run on the audio thread. **Lesson from JS8's decoder (js8core legacy_decoder.cpp, receiver.cpp, cpu_fence):** its decode runs on its own thread with its own copy of the audio, kept off one core for the screen (4 cores), so a slow pass can't touch the audio; 3 passes with subtraction; and no OSD: instead up to 4 LDPC tries with different soft-bit sets (first/last 24 bits erased), a cheap form of fix 3. **Done 11-08 (VE7NHW: yes), b56a8901 on `ft8-contest-messages`:** extra passes on their own thread "ftx-extra" (own slot copy, spectrum, tables; off the last core, nice 5; up to 0.8 of a slot; finds handed back on the audio thread, callsign hash table under a lock). PC: same 14 and 18 decodes, ThreadSanitizer clean. test-package-4-wf 6bdf5562 also makes #351's snr_noise[] __thread: **the passes PR must do the same once #351 is in.** **Test image: run 42521756042 (name review-fixes):** adds this + RFG + list follow + the review fixes. **Next:** flash on VE7NHW's word, read the "extra passes" timing; **Fix 3 done 11-08 as JS8 (src/ft8/decode_retry.c, in the extra passes only, top 60 candidates; VE7NHW asked):** modest: ft8sim 30 files/level, without/with/jt9: -20 dB 11/16/29, fading -16 dB 17/19/30, fading -18 dB 4/5/27; no false decodes in 40 noise files; **no gain on the two real recordings (14, 18)**. The 7 misses on 210703 all have good candidates, the decoder just fails them. What jt9 still has: fine time/frequency sync on a baseband copy and coherent multi-symbol metrics, plus OSD. Test image with everything: test-package-4-wf 6f86038a, run 42524435049 (name ft8-fix3).
- [x] **Code review of the six 11-08 branches, findings checked by hand, **all 7 fixed 11-08 (VE7NHW: "yes do all")**, 4bd096d9 on ft8-contest-messages, 58191fe7 on ft8-psk-reporter, 31796d75 on shared-waterfall, new tests in test_ft8_qso.cpp pass; test-package-4-wf cbacb2f1, **test image run 42521756042 (name review-fixes)**, replaces 42517403298 (built OK, has none of these fixes). Findings:** ft8-contest-messages: (1) ftx_worker_free leaves slot_len/heard.n, so FT8->FT4 late in a slot can submit an FT8-sized job on the new 90000-float buffer: heap overrun in ftx_subtract; (2) "RR73; ME <...> -11" makes a candidate called "..." in qso.cpp; (3) qso.cpp treats "TU; A B 579 MA" as DXpedition: station "579" (tighten to "RR73;"); (4) extra-pass finds land on the new band after a band change (cancel in clean_screen). ft8-psk-reporter: (5) red QSO path keeps the PSK line's width/opacity; (6) MQTT timers use wall clock (use CLOCK_MONOTONIC). shared-waterfall: (7) audio_worker and wspr_app still compute the old spectrum with on_psd NULL. Not bugs: RFG on WSPR turns the schedule off (deliberate, kept; VE7NHW can still say otherwise); FT8 waterfall spgram reset = same as XARA's/JS8-style. For the PRs: main_screen.c (RTTY RFG) and map_scene.c changes go in their own PRs.
- [x] **Radio test 11-08 evening: extra passes work (+4, +10, +8 on 18-19 first-pass decodes, 7-7.5 s), then the decoder hung about 2 min in.** Cause: ft8_lib's hashtable_add/lookup never stop searching once its 256 places are full (audio thread 100 % in libft8, hash_mux held, ftx-extra waiting, cbufferf_write errors flood the log); main has the same latent bug, the passes just fill it faster. **Card run 42524435049 has this hang: don't use FT8 long on it.** Fixed in `src/ft8/call_table.c` (ft8-contest-messages; 1024 places, bounded search, oldest replaced; tests pass, 14/18 unchanged, ThreadSanitizer clean); touches FT2 too (shared table, a bug fix only). Worth its own PR. **Test image: test-package-4-wf 1327bf36, run 42533266989 (name ft8-table-fix).** Also: radio clock measured +4 s at 23:00Z (not yet known why; decodes were fine at 22:58): set by hand to within 0.2 s, RTC not written yet (radio was busy); after a restart it needs setting again, then one hwclock write when idle.
- [x] **FT8/FT4/FT2: PSK Reporter "who hears me" on the map, as GridTracker (VE7NHW 2026-11-08): built, not yet on the radio.** OpenX6100 branch `ft8-psk-reporter` acfdd8e0 (worktree ~/Work/x6100/wt-ft8-psk):
  `src/psk_reporter.c` = small MQTT 3.1.1 client, own thread, plain TCP to mqtt.pskreporter.info:1883, topic `pskr/filter/v2/<band>/<mode>/<call, . for />/#`, retry 90 s as GridTracker (read in gitlab.com/gridtracker.org/gridtracker2: mqttPsk.js, spots.js). Page 3 button "PSK Rep", off by default, saved (`ft8_psk`, `ft8_psk_min` 5-60, default 15).
  Press = on/off; after turning on, MFK sets the minutes for 4 s; held while on = the time only. Colour **blue (VE7NHW chose A 11-08, mock-ups ~/Pictures/x6100-psk-reporter-mockups)**, line brightness by SNR as GridTracker (floor 110/255), gone at the time limit. Feed has no history: only reports after it's switched on. Fetches only, doesn't upload what we hear.
  Touches shared map_scene.c/.h (path_opa, status3). Tested: client against the real feed (busy call 220 reports in 35 s, 36-51 s old, band change, stop, ASan clean); map, count, 15/30 min, off in the UI harness (scratch copy). **Not tested: the MFK/button code in dialog_ft8.c (compile only).**
  **Test image: test-package-4-wf b60569d2, run 42506841968 (name wf-ft8-psk-filter; 42504851399 failed on the U-Boot mirror download): waterfall + contest + passes + NR/NB/notch off + RX filter + PSK Reporter.** **Next:** flash on VE7NHW's word; radio test of the button, MFK time, lines after a transmission; then PRs.

## Moved 2026-11-06 (the old *In short* of 11-06, replaced)

1. **Card:** the 3D spectrum test image (run 42236779844, `spectrum-3d` bf28f79 = OpenX6100 main 2dd8d1e + 3D), flashed 2026-11-06 (DATA backup **sd-backup-2026-11-06-091854**). VE7NHW loves the 3D view.
2a. **3D spectrum: PR #344 opened 11-06** (reviewers Georgy + Kris asked; 3D size now 35-55 %, both height sliders kept). **Next session (VE7NHW 11-06):** measure CPU on the radio, 2D vs 3D (x6100-cpulog).
2. **OpenX6100:** **Kris's six apps (WSPR, PSK31, Olivia, RTTY, FT2, JTTY) merged by VE7NHW 2026-11-05** (main 2dd8d1e, tests green); **no v1.3.0 tag** until Kris's review features are in (his e-mail 11-05). #329 WeFax Post Proc.: Hany answered, kit + his e-mail in, our review posted (ESC/F5 in full screen, `_pp.png` without .tmp); waiting on Hany, then Kris/Georgy approve.
3. **Test package (11-06, VE7NHW: "everything in one test build", link for Kris/Georgy in the e-mail he sent; the
   link follows when built).** Public: branch `test-package` (randal007/x6100-js8) = OpenX6100 main + the nine fix PRs
   #335-#343 + `shared-map` (maps + WSPRnet, merged with #336) + 3D (#344, 7d8b20bd) + XARA (#345), image **run
   42276379917** with `buildroot_ref=xara` (Buildroot#10), tests run 42276383681. Private (Kris/Georgy/Hany only): branch
   `test-package-nr` in randal007/x6100-nr2 = the same + the NR selector, image **run 42277592227** (2-core runners,
   slow); then a private prerelease with the image + base r13nr2j (release notes must cite Warren's permission and
   credit WDSP NR2) and a short follow-up e-mail with both links. **Next:** when each run is green, give VE7NHW the
   link/e-mail; `gh run view <id> -R <repo>`. Maps checked on the PC 11-06: JS8 map old vs new same (only clock,
   blink phase, live decode SNR differ), FT8/WSPR map checks all pass (harness in wt-shared-map/tools, git-excluded).
3b. **Kris's answer (e-mail 11-06):** findings real; merges our fix PRs in the order #338, #343, #342, #339, #340,
   #341, #337, #336, the FT2 slot fix at once; asked (done 11-06): #335 split (signal reports -> **#346**, waits for
   VE7NHW's bench check FT2/FT4 vs WSJT-X) and #336's wsprd from WSJT-X 2.7.0 itself. Then he rebases digi-qso-log
   (takes our PSK31 echo fix, FIGS fix into his streaming RTTY TX, Auto DX with his name/QTH pick-up). **1.3.0:** map +
   WSPRnet + 3D (#344) once both have tested; XARA (#345) after 1.3.0. His next: PSK31/Olivia dial by region (7.040 /
   7.070), RTTY serials + AFC, Olivia 8/500, WSPR type 2/3 transmit.
3e. **11-06 evening (Kris merged #335-#343):** done after: #344 stock-base fix (history dropped with base_freq 0:
   4b2c6259) + **band bar** (16255634: band plan by ITU region in a bar under the 3D view, Auto from QTH via #348's
   itu_region.c, Settings > UI > Band plan; colours said once on the message line; corner labels gone in 3D; PC-rendered);
   **#351** FT8/FT4/FT2 signal reports vs the band background (equal to jt9 for FT8, FT4/FT2 within 1 dB); XARA
   **--local-only** (randal007/xara 2a33e96, Buildroot#10 35b594b, #345 5ea13d46); **#349** Hany's #329 points 1+2
   (Hany OK'd); **#352** multi-line echo colour (Kris's #343 note); private release **nr2-test4** (image + r13nr2j).
   Test package 2: public `test-package-2` run 42314478755, private `test-package-nr-2` run 42314517564. Hany's
   NAVTEX **#350** (number 110) vs XARA: only both buttons after JTTY; merge main into xara-app after #350
   (NavTex first). **Code reviews of the day (11-06 late), all fixed and pushed:** #344 d25cd02e, #349 dd7d740b,
   #351 221e7a71, #352 969ef705, **#345 99e07c89** (band keys refused while keyed/linked, per-band mode + mem_load on
   close as JS8, old peer cleared on exit, status race, BT KISS at BT_SECURITY_MEDIUM (needs an APRSdroid-over-BT
   test), NR/NB re-check, SWR guard, recording stops at 100 MB free, mixer not reopened per key); xara --local-only
   read by hand, no findings. **XARA renumbered 120** (VE7NHW: DSC = 119, Hany's, apps in order); **#345 +
   Buildroot#10 out of draft** (VE7NHW 11-06). Test package 2 runs predate these fixes. Region borders for Kris
   (#348's itu_region.c): Iceland, Azores, Cape Verde come out Region 2. **Next:** review #350; links for VE7NHW when
   the runs are green; a test package 3 with the fixes if VE7NHW wants. **DSC = #353** (Hany, 119): reviewed, comment posted by VE7NHW (issuecomment-6702493233: UNKNOWN FORMAT on a late-phased call, wrong channel stamp, Tune ignores freq lock, details index). **XARA on its own APP page 6** (c9778d65, VE7NHW 11-06): NavTex + DSC fill page 5; NavTex+DSC+XARA test-merged, builds (the later one keeps both lines after JTTY). Card: test package 3 NR build b807534, backup sd-backup-2026-11-06-214744. **#347 reviewed** (Kris), comment posted by VE7NHW (issuecomment-6702647797: RST row logs the QSO, RTTY keyed after Stop while the next app opens, BT SPP dropped on close...). **Buildroot#10 reviewed**, fixes 051ab64 (Rust target/CPU from Buildroot, cross env, CARGO_TARGET_DIR, config merge, licence note, ~120 MB Rust download); check build run 42341267148; xara.hash still to add (from a build). **#344 4d3b9f1c** (VE7NHW): segment tag ("40m  CW", fades after 2 s) at the 3D view's top left, band_info banner off in 3D, 2D/3D label removed; compile-checked only. **#348 reviewed**, comment posted by VE7NHW (issuecomment-6700528697: region borders, own PR for itu_region.c; take his fixed file into #344). **#350 reviewed:** 10 findings (Delete hits the wrong bulletin, live text stops following, empty bulletin won't load...), comment for Hany not written yet; his .tmp saves (#350 16627169, #353 b40253a0) checked OK. **Test package 3 built (11-06):** public run 42319770123 green (tests 42319773048 green, artifact expires 12-07); the private one too. #350 review posted by VE7NHW (issuecomment-6702413160).
3d. **Public test package BUILT 11-06: run 42276379917 green** (tests 42276383681 green), artifact
   sdcard.test-upstream.img.zip, expires 2026-12-06; link https://github.com/randal007/x6100-js8/actions/runs/42276379917
   (VE7NHW sends it). Private NR one (run 42277592227) still building. #343 approved by Kris 11-06, his note for later:
   a multi-line macro's echo loses its colour after the line break (panel_tx_text_cb splits on spaces only).
3c. **Signal reports (11-06):** #346 closed (PC test: wrong). Proper fix WIP, branch `ft-snr-baseline` (OpenX6100,
   worktree wt-ft-snr, not a PR): noise from the band's background; vs WSJT-X jt9: FT4/FT2 exact, FT8 exact at -18
   but 2-4 dB low when strong. **Next:** check the uint8 ceiling (dbg build in scratchpad snr/, needs `-I base/ft8`),
   then the PR. Test setup: research/reviews/ft-snr-check.md. **XARA ports (VE7NHW 11-06: not phone-only, laptops
   too):** proposed bind to the WiFi address (xara `--addr`, app passes 0.0.0.0 now) + local addresses only + show
   who is connected; waiting for VE7NHW's OK. Follow-up e-mail to Kris drafted in the session (XARA for 1.3.0).
4. **Waiting on others:** Kris (e-mail 11-06): **WSPRnet is ours** (upload spots + fetch "heard you"), shared map + FT8/FT4/FT2 go-ahead; he's on review asks #1/#2/#6 for PSK31/Olivia/RTTY in one change (branch `digi-qso-log`: Log QSO as JTTY, typing joins the TX with idle fill, <NAME> <QTH> <MYNAME> <MYQTH> <MYLOC>, RX/TX text to a file per mode, RST received fixed 597→579), testing with QSOs before his PR, then the rest of the list; Hany: #329 review points, NavTex/DSC; EiBi's B26 schedule.
5. **VE7NHW, any time:** QSOs on the new modes; bench power test (PSK31/Olivia vs steady modes at ~0.3 W into the amp); delete old branches (OpenX6100 `wefax-channels-merge`, `js8-groundwork`, `js8-engine`, `tx-level`, `js8-library`, `js8-app`, `murus-*`, `wspr-v13`/`psk31-v13`/`olivia-v13`/`rtty-v13`/`ft2-v13`/`app-pages-v13`/`kris-apps-v13` (merged into Kris's branches); ours `js8-library`, `js8-app`, `ox-murus-sstv-stacked`, `four-apps-test`, `claude/bold-albattani-x66yc5`, `claude/cool-goldberg-68nv8y`); pick where to back up CLAUDE.local.md; ask Jake 2E0JFS for his repo link.
6. **XARA (11-06):** PRs **#345** (app, branch `xara-app`) + **AetherX6100Buildroot#10** (xara package, branch `xara`), every test setting kept; out of draft 11-06 late, app number 120. Images building: **VE7NHW's with full speed: private x6100-xara run 42273143987** (never share); free-speed one for the PRs: x6100-js8 run 42273148030. VE7NHW asked Kris/Georgy on #10 to turn full speed on by default. **Next:** check both runs; flash when VE7NHW says. Reviewers gdyuldin + myshak1 added (VE7NHW, 11-06).

## Moved 2026-11-02

- [x] **Channel List crash on an empty list (VE7NHW, 2026-10-28): OpenX6100 #330** (branch `channels-empty-list`,
  worktree `~/Work/x6100/wt-channels-empty`). Pressing Channels (APP page 5) restarted the GUI every time; the V/M key
  runs the same function. Core dump from the radio (prlimit + core_pattern to /tmp/cores, header and stack read over the
  console): SIGSEGV at address 4 in `lv_table_set_row_cnt` <- `refresh_table` <- `build_window` <- `construct_cb`.
  With `/mnt/channels.json` empty the new table shrinks from 1 row to 0 and LVGL 8.3 frees cell 0, never set, without
  a NULL check (lv_table.c:228). Fix: one `lv_table_set_cell_value(table, 0, 0, " ")` in build_window(); reproduced and
  fixed on the PC (LVGL ad032d8, ASan). **Next:** review by Hany / Georgy / Kris; radio test with the next build. Note:
  the app_logs files stop at ~18 KB because x6100_gui's stdout is block-buffered and lost at a crash.
- [x] **Two finds from VE7NHW's kris-apps test (2026-10-27).**
  - **VOL press with Menu/Macros open closed the whole PSK31/Olivia app** (RTTY fine). Kris's shared `digi_menu.c`: the list
    is deleted while VOL is still down, LVGL resets the keypad and the held key repeats ESC into the window. Reproduced and
    fixed on the PC (LVGL 8.3 + this file, ASan): `lv_indev_wait_release()`. **PRs myshak1#16 (psk31), #17 (olivia), #18 (rtty).**
  - **Channel List only from the hand mic's V/M key.** VE7NHW: a Channels button on the APP page. **OpenX6100 #328** (branch
    `channels-app-button`): Channels on a new APP page 5 (NavTex/DSC fixed places beside it), Channels as a long-press
    action, number 117; for Hany to decide.
  - **Test image:** branch `kris-apps-test2` 4dec903 (kris-apps-test + both), test-image.yml run **40524047751**.
    **Next:** VE7NHW flashes it and checks VOL with Menu/Macros open in PSK31/Olivia/RTTY, and Channels from APP 5 and a long press.
- [x] **Hany's apps, after v1.2.0 (2026-10-25).** **2026-10-27: Georgy approved #324 and #325 (both clean); VE7NHW asked Kris on #326 (in v1.2.0 or after its tag?) and Georgy on webserver #6 for a v0.0.5 tag. Webserver #6 merged by Georgy: Buildroot bump opened as AetherX6100Buildroot#9 (pins the merge commit; to the tag if Georgy makes v0.0.5).** All four first PRs merged (#309, #310, #311, **#312 merged 10-25**).
  Hany's own repo TheMurusTeam/x6100_gui `1ko125` (remote `murus`) now carries our changes (his comment, 10-22),
  so new work comes from there. **Opened 10-25 (VE7NHW: "add the PR for all the WeFax updates"):** **#324** shared
  app radio set-up (app_radio.c: USB-D locked, zoom 4x unsaved, DSP off while open; Hany author; own build line) and
  **#325** WeFax Denoise + Auto Slant 70 + USB-D/zoom + tests/wefax PC tools (base app-radio; ML training kit and
  .exe tools left out). PC test (our own chart through wefax_gen): at 4 dB Denoise missed the start, classic didn't;
  asked Hany on #325; **fixed by Hany 10-25 (5e8df85: START judged on the classic discriminator), on #325 as e4fa888; retest: start found at 8.5/6/4/3 dB.** Kris approved #324 and #325 (note: NR button turns NR back on while WeFax holds DSP off; Hany fixed it 10-25, 3a9adae, same code as JS8's dsp_held_cb: on #324 as 3af8902, merged into #325). Hany (email 10-25): NavTex/SITOR-B/DSC decoder ready (beats Venturi's in 95% of his regression set), SITOR-B text view to match RTTY's font setting first; may open his first PR himself. **Still to come:** NavTex (now LGPL, own SITOR-B receiver) and DSC (Nayuki QR code, MIT) on APP
  page 5 (a shared "page 5 on" commit as with page 4); webserver SSTV page (OpenX6100/x6100_webserver#6, needs
  Georgy's review, then a webserver tag and a Buildroot version bump); mic MODE key; FT8 CB (VE7NHW undecided);
  SSTV RX/TX radio test. Email to Hany about NavTex/DSC: drafted 10-25.
- [x] **Hany's 1KO125 1.1-beta1 = the reference for his PRs (Hany's email, 2026-10-27; tag d65e605).** VE7NHW: get WeFax
  Post Proc. in now for a release soon. **OpenX6100 #329** (branch `wefax-postproc`, base `wefax-denoise` until #325 merges):
  Hany's 12 commits (neural-network Post Proc., model `rootfs/usr/share/x6100/wefax_pp.bin` 1.2 MB, full-screen fax view);
  training kit and NOTICE header lines left out; WeFax files otherwise identical to the tag. GCC 12 clean; PC test at 6 dB
  (`~/Pictures/wefax-postproc-pc-test.png`). **#328 (10-28, Kris's email for Georgy: no empty buttons):** pages 3/4 without empty places, page 5 gone: APP 3 WiFi, BT, JS8, WeFax; APP 4 SSTV, Channels (a6d8dc4; kris-apps-test3 still has the old page 5). Before that: Channels moved to place 4 of APP page 5 (58828f3), matching his
  NavTex / DSC / FT8 CB places; VE7NHW: FT8 CB not coming in. Reply email to Hany drafted 10-27 (in the chat and clipboard; no licence questions, VE7NHW's wish): VE7NHW sends it.
  **Test image:** branch `kris-apps-test3` f38b0d7 (kris-apps-test2 + #328's move + #329), test-image.yml run
  **40572901929**; replaces kris-apps-test2. **Next:** VE7NHW flashes and tests the kris-apps list plus WeFax Post Proc.
- [x] **Kris's apps (WSPR #271, PSK31 #272, Olivia #274, RTTY #276, FT2 #288) ready for v1.3.0 (2026-10-25, VE7NHW: "fix all the issues, PRs for all").**
  **2026-11-01 (after v1.2.0, all five conflicted with main):** branches `wspr-v13`, `psk31-v13`, `olivia-v13`, `rtty-v13`,
  `ft2-v13` in OpenX6100 (worktree `~/Work/x6100/wt-kris-v13`), each = Kris's branch + our open fixes + main (v1.2.0);
  shared commit `app-pages-v13` 3e46387 (page 4 one app per line). Layout (PR order): APP 4 = SSTV, Channels, WSPR,
  PSK31; APP 5 = Olivia (labels "APP n:5"). digi_menu.c: the branches' copy with the VOL fix (main's Callsign /P list
  gets it too). All ten pairs merge cleanly. **PRs myshak1#19 (WSPR), #20 (PSK31), #21 (Olivia), #22 (RTTY), #23 (FT2)**
  (supersede #8-#18), noted on #271. Combined branch `kris-apps-v13` 48ed610, test image run **41362789243**.
  **2026-11-02: Kris merged #19-#23 into his branches** (closed #8-#18 as folded in); layout OK with him, **JTTY will
  follow on page 5 after Olivia**. All five OpenX6100 PRs now approved + MERGEABLE. He asks **VE7NHW or Georgy to merge
  #271/#272/#274/#276/#288 for 1.3.0 once tested**; he tests the image (run 41362789243, succeeded) on his radio first.
  **Next:** VE7NHW (and Kris) test that image; then VE7NHW merges the five in OpenX6100 (the merge button is his).
  **2026-11-02 later:** #332 (S-meter ±10 dB) **merged by Kris**. **#333 JTTY** (Kris, +5284, GPL-3.0, APP 5 after
  Olivia, MODE_JTTY 10, app 118; review asked of VE7NHW + Georgy): reviewed by Claude (TX only on a key, power/freq
  restored, clean close; merges with all five; JTTY files GCC 12 clean); notes: ESC as the TX thread wakes keys 250 ms of
  silence, copyright-name edits in shared files, two shared level codes later (steady vs swinging modes). **Approved 11-02** (VE7NHW OK'd). Six-app test image: branch `kris-apps-jtty-test` (OpenX6100), run **41507827952**.
  **#329:** Kris (review, 11-02): wants the model's training kit (tests/wefax/ml, in Hany's 1ko125), the model's licence
  and training data, the NOTICE line gone (done 13271c3), Hany's empty author e-mail, Post Proc. time on the radio.
  **Reply posted 11-02** (VE7NHW OK'd, licence question included): asks Hany OK to add the kit without .exe, the model's
  licence + training charts, his author e-mail; we measure Post Proc. time on the radio.
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

## Moved 2026-10-26

- [x] **v1.2.0: JS8 4/4 + the Murus apps into OpenX6100 (2026-10-22).**
  **#311 retargeted to main** (VE7NHW's go) and murus-wefax moved up to the
  combining commit (Channel List + mic keys, a7cb47d), so #311 carries #310;
  description and comment updated. **JS8 header lines** (VE7NHW: "Copyright (c)
  2026 Randal Warren VE7NHW" + "Part of OpenX6100", as Kris's 10-22 commit;
  the ported assembler/render and test_js8.cpp also credit the JS8Call-improved
  authors): #308 46c5e8f, our main 59a738e. **Test image of all four failed at
  the link** (run 39849277139: undefined tx_player_base_gain_offset): our
  merge 92e6ec6 (2026-10-20, app-page4-on into murus-wefax) kept the old
  CMake source line and dropped #306's tx_level.c/tx_player.c; nothing on
  main calls them, so compile checks passed. **Fixed** 320cffb (#311) and the
  same commit 1632779 (#312); checked nothing else lost; note posted on both.
  **Rebuilt: run 39857680626, success**, downloaded to builds/ci-39857680626/
  (branch `four-apps-test` 462eca3 = main 2f00e11 + js8-app 46c5e8f +
  murus-wefax 320cffb + murus-sstv 1632779; break check clean). NR2 selector
  left out (VE7NHW: option B; it cherry-picks cleanly if wanted, private build
  only). **Kris's release plan** (email 10-22, agreed): merge #308, #310,
  #311, #312, then bump-v1.2.0 and #321 (approved by us), fixes only, Kris
  radio-tests main before tagging. v1.3.0: OpenVoiceCommand (in the image,
  Georgy), PSK31/Olivia/WSPR/FT2/JTTY/RTTY moved onto tx_player; we asked Kris
  to add JS8/WeFax/SSTV/Channel List to its windows and whether its power/NR/
  filter commands respect JS8's locks; JTTY needs an APP place (offered help).
  **2026-10-24 (Kris's email):** he checked all four still merge in order
  onto main with tx_level.c/tx_player.c in the build list; merges them
  10-25 (#308, #310, #311, #312) unless VE7NHW's test finds something, and
  retargets #312 himself. On our v1.3.0 questions: JS8, WeFax and SSTV are
  in its windows list (Channel List after #310 is on main); while JS8 or
  any digital window is open it refuses power above 5 W, NR/NB/notch on and
  filter changes. **Tested 2026-10-24 evening by VE7NHW:** all four open and work, APP places right, JS8 all good, Channel List from the mic keys, WeFax fine; SSTV opens (no SSTV signals on the band, receive not tried on air). VE7NHW: merge all four; reply to Kris says go ahead. **2026-10-25 13:41-13:42Z Kris merged #323, #308 (JS8 4/4) and #310.** #311 shows "conflicting" on GitHub although git merges it cleanly into main 0a7daa0 (checked: real merge, break check and build list clean; #312 then merges cleanly on top). Fixed with VE7NHW's OK: main merged into murus-wefax (b6fa921), GitHub shows it mergeable again; note on #311. **JS8 in OpenX6100 main = our main** except the intended three: JS8_APP_VERSION "OpenX6100 JS8 " VERSION, rotary_cb's dt (#300), one UPSTREAM.md note; against RC1.1 only header lines and #320's test fix. **#311 and #312 merged by Kris 10-25 18:24/18:42Z; #326 (bump to v1.2.0, Kris) approved by us 10-25.** **Next:** Kris radio-tests main and tags v1.2.0; then bring our main up to OpenX6100 main (break check, build-list check).
  **Done 2026-10-25:** Kris merged #323, #308 and #310 (13:41-13:42Z), then #311 (after our main merge b6fa921 cleared GitHub's false conflict) and #312 (18:24/18:42Z); #326 (bump to v1.2.0) approved by us. Leftover: the tag and our fork, new item "v1.2.0 tag" in PLAN.md.
- [x] **JS8 into OpenX6100**, four pull requests one at a time
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
  **2026-10-16:** #301 approved by gdyuldin; Kris's three points done (b040b8f) and answered; Buildroot #8 (Boost/FFTW, licence label) opened. **3/4 and 4/4 prepared by cloud sessions:** draft PRs here #16 (`js8-library`) and #17 (`js8-app`, merges OpenX6100 main 9714584 first, since js8-engine sits on js8-groundwork), both checked on this PC (files identical to main, GCC 12, CI green). **Next:** Kris's OK + Buildroot #8 merged -> VE7NHW merges #301; then 3/4 as a real PR from `js8-library`, then 4/4; 4/4 gets the tx_level fix when there is one.
  **2026-10-18: Buildroot #8 then #301 merged by VE7NHW** (f032490; Georgy approved both, Kris left the final word to him), **#306 merged** (49328dd). **3/4 = OpenX6100/x6100_gui#307** (branch `js8-library`, 6367a0f: rebased onto main 49328dd, four commits; the "drive-level tests wait" commit dropped since they are #306's test_tx_level; the workflow also runs test_tx_level; files identical to our main 728b8d0; 29 src/js8 files GCC 12 clean; tests green on the PR): opened as draft, marked ready after VE7NHW OK'd the text, reviewers gdyuldin + myshak1. **4/4 = #308** (branch `js8-app`, 2133235, draft, base `js8-library` so only its own commits show): app files from main, button/case/build (no tx copy: #306's), and **JS8_APP_VERSION = "OpenX6100 JS8 " VERSION** (main.h, so "OpenX6100 JS8 v1.0.2" now; VE7NHW's choice 2026-10-18: OpenX6100's own name and number; our fork keeps "X6100 JS8 <release>"). GCC 12 clean except dialog_js8.c (needs gpsd headers: image only); break check clean; tests green. Test image (OpenX6100 + JS8 only): test-image.yml run **39139987043: built** (js8-app 2133235, before the #313 merge; dialog_js8.c compiles with GCC 12), downloaded to builds/ci-39139987043/, not flashed. Texts in docs/upstream-prs/20-js8-library-pr.md, 21-js8-app-pr.md. **Next:** image result into #308; VE7NHW radio-tests it if he wants; #308 ready once #307 is merged (GitHub retargets it to main); #300 then gives JS8 the `dt` argument (as all-apps-test 4c159c5); our drafts #16/#17 to close.
  **Done 2026-10-25: 4/4 (#308) merged by Kris.** JS8 in OpenX6100 main = our main except JS8_APP_VERSION ("OpenX6100 JS8 " VERSION), rotary_cb's dt (#300) and one UPSTREAM.md note.
- [x] **Murus apps (WeFax, NavTex, SSTV) into OpenX6100, for Hany
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
  **2026-10-15/16:** Hany's full OK (LGPL headers, 7(b) dropped). Cloud drafts #12-15 done, then rebuilt here as `ox-murus-groundwork/-wefax/-sstv/-channels` on OpenX6100 main 9714584 (Hany's commits as received, ours under VE7NHW, licence commit on top; GCC 12 clean); drafts #12-15 closed, their branches deleted. VE7NHW: send as Hany wrote it (WAV recorder, mic keys), reviewers decide in the PRs. Info window licence text asked on #302; **Hany OK 2026-10-17**, changed on `ox-murus-groundwork` (e574b53: LGPL-2.1-or-later, "Originally from the 1KO125 firmware, part of OpenX6100", source link instead of the missing /usr/share/x6100/COPYING; his credit kept). `ox-murus-wefax` / `-sstv` rebased onto it (b953082 / 5f66c00, pushed with VE7NHW's OK). In all-apps-test on the card. **Next:** radio test; real PRs after Kris's apps are merged (groundwork first).
  **2026-10-18 (VE7NHW: send what doesn't clash; he had opened the apps on all-apps-test, "opened fine and kind of worked", no full test):** trial merges: groundwork and Channel List merge cleanly with main 49328dd and with Kris's #271/#272/#274 (groundwork vs #276: src/CMakeLists.txt only); WeFax/SSTV clash with #271/#272/#274 in buttons.cpp / main_screen.c / settings_types.h. GCC 12 clean. **Opened: groundwork = #309** (branch `murus-groundwork` = ox-murus-groundwork e574b53), **Channel List = #310** (`murus-channels` = 87c7375), reviewers gdyuldin + myshak1; texts in docs/upstream-prs/22-, 23-. **Then (VE7NHW: the places are known, send them now, as in our test build): WeFax = #311** (branch `murus-wefax` = ox-murus-wefax b953082, base murus-groundwork) and **SSTV = #312** (branch `murus-sstv` = ox-murus-sstv + a merge of WeFax, 72f16be: page 4 = empty, empty, WeFax, SSTV; 109 / 111; break check clean; GCC 12 clean; base murus-wefax; copy in our repo as ox-murus-sstv-stacked). Texts 24-, 25-. Known clashes (trial merges): WeFax/SSTV vs Kris's #271/#272/#274 (buttons.cpp, main_screen.c, settings_types.h), WeFax/SSTV vs Channel List #310 (main_screen.c mic keys). **Next:** whichever is merged second, a merge commit from us (rerere has the WeFax/SSTV resolution); when #309 / #311 merge, retarget #311 / #312 to main (`gh pr edit N --base main`); SSTV TX on the dummy load still untested; tell Hany on #302. **Same day, VE7NHW: every button in its final place from the start, as in all-apps-test** (page 3 WiFi, BT, WSPR, JS8; page 4 PSK31, Olivia, WeFax, SSTV). Ours already were (JS8 #308 page 3 last; WeFax/SSTV page 4 places 3/4; Kris's WSPR #271 page 3 place 3). Kris's PSK31/Olivia both sat in WSPR's place: **myshak1/x6100_gui#5** (psk31 -> page 4 place 1, branch psk31-app-page4) and **#6** (olivia -> page 4 place 2, olivia-app-page4), page 4 added as #311 does. Waiting for Kris.
  **Then (VE7NHW: "how it is in the test build is how it should be, no matter which is approved first"): OpenX6100/x6100_gui#313** (branch `app-places`, f84c00c, from main 49328dd): page 3 WiFi, BT, WSPR, JS8 and page 4 PSK31, Olivia, WeFax, SSTV with each place NULL until its app comes, and in buttons.cpp, settings_types.h, main_screen.c, src/CMakeLists.txt a comment line per app with its lines below it (a name line between any two apps' lines: git merges them in any order; tried on a scratch repo first). All six app branches take in #313's commit (a merge) and move their lines under their names: js8-app a6eb3c9 (#308), murus-wefax 0137f53 (#311), murus-sstv c305264 (#312), Kris's psk31-app-page4 f1a12b1 (myshak1#5), olivia-app-page4 29414fb (#6), wspr-app-places 6793ccf (**myshak1#7**, new). Checked: all 15 pairs merge cleanly (git merge-tree), all six together on main give exactly the test-build layout, GCC 12 clean, break check clean. Scripts: scratchpad slots.py / fill.py (not kept). Still clashing, outside the places: #310 vs WeFax/SSTV (main_screen.c mic UP/DOWN), #276 vs WeFax/SSTV (src/CMakeLists.txt, spectrum.h), #288 vs main itself (Kris's). Note to Hany posted on #302 (discussioncomment-19020516). **Kris merged myshak1#5, #6, #7 the same afternoon (16:07-16:08Z)**: #271/#272/#274 now use the fixed places; checked: Kris's three and our three (js8-app, murus-wefax, murus-sstv) merge pairwise without conflicts. **Next:** reviews of #313; the one-time merges above when they come up.
  **2026-10-20: Georgy and Hany on the PRs, all done the same day (VE7NHW: do all recommended; bring them up to Hany's latest beta):** #309 recorder back to MP3 (Georgy questioned it, Hany: development only; a4c450d) + Web Manager line out of the SSTV Info text (55cf07c); #311 WeFax: Test removed, fax names in UTC (111cb98), beta 5's decoder change (be6bbe1, Hany author); #310 Channel List: channels.json via .tmp + fsync + rename, window recall respects the freq/mode lock, unused functions out, **list starts empty** (VE7NHW's pick), and with no channels mic UP/DOWN tunes a step as on main (4d4135b); #312 SSTV **brought up to Hany's 1.0.2 beta 5** (304d2e7 Hany author: Denoise CLASSIC/ML with sstv_dnn.bin, listening on during the final decode, TX panel, REPLAY picks a picture; new files LGPL 4ce2f85, same terms as before, VE7NHW) + freq/mode/band locked while transmitting (user's own locks restored), Test RX and the test-TX branch and all unused code removed (c5508c4). Replies posted on #309-#312. Beta 5 tarball sha256 aee25201..., unpacked in ~/Work/x6100/reference/1ko125-1.0.2-beta5/. All branches GCC 12 clean; pairwise merges clean except #310 vs WeFax/SSTV (mic keys, known). **Next:** radio test of the four (SSTV receive, then TX into the dummy load); reviews.
  **2026-10-20 (evening): Kris's reviews, all answered (VE7NHW: do 2-7).** #314 v1.1.0 bump merged (release coming; JS8 not in it). **#300 merged by VE7NHW**; JS8 4/4 brought up to it (rotary_cb gets dt; main merged; sends "OpenX6100 JS8 v1.1.0"). #309 approved by Kris, merge after v1.1.0 (agreed). #310 approved; README-broadcast.md added (EiBi, season A26, terms: free to use/copy/distribute; B26 from 2026-10-25), asked Hany about refreshing /mnt/broadcast.csv when the firmware's copy is newer. #311 changes requested: after v1.1.0; page 4 on (done: shared commit app-page4-on 022475a on Kris's **#318** "no empty page 4", which we approved; in #311/#312 and offered to Kris as myshak1#8 PSK31 / #9 Olivia); V/M+UP/DOWN with the Channel List: proposed Hany's way (WeFax open: its list/stations, else the Channel List, none saved: a step); **Hany agreed on #311 (2026-10-20)**; the combining merge is ready on branch `wefax-channels-merge` (a7cb47d, OpenX6100; GCC 12 clean, merges with SSTV): bring it into whichever of #310/#311 goes second. **#319** (Kris: FT8/FT4 TX 0.5 s into the slot) approved by us with two notes. ptt.log (#316) moved to lines of its own so it merges with every app PR. Pairwise merges: all clean except #310 vs #311/#312 (mic keys; the combining commit when the second goes in).
  **Done 2026-10-25:** #309, #310, #311, #312 all merged. Leftover (NavTex, DSC, WeFax update #324/#325, webserver page, mic MODE key): item "Hany's apps, after v1.2.0" in PLAN.md.
- [x] **all-apps-test (d91a675 on the card since 2026-10-17):** ~~JS8 TX at 5 W~~ done 2026-10-17 (ALC fine at every power); VOL with Menu/Macros open in RTTY, PSK31, Olivia (Kris's #4 fix, merged by him); FT8 knob fast (#300, merged); Kris's WSPR (marker, decode window), PSK31/Olivia (waterfall stops while keyed, averaging), RTTY's new page 2; Hany's WeFax, SSTV (TX on the dummy load first), Channel List (mic V/M, GENE, NW, UP/DOWN); the first boots with the console connected (FT2).
  **Superseded 2026-10-25** by the kris-apps image (run 40221803013): its open checks are in the "Kris's apps" item in PLAN.md.

## Moved 2026-10-22

- [x] **Kris's reviews of 2026-10-21, all done the same day (VE7NHW: "do all recommended").**
  #311/#312 **didn't compile** (our beta-5 update left WeFax's Clear printing
  removed decoder functions; our PC check lacked the build's
  -Werror=implicit-function-declaration, now added): fixed d4cb0df, merged
  into murus-sstv f0a4eaf and wefax-channels-merge. #316 ptt.log: copied
  source name (no freed thread buffer), "Bluetooth PTT" source, writer
  thread + queue (PTT never waits for the SD card), SWR wording (143c336,
  ThreadSanitizer-clean on the PC). #315: noise-floor levels a switch, on
  in FT8 only (c99052f). #310: broadcast.csv read in place, no copy
  (c84c25f; also in wefax-channels-merge 71c0a18). Replies posted.
  #308 and #320 approved by Kris; **fresh JS8 4/4 test image: run
  39668627012** (js8-app 2851576 + main 6459858 = v1.1.0).
  **Later 2026-10-21/22:** Kris **approved #311, #312, #315, #316** (all of
  today's fixes). #310: Kris found the schedule loaded at every boot (11.5
  MB RAM): now loaded on first GENE/NW, texts in one pool, shrunk (482a91a,
  0.8 MB; same 231,425 matches as before on the PC); reply posted; in
  wefax-channels-merge f800528. Hany replaced the SSTV pictures (colour
  bars + test cards, AI generated, TheMurusTeam/x6100_gui 1ko125 d9932045)
  and answered on #312; **in #312 since 2026-10-22 (a53be16, Hany author, VE7NHW
  asked; auto mode needed his explicit OK for a third party's files)**;
  reply posted. Hany's new repo TheMurusTeam/x6100_gui (fork of
  OpenX6100, branch 1ko125 on v1.1.0): new LGPL NAVTEX/SITOR-B decoder
  (fldigi GPL code removed), DSC, WeFax Denoise/fading demodulator: later
  PRs (NavTex app; WeFax changes as a follow-up to #311, not into it).
  4/4 image run 39668627012 **built**, in builds/ci-39668627012/.
  **Was Next:** (1) **SSTV default pictures** (tx_pics/defaultpic1-4.png, Kris
  asked where they come from): they are Tweety/Sylvester, Popeye, the
  Minions, Tom and Jerry: studio-owned characters that can't ship in the
  image. VE7NHW + Hany decide what replaces them; then answer Kris on #312
  (also: SSTV's own TX level vs tx_player, "later" is fine). (2) VE7NHW
  radio-tests the 4/4 image, then merges #308; #320 and #309 can be merged
  now (Kris approved). (3) EiBi B26 into #310 when published (404 on 10-21).
  **Closed 2026-10-22:** all reviews answered and approved; #320/#309/#316/#315/#305 merged by VE7NHW; the SSTV pictures replaced (Hany's, in #312); the 4/4-only image dropped for the four-apps image (see *v1.2.0*). Leftover: EiBi B26 into #310 (own item).


## Moved 2026-10-20

- [x] **Release Candidate 1.1 (VE7NHW 2026-10-17): the transmit-level fix.**
  On main: 6a5d879 (version "X6100 JS8 RC1.1", README *New in Release
  Candidate 1.1*, docs/releases/js8-rc1.1.md with every feature); the
  four tx files identical to OpenX6100 #306; drive-level tests in
  tests/test_tx_level.cpp. Tests run 39093417631 on main. **Not yet:**
  the tests.yml step for test_tx_level (the login lacks the `workflow`
  scope: `gh auth refresh -h github.com -s workflow`, then add
  `test_tx_level` to the build targets and a step, as in commit eca5c6b
  on the local branch rc11). Tests run 39093417631 green; **tag
  `js8-rc1.1` pushed, Build image run 39093596329 started 2026-10-17.**
  **Published 2026-10-17** (run 39093596329; page titled with the notes,
  prerelease; sha256 bd5cc449... matches, copy in releases/js8-rc1.1/).
  Left: Facebook post. **tests.yml step done 2026-10-18** (36d679f, after VE7NHW's `gh auth refresh`). Done by the background job: `gh release edit js8-rc1.1 -R randal007/x6100-js8 --title "X6100 JS8 Release Candidate 1.1" --notes-file docs/releases/js8-rc1.1.md --prerelease`,
  sha256 check, and a Facebook post (the RC1 one is the template:
  ~/Pictures/x6100-js8-rc1-facebook/post.txt).
  **Closed 2026-10-20:** published, tests.yml step done; the Facebook post is in *In short* (VE7NHW, any time).

- [x] **One transmit-level (ALC) module for every digital app (2026-10-14,
  VE7NHW: needs fixing; PSK31 and Olivia put out less than JS8 with the
  XPA125B).** FT8's tx_worker corrects per chunk from single readings
  (saves its offset); PSK31/Olivia/WSPR copy it but only read FT8's offset,
  never save. Ours (tx_level.c, beta 4.6) averages, ramps, holds at the
  ALC's onset, saves. PSK31 reads lower partly by nature (average < peak).
  **Discussion opened: OpenX6100/x6100_gui#303** (text in
  [upstream-prs/15-tx-level-discussion.md](upstream-prs/15-tx-level-discussion.md)):
  offered tx_level/tx_player as a PR of their own before JS8 4/4.
  **Next:** their answer; a JS8-vs-Olivia wattmeter comparison if useful.
  **2026-10-16:** Georgy + Kris agree (one stored level + PSK31 correction, clamp the stored value; Kris moves WSPR/PSK31/Olivia/RTTY/JTTY once we send it). **VE7NHW: our tx_level stops at 3.8 W of 5 W, no amp, ALC 0, in JS8**: told them on #303 we measure first. Suspects (from the code): drive at its 0 dB ceiling, the radio's own power reading saying ~5 W, or the 8 dB ramp limit. Log ("TX level:" every 0.5 s) in all-apps-test, on the card. **Next:** one JS8 TX at 5 W on the dummy load, read /tmp/x6100_log.txt over the console, fix, then the PR (with the clamp).
  **2026-10-17 (cloud session): likely cause found, branch `tx-level-fix`** (from main 19dd03a, test image Build image run 39059735209, Tests run 39059737055). IU1WXY's copy of our tx_level.c (his repo, c2e419d) found it: the radio sends only ~2.5 readings a second (his figure, not yet ours), so our window of 12 *readings* was ~5 s, two decisions a JS8 frame, ~1 dB up a transmission; our #303 post ("½-second averages, 1 dB/s") described the code wrongly. Branch: 21797ed window = 12 *blocks* (as his) + ALC "shows" from 0.3 not 0.1 (with 1-2 readings a window one stray 0.1 held it at 1.9 W of 5 in the model); f7214fe tx_player_stream() (WeFax/SSTV) + learned level floor -42 dB (Kris's clamp; 5 W needs ~-36 dB total on rev 3 bases per IU1WXY and Hany); a099c48 TEST log (readings per window, base rev; drop before the PR). Unit tests: new 2.5-readings case (fails on main, passes), lag sweep both ways, fast suite green. Not in it: PSK31 correction (Kris's apps not on main), IU1WXY's ALC 1/3 thresholds. **VE7NHW asked for it in all-apps-test instead (2026-10-17):** 21797ed, f7214fe and a099c48 put on all-apps-test after reverting 45358a2 (the old TEST log, which a099c48 replaces): d91a675, tx files identical to tx-level-fix, GCC 12 clean, Build image run **39059910797**; tx-level-fix's own image run 39059735209 cancelled. **2026-10-17: radio-tested by VE7NHW** ("ALC works great on any power output"; log: 5 W reached in the first transmission, 2.3 -> 5.2 W in ~6 s, held 5.4-5.5 W; 0.3 W held; an overdriven start, ALC 3.5, came down to 0.4 in ~5 s; **6-7 readings per half second**, so IU1WXY's 2.5/s doesn't hold on ours, and which of the two changes fixed 3.8 W isn't separated). **Sent as OpenX6100/x6100_gui#306** (branch tx-level b66a969: module on its own, generic comments, PSK31 correction as a base_gain_offset term, tests/test_tx_level.cpp 6 cases); pointer on #303. **On main and released as RC1.1 (2026-10-17)**: tx-level-fix without the TEST commit, the four files identical to #306, tests moved to tests/test_tx_level.cpp (CI runs it). **Next:** reviews of #306; JS8 4/4 (#17) to carry it (or drop its copy if #306 is merged first). **2026-10-18: #306 merged by VE7NHW** (Georgy approved; 49328dd); 4/4 (#308) uses it, no copy. Left for Kris: moving his apps onto it. Old plan text: VE7NHW flashes all-apps-test, JS8 TX at 5 W on the dummy load: wattmeter + "TX level: N readings" lines; if 5 W, the #303 PR (from OpenX6100 main, without TEST, with the PSK31 correction and a correction of our #303 description). Nothing posted on #303 yet.
  **Closed 2026-10-20:** #306 merged by VE7NHW 2026-10-18 (Georgy approved); JS8 4/4 uses it. Kris moves his apps onto it himself.

- [x] **FT8 window: the knob moves the TX frequency 1 Hz a click
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
  **2026-10-16: reworked as Georgy asked** (6050dd9: apps' `rotary_cb(diff, dt)`, FT8 speeds up from the knob's `dt`); Kris told his apps need the extra argument. In all-apps-test. **Next:** radio test, Georgy's review.
  **2026-10-18: approved by Georgy.** Held (Claude's advice, VE7NHW asked why): it changes every app's knob callback, so Kris's WSPR/PSK31/Olivia would no longer match; either order works, the question is who updates them. Plan: after Kris merges his apps, a commit on #300 updating their rotary_cb, then VE7NHW merges.
  **Closed 2026-10-20:** #300 merged by VE7NHW (Georgy and Kris approved; Kris updates his apps' rotary_cb); JS8 4/4 takes dt. The radio check of the knob is in *To try on the radio*.

## Moved 2026-10-17

- [x] **IU1WXY firmware (Giovanni Ricciardi IU1WXY, github.com/IU1WXY/X6100-IU1WXY)
  takes OpenX6100 and our work, sometimes from open PRs (2026-10-16).**
  Kris emailed him (cc gdyuldin, VE7NHW) asking to wait for OpenX6100
  releases; Giovanni won't wait, "door open". VE7NHW replied backing Kris
  (wait for releases; fixes back as PRs welcome). Checked in his repo
  (2026-10-16): licences in order (GUI LGPL, js8core LICENSE kept, image
  GPLv3, source public per tag), our JS8 headers kept, credits "From
  randal007's x6100-js8", JS8_APP_VERSION "X6100 IU1WXY" (on-air STATUS
  tells his builds apart); everything of ours he has was released first,
  except Bluetooth security #297 (per Kris's list). His credits list "the
  integration of JS8 ... into the radio" under his own work. VE7NHW doesn't
  like how he works. **Offered, not decided:** a "Using this code" note in
  the README (take from releases, send fixes back, report bugs on other
  firmware to its author) and a firmware question in the issue template.
  Waiting for Giovanni's answer.
  **2026-10-17, closed:** Giovanni wrote he was removing or rewriting our
  code; his v0.127 (same day) says "rewritten as own code": the JS8 app
  and library renamed, ~5% of lines matching ours (was ~93%), the layout
  and comments still following ours; js8core still our copy (371 of 379
  files identical) without UPSTREAM.md. **VE7NHW: no more emails, ignore
  him** ("stealing ideas and rewriting them without helping the
  community"). The reply drafted that day was not sent. (This entry was
  dropped from PLAN.md in e140beb without being moved here.)

## Moved 2026-10-16

- [x] **Licence lines on our JS8 files tidied (VE7NHW, 2026-10-16):** the 52
  files that follow desktop JS8Call (app, waterfall, most of src/js8,
  test_js8, js8_wavgen) GPL-3.0-only -> **GPL-3.0-or-later** as desktop's
  (a8aa43c); the LGPL ones stay; tests/test_js8_db.cpp got its LGPL line
  (in OpenX6100 since #290: carry it in 3/4); README *License* lists the
  split (e99a61c: names only desktop as "or later", js8core's LICENSE says
  GPLv3 only). Drafts #16/#17 already carry the new lines.

## Moved 2026-10-16

- [x] **Helping Kris: PSK31, Olivia, FT2 conflicts (2026-10-14, VE7NHW
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
  **Done 2026-10-16:** Kris merged main into his branches himself; myshak1/x6100_gui#1-3 closed, our `psk31/olivia/ft2-merge-main` branches deleted.

- [x] **PSK31 / Olivia tried on the radio (digimodes-test, 2026-10-14):**
  comments posted on #272 and #274 (VE7NHW's findings): the waterfall
  keeps scrolling while keyed and shows blank static (not the band, not
  our TX; TX itself fine, full power; first comment wrongly said TX audio,
  corrected) (suggested: ignore capture while keyed, as JS8, pause or
  black rows); the scrolling flicker JS8 had (suggested, optional: our
  row averaging, avg += a*(row-avg), a 0.65/0.5/0.35). Waiting for Kris.
  **Done 2026-10-15/16: Kris took both suggestions** (#274 f55afef7, #272 35296e06: waterfall stops with a red frame while keyed, rows averaged at 0.5; Olivia text follows *Decoder text*). Retest: in "To try on the radio".

- [x] **WSPR test build (2026-10-14, VE7NHW wants to try WSPR):** branch
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
  **Closed 2026-10-16:** superseded by digimodes-test, then all-apps-test (Kris's d6377057 is in); branches wspr-test, wisdom-wspr-test, bt-security-js8 deleted. Retest: in "To try on the radio".

## Moved 2026-10-15

- [x] **JS8 hangs 5-13 s when closed soon after opening: FFTW measured on
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
  for Kris), *Build image* run **38637209733** (on the card). Then + Kris's
  **RTTY transmit #276** (approved by gdyuldin, waiting for Kris to
  merge; merged cleanly) + the unstripped-GUI CI step: digimodes-test
  24c9481, *Build image* run **38651955343**; flash it with core dumps set
  up on the card before its first boot (crash hunt).
  **Next:** flash when VE7NHW says; time the one-time setup, APP during
  it, then reopen twice and read "first decode pass since JS8 opened".
  **Done 2026-10-15: radio-tested by VE7NHW on digimodes-test** (the setup
  runs once after a new image, decodes from the start on every boot after,
  and JS8 closes at once when closed a few seconds after opening, first and
  second open alike: the second-open puzzle didn't show). **PR #11 merged
  into main** (1b36ead; main merged into the branch first, PLAN.md conflict
  only). Goes to OpenX6100 with 3/4.
  Earlier: **To a cloud session (VE7NHW 2026-10-14, using the Claude Code cloud
  credits, which expire 2026-11-04):** brief in
  [tasks/fftw-wisdom.md](tasks/fftw-wisdom.md); it opens a PR on branch
  `fftw-wisdom`, then image + radio test here. Then the same credits for a
  deep bug review of JS8 parts 3 and 4 (library + app) before they go to
  OpenX6100. FFTW is used only by the decoder (checked): no other planner
  in the GUI program to clash with.
  Side note: the main decode lane is 25-38 % busy on the radio with a few
  windows waiting (rx_all on, 40 m): a separate question.

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
