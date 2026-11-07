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
   the runs are green; a test package 3 with the fixes if VE7NHW wants. **DSC = #353** (Hany, 119; not reviewed yet). **#348 reviewed**, comment posted by VE7NHW (issuecomment-6700528697: region borders, own PR for itu_region.c; take his fixed file into #344). **#350 reviewed:** 10 findings (Delete hits the wrong bulletin, live text stops following, empty bulletin won't load...), comment for Hany not written yet; his .tmp saves (#350 16627169, #353 b40253a0) checked OK. **Test package 3 built (11-06):** public run 42319770123 green (tests 42319773048 green, artifact expires 12-07); the private one too. #350 comment for Hany drafted, VE7NHW posts.
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

- [ ] **XARA into OpenX6100 (VE7NHW, 2026-11-06): groundwork as two draft PRs, a test build (not a release).**
  VE7NHW: "bring this into OpenX6100 now", experimental, the build that worked best (build 22, xara 919f5dd), next free
  APP place. Decided: **built in, button shows; draft PRs; every setting of the test builds kept, Full speed
  included** ("it's just a test build"). **OpenX6100/x6100_gui#345** (branch `xara-app` d6fb6491, worktree
  `~/Work/x6100/wt-xara-app`: build 22's dialog_xara.c + EA5HVK credit; APP 5 after JTTY, number 120 since 11-06: DSC is 119) and
  **OpenX6100/AetherX6100Buildroot#10** (branch `xara` 479755f, worktree `~/Work/x6100/wt-br-xara`: package xara from
  randal007/xara 919f5dd + host-xara-rust = Rust 1.97.1, crates vendored at download; **`BR2_PACKAGE_XARA_REGISTERED`**
  adds the paid speeds, **off in X6100_defconfig**: Kris's free-speeds condition, and public images would hand them to
  everyone). Without it xara refuses Full speed. PC check: vendor + offline build OK; ARM syntax check OK.
  **Test images:** VE7NHW's, with registered speeds: private **randal007/x6100-xara run 42273143987** (test-image.yml
  got an `xara_registered` switch, 4a83d407; never share it; may take ~4 h without ccache); public free-speed one for
  the PRs: **x6100-js8 run 42273148030**. Reviewers gdyuldin + myshak1 added by VE7NHW (`!`).
  **Full speed by default:** VE7NHW wants it on in X6100_defconfig; asked Kris + Georgy on Buildroot #10 (his comment
  2026-11-06, issuecomment-6692795901). If both agree: add `BR2_PACKAGE_XARA_REGISTERED=y` there.
  **Next:** check both runs; VE7NHW flashes the private one when he says; then the PR to-dos (ports locked to the
  phone, low-power TX level, libxara 0.1.4, manual, release decisions on Full speed/Record/test settings).
- [ ] **Channel List still crashes on CLOSE with an empty list (VE7NHW, 2026-10-28, on c910ca5 which has #330).** Opening
  is fixed; closing the empty list restarts the GUI. Not found by reading close_common()/overlay_close()/dialog_destruct();
  the PC test (empty table deleted) doesn't crash. Not fixed yet (VE7NHW: later). **Next:** core dump on the radio as for
  #330 (prlimit + core_pattern to /tmp/cores), and ask VE7NHW how it was closed (F1 Close, V/M, APP/GEN) and from where
  (main screen or over an app).
  Channel List *open* on an empty list is fixed (#330, in v1.2.0); this close crash shipped in v1.2.0 (VE7NHW 11-01: probably not an issue, test later).
- [ ] **1.3.0: Kris's five apps + JTTY, tested and merged (2026-11-02).** All five (#271 WSPR, #272 PSK31, #274 Olivia,
  #276 RTTY, #288 FT2) approved and mergeable after Kris merged our myshak1#19-23 (v1.2.0 merged in, our #10-18
  folded in). Layout agreed with Kris: APP 4 = SSTV, Channels, WSPR, PSK31; APP 5 = Olivia, JTTY. **#333 JTTY** approved
  by us 11-02 (notes: ESC/250 ms key-up, copyright-name edits outside the app, two level codes later); Kris fixed 1 and 2 (dd9c698, 234ad35), agreed 3 for #303; **Georgy approved 11-03**.
  Kris asks **VE7NHW or Georgy to merge the five once tested**. **Six-app test image:** branch `kris-apps-jtty-test`
  (OpenX6100, da16317 = main + the five + JTTY), run **41507827952**; flashed 11-03. **11-03:** VE7NHW opened/closed
  every app and sent a couple of CQs on each mode on air, all fine; no QSOs yet. Review report (functions vs desktop
  apps, map mock-ups): claude.ai/artifact/4ectjLqnJbxj1uuUu7RkcL.
  **Test list:** RTTY TX (dummy load), PSK31, Olivia, WSPR (VE7NHW and VE7NHW/P: "plain call only"), FT2, JTTY, VOL
  with Menu/Macros open in PSK31/Olivia/RTTY (only the list closes), APP pages 4 and 5, Channels from APP 4 and a long
  press.
  **Kris (e-mail 2026-11-05): merge all six now, as they are; hold the v1.3.0 tag until the apps are complete** (he's
  working on the review's features, own PRs on main; logging / typing while sending / name+QTH as one PR for
  PSK31/Olivia/RTTY via digi_qso + digi_next). Checked 11-05: all six CLEAN; merged in a row onto main 0fc4305 with no
  conflict, every case has its break, nothing dropped from the build list; the result = the radio's test image + only
  JTTY's ESC fix (dd9c698, read: fine; jtty_tx.c GCC 12 clean). **All six merged by VE7NHW 2026-11-05 20:34Z** (main 2dd8d1e = the radio's test image + JTTY's dd9c698; breaks checked). **Next:** Kris's feature PRs, then v1.3.0.
- [ ] **Review of Kris's six apps vs the desktop programs (2026-11-03), for Kris.** Report written in VE7NHW's voice
  (PDF + source kept locally, see CLAUDE.local.md). Ranked asks: 1 Log QSO in PSK31/Olivia/RTTY, 2 type while sending,
  3 WSPRnet upload/save spots, 4 bench FT2 vs Decodium and WSJT-X Improved, 5 Olivia Status/LOCK % button back (dropped
  10-16, still computed), 6 name/QTH macro fields, 7 RTTY serials, 8 WSPR type 2/3 (/P), 9 Olivia 8/500, 10 RTTY
  AFC/unshift-on-space, 11 shared map. JTTY's 1200-1800 Hz marker limit is by design. **Map decided (VE7NHW 11-03):**
  if done, it's the JS8 map made shared, for WSPR and FT8/FT4/FT2 only (not #287's map; not JTTY: no grids in its
  exchanges). **Sent to Kris by e-mail (PDF) 2026-11-05**, the Olivia LOCK % button asked there. **Next:** Kris's answer.
- [ ] **Shared map for every app (VE7NHW, 2026-11-05): FT8/FT4/FT2 and WSPR first.** Plan, design and steps:
  [SHARED_MAP_PLAN.md](SHARED_MAP_PLAN.md). Branch `shared-map` (from OpenX6100 main 2dd8d1e, worktree
  `~/Work/x6100/wt-shared-map`). Mock-ups round 2 sent (FT8 full map / FT8 split / WSPR full map); e-mail to Kris
  **sent 11-05** (we take point 11; WSPR hook-in his if he wants; asked: WSPRnet ours or his?). **Step 1 done 11-05** (b3e3d74: map_widget.c, JS8's
  map pixel-identical in the harness). **Steps 2-3 done 11-05** (0e1b292: map_scene.c,
  FT8 Show Map; PC test pictures). **Step 4 done 11-05** (58908ba: WSPR map, heard-you
  ready for WSPRnet, farthest DX on both). Test image run 42150892758 failed (U-Boot download), **rebuilt as run 42169441684**. **Kris 11-06: WSPRnet ours; his WSPR map asks (dB tag, heard here / heard you lines, empty map without errors) are already in 58908ba.** **WSPRnet done 11-06 (65058a7e):** spots to /mnt/wspr_spots.txt always; page 3 "WSPRnet" switch (saved, off by default) posts spots/wsprstat to wsprnet.org like WSJT-X and fetches "heard you" from wspr.live every 5 min (plain HTTP, own thread); PC-tested with a stand-in server + real wspr.live read. Code review 11-06: fixes 6907627c (queue race, band dial, redraw per item -> 1 s timer, list+waterfall hidden under
  the map, FT8 Show: CQ map-only messages, FT8 keyboard focus, stale spots, status2 rows), 450db525 (no TX across a
  band change); **merged with #336 (c062cdc3)**: WSPRnet drops its +1.5-tone and type-2 guards, uploads type 3 calls
  without brackets, skips "<...>"; the map skips "<...>". PC checks 11-06 (harness): JS8 map same as before the
  widget; FT8/WSPR map checks pass, type 2/3 placed. In the test package. **Next:** VE7NHW tries FT8's and WSPR's maps
  + WSPRnet on the radio (test package); then the PRs (widget first, then per app; needs #336 for WSPR)
  on the radio; then the PRs (step 5). **Decided (VE7NHW 11-05):** FT8 full map (mock-up 1); WSPR map
  now without WSPRnet, room for "heard you" later; e-mail asks Kris whether we do the WSPRnet part too.
- [ ] **3D spectrum with a 2D/3D switch (VE7NHW, 2026-11-05), like AetherSDR's.** AetherSDR's "3D" is the spectrum
  with its last few seconds stacked as lines going back into the distance (each newer line hides the ones behind),
  waterfall unchanged underneath. Shared GUI (`src/spectrum.c`), so OpenX6100 branch `spectrum-3d` (not created yet),
  test images from our *Build image*, a PR once VE7NHW likes it on the radio. **Drawn by the main processor** (as the
  spectrum and waterfall now, straight into the screen buffer), not the Mali-400 graphics chip: its kernel driver
  (lima) is on, but the image has no Mesa, nothing uses it, LVGL 8.3 can't draw with it, and it can't help with the
  real load (DSP, decoders). Revisit only if the 3D view measures too heavy (x6100-cpulog). **Mock-ups 2026-11-05**
  (`~/Pictures/x6100-spectrum-3d-mockups`, script `~/Work/x6100/research/spectrum-3d/mock.py`, simulated band, real
  top bar and sizes): 1 today's 2D, 2 gold lines (24), 3 coloured by strength (40), 4 coloured at the tallest
  spectrum (260). **Picked (VE7NHW 11-05): look 3** (coloured by strength), plus a **"3D size" %** setting (how much
  of the screen under the top bar is 3D vs waterfall; mock-ups 5 at 35/50/70 %; 35 % = today's 160 px). Proposed: its
  own setting used only in 3D (2D keeps Spectrum height), may go past today's 260 px limit; switch = a display
  setting + tapping the 2D/3D label. **Confirmed by VE7NHW 11-05. Built 11-05:** branch `spectrum-3d` (OpenX6100 and
  our fork, d3b8ef1, worktree `~/Work/x6100/wt-spectrum-3d`): Settings > UI "Spectrum 3D" (off by default) + "3D size"
  30-70 % (default 50), 2D/3D label on the spectrum (tap switches), a trace every 100 ms, smooth drift, history follows
  tuning; ARM compile check OK; PC test of the renderer (`research/spectrum-3d/pctest`, real code cut from spectrum.c)
  matches mock-up 3, ~0.5 ms a frame on the PC. Test image **run 42169391112**. **VE7NHW loves it (11-06, image
  42236779844 = bf28f79).** **Decided 11-06 (VE7NHW):** 3D size 35-55 % (7d8b20b; = 2D's 160-260 px, CW/RTTY panel
  safe), keep both height sliders. **PR #344 opened 11-06** (reviewers gdyuldin + myshak1). **Next session (VE7NHW 11-06,
  he's curious too):** CPU load on the radio, 2D vs 3D: `~/Work/bin/x6100-cpulog` a few minutes each way, same band
  and zoom, radio idle (no TX); result on #344. Then reviews; VE7NHW merges once approved.
- [ ] **Kris's apps review + fixes (VE7NHW 11-06: "fix all issues and open a PR", all six apps): PRs open, waiting on
  Kris's review + radio test.** Findings + status: ~/Work/x6100/research/reviews/kris-apps-review.md. PRs (branches in
  OpenX6100, worktrees wt-*): **#335 FT2** (silent TX 3 of 4 slots!, time sync, SNR +8/+5 dB FT2/FT4: bench vs WSJT-X,
  UDP slot stamps), **#336 WSPR** (band change keyed, TX off, WSJT-X wsprd unpack GPL-3 type 2/3, decode cap 119.5 s),
  **#337 JTTY**, **#338 ADIF JTTY mode** (shared), **#339 PSK31**, **#340 Olivia** (macro rows keyed the radio, deaf
  RX at 32/1000 <610 Hz, TX end click, LOCK % back, Mode into Menu), **#341 RTTY** (FIGS after space, closed app TX,
  squelch default 0), **#342 digi menus 20 rows + Auto DX only when empty** (shared), **#343 panel echo wrap**
  (shared). All merge clean together: test branch `kris-fixes-test` (our repo), tests + image started 11-06.
  Overlap with Kris's unpublished digi-qso-log (PSK/Olivia/RTTY dialogs, digi_qso.c) flagged in the PRs. Not done:
  tx_player for PSK/Olivia/RTTY (#303), PSK FFT off the DSP lock, Olivia 8/500, RTTY AFC/USOS-RX, licences.
  shared-map must be rebased on #336 (drop its +1.5-tone and type-2 guards in wspr_net.c; type 3 calls "<CALL>").
- [ ] **Persistent waterfall (VE7NHW, 2026-11-06): idea, later.** Tuning with the knob leaves the newly uncovered
  edge black until it fills. Wanted: no black. Each waterfall row already keeps its base_freq/width (waterfall.c);
  at zoom 1 the screen shows the radio's whole 100 kHz, so the fill would come from remembering rows already seen
  (a wider band memory), at zoom > 1 maybe also from the unseen part of the received span: check what dsp hands over.
  3D spectrum (spectrum-3d): **VE7NHW loves it** (11-06, on the radio). Colour levels smoothed over 4 s (lightning
  crash recoloured the stack, a9b7321b). **Code review 11-06** (standard /code-review, not ultra: free ultra used up;
  findings in research/reviews/): 3D 1-3,6,8,9 fixed (bf28f791); maps 1-9 + 2 extras fixed (6907627c). 3D size range
  and sliders decided (see the 3D item, PR #344). Open: map_view.c/wspr_map.c shared scaffolding into map_scene (cleanup). Images: 3D 42236779844, maps 42236796961.
- [ ] **Radio looks / themes, Thetis-style (VE7NHW, 2026-11-05): idea, after the 3D spectrum.** VE7NHW likes
  Thetis skins (many made by the community). Not started; shared GUI (`src/styles.c` skins), so OpenX6100.
- [ ] **#329 WeFax Post Proc. (Hany's 1KO125 1.1-beta1, tag d65e605).** Kris's review (11-02) wants the model's training
  kit (`tests/wefax/ml` in Hany's `1ko125`, LGPL, without the .exe), the model's licence and training charts, Hany's
  empty author e-mail ("Hany El Imam <>"), Post Proc. time on the radio. NOTICE lines removed (13271c3, worktree
  `~/Work/x6100/wt-wefax-pp`). Our reply with the questions posted 11-02. **Hany answered 11-04:** kit OK without the
  .exe files; model LGPL 2.1+, trained on Northwood GYA + DWD Hamburg charts through the kit's channel simulator (no
  NOAA, no recordings; his README-wefax_pp.md); e-mail info@murus.it; Post Proc. ~2.5 min a full chart on the radio,
  in the background. **11-05:** worktree branch rebuilt: his 12 commits now "Hany El Imam <info@murus.it>" (same tree,
  same base; old head kept as local branch `backup-wefax-postproc-13271c3`), + kit and model README as his commit
  124d561 (from 1ko125 8341bed8), + our c011244 (bin/ made, .exe ignored). Force-pushed by VE7NHW 11-05 (c011244; GitHub now
  links the commits to TheMurusTeam). Reply posted 11-05. Our own timing dropped (VE7NHW 11-05): Hany's ~2.5 min on the radio answers Kris's question.
  **Our review posted 11-05** (we opened the PR, so approval must come from Kris or Georgy): merges clean with the six
  apps; PC run under ASan clean, same result 1 vs 3 threads (quality not judged: synthetic noise). Asks (code reading):
  1 ESC/F5 in the full-screen view deletes the focused canvas in its own key handler (held key -> long press into the
  browser; fix lv_indev_wait_release as textarea_window.c), 2 pp_write_png() straight to NAME_pp.png (no .tmp +
  rename), 3 small (unused pp_result_name, -3 comment, Save Fax during cleaning). Offered to commit 1+2.
  **Next:** Hany's answer; then Kris/Georgy approve, VE7NHW merges.
- [ ] **Hany's next apps:** NavTex (LGPL, own SITOR-B) and DSC (Nayuki QR, MIT) on APP 5 after JTTY (his PRs or ours?),
  mic MODE key, FT8 CB (VE7NHW undecided), SSTV RX/TX radio test.
- [ ] **Jake 2E0JFS's UI work (VE7NHW, 2026-11-02):** VE7NHW heard he has a fork updated to v1.2.0. Not found on
  GitHub (no account with his call; no fork of OpenX6100 pushed since v1.2.0 but Kris's and Hany's; OpenX6X00's 15
  forks; code/commit search). Known: he designed the Black Metal and Total Black themes credited in OpenX6X00
  (README, docs/MANUAL.md, src/theme.c), code by others. **Next:** VE7NHW asks him for the link.
- [ ] **OpenX6X00 (Guru-RF/OpenX6X00, Joeri Van Dooren ON6URE; not OpenX6100) and our JS8 code (checked 2026-10-27,
  VE7NHW asked).** Created 10-20. Credits R1CBU, R2RFE, Hany, Kris, randal007 (map, PSK/Olivia merge...), keeps R1CBU's
  copyright lines, JS8 files under GPL-3. **But** `src/js8_window.c` holds 41 of the long sentences of our `dialog_js8.c`
  and `src/js8/macros.cpp` is 95 % ours (code lines), both without "Copyright (c) 2026 Randal Warren VE7NHW"; the credits
  don't say the JS8 app is from our port; their README says every author's notice stays. Probably inherited from the
  code they forked (not confirmed). Facebook clarification drafted for VE7NHW (OpenX6100 ≠ OpenX6X00, Georgy R2RFE leads),
  without the licence point. **Next, if VE7NHW wants:** a friendly issue/message to Joeri (notice back in those files + a
  credit line), VE7NHW posts it.
- [ ] **Our fork up to v1.2.0.** **OpenX6100 v1.2.0 released 2026-11-01 18:21Z by Kris** (JS8, WeFax with Denoise,
  SSTV, Channel List with its APP button; #324/#325/#328/#330/#331 went in before the tag; Buildroot #9 in, webserver
  v0.0.5). **2026-11-01:** branch `merge-v1.2.0` (worktree `~/Work/x6100/wt-merge-v1.2.0`, 93bb0c91): src/ now exactly
  v1.2.0's (the plain merge had registered JS8 twice in main_screen.c and src/CMakeLists.txt: took upstream's);
  kept our workflows, README, UPSTREAM.md, cty.dat; STATUS now says "OpenX6100 JS8 v1.2.0"; harness builds
  (rotary_cb(diff, 0)). Image run **41362032030**, tests run **41362034142**. Facebook post for v1.2.0 drafted
  (`~/Pictures/openx6100-v1.2.0-facebook/post.txt`). **Next:** VE7NHW flashes and checks it; PR into main.
  VE7NHW (11-01): after this, we **may stop updating the fork** (people use OpenX6100's releases): undecided.
- [ ] **Two transmit-level codes (VE7NHW decided 2026-11-02): one shared for steady modes (tx_level: JS8, FT8/FT2, RTTY, WSPR, JTTY), one shared for modes whose strength swings (PSK31, Olivia), not one per app.** Told Kris in the #333 approval (note 3).
  **The swinging-mode code also gets a manual TX level (VE7NHW, 2026-11-02):** a TX level setting in each app's
  Settings (press, turn the MFK, press), the MFK adjusting it live while sending, and the ALC and power shown on
  screen while sending; saved per app. The peak-matched calibration gives the starting point, the user trims it.
  Background: VE7NHW: all apps should share one
  ALC/level code, *except* modes without a steady carrier, where JS8's code doesn't fit. Checked Kris's
  modulators + simulated (research/tx-envelope/papr.py): **steady (peaks = average): JS8, FT8/FT2, RTTY, WSPR, JTTY;
  swinging: PSK31 (idle 3.0 dB peaks over average, typing 1.2 dB) and Olivia (3.7 dB, 75% overlapped tones).** Our
  tx_level and Kris's PSK31/Olivia loops (FT8's get_correction) all raise the drive until the *measured* power reaches
  the setting: if the radio's meter reads average power, PSK31/Olivia peaks end up 2-2.4x the setting and the ALC
  squashes them (splatter). **Next:** bench test (dummy load, MFJ peak vs average, the radio's own power reading,
  ALC) with PSK31 idle and Olivia; then a plan with Kris: steady modes on tx_level, PSK31/Olivia aiming their peaks
  at the setting with no ALC action.
  **VE7NHW (11-02): fine at normal power, fails at sub-1 W into an amp.** Why, from tx_level.c (a reading of the code,
  to confirm on the bench): the ramp-up stops only when the ALC shows (>= 0.3) or the average power reading reaches the
  setting - 0.05 W. At 5 W a swinging signal's peaks hit the radio's power limit and the ALC shows early, so it holds
  (peaks squashed, but bounded). At 0.3 W the ALC stays at 0, and the power reading (0.1 W steps) sees only the
  average, 42-50% of the peaks: it keeps raising, up to 8 dB, until the average reaches the setting, so the peaks reach
  2-2.4x the drive the amplifier was set up for. **Idea:** no feedback for these modes: take the steady-tone level
  learned for that power (FT8's/JS8's calibration) and scale the audio so the signal's *peaks* equal that tone.
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
