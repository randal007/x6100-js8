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
1. **Card (11-10 22:35): test image run 42923721064** (test-image.yml, name tx-timing = `test-package-6` 64c5e8d7: test-package-5 + TX LED meter + see-through FT8 list + ft8-tx-timing; the flash script prints d2235a2, our main the workflow ran on). DATA backup **sd-backup-2026-11-10-223508**, DATA identical after the flash. **Next, on the radio:** the LED meter + list, FT8 CQ with the timing lines ("FT8/FT4 TX slot: thread woke", "FT8/FT4 TX start, s into the slot"): copy /tmp/x6100_log.txt over the console before power-off (the app log reaches /mnt/app_logs only when the GUI exits). Still untested: RFG press, list following, WiFi "Not connected" line, WSPR, JS8 TX rows/CPU, clock.
2. **#371 (FT8 extra passes), waiting on Kris.** Radio numbers posted 11-10 (+28 %, 5.5-7.6 s, 0 false on 300 noise slots, slot fix). Late TX starts: DATA ptt.log (copies research/ft8-tx-timing) keys at 0.05-0.15 s normally; after 0.5 s: 11-07 0/8 (no passes), 11-08 3/29, 11-09 0/13, 11-10 2/17 (3rd + 4th TX after opening FT8, next 15 fine). Not the TX audio (6 kHz, a few ms). Branch `ft8-tx-timing` 596146b5 (our fork, test only) logs each TX start step. **Late-TX comment posted by VE7NHW 11-10 (issuecomment-6782880621)**, text in research/reviews/pr371-late-tx.md. **Next:** read the timing lines after the radio test, fix the slow step in its own PR.
3. **OpenX6100:** #372 (Kris, RTTY contest): all 9 fixed, our new point (single serial at the end of an over lands in the next QSO) posted 11-10, waiting on Kris; since #367 it conflicts in src/rtty_buttons.h (Kris merges main). **11-11:** #367 merged by Kris (radio-tested), #353 + #349 merged; Kris's #374 (Olivia 8/500) and #376 (FT2 3578/7052) approved by VE7NHW; webserver#7 (DS1UFX, JS8/GhostNet/FT2 lists) not reviewed yet. #371 waiting on Kris; #349, #351 on review. #353 (Hany, DSC): ships file out, VE7NHW said go ahead with his own list if Kris and Georgy agree (reply sent by VE7NHW, research/emails/reply-hany-dsc-ships.txt), squash merge asked. #358 + Buildroot#11 (Markus DL2ZW, U-Boot 2026.10) reviewed 11-10 by hand + /code-review: correct, #11 must merge first; Georgy decides (VE7NHW: no boot-test image from us). **When #358 lands:** drop `Replace u-boot` from our main.yml + test-image.yml (and x6100-xara / x6100-nr2) and move their U-Boot fallback to 2026.10, or every build fails on the missing .bin. XARA #345 + Buildroot#10 frozen (x0rro e-mailed); VARA HF 4.9.1 (11-04) test vs xara waits on VE7NHW downloading the zip.
4. **New 11-10:** TX meter + see-through FT8 list (VE7NHW picked the thin LED rows with ALC, list ~55 %), branches `tx-meter-led`, `ft8-list-see-through` in OpenX6100, PRs after the radio look. Jake 2E0JFS's build reviewed (research/jake-2E0JFS/features.md), VE7NHW asked Jake for the repo 11-10; Jake (16:39) asked about Giovanni and offered to open his source at the weekend; short reply in VE7NHW's voice written (research/jake-2E0JFS/reply-to-jake-2026-11-10.txt, sending not confirmed). Themes: plan waits for Jake's source (themes item). **AI guide: split into four 11-11 (Kris's e-mail; VE7NHW: split now):** #377 CI + checks, #378 Build image, #379 tools/radio (same commits replayed from ai-guide onto main a7597be6, same file content), #373 (draft) keeps AGENTS.md/docs/skills/templates and shows the others' files until they merge. Kris's points 1-3 and 5 fixed on ai-guide (41f14aa8 colour ratchet dropped, 89e280d5 licences + GPL copy line, 4e1ab519 fork + Linux-only notes). Reply to Kris written (research/emails/reply-kris-ai-guide-2026-11-11.txt, on the clipboard, sending not confirmed). Waiting on Georgy (required checks, -Werror=implicit-fallthrough) and reviews.
5. **VE7NHW, any time:** test package 4 feedback (Kris, Georgy, Hany); QSOs on the new modes; bench power test (PSK31/Olivia at ~0.3 W into the amp); where to back up CLAUDE.local.md; delete ~/Work/please-delete (old worktrees, Jake zip duplicate). GitHub private minutes 90 % used until 12-01.

- [ ] **Decoders vs WSJT-X (latest, PC: FT8 17 and 19 of jt9's 21 after fixes 1-3, see the PR item below; the first numbers follow) (VE7NHW 2026-11-08 asked), PC benches in ~/Work/x6100/research/ft8-decoder-vs-wsjtx and
  wspr-decoder-vs-wsjtx (NOTES.md each).** **FT8: about half of WSJT-X** (real samples: 10/21 and 8/21). Why:
  1 contest messages (RTTY Roundup, Field Day, DXpedition, EU VHF) are decoded but dropped: ft8_lib unpacks only
  standard/non-standard/free text/telemetry (worker.c skips the rest); 2 no subtract-and-decode-again pass (signals
  next to strong ones); 3 no deep decoding (OSD/a-priori) for -14..-20 dB. **WSPR: close** (6/9 on the real sample,
  SNR within 1-3 dB; weak limit ~-28 dB vs WSJT-X ~-31; misses DT beyond -1..+2.4 s and overlapped signals).
  **FT8 fix 1 done 11-08 (VE7NHW: yes):** `src/ft8/contest_msg.c`, OpenX6100 branch `ft8-contest-messages` dad090c8
  (worktree ~/Work/x6100/wt-ft8-contest): 0.1 DXpedition, 0.3/0.4 Field Day, 3 RTTY Roundup, 5 EU VHF, as WSJT-X's
  unpack77; calls via ft8_lib's decode_std + hash table. PC: contest recording 8 -> 14, every ft8sim type = jt9's text.
  **FT8 fix 2 done 11-08 (VE7NHW: "if that's how WSJT-X does it"; FT2 left single-pass, he asked):** `src/ft8/subtract.c`
  (WSJT-X subtractft8 on a 64/symbol grid) + 2 more passes in worker.c, same branch 1ece99ee. They run **after the next
  slot's TX decision** (audio_worker -> ftx_worker_decode_more after on_tick): TX timing unchanged; none after a TX;
  stop 2.5 s into the new slot; decodes reported as the slot just ended. PC: busy band 10 -> 14, contest 14 -> 18 (jt9
  21 each), FT4 mix 5 -> 6 (jt9 8); 110-135 ms a slot on the PC: **on the radio unknown**, logged each slot as
  "FT8/FT4 extra passes: N more decoded (of M), X ms" in /tmp/x6100_log.txt. **Test image: test-package-4-wf
  1b03d32a = waterfall + contest + passes, run 42444841698** (built OK, downloaded; 42442545706 cancelled). **Next:** radio test + the log
  times (if a slot's passes take > ~2 s, cut to 1 pass or speed up subtract.c); split into two PRs (contest, passes; FT2 gets
  only the contest part); fix 3 (deeper decoding: OSD / a-priori) after, once the radio timings are in. WSPR: no fix planned yet.
- [ ] **RFG knob press stops TX first, as JS8 (VE7NHW 11-08), and FT8's list follows the newest (VE7NHW 11-08).** OpenX6100 branches `rfg-stops-tx` 565f570c (FT8/FT4/FT2, WSPR, PSK31, Olivia, RTTY in main_screen.c, SSTV, CW and voice messages, SWR scan; JTTY and JS8 did already) and `ft8-list-follow` (20 s after the last MFK turn it returns to the newest). On the card (run 42533266989); PRs #366, #367, #368 opened 11-08 before a radio check of the RFG press and list: check them on the next test.
- [ ] **New 11-09, branches from OpenX6100 main, compile-checked, not on the radio, no PRs yet (after a radio test):** `ft8-psk-line-connected` 01d06815 (VE7NHW: the map's blue line says "Not connected to PSK Reporter" while on but not connected; "heard you" leaves the legend) and `ft8-late-start` (VE7NHW: "do it like WSJT-X": a picked station starts up to 75 % into the slot, FT8 11.25 s, FT4 5 s, the missed part left out by tx_worker.c; CQ and auto replies keep 1.5 s, FT2 unchanged; read #241 (bg7nzl, draft, same idea with 5 s), no code used). Radio test 11-09 so far: FT8 first 3 min +1/+5/+3/+1/+3 extra decodes, 6.3-7.7 s a slot, no "cannot write"; **radio shut down by VE7NHW at 642 s, so the log after 158 s (log writes in chunks) not read yet: /mnt/app_logs at the next start.** Clock ~+0.8 s, not set. WiFi: VE7NHW's router antenna broke and the radio is far from it (joins, handshake timeout): PSK lines need the radio closer or a hotspot.
- [ ] **Code review 11-09 of the new work (JS8 TX rows, late start, PSK warning, #367/#371 fixes), checked by hand and fixed:** #371's slot fix was wrong (a slot is submitted before the boundary, so finds carried the slot before, parity reversed): f587e404; late start only for the first TX after a pick (dd1f3fe7); legend stays with the lines (382dd1d8); JS8 no row from silence, monitor failure logged (7a23cbec). Not changed: JS8's monitor join with no timeout and always-open stream (same as merged app_wf.c; check CPU on the radio); duplicate of app_wf's monitor (keeps JS8 separate).
- [ ] **Leftovers of the merged 11-08 work:** PSK Reporter's blue lines not yet seen on the radio (needs WiFi); WSPR after the filter change; AGC Slow vs Off on one band (5 min each, twice; proposed, no hurry); Kris's follow-ups in the item below.
- [ ] **Kris 11-09: merged #362-#366 and #368-#370.** Open: **#367 merged by Kris 11-11.** Was: changes requested (RFG stop must also call digi_qso_menu_tx_stopped and refresh the buttons, as rtty_buttons.c's Stop: confirmed in the code); **#371** wants false decodes over 200 noise slots + gain vs one pass (known: 10->17, 14->19), the slot stored with each extra_found_t (a late find takes the next slot's info), and the radio's per-slot log under TX and on a busy band (needs run 42540476377 flashed). Follow-ups, not urgent: #365 one shared DSP-off helper; #366 SSTV's second press doesn't close, WSPR's press doesn't stop a TX waiting for its slot. **Done 11-09 (VE7NHW OK'd):** #367 goes through rtty_buttons_stop() (branch merged with main); #371 finds carry their own slot (ftx_worker_extra_slot, extra_msg_cb). PC with this code: one pass 10 and 14, with the extra passes 17 and 19 (all in jt9's list), finds keep their slot past the next boundary, ThreadSanitizer clean, **0 FT8 decodes on 300 noise-only slots** (200 white noise, 100 ft8sim noise). Compile checks OK. **Mercury: not now (VE7NHW 11-09).** **Next:** the radio numbers for #371, then reply to Kris.
- [ ] **PRs opened 11-08 (VE7NHW: "start all the PRs"):** #362 shared waterfall, #363 shared map path opacity + status3, #364 PSK Reporter (on #363; end to end not seen on the radio, WiFi had no lease), #365 NR/NB/notches off + RX filter, #366 RFG stops TX (apps), #367 RTTY's RFG (main_screen.c), #368 FT8 list follows. **Second code review 11-08 (call table, retries, fixes) checked by hand and fixed (88cbe28a, 971802b4):** retries use tone amplitude as JS8 (power was wrong) -> busy band 14->17, contest 18->19, all in jt9's list, 0 false in 100 noise files; calls age once a slot; passes stop mid-list; the no-op band-change cancel removed; #351 got snr_noise[] __thread (bd21e521). Not fixed: finds delivered in the ~120 ms before the boundary carry the old slot (can't happen on the radio: a pass takes seconds). **#369 call table (first: a hang also in main), #370 contest messages (on #369), #371 extra passes + retries (on #370), opened 11-08.** Old branch ft8-contest-messages is the source, delete once merged. **Test image with all this + JS8 TX rows: test-package-4-wf 30f184fa, run 42540476377 (name js8-tx-wf), built OK, not flashed yet.** All ten PRs have had a code review; the fixes after it were checked by hand. **JS8 TX waterfall (VE7NHW asked 11-08):** branch `js8-tx-waterfall`, rows from the playback monitor, red edge, own FFT/thread; harness compiles (link needs stubs); in test-package-4-wf (59d5b921 on the branch), not on the card, not tried on the radio, not code-reviewed yet: radio test, review, then PR (VE7NHW: PR only after he has seen it).
- [ ] **XARA into OpenX6100 (VE7NHW, 2026-11-06): two PRs, out of draft 11-06 late (experimental, for 1.3.5 and beta testers).**
  **x0rro discontinued xara on 2026-11-08 21:57Z:** x0rro/xara is now one commit with a README saying development and distribution have stopped (no reason given); all history, tags and releases removed there, 0.1.4/libxara never reached us. Our copies: **~/Work/x6100/reference/xara-archive-2026-11-09/** (every branch and tag incl. x0rro's v0.1.0-v0.1.3, MIT, and our unpushed x6100-on-013 port; bundle verified, SHA256SUMS) and randal007/xara. **FROZEN (VE7NHW 2026-11-09): #345 and Buildroot#10 don't go into OpenX6100 until we know what happened.** Both back to draft 11-09 (VE7NHW, comments posted; Kris had suggested the same hold and Mercury, Rhizomatica GPL-3, as an alternative: not VARA on the air, 32-bit ARM and R16 speed unknown; VE7NHW to answer). E-mail to x0rro sent by VE7NHW (research/xara-bench/email-x0rro-discontinued.txt). **11-10:** VARA HF 4.9.1 out 11-04 (VE7NHW: debate that it "poisons" clones; not found in any public source yet). PC link (HL2 off, virtual cables, ~/Work/xara 0.1.1 free speeds) vs VARA 4.9.0: mail both ways OK (2583/2562 B, 175 bps). 4.9.1 not tried yet: its zip is behind Cloudflare, VE7NHW downloads it. Next: decide on the answer. No second copy of the archive wanted (VE7NHW).
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
  widget; FT8/WSPR map checks pass, type 2/3 placed. In the test package. **Radio test 11-07 (VE7NHW, test package 4): maps look good, WSPRnet works**; the blue "heard you" stations had no dB tag -> 861490b0 (their tag = how they heard you); FT8 map had no dB tags at all (the white numbers were count tags) -> VE7NHW: only CQ stations, inside the green CQ tag ("CQ -12"), no white dB tags (they read as counts) -> 4e83fd71; both compile-checked, not yet on the radio. **PRs opened 11-07 (VE7NHW: "send all the PRs now"):** #354 map widget + scene (branch map-widget, base main), #355 FT8/FT4/FT2 map (ft8-map, base map-widget), #356 WSPR map + WSPRnet (wspr-map, base map-widget); split from shared-map by file, the three merged = shared-map + main (checked identical); worktree ~/Work/x6100/wt-map-prs. Test build with the tag fixes: public run 42404582223 (test-package-4 d4503d19, green). **All three merged 2026-11-08** (after Georgy's approval). **Next:** VE7NHW checks the CQ dB tags and the heard-you tags on the radio (flash run 42404582223 on his word). OpenX6100 map branches deleted 11-08.
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
- [ ] **TX box over the apps' waterfall + see-through FT8 list (VE7NHW, 2026-11-10).** The PWR/SWR box (tx_info.c, shared GUI) is TOP_BLOCK_BIG_HEIGHT (126 px) tall, the apps' top bar is 70 px, so in every app that transmits it hangs 56 px over the waterfall. Mock-ups on his FT8 TX screenshot: ~/Pictures/x6100-tx-box-mockups (script research/tx-box-mockups/mock.py). **Picked (VE7NHW 11-10): A + list 55 % opaque** (3-list-55.png), then the LED look (5-led-segments.png), refined to 9-led-thin-alc.png: three thin LED rows PWR / SWR / ALC (4 px segments like the theme's bars), values on the right; mock2.py: PWR and SWR as two thin bars inside the 70 px top bar, numbers on the right; FT8's list rows 55 % opaque over the waterfall. **Built 11-10 (VE7NHW: test build):** OpenX6100 branches `tx-meter-led` 6f8db4f7 (tx_info.c + styles.c: box 70 px, three LED rows, values on the right) and `ft8-list-see-through` 287bc525 (table_view.c: list 30 % + rows 30 %, about half the waterfall covered); both ARM compile-checked, not on the radio. Test branch `test-package-6` (our repo) = test-package-5 + both, image **run 42908859237** (name tx-led). **Next:** flash on VE7NHW's word, look on the radio (main screen too: the box no longer covers the spectrum's top), then the two PRs.
- [ ] **AI guide for OpenX6100 (VE7NHW 11-10: "do all recommended", one big draft PR).** Branch `ai-guide` in OpenX6100 (worktree ~/Work/x6100/wt-ai-guide, 19 commits on upstream/main a5ced9c6, head 8e2a3f49; the first five: AGENTS.md + CLAUDE/GEMINI/copilot pointers, docs/working-with-ai.md, PR template, README section; tools/checks (app-case break, build list) + checks.yml; Build image test images from any branch + buildroot_ref + unstripped GUI; tools/radio (console, screenshot, cpulog, flash, syncheck, udev rule) cleaned of this PC; skills /new-app /pr-check /close-session). Modelled on AetherSDR (read-only copy ~/Work/x6100/reference/aethersdr). PR text: docs/upstream-prs/ai-guide-pr-body.md; feedback e-mail to Georgy/Kris/Hany: research/emails/ai-guide-feedback.txt. **Draft PR #373 opened 11-10** (VE7NHW OK; Merge checks + JS8 tests green on it); e-mail with its link sent by VE7NHW to Georgy, Kris, Hany (11-10). **Parts 6-10 added 11-10 (VE7NHW: "go ahead with 1 to 5" of AetherSDR extras):** all 11 unit tests in CI (2nd job on 22.04 with ft8lib, SQLite 3.40.1, liquid-dsp 1.4.0; check_tests_in_ci.py; first run found test_cfg stale since FT2, fixed 94cb6863; all green run 42917671229), ratchets (i2c, colours; colours = Georgy's call, asked in the PR), voice-mode warning (Channels says nothing on opening: Hany's), GUI memory trend in x6100-cpulog (not run on the radio yet), bug report form. Asked Georgy about required checks on main. **Code review 11-10** (/code-review high): 10 findings checked, fixed in fc48d643..8e2a3f49 (console waits for a prompt after a gap, checks string-aware + stacked empty case + ratchet must be lowered + tests must be run, --base dropped, flash finds SD slots and fails on a DATA diff, Build image env + "/" names, sha256 on downloads, unstripped = function names only); -Werror=implicit-fallthrough offered to Georgy as its own PR; all green (42920659881, 42920660062). Flash tool run with the real card up to Write? (11-10, answered no; backup sd-backup-2026-11-10-160347). Our CLAUDE.md will conflict (add/add) when upstream with #373 is merged into ours: take the shortened JS8-only version then. **11-11 Kris's review (e-mail):** colour ratchet out, licence wording, GPL copy line, split into four, fork/Linux notes: all done, PRs #377/#378/#379 + #373 (see In short). **Next:** Georgy's answer and reviews; reviewers on #377-#379 (VE7NHW adds them with `!`). After #373 merges: shorten our CLAUDE.md to the JS8 parts.
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
  **11-10 (VE7NHW):** wants switching between themes and making new ones, done cleanly (no bugs, no bloat).
  **No plan until Jake 2E0JFS's source arrives** (his themes, see the Jake item). Draft direction from that
  session, to check against Jake's code: 1 colour roles (code never writes a hex; ours has 128 `lv_color_hex`,
  72 in dialog_js8.c), 2 shape numbers (radius, border, fonts, palette) as data, 3 layout changes as widget
  variants picked by name (smeter_bar.c / smeter_needle.c ...), not theme flags: OpenX6X00 (reference/openx6x00
  src/theme.h) did flags and now checks `deck`/`vision`/`mono` 46/65/56 times across 25 files. Georgy to agree first.
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
- [ ] **Hany's next apps:** NavTex #350 and DSC #353 are his PRs (reviewed 11-06, see *In short* 4); still to come:
  mic MODE key, FT8 CB (VE7NHW undecided), SSTV RX/TX radio test.
- [ ] **Jake 2E0JFS's UI work (VE7NHW, 2026-11-02):** **11-10: Jake sent his build (SD image only, v1.2.0-2E0JFS beta 0.2-fix9b, for R13), kept in ~/Work/x6100/reference/jake-2E0JFS-2026-11-09; feature list + recommendation in ~/Work/x6100/research/jake-2E0JFS/features.md (16 features, all shared GUI: TX/Ext Pwr calibration, needle S-meter, custom colours, themes, VOL return, Wi-Fi messages, ...); reply to Jake drafted (reply-to-jake.txt): his own PRs per feature on current main, source on GitHub, calibration after Georgy. VE7NHW looked at the build on the radio 11-10 (13:16-13:54); sending the reply not confirmed.** Earlier: VE7NHW heard he has a fork updated to v1.2.0. Not found on
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


- **Sharper WSPR waterfall** (VE7NHW 2026-11-08 asked why it looked blocky, saw mock-ups in ~/Pictures/x6100-wspr-waterfall-mockups: "they all kinda look the same, just leave it how it is").
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
- **FT8's waterfall on the lower layer:** an idea for OpenX6100, not ours. *(Reopened by VE7NHW 2026-11-08: the shared
  waterfall item in Now.)*
- **Map polish "from use":** too vague; new map wishes become their own
  items.
