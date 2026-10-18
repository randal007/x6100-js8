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
1. **Card:** a private NR2 test build, flashed 2026-10-18 (details in the private repo), backup **sd-backup-2026-10-18-080246**. It is RC1.1 plus the NR2 work: **not** the digital apps of all-apps-test.
2. **Releases:** **RC1.1 is out** (`js8-rc1.1`, 2026-10-17: the transmit level reaches the setting; page titled, prerelease, sha256 checked). Left: a Facebook post if VE7NHW wants one (tests.yml step done 2026-10-18).
3. **JS8 into OpenX6100:** 1/4 and **2/4 merged** (#301 + Buildroot #8, VE7NHW 2026-10-18), **transmit level #306 merged**. **3/4 = #307** (ready, reviewers asked, tests green); **4/4 = #308** draft stacked on #307 (version "OpenX6100 JS8 v1.0.2"), test image run 39139987043. **Hany's four opened:** groundwork #309, Channel List #310, WeFax #311 (on #309), SSTV #312 (on #311); each clashes textually with Kris's apps / each other's mic keys: whoever goes second gets a fix-up commit from us.
4. **Waiting on others:** Georgy + Kris: reviews of #307, #309, #310; Georgy: #305 (±10 dB?); Kris: merging his apps (approved; our #4 fix is in them). #300 approved by Georgy, held until Kris's apps are in.
5. **VE7NHW, any time:** radio test of 4/4's image (run 39139987043) when he wants; delete OpenX6100 branches `js8-groundwork`, `js8-engine`, `tx-level`; delete branches `claude/bold-albattani-x66yc5`, `claude/cool-goldberg-68nv8y`; pick where to back up CLAUDE.local.md; send the RC1 email to Giovanni if wanted (draft given 2026-10-17). Radio: VOL with Menu/Macros open in RTTY/PSK31/Olivia.

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
- [ ] **Release Candidate 1.1 (VE7NHW 2026-10-17): the transmit-level fix.**
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
  **2026-10-16:** #301 approved by gdyuldin; Kris's three points done (b040b8f) and answered; Buildroot #8 (Boost/FFTW, licence label) opened. **3/4 and 4/4 prepared by cloud sessions:** draft PRs here #16 (`js8-library`) and #17 (`js8-app`, merges OpenX6100 main 9714584 first, since js8-engine sits on js8-groundwork), both checked on this PC (files identical to main, GCC 12, CI green). **Next:** Kris's OK + Buildroot #8 merged -> VE7NHW merges #301; then 3/4 as a real PR from `js8-library`, then 4/4; 4/4 gets the tx_level fix when there is one.
  **2026-10-18: Buildroot #8 then #301 merged by VE7NHW** (f032490; Georgy approved both, Kris left the final word to him), **#306 merged** (49328dd). **3/4 = OpenX6100/x6100_gui#307** (branch `js8-library`, 6367a0f: rebased onto main 49328dd, four commits; the "drive-level tests wait" commit dropped since they are #306's test_tx_level; the workflow also runs test_tx_level; files identical to our main 728b8d0; 29 src/js8 files GCC 12 clean; tests green on the PR): opened as draft, marked ready after VE7NHW OK'd the text, reviewers gdyuldin + myshak1. **4/4 = #308** (branch `js8-app`, 2133235, draft, base `js8-library` so only its own commits show): app files from main, button/case/build (no tx copy: #306's), and **JS8_APP_VERSION = "OpenX6100 JS8 " VERSION** (main.h, so "OpenX6100 JS8 v1.0.2" now; VE7NHW's choice 2026-10-18: OpenX6100's own name and number; our fork keeps "X6100 JS8 <release>"). GCC 12 clean except dialog_js8.c (needs gpsd headers: image only); break check clean; tests green. Test image (OpenX6100 + JS8 only): test-image.yml run **39139987043**. Texts in docs/upstream-prs/20-js8-library-pr.md, 21-js8-app-pr.md. **Next:** image result into #308; VE7NHW radio-tests it if he wants; #308 ready once #307 is merged (GitHub retargets it to main); #300 then gives JS8 the `dt` argument (as all-apps-test 4c159c5); our drafts #16/#17 to close.
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
  **2026-10-16:** Georgy + Kris agree (one stored level + PSK31 correction, clamp the stored value; Kris moves WSPR/PSK31/Olivia/RTTY/JTTY once we send it). **VE7NHW: our tx_level stops at 3.8 W of 5 W, no amp, ALC 0, in JS8**: told them on #303 we measure first. Suspects (from the code): drive at its 0 dB ceiling, the radio's own power reading saying ~5 W, or the 8 dB ramp limit. Log ("TX level:" every 0.5 s) in all-apps-test, on the card. **Next:** one JS8 TX at 5 W on the dummy load, read /tmp/x6100_log.txt over the console, fix, then the PR (with the clamp).
  **2026-10-17 (cloud session): likely cause found, branch `tx-level-fix`** (from main 19dd03a, test image Build image run 39059735209, Tests run 39059737055). IU1WXY's copy of our tx_level.c (his repo, c2e419d) found it: the radio sends only ~2.5 readings a second (his figure, not yet ours), so our window of 12 *readings* was ~5 s, two decisions a JS8 frame, ~1 dB up a transmission; our #303 post ("½-second averages, 1 dB/s") described the code wrongly. Branch: 21797ed window = 12 *blocks* (as his) + ALC "shows" from 0.3 not 0.1 (with 1-2 readings a window one stray 0.1 held it at 1.9 W of 5 in the model); f7214fe tx_player_stream() (WeFax/SSTV) + learned level floor -42 dB (Kris's clamp; 5 W needs ~-36 dB total on rev 3 bases per IU1WXY and Hany); a099c48 TEST log (readings per window, base rev; drop before the PR). Unit tests: new 2.5-readings case (fails on main, passes), lag sweep both ways, fast suite green. Not in it: PSK31 correction (Kris's apps not on main), IU1WXY's ALC 1/3 thresholds. **VE7NHW asked for it in all-apps-test instead (2026-10-17):** 21797ed, f7214fe and a099c48 put on all-apps-test after reverting 45358a2 (the old TEST log, which a099c48 replaces): d91a675, tx files identical to tx-level-fix, GCC 12 clean, Build image run **39059910797**; tx-level-fix's own image run 39059735209 cancelled. **2026-10-17: radio-tested by VE7NHW** ("ALC works great on any power output"; log: 5 W reached in the first transmission, 2.3 -> 5.2 W in ~6 s, held 5.4-5.5 W; 0.3 W held; an overdriven start, ALC 3.5, came down to 0.4 in ~5 s; **6-7 readings per half second**, so IU1WXY's 2.5/s doesn't hold on ours, and which of the two changes fixed 3.8 W isn't separated). **Sent as OpenX6100/x6100_gui#306** (branch tx-level b66a969: module on its own, generic comments, PSK31 correction as a base_gain_offset term, tests/test_tx_level.cpp 6 cases); pointer on #303. **On main and released as RC1.1 (2026-10-17)**: tx-level-fix without the TEST commit, the four files identical to #306, tests moved to tests/test_tx_level.cpp (CI runs it). **Next:** reviews of #306; JS8 4/4 (#17) to carry it (or drop its copy if #306 is merged first). **2026-10-18: #306 merged by VE7NHW** (Georgy approved; 49328dd); 4/4 (#308) uses it, no copy. Left for Kris: moving his apps onto it. Old plan text: VE7NHW flashes all-apps-test, JS8 TX at 5 W on the dummy load: wattmeter + "TX level: N readings" lines; if 5 W, the #303 PR (from OpenX6100 main, without TEST, with the PSK31 correction and a correction of our #303 description). Nothing posted on #303 yet.
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
  **2026-10-15/16:** Hany's full OK (LGPL headers, 7(b) dropped). Cloud drafts #12-15 done, then rebuilt here as `ox-murus-groundwork/-wefax/-sstv/-channels` on OpenX6100 main 9714584 (Hany's commits as received, ours under VE7NHW, licence commit on top; GCC 12 clean); drafts #12-15 closed, their branches deleted. VE7NHW: send as Hany wrote it (WAV recorder, mic keys), reviewers decide in the PRs. Info window licence text asked on #302; **Hany OK 2026-10-17**, changed on `ox-murus-groundwork` (e574b53: LGPL-2.1-or-later, "Originally from the 1KO125 firmware, part of OpenX6100", source link instead of the missing /usr/share/x6100/COPYING; his credit kept). `ox-murus-wefax` / `-sstv` rebased onto it (b953082 / 5f66c00, pushed with VE7NHW's OK). In all-apps-test on the card. **Next:** radio test; real PRs after Kris's apps are merged (groundwork first).
  **2026-10-18 (VE7NHW: send what doesn't clash; he had opened the apps on all-apps-test, "opened fine and kind of worked", no full test):** trial merges: groundwork and Channel List merge cleanly with main 49328dd and with Kris's #271/#272/#274 (groundwork vs #276: src/CMakeLists.txt only); WeFax/SSTV clash with #271/#272/#274 in buttons.cpp / main_screen.c / settings_types.h. GCC 12 clean. **Opened: groundwork = #309** (branch `murus-groundwork` = ox-murus-groundwork e574b53), **Channel List = #310** (`murus-channels` = 87c7375), reviewers gdyuldin + myshak1; texts in docs/upstream-prs/22-, 23-. **Then (VE7NHW: the places are known, send them now, as in our test build): WeFax = #311** (branch `murus-wefax` = ox-murus-wefax b953082, base murus-groundwork) and **SSTV = #312** (branch `murus-sstv` = ox-murus-sstv + a merge of WeFax, 72f16be: page 4 = empty, empty, WeFax, SSTV; 109 / 111; break check clean; GCC 12 clean; base murus-wefax; copy in our repo as ox-murus-sstv-stacked). Texts 24-, 25-. Known clashes (trial merges): WeFax/SSTV vs Kris's #271/#272/#274 (buttons.cpp, main_screen.c, settings_types.h), WeFax/SSTV vs Channel List #310 (main_screen.c mic keys). **Next:** whichever is merged second, a merge commit from us (rerere has the WeFax/SSTV resolution); when #309 / #311 merge, retarget #311 / #312 to main (`gh pr edit N --base main`); SSTV TX on the dummy load still untested; tell Hany on #302. **Same day, VE7NHW: every button in its final place from the start, as in all-apps-test** (page 3 WiFi, BT, WSPR, JS8; page 4 PSK31, Olivia, WeFax, SSTV). Ours already were (JS8 #308 page 3 last; WeFax/SSTV page 4 places 3/4; Kris's WSPR #271 page 3 place 3). Kris's PSK31/Olivia both sat in WSPR's place: **myshak1/x6100_gui#5** (psk31 -> page 4 place 1, branch psk31-app-page4) and **#6** (olivia -> page 4 place 2, olivia-app-page4), page 4 added as #311 does. Waiting for Kris.
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
  (VE7NHW, 2026-10-18). Status and notes are in the private repo (see
  CLAUDE.local.md), not here.

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
  **2026-10-16: reworked as Georgy asked** (6050dd9: apps' `rotary_cb(diff, dt)`, FT8 speeds up from the knob's `dt`); Kris told his apps need the extra argument. In all-apps-test. **Next:** radio test, Georgy's review.
  **2026-10-18: approved by Georgy.** Held (Claude's advice, VE7NHW asked why): it changes every app's knob callback, so Kris's WSPR/PSK31/Olivia would no longer match; either order works, the question is who updates them. Plan: after Kris merges his apps, a commit on #300 updating their rotary_cb, then VE7NHW merges.
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

## To try on the radio (no programming)

- [ ] **all-apps-test (d91a675 on the card since 2026-10-17):** ~~JS8 TX at 5 W~~ done 2026-10-17 (ALC fine at every power); VOL with Menu/Macros open in RTTY, PSK31, Olivia (Kris's #4 fix); FT8 knob fast; Kris's WSPR (marker, decode window), PSK31/Olivia (waterfall stops while keyed, averaging), RTTY's new page 2; Hany's WeFax, SSTV (TX on the dummy load first), Channel List (mic V/M, GENE, NW, UP/DOWN); the first boots with the console connected (FT2).
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
