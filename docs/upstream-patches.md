# What we carry on top of upstream

Our image is gdyuldin's **x6100_gui** (the GUI, with JS8 added) built by
gdyuldin's **AetherX6100Buildroot** (the Linux image). Anything we change
outside our own JS8 code can be lost or clash when either moves on. **When
updating to a newer upstream, go through this list**, and send the fixes
that help everyone upstream as PRs, so they stop being ours to carry.

## Code reviews of others' PRs (2026-11-06, comments posted by VE7NHW)

| PR | Author | Our comment |
|---|---|---|
| #347 PSK31/Olivia/RTTY: Log QSO, typing while sending, name/QTH | Kris | issuecomment-6702647797 (10 points: RST row logs the QSO, RTTY keyed after Stop while the next app opens, BT SPP dropped on close, ...) |
| #348 PSK31/Olivia: calling frequencies by ITU region | Kris | issuecomment-6700528697 (region borders; itu_region.c in its own PR; #344 uses the same file) |
| #350 NavTex (110) | Hany | issuecomment-6702413160 (10 points: Delete hits the wrong bulletin, live text stops following, ...) |
| #353 DSC (119) | Hany | issuecomment-6702493233 (9 points: UNKNOWN FORMAT on a late-phased call, wrong channel stamp, ...). 11-07: Hany fixed 1-6, 8, 9 (cf0762c4..c3bb3f1b, radio-tested), 7 as a later PR; our follow-up issuecomment-6710003416 (browser deletes a .tmp being saved; tune history fallback), both fixed by Hany (e65e69c6, 3e852f27) |

Our own, after their reviews (11-06): #344 4d3b9f1c (segment tag, no 2D/3D label) + a3cd1bd6 (11-07, after the radio test: tag on the spectrum view, banner hidden in 3D), #345 out of draft (99e07c89 review fixes, Record off, app 120 on its own APP page 6 c9778d65), AetherX6100Buildroot#10 out of draft (051ab64 review fixes).

## Kris's apps: review fixes (2026-11-06)

From code reviews of the six merged apps (findings: ~/Work/x6100/research/reviews/kris-apps-review.md). Kris (e-mail 11-06): merges them in the order #338, #343, #342, #339, #340, #341, #337, #336,
the FT2 slot fix (#335) at once, then rebases his digi-qso-log on main; map + WSPRnet + 3D (#344) for 1.3.0 after
both test them; XARA (#345) after 1.3.0. #335-#343 **all merged by Kris 11-06**. All
open, waiting on Kris's review and a radio test; they merge together cleanly (test branch `kris-fixes-test`
in our repo).

| PR | What | Branch |
|---|---|---|
| OpenX6100/x6100_gui#335 | FT2: TX silent in 3 of 4 slots, time sync, UDP slot stamps (SNR split out, Kris 11-06) | ft2-fixes |
| #346 | FT4/FT2 fixed offset: **closed** (PC test vs jt9: wrong) | ft4-ft2-snr |
| #349 | WeFax Post Proc.: #329 review points 1+2 (Hany asked, OK'd) | wefax-pp-followup |
| #351 | FT8/FT4/FT2 signal reports vs the band background (= jt9) | ft-snr-baseline |
| #352 | Panel: multi-line TX echo colour (Kris's #343 note) | panel-echo-lines |
| #336 | WSPR: band change while keyed, TX off, wsprd unpack from WSJT-X 2.7.0 (Kris asked for the original, d4c92969), decode cap | wspr-fixes |
| #337 | JTTY: not-sent rows, closed messages, no-player spin, logging, serial, knob | jtty-fixes |
| #338 | ADIF: JTTY mode both ways (shared) | adif-jtty-mode |
| #339 | PSK31: own echo as RX, Stop mid-character, unkey on close | psk31-fixes |
| #340 | Olivia: macro rows that keyed, deaf RX, TX end click, LOCK % back | olivia-fixes |
| #341 | RTTY: FIGS after space, Stop click, closed app transmitting, squelch default | rtty-fixes |
| #342 | Digi menus 20 rows, Auto DX only when empty (shared) | digi-menu-autodx |
| #343 | Panel: TX echo wrap/colour, RX wrap, buffer bound (shared) | panel-tx-echo |

## FT8/FT4/FT2, WSPR and map work (2026-11-08)

**Merged by Kris 2026-11-09: #362-#366, #368-#370.** Open: #367 (changes requested, fix 3623c99c pushed, comment posted), #371 (Kris wants numbers and a radio run; slot fix f587e404 pushed). Our review of Kris's #372 posted 11-09 (9 points). New branches, no PR yet (radio test first): `ft8-late-start`, `ft8-psk-line-connected`, `js8-tx-waterfall`.

**2026-11-10:** #372: Kris fixed all nine (4fb15026, fb737ea3); our re-read posted by VE7NHW (issuecomment-6777172684): all fixed, one new point (a single serial at the end of an over waits for the next word and lands in the next QSO). #353: Hany took dsc_ships.bin out (1beea569, YaDDNet data not GPL), asks for a squash merge; VE7NHW told him (by e-mail/chat) to build his own list if Kris and Georgy agree. #371: radio numbers in (36 slots under TX, +28 %, 5.5-7.6 s), reply posted by VE7NHW (issuecomment-6780366219). New branches, no PR yet (radio look first): `tx-meter-led` 6f8db4f7 (TX meter as three LED rows in the small top bar), `ft8-list-see-through` 287bc525.

**2026-11-11:** Kris merged #367 (radio-tested: RFG in the 73 macro stops TX, not logged, Next repainted); #353 (DSC) and #349 also merged. Kris opened #374 (Olivia 8/500) and #376 (FT2 80 m 3578, 40 m 7052 kHz, migration 6): read through, both approved by VE7NHW 21:40Z (both merge clean; no other open PR adds a migration 6; test_olivia not in tests.yml until #373). #372 now conflicts with main in src/rtty_buttons.h (#367): Kris to merge main. New: x6100_webserver#7 (DS1UFX, JS8/GhostNet/FT2 lists), not reviewed yet. AI guide: Kris's review e-mail (5 points) all done; split into #377 (unit tests in CI + merge checks), #378 (Build image test images), #379 (tools/radio); #373 stays the guide (draft). Reply to Kris written, not sent yet.

| PR | What | Branch |
|---|---|---|
| #360 | Band bar: phone segments say "All modes" (**merged 11-08**) | band-bar-all-modes |
| #362 | Shared waterfall for FT8/FT4/FT2 and WSPR, with TX rows (**merged 11-09**) | shared-waterfall |
| #363 | Shared map: path opacity + status3 | map-path-opacity |
| #364 | FT8/FT4/FT2: PSK Reporter, who hears you on the map | ft8-psk-reporter |
| #365 | FT8/FT4/FT2 and WSPR: NR/NB/notches off, RX filter | ft-rx-dsp-off |
| #366 | RFG press stops TX, doesn't close (apps) | rfg-stops-tx |
| #367 | RTTY's RFG press (main_screen.c) | rtty-rfg-stops-tx |
| #368 | FT8 list follows the newest | ft8-list-follow |
| #369 | Callsign table that can't hang (ft8_lib's can, also in main) | ft8-call-table |
| #370 | Contest and DXpedition messages | ft8-contest-text |
| #371 | Extra decode passes on their own thread + JS8-style retries | ft8-extra-passes |

Not yet a PR: `js8-tx-waterfall` (JS8 TX rows, waits for a radio test). `ft8-contest-messages` is the old source of #369-371: delete after they merge. Follow-ups pushed to open PRs on 11-08: #349 fsync (5a750b94), #351 comment and `__thread snr_noise` (7f7980d0, bd21e521).

## XARA (2026-11-06): drafts, experimental

**Frozen 2026-11-09 (VE7NHW):** x0rro discontinued xara on 11-08; #345 and Buildroot#10 stay out of OpenX6100 until we know why (docs/PLAN.md).

VE7NHW decided to bring XARA into OpenX6100 as groundwork (it was private until now; the private
repo randal007/x6100-xara keeps the test builds with VARA's registered speeds, never shared).

| PR | What | Branch |
|---|---|---|
| OpenX6100/x6100_gui#345 (ready 11-06) | XARA app, APP page 5 after JTTY, ACTION_APP_XARA = 120 (DSC 119); review fixes 99e07c89; all test-build settings incl. Full speed, EA5HVK credit | xara-app (worktree `~/Work/x6100/wt-xara-app`) |
| OpenX6100/AetherX6100Buildroot#10 (draft) | xara package (randal007/xara 919f5dd = build 22's; BR2_PACKAGE_XARA_REGISTERED for paid speeds, off by default) + host-xara-rust (Rust 1.97.1: Buildroot has 1.67) | xara (worktree `~/Work/x6100/wt-br-xara`) |

Texts: [upstream-prs/xara-gui-pr-body.md](upstream-prs/xara-gui-pr-body.md),
[upstream-prs/xara-buildroot-pr-body.md](upstream-prs/xara-buildroot-pr-body.md).

## PRs to send (the queue)

Ready-to-send texts and patches (checked against gdyuldin's current code):
[upstream-prs/](upstream-prs/README.md).

Everything here is outside our JS8 app. Tick it when the PR is merged and
drop our copy.

| # | To | What | Status |
|---|---|---|---|
| 1 | gdyuldin/AetherX6100Buildroot | gpsd with a control socket (`-F /var/run/gpsd.sock` in S50gpsd): USB GPS hotplug works (section 1) | **MERGED 2026-10-10: OpenX6100/AetherX6100Buildroot#2** (gdyuldin's repos moved to the OpenX6100 organization). Our CI step now skips itself (it checks for `-F` first); remove it when convenient |
| 2 | gdyuldin/x6100_gui | USB keyboard hot plug (`src/keyboard.c`), Bluetooth keyboards found and followed (`src/keyboard.c`, `src/usb_devices.cpp`, `src/pubsub_ids.h`), then the other keyboard fixes (section 2) | **MERGED 2026-10-10: OpenX6100/x6100_gui#281** (myshak1 approved and merged, 0e6662e; with the Callsign lowercase fix). Our copy goes when we next merge upstream |
| 3 | OpenX6100/x6100_gui | JS8 itself, in four PRs: see [JS8 into OpenX6100](#js8-into-openx6100-the-plan) below | planned (2026-10-11); first the app-ID discussion (OpenX6100/x6100_gui discussion 283). **Licence settled (discussion 23, gdyuldin 2026-10-13):** every existing file stays LGPL-2.1-or-later, the engine in `third-party/js8core` with its own GPLv3 LICENSE, an image with JS8 is GPLv3 as a whole; **JS8 in the default build** (no off switch needed). He asks for a short README / build-docs note on the image's licence and where the corresponding source is (Buildroot configs, `make legal-info`): **goes in 2/4**, as our reply said ([12-licence-reply.md](upstream-prs/12-licence-reply.md), then discussioncomment-18903656). 1/4 = #290, merged 2026-10-14 (approved by myshak1 and gdyuldin); **2/4 = #301** (js8-engine) opened 2026-10-14 **2026-10-14: 1/4 #290 merged** (VE7NHW, approved by both); **2/4 opened as #301** (js8-engine, three commits: unchanged import 1f8d390a, our patches, build + README licence note incl. Buildroot legal-info; asked gdyuldin who adds legal-info to Build image). **2026-10-15/16:** #301 approved by gdyuldin (wants `legal-info` as its own PR); Kris's three review points done (b040b8f: README says the GUI *program* is GPL-3.0, CRC++/Eigen licence files) and answered; Boost/FFTW + licence label in **AetherX6100Buildroot#8** (row 21). 3/4 and 4/4 made by cloud sessions ([tasks/js8-parts-3-4.md](tasks/js8-parts-3-4.md), base copy `ox-js8-engine`): draft PRs randal007/x6100-js8 #16 (`js8-library`) and #17 (`js8-app`), checked on Randal's PC 2026-10-17 (identical to main, GCC 12, CI green); the real PRs once #301 is merged. **2026-10-18: Buildroot #8 + #301 merged** (VE7NHW); **3/4 = #307** (js8-library, ready, reviewers asked; [text](upstream-prs/20-js8-library-pr.md)); **4/4 = #308** (js8-app, draft stacked on #307, version "OpenX6100 JS8 v1.0.2"; [text](upstream-prs/21-js8-app-pr.md)). **2026-10-20: 3/4 #307 merged** (Georgy approved, VE7NHW merged), with #313 fixed places; **4/4 #308** on main (OpenX6100 main + #300 merged in, rotary_cb dt), ready, waiting for review. **2026-10-21: OpenX6100 v1.1.0 tagged** with parts 1-3 (not 4/4). #320 fixes the flaky time-search test. **2026-10-21: #308 approved by Kris** (merges cleanly after #318/#319); fresh test image run 39668627012 (js8-app 2851576 + v1.1.0) for a radio test, then VE7NHW merges. **#320 approved by Kris**, ready to merge. **2026-10-22:** test image run 39668627012 downloaded (builds/ci-39668627012), not yet on the radio. **2026-10-25: 4/4 #308 MERGED by Kris (Georgy and Kris approved): JS8 is in OpenX6100, for v1.2.0.** |
| 4 | JS8Call-improved/Android-port (js8core) | local patches 1-15, e.g. 14 (Ultra decoded on Turbo's schedule) and 15 (Ultra on its own decode thread, a thread-start hook) | listed in [UPSTREAM.md](../third-party/js8core/UPSTREAM.md); bugs already reported in issue #104; **patches 1 (includes) and 16 (compound calls) merged upstream 2026-10-14 in #108** (14 withdrawn) |
| 5 | gdyuldin/x6100_gui | `dialog_rotary` timestamp-based knob speed (as his main knob) | offered to gdyuldin 2026-10-06 |
| 6 | gdyuldin/AetherX6100Buildroot | uhid in the kernel (`CONFIG_UHID=m`) + `modprobe uhid` in S40bluetoothd: Bluetooth LE keyboards type (section 1) | **MERGED 2026-10-10: OpenX6100/AetherX6100Buildroot#3** (myshak1, f1447e0). Our CI step skips itself (it checks for `CONFIG_UHID` / `modprobe uhid` first); remove it when convenient |
| 7 | gdyuldin/AetherX6100Buildroot | Bluetooth pairings kept across updates: `/var/lib/bluetooth` in S04restore_backup_config's backup (section 1) | **not sent:** myshak1's **OpenX6100/AetherX6100Buildroot#4** (open, 2026-10-10) has `S39btstate` (pairings in /mnt/bluetooth.tar), more complete. **Drop our CI step when #4 is merged**, or both would save and restore the pairings |
| 8 | gdyuldin (feedback, not a PR) | for his `ver_1.1` Bluetooth screen: keyboards need the agent's `DisplayPasskey` / `DisplayPinCode` (missing: a keyboard asking you to type a code would fail), a paired keyboard marked Trusted, and pairing the moment a keyboard appears (many accept a pairing only for seconds); the image's `bt-agent --capability=NoInputNoOutput` + always discoverable accepts any pairing without asking, and cancelled ours | **sent 2026-10-10 as a comment on OpenX6100/x6100_gui#280; ALL addressed by myshak1 the same day (e38637e on that branch):** LE keyboards listed (filter `auto`, input devices only), passkey/PIN shown in the window, Pair() sent the moment the keyboard appears, discoverable and pairable only 3 min after Scan, agent re-claims default. He has no keyboard and asked us to test on his branch. **Tested 2026-10-11 on VE7NHW's radio: all worked first try; report + log on #280.** Into our build only once merged in OpenX6100 (VE7NHW: wait, "do it proper"; notes in PLAN.md) |
| 9 | OpenX6100/AetherX6100Buildroot | Kernel patch 9002: btusb keeps HCI events ahead of ACL data (poll sync on from probe, a whole jiffy's hold, the event before the worker), so BLE keyboards pair every time; btmon showed the SMP keys arriving before Encryption Change | **MERGED 2026-10-12: OpenX6100/AetherX6100Buildroot#5** (gdyuldin approved). Our copy and CI step removed the same day (7253349) |
| 10 | OpenX6100/x6100_gui | Bluetooth window: Scan in LE / classic turns, a keyboard waited for by name (LE only), old same-name pairings forgotten, Connect ends a Scan, highlight follows the device, WiFi off/on re-setup (`bt_dev.c`, `bt_ctl.c`, `dialog_bt.c`) | **MERGED 2026-10-12: OpenX6100/x6100_gui#291** (gdyuldin approved; from branch `bt-keyboard-pairing` in OpenX6100's repo). OpenX6100's three files are identical to ours |
| 11 | OpenX6100/x6100_gui (issue) | Bluetooth security: CAT RFCOMM channel 1 accepts unpaired connections (and can key PTT), visible and pairable from boot until the window is first opened (bt_start.sh + bt-agent), headset PTT button taken from any device | **issue #292 opened 2026-10-12.** myshak1 agreed with all three; split: **point 3 (headset PTT) is his: x6100_gui#296**, approved by VE7NHW 2026-10-13. **Points 1 + 2 are ours, opened 2026-10-13:** x6100_gui **#297** (CAT RFCOMM `BT_SECURITY_MEDIUM`, branch `bt-security`) and AetherX6100Buildroot **#6** (bt_start.sh hidden, not pairable, no bt-agent; `JustWorksRepairing` kept, branch `bt-security`), reviewers gdyuldin + myshak1. Tested on VE7NHW's radio: keyboard pair/forget/re-pair, FT8CN CAT from a paired phone with the keyboard connected, from an unpaired PC the radio is invisible and CAT refused. **Review 2026-10-13:** #297 approved by myshak1; #6 changes requested (myshak1): done the same day: `JustWorksRepairing = confirm` (the agent decides, yes only during Scan), bt_start.sh retries until hidden + not pairable, main.conf Discoverable/PairableTimeout 180, the window sets PairableTimeout on every Scan, stale comments (Buildroot d92cb37, x6100_gui 8fc9068); radio retest to post Kris approved #6 pending retest (his leftovers done in 9a70644). **Radio retest passed 2026-10-13**; it found the udev rule running bt_start.sh on every connection (`hci0:<handle>`): fixed in Buildroot af4c2ae (`ENV{DEVTYPE}=="host"`). Results posted; waiting for Kris to merge #6 + #297 **2026-10-14: #297 and Buildroot #6 merged by VE7NHW** (both approved); in beta 5.2. |
| 12 | OpenX6100/x6100_gui (reviews) | #271 WSPR (Kris's two fixes in), #296 headset PTT | **approved by VE7NHW 2026-10-12 / 2026-10-13** **#295 approved 2026-10-16** (Buildroot #7 fixed our point). |
| 13 | OpenX6100/x6100_gui | FT8: the knob moves the TX frequency 1 Hz a click however fast it turns (the 5x/10x looked at one diff; 1.0 hands over each click): count clicks per 30 ms, as JS8's 10fdf95 | **PR #300 opened 2026-10-13** (branch `ft8-knob-speed`), reviewers gdyuldin + myshak1; radio test pending (image run 38478388820) **2026-10-14: gdyuldin asks for a rework** (reuse the main screen's knob speed `dt`, main_screen.c ~931, via an API change, not our own count). **2026-10-16: reworked as he asked** (6050dd9, normal commits on top: apps' `rotary_cb(diff, dt)`, FT8 speeds up from `dt`; description updated, Kris told his WSPR/PSK31/Olivia need the extra argument). In test image all-apps-test. **2026-10-18: approved by Georgy**; held until Kris's apps are in (then a commit updating their rotary_cb). **MERGED 2026-10-20** (VE7NHW; Georgy and Kris approved). |
| 14 | OpenX6100/x6100_gui (discussion) | Bluetooth audio for phone digital apps (FT8CN / FT8TW) with our hand test (HSP link, routing, RX weak with no FT8 signals, CAT PTT keys the mic path) | **Discussion #299 opened 2026-10-13**; Kris answered the same day: not his plan (CAT over BT is gdyuldin's), TX easy, RX the open question (Android call processing), WiFi (Icom network protocol) as fallback |
| 15 | myshak1/x6100_gui (his fork) | Conflict fixes for his #272 PSK31, #274 Olivia (both vs the Bluetooth window) and #288 FT2 (vs our #290): main merged into his branches, the Bluetooth case's `break` kept | **PRs myshak1/x6100_gui#1, #2, #3 opened 2026-10-14** (from OpenX6100 branches psk31-merge-main, olivia-merge-main, ft2-merge-main; descriptions show our lines); waiting for Kris |
| 16 | OpenX6100/x6100_gui (comments) | Radio tests of Kris's apps: #271 WSPR (receive window followed Center, TX unmarked), #272 PSK31 / #274 Olivia (waterfall shows static while keyed; the scrolling flicker and our row averaging) | **Posted 2026-10-14.** WSPR: Kris fixed it the same evening (d6377057, decodes 1400-1600 always, thin TX marker). PSK31/Olivia: waiting |
| 17 | OpenX6100/x6100_gui (discussion) | WeFax, NavTex, SSTV from the Murus team (Hany): licence for his files, authorship, app numbers 109-111, a 4th APP page, one PR per app | **Discussion #302 opened 2026-10-14** ([text](upstream-prs/14-murus-apps-discussion.md)); **Hany answered 2026-10-15** (five apps incl. DSC and the Channel List; LGPL-2.1-or-later as they go in; NavTex last); our reply on headers and PR order ([text](upstream-prs/16-murus-licence-reply.md)). PR plan: [tasks/murus-apps.md](tasks/murus-apps.md) **2026-10-15: Hany's full OK** (headers LGPL, 7(b) dropped, unlabelled files LGPL, order agreed). **2026-10-16:** branches `ox-murus-*` ready (licence commit on top); Info window licence text asked (discussioncomment-18984760). **2026-10-17:** Hany OK to change the Info window credits text; done on ox-murus-groundwork e574b53 (LGPL-2.1-or-later, source link), wefax/sstv rebased onto it. **2026-10-18: groundwork #309 and Channel List #310 opened** ([texts](upstream-prs/22-murus-groundwork-pr.md), [23](upstream-prs/23-murus-channels-pr.md)); **WeFax #311 and SSTV #312** opened the same day, stacked ([24](upstream-prs/24-murus-wefax-pr.md), [25](upstream-prs/25-murus-sstv-pr.md)). **2026-10-21 (Kris's reviews):** #311/#312 didn't compile (WeFax Clear still printed the removed gray statistics): fixed d4cb0df, merged into #312 f0a4eaf. #310: broadcast.csv read in place (c84c25f). #309/#310 approved by Kris, mergeable (#309 first). #312 open questions: the four default TX pictures are studio cartoon characters (can't ship; VE7NHW + Hany decide), SSTV's own TX level (later). **2026-10-22: #309 merged by VE7NHW** (Kris approved). Kris approved #311/#312; #310 RAM fix (schedule loaded on first use, 11.5 -> 0.8 MB, 482a91a) waits for his OK; Hany's new SSTV pictures in #312 (a53be16). #311/#312 still based on murus-groundwork/murus-wefax: retarget to main before merging. Hany's new repo TheMurusTeam/x6100_gui (LGPL NAVTEX, DSC, WeFax Denoise) for later PRs. **2026-10-22:** #311 retargeted to main, murus-wefax = the combining commit (carries #310); a test image of the four found our 10-20 merge had dropped tx_level.c/tx_player.c from src/CMakeLists.txt (link error with JS8): fixed 320cffb (#311) / 1632779 (#312), note posted on both; rebuilt image run 39857680626 OK. **Kris's plan: v1.2.0 = #308, #310, #311, #312** (+ bump-v1.2.0, #321), fixes only after; #312 to main once #311 is in. **2026-10-25: #310, #311 (after our main merge b6fa921 cleared GitHub's false conflict) and #312 MERGED by Kris.** Next from Hany's repo: rows 37-38, then NavTex, DSC. |
| 18 | OpenX6100/x6100_gui (discussion) | One transmit-level (ALC) module for every digital app: FT8's per-chunk scheme copied into PSK31/Olivia/WSPR (which never save their level) vs our tx_level.c (averaged, ramps, holds, saves); offered tx_level/tx_player as their own PR | **Discussion #303 opened 2026-10-14** ([text](upstream-prs/15-tx-level-discussion.md)) **2026-10-15:** Georgy + Kris agree (one stored level, PSK31 correction, clamp the stored value; Kris moves his apps once we send it). **2026-10-16:** told them we measure first (3.8 W of 5 W, ALC 0, in JS8). **2026-10-17:** sent as #306 (row 22); pointer posted (discussioncomment-19004580). |
| 20 | OpenX6100/x6100_gui | From Hany's 1KO125 (his commits): spectrum auto levels follow the zoom offset (floor rose 3 dB per zoom step); S-meter offset setting -30..+30 dB for display and CAT | **#304 and #305 opened 2026-10-15** (branches zoom-auto-levels, s-meter-offset), reviewers gdyuldin + myshak1; radio test pending (run 38761835319) **#304 MERGED 2026-10-16** (VE7NHW; R12/R13 drift confirmed by Hany and VE7NHW). **#305:** Kris approved; Georgy doubts ±30 dB; we asked if ±10 dB is OK (2026-10-16), range unchanged until he answers. **2026-10-22: #305 (S-meter offset) merged by VE7NHW** (Kris approved). |
| 19 | JS8Call-improved/Android-port (issue) | FFTW wisdom: js8core measures its plans on every start (desktop keeps js8call_wisdom.dat) | **#109 opened 2026-10-14**; punk-kaos: belongs in js8core, he'd planned it |
| 21 | OpenX6100/AetherX6100Buildroot | JS8 dependencies (boost headers, fftw-single) in `x6100_gui.mk` + `BR2_PACKAGE_BOOST`; `X6100_GUI_LICENSE` GPLv2 -> `LGPL-2.1+, GPL-3.0 (JS8 engine)` (Kris's #301 review) | **#8 opened 2026-10-16** (branch js8-deps, [text](upstream-prs/18-buildroot-js8-deps.md)); before or with #301. **After it merges:** drop main.yml's *Enable JS8 dependencies* step **MERGED 2026-10-18** (VE7NHW, Georgy approved). Our step now adds the packages twice: harmless (Buildroot sorts out duplicates), drop it when convenient. |
| 22 | OpenX6100/x6100_gui | One transmit level (ALC) for the digital modes: tx_level + tx_player on their own, one stored level (never below -42 dB), PSK31 correction via base_gain_offset (not stored), tx_player_stream(), tests/test_tx_level.cpp | **#306 opened 2026-10-17** (branch tx-level, b66a969, from main 9714584), reviewers gdyuldin + myshak1, after VE7NHW's radio test of all-apps-test d91a675 (5 W in the first transmission, 0.3 W steady). Corrects our #303 description; pointer posted on #303.  Also on our main, in RC1.1 (2026-10-17). **MERGED 2026-10-18** (VE7NHW, Georgy approved; 49328dd). |
| 23 | myshak1/x6100_gui (his fork) | digi_menu: closing a list with VOL crashed the GUI (list taken out of the group before lv_obj_del, CANCEL to freed memory) | **#4 opened 2026-10-17** into his `rtty` (branch digi-menu-esc-crash in OpenX6100), noted on #276; same file in #272/#274. **Merged by Kris 2026-10-18** into rtty; same commit on #272/#274. |
| 24 | myshak1/x6100_gui (his fork) | APP layout for all the apps from the start (page 3 WiFi, BT, WSPR, JS8; page 4 PSK31, Olivia, WeFax, SSTV): PSK31 to page 4 place 1, Olivia to place 2 | **#5 and #6 opened 2026-10-18** into his `psk31` / `olivia` (branches psk31-app-page4, olivia-app-page4 in OpenX6100) **Merged by Kris 2026-10-18 (#5, #6, and #7 for WSPR).** Then Kris's **#318** (no empty page 4 in v1.1.0, approved by us): shared commit `app-page4-on` (022475a) offered as **myshak1#8** (PSK31) and **#9** (Olivia), also in #311/#312. |
| 25 | OpenX6100/x6100_gui | Fixed places for the six apps on their way in (APP pages 3 and 4, app numbers, window cases, build lines): they merge in any order without conflicts, the layout never changes | **#313 opened 2026-10-18** (branch app-places, [text](upstream-prs/26-app-places-pr.md)); #308, #311, #312 and myshak1#5, #6, #7 updated to fill their places **MERGED 2026-10-20** (VE7NHW; Georgy approved). |
| 26 | OpenX6100/x6100_gui | From Hany's 1KO125 beta 5, outside the apps: ptt.log (who keyed the transmitter) | **#316 opened 2026-10-20** (branch ptt-log, Hany author) **2026-10-21:** Kris's review done in 143c336 (copied source name, Bluetooth PTT source, writer thread, SWR wording); waiting for his re-review. **2026-10-22: merged by VE7NHW** (Kris approved). |
| 27 | OpenX6100/x6100_gui | From Hany's beta 5: the app waterfalls' colours follow the audio noise floor | **#315 opened 2026-10-20** (branch waterfall-noise-floor, Hany author) **2026-10-21:** made a switch, on in FT8 only, as Kris asked (c99052f); waiting for his re-review. **2026-10-22: merged by VE7NHW** (Kris approved). |
| 28 | OpenX6100/x6100_webserver | From Hany's beta 5: SSTV page (upload/delete SSTV pictures and templates) | **Merged by gdyuldin 2026-10-27.** Buildroot: **AetherX6100Buildroot#9** (branch webserver-sstv-page) approved by gdyuldin 2026-10-28, who tagged webserver **v0.0.5** (= e6eab54b); PR now pins the tag (c9eccd7). **#6 opened 2026-10-20** from fork randal007/x6100_webserver (no push access in that repo) |
| 29 | OpenX6100/x6100_gui (discussion) | From Hany's beta 5: re-send the BASE registers after an I2C write error, stop a TX nobody asked for; should X6100Control return write errors? | **#317 opened 2026-10-20** ([text](upstream-prs/27-i2c-discussion.md)) |
| 30 | OpenX6100/x6100_gui (reviews) | Kris's #318 (APP page 4 only with its first app) and #319 (FT8/FT4 TX 0.5 s into the slot, as WSJT-X) | **Both approved by VE7NHW 2026-10-20** (#319 with two notes from the code) |
| 31 | OpenX6100/x6100_gui | Channel List (#310) + WeFax (#311) microphone keys combined as in Hany's firmware (Hany agreed on #311) | **Ready 2026-10-20 on branch `wefax-channels-merge`** (a7cb47d), for whichever of the two is merged second **2026-10-21:** WeFax fix + broadcast.csv change merged in (71c0a18). **2026-10-22:** schedule-on-first-use merged in (f800528). **2026-10-22: now murus-wefax itself (#311)**; the branch wefax-channels-merge is stale, to delete. |
| 32 | OpenX6100/x6100_gui | test_js8: the time-search test fed 34.5 s faster than real time could miss its window (race in the test, not in TimeSearch); feed exactly the window | **#320 opened 2026-10-21** (Kris found it on #314 and gave the fix; same change on our main dc6419b) **2026-10-21: approved by Kris** (0 failures in 30 runs); ready to merge. **2026-10-22: merged by VE7NHW.** |
| 33 | OpenX6100/x6100_gui | JS8 file headers: a copyright line ("Randal Warren VE7NHW") and "Part of OpenX6100" under the licence line, as Kris did for his files; ported files also credit the JS8Call-improved authors | **In #308 2026-10-22** (46c5e8f, comment posted); same on our main 59a738e. |
| 34 | OpenX6100/x6100_gui (review) | Kris's #321: his copyright + "Part of OpenX6100" lines in his Bluetooth, FT8 UDP, FT8 country files | **Approved by VE7NHW 2026-10-22** (as is; VE7NHW didn't want his small part in bt_dev/bt_ctl named). Kris merges it after the four apps (his v1.2.0 plan). |
| 35 | OpenX6100/x6100_gui (review) | Kris's #323: FT8/FT4 with non-standard own callsigns (type 4 answers, own call in the hash table), /P and /QRP list in Callsign (fixes #322) | **Approved by VE7NHW 2026-10-24** with three notes (WSPR silent with a "/" call, digi_menu.c listed twice, ESC in the list saves nothing). **Merged by Kris 2026-10-25.** |
| 36 | myshak1/x6100_gui (his fork) | Kris's v1.3.0 apps fixed: #10 WSPR "plain call only" (wspr_app_tx_blocked_reason); #11 main merged into rtty (CMakeLists, spectrum.h); #12 main merged into ft2 (worker.c includes, FT2 = 4 / 9); #13-#15 tx_level.c/tx_player.c back in WSPR's, PSK31's, Olivia's build line (our 10-18 app-places merges, myshak1#5-7, had dropped them) | **Opened 2026-10-25** (VE7NHW: "fix all the issues, PRs for all"); branches wspr-plain-call, rtty-merge-main, ft2-merge-main, wspr/psk31/olivia-build-list in OpenX6100. Test image of all of them: run 40221803013 (kris-apps-test), on VE7NHW's card. myshak1#8/#9 (page 4 on) now redundant (main has it since #311). |
| 37 | OpenX6100/x6100_gui | From Hany's repo (TheMurusTeam/x6100_gui 1ko125): app_radio.c, the receiving apps' radio set-up (USB-D locked, zoom 4x unsaved, NR/NB/notch off and held off as JS8's dsp_held_cb) | **#324 opened 2026-10-25** (branch app-radio, Hany author; NR hold 3af8902 from his 3a9adae after Kris's note). **Approved by Kris.** |
| 38 | OpenX6100/x6100_gui | From Hany's repo: WeFax Denoise (fading demodulator + row sharpening; START judged on the classic discriminator, his 5e8df85 after our PC test), Auto Slant 70, USB-D/zoom while open, tests/wefax PC tools (no ML kit, no .exe) | **#325 opened 2026-10-25** (branch wefax-denoise, base app-radio, Hany author). **Approved by Kris.** v1.3.0 material. |
| 39 | OpenX6100/x6100_gui (review) | Kris's #326: bump to v1.2.0 | **Approved by VE7NHW 2026-10-25.** **Merged 2026-10-29 by Kris; v1.2.0 tagged and released 2026-11-01** (JS8, WeFax + Denoise, SSTV, Channel List; #324/#325/#328/#330/#331 in). |
| 40 | myshak1/x6100_gui (his fork) | digi_menu.c: VOL press with Menu/Macros open closed the whole PSK31/Olivia app (the list's delete resets LVGL's keypad, the held key then repeats ESC into the window); `lv_indev_wait_release()` | **#16 (psk31), #17 (olivia), #18 (rtty) opened 2026-10-27** (branches `digi-menu-vol-close-*` in OpenX6100), found by VE7NHW on the radio, reproduced and fixed on the PC (LVGL 8.3, ASan) |
| 41 | OpenX6100/x6100_gui | Channel List without the hand mic: a Channels button on a new APP page 5 (fixed places for NavTex and DSC beside it), Channels as a long-press action, number 117 | **#328 opened 2026-10-27** (branch `channels-app-button`), for Hany to decide. 10-27: Channels moved to place 4 of page 5 (58828f3) to match Hany's 1.1-beta1 layout (NavTex, DSC, FT8 CB). **10-28 (Kris's email, Georgy's wish: an empty button looks like a bug):** no empty places, page 5 gone: APP 3 WiFi, BT, JS8, WeFax; APP 4 SSTV, Channels (a6d8dc4); then Kris merges #324/#325/#328 and builds the v1.2.0 test image **Merged 2026-10-29 by Kris; in v1.2.0.** |
| 42 | OpenX6100/x6100_gui | Channel List crashed the GUI when it opened on an empty list (LVGL 8.3 frees an unset table cell; core dump from the radio, reproduced and fixed on a PC under ASan). One line in build_window() | **#330 opened 2026-10-28** (branch `channels-empty-list`), for Hany (TheMurusTeam), gdyuldin, myshak1 **Merged 2026-10-29 by Kris; in v1.2.0.** |
| 43 | OpenX6100/x6100_gui | From Hany's 1KO125 1.1-beta1 (his reference tag, email 10-27): WeFax Post Proc. (neural network cleans each saved fax into NAME_pp.png, plain C, model wefax_pp.bin 1.2 MB) and the full-screen fax view; 12 commits, Hany author; no training kit | **#329 opened 2026-10-27** (branch `wefax-postproc`, base `wefax-denoise` until #325 merges). In test image kris-apps-test3 (run 40572901929) **2026-11-02:** Kris's review: training kit, model licence + training charts, Hany's empty author e-mail, Post Proc. time; NOTICE lines removed (13271c3); our questions to Hany posted. **2026-11-04:** Hany answered (kit OK without .exe, model LGPL 2.1+ trained on GYA + DWD charts, e-mail info@murus.it, ~2.5 min a fax). **2026-11-05:** branch rebuilt with his e-mail on his 12 commits + the training kit (124d561) + c011244; force-pushed by VE7NHW. **2026-11-05:** our review posted (ESC/F5 in full screen, _pp.png without .tmp, small ones; offered fixes); needs Kris's or Georgy's approval. |
| 44 | myshak1/x6100_gui | Main (v1.2.0) merged into Kris's five app branches, our open fixes (#10-#18) folded in; APP 4 = SSTV, Channels, WSPR, PSK31; APP 5 = Olivia (shared commit `app-pages-v13`) | **myshak1#19-#23 opened 2026-11-01, merged by Kris 2026-11-02** (#8-#18 closed as folded in). OpenX6100 #271/#272/#274/#276/#288 approved + mergeable; Kris asks VE7NHW or Georgy to merge once tested. |
| 45 | OpenX6100/x6100_gui | S-meter offset -10 to +10 dB (was ±30, Georgy's point on #305, never changed before #305 merged) | **#332 opened 2026-11-01, merged by Kris 2026-11-02.** |
| 46 | OpenX6100/x6100_gui (review) | Kris's #333 JTTY (WSJT-X 3.2's mode, from scratch, GPL-3.0; APP 5 after Olivia, MODE_JTTY 10, app 118) | **Approved by VE7NHW 2026-11-02** (notes: ESC/250 ms key-up, copyright-name edits outside the app, two shared level codes later: steady vs swinging modes). **2026-11-02:** Kris fixed the ESC race and took the shared credit lines out; level codes for #303. **2026-11-03: Georgy approved.** **Merged by VE7NHW 2026-11-05** with #271/#272/#274/#276/#288 (Kris asked by e-mail: merge now, v1.3.0 tag later). |
| 47 | OpenX6100/x6100_gui | The shared map ([SHARED_MAP_PLAN.md](SHARED_MAP_PLAN.md)): JS8's map drawing as `map_widget.c` (JS8 pixel-identical), `map_scene.c` (station map + window handling for any app), FT8/FT4/FT2 Show Map (`ft8_ui/map_view.c`), WSPR Show Map (`wspr_map.c`, heard-you ready for WSPRnet), farthest DX | **Branch `shared-map` pushed 2026-11-05** (b3e3d74, 0e1b292, 58908ba; no PR yet). Kris told by e-mail (we take point 11; WSPRnet: asked). Test image run 42150892758. Radio-tested by VE7NHW 11-07 (maps good, WSPRnet works); CQ tag carries the dB ("CQ -12"), heard-you squares their dB. **PRs opened 2026-11-07: #354 widget + scene (base main), #355 FT8/FT4/FT2 map and #356 WSPR map + WSPRnet (both on map-widget).** Reviewers gdyuldin + myshak1 added by VE7NHW on all three. Heads-up on #287 (YunongDai2005's FT8 map) posted 11-07 (issuecomment-6711770105). **Approved by gdyuldin 11-07; merged by VE7NHW 2026-11-08 00:21Z** (#354 0a189532, #355 8a31bb50, #356 82d27293; #355/#356 retargeted to main first); map files on main = shared-map. Branches map-widget, ft8-map, wspr-map deleted by VE7NHW 11-08. |
| 48 | OpenX6100/x6100_gui | Spectrum 3D: past traces stacked into the distance (AetherSDR style), 2D/3D switch (Settings > UI + tap on the label), own **3D size** 35-55 % (Spectrum height stays for 2D), coloured by strength, levels follow auto slowly, nothing kept in TX; `get_time_us()` in time.h | **#344 opened 2026-11-06** (branch `spectrum-3d`, 7d8b20b), reviewers gdyuldin + myshak1 (added by VE7NHW). Radio-tested by VE7NHW on bf28f79 (image 42236779844); the 35-55 % range (VE7NHW 11-06, was 30-70) only compile-checked. CPU on the radio (2D vs 3D) not measured. |
| 49 | OpenX6100/x6100_gui | How we work with AI: AGENTS.md (+ CLAUDE/GEMINI/copilot pointers), docs/working-with-ai.md, PR template, merge checks (tools/checks + checks.yml), Build image test images from any branch + buildroot_ref + unstripped GUI, tools/radio, skills /new-app /pr-check /close-session | **Split 2026-11-11 (Kris):** #377 `ci-checks`, #378 `build-image-test`, #379 `radio-tools`, #373 the guide. **#373 draft opened 2026-11-10** (branch `ai-guide`); feedback asked by e-mail (Georgy, Kris, Hany). Same day: parts 6-10 (all unit tests in CI, ratchets, voice warning, memory trend, bug form) and a /code-review with its 10 findings fixed; head 8e2a3f49, all checks green. x6100-flash run with the card up to Write? (no write). Open for Georgy: colour ratchet, required checks, -Werror=implicit-fallthrough |

## JS8 into OpenX6100: the plan

Agreed with VE7NHW 2026-10-11. VE7NHW is a maintainer of OpenX6100
(team *Maintainers*, with myshak1; gdyuldin is the owner), so the branches
go **in OpenX6100/x6100_gui itself** (no fork), one PR each, **reviewed
and merged by gdyuldin or myshak1**, never merged by us alone. One PR at
a time: the next once the previous is merged.

Sizes against OpenX6100 main (0e6662e, with #281), 2026-10-11:

| PR | Branch | What | Size |
|---|---|---|---|
| 1 | `js8-groundwork` | The changes to code other apps use: radio helpers (TX filter, RX DSP off, speaker play), keypad hold time, JS8's settings group and its `js8_db` (outside upstream's migrations), JS8 + GhostNet frequency rows, `MODE_JS8` in the QSO log and ADIF (+ upstream's SSB-written-as-AM fix), the digital-mode types, the lv_waterfall ring buffer (every app's waterfall cheaper; clear = opaque black). Nothing a user sees on its own; small enough to read every line. **Built 2026-10-11: branch in OpenX6100 at 74b6dfb (16 files, +422 −18); merges cleanly with each of myshak1's open PRs** | 16 files, +422 −18 |
| 2 | `js8-engine` | **Opened 2026-10-14: #301** (35cbb7f; three commits: unchanged import, our patches, build + README licence). `third-party/js8core` (JS8Call-improved/Android-port + our patches, listed in its UPSTREAM.md). **The licence note goes here** (GPL-3.0 engine; combines with the GUI's LGPL-2.1-or-later, the image as a whole is then GPL-3.0; or JS8 as a build option off by default: their call) | 8,000 lines own code; 150,000 vendored (Eigen, CRCpp); 524,000 generated JSC tables |
| 3 | `js8-library` | `src/js8` (messages, inbox, relays, stations, history, ...) and its unit tests (`tests/test_js8*.cpp`), with a CI job to run them | ~10,000 + 4,700 lines |
| 4 | `js8-app` | `src/dialog_js8.c`, `src/js8_*` (waterfall, clock), JS8's TX modules `tx_player` / `tx_level` (new files only JS8 uses: moved here from PR 1 on 2026-10-11), the marks font, the map, the app button and number (`ACTION_APP_JS8` = 112, in `buttons.cpp`, `main_screen.c`, `settings_types.h`, `src/CMakeLists.txt`): after this one JS8 is in the firmware. **Before it:** (a) the APP menu: page 3 has room for 4 apps and WiFi, Bluetooth, WSPR, PSK31, Olivia and JS8 make 6, so a 4th APP page (buttons.cpp counts pages by itself; tried in a trial merge) and an agreed layout with myshak1; (b) one cty.dat: myshak1's #282 added `third-party/cty/cty.dat` (VER20240202, installed to /usr/share/x6100/cty.dat, /mnt/cty.dat overrides); JS8 should read that one (same override) and we offer our newer Big CTY (VER20260915; his dxcc.c parses it) | ~14,000 lines |

Why four: one PR would be unreviewable (gdyuldin asked for small PRs);
the map stays in PR 4 because it is woven through `dialog_js8.c` and a
map-less app would be a version never used on the air. Each PR builds and
works on its own and needs only the ones before it.

**Licences in 3/4 and 4/4 (VE7NHW, 2026-10-16):** our JS8 files that follow desktop JS8Call's code are GPL-3.0-or-later (were GPL-3.0-only until 2026-10-16), the rest LGPL-2.1-or-later; the README's *License* lists which. Say so in each PR. `tests/test_js8_db.cpp` (in OpenX6100 since #290) got its LGPL line on our main only: carry it in 3/4.

Not sent: the UI harness (`tools/js8_ui_harness`, 6,700 lines; later if
they want it), our docs and research, `x6100-bt-pair` (myshak1's Bluetooth
window replaces it).

**Before PR 1: app IDs (settled 2026-10-11).** `press_action_t` values are
saved on the card only when an app is offered as a long-press action. In
OpenX6100/x6100_gui discussion 283 (our post, text in
[upstream-prs/7-app-ids-discussion.md](upstream-prs/7-app-ids-discussion.md))
gdyuldin suggested appending new apps at the end and leaving them out of
the long-press list, so nothing is stored and nothing can clash. myshak1
took that for his apps (no long-press entries; explicit values kept anyway:
WSPR 113, PSK31 114, Olivia 115, Bluetooth 116), and so did we: JS8 opens
from the APP menu only since 2026-10-11 and `ACTION_APP_JS8` has no fixed
number. Other numbers stored on the card, for the same discussion: `MODE_JS8` = 8
in `qso_log.db` (upstream ends at RTTY = 7), digital-mode types
`CFG_DIG_TYPE_JS8` = 2 / `CFG_DIG_TYPE_JS8_GHOSTNET` = 3. JS8 doesn't use
upstream's migration numbers (its `js8_db` table marks what it did).

## 1. The Linux image (AetherX6100Buildroot), patched in our build

Our image build (`.github/workflows/main.yml`, *Build image*) checks out
AetherX6100Buildroot and edits it before building. These edits live **only
in that workflow**: a new Buildroot doesn't remove them, but a changed line
upstream can make one stop applying.

| Step in main.yml | What it changes | Why | Upstream? |
|---|---|---|---|
| *Patch GUI mk* | `x6100_gui.mk`: build our checkout instead of OpenX6100's repo (Buildroot checked out from OpenX6100/AetherX6100Buildroot since 2026-10-11) | builds our GUI | no (ours only) |
| *Enable JS8 dependencies* | `x6100_gui.mk` + `X6100_defconfig`: boost (headers) and fftw-single | js8core needs them | with the JS8 PR |
| ~~*Enable gpsd hotplug (control socket)*~~ | `S50gpsd`: gpsd with `-F` | a GPS plugged in after boot | **merged: AetherX6100Buildroot#2; step removed 2026-10-11** |
| ~~*Enable Bluetooth LE keyboards (uhid)*~~ | kernel `CONFIG_UHID=m`, `modprobe uhid` | LE keyboards type | **merged: AetherX6100Buildroot#3; step removed 2026-10-11** |
| ~~*Keep Bluetooth pairings across updates*~~ | `/var/lib/bluetooth` in S04's backup | pairings across updates | **replaced by myshak1's S39btstate (`/mnt/bluetooth.tar`), AetherX6100Buildroot#4; step removed 2026-10-11** (old cards' `backup.tgz` still holds a pairing; S39btstate then sees `/var/lib/bluetooth` and leaves it) |

**Bluetooth on this radio, for whoever picks this up:** the WiFi and
Bluetooth chip (RTL8723BU, on USB) is one part with one power pin: the
GUI's WiFi switch (APP > WiFi, `wifi_power_on()` in `src/wifi.cpp`)
powers both, so with WiFi off there's no `hci0`. v1.0.2 has no pairing
screen (gdyuldin's `ver_1.1` branch adds one): pair with `bluetoothctl`
over the console, as its own default agent (`agent KeyboardDisplay`,
`default-agent`): the image's `bt-agent --capability=NoInputNoOutput`
(started by `/usr/bin/bt_start.sh` from a udev rule) cancelled the
pairing. Pairings live in `/var/lib/bluetooth` on the rootfs, which a
re-flash replaces; our build adds them to `S04restore_backup_config`'s
backup to DATA (above). Pairing: `x6100-bt-pair` (our `rootfs/usr/bin`,
installed by the GUI's CMake), the same steps as a script.

The gpsd, uhid and pairings-backup steps **fail the build** if the line they change isn't
what they expect, so they can't be lost silently: check upstream's S50gpsd /
S40bluetoothd / S04restore_backup_config and adjust (or drop the step if
upstream took the fix).

**PR to send (AetherX6100Buildroot):** "gpsd: start with a control socket
so USB GPS hotplug works": the one-line change above; the test: boot with
the GPS unplugged, plug it in, `gpspipe -w` lists the device and
`ntpq -n -p 127.0.0.1` reaches `.GPS.`.

**Not pinned:** the workflow takes AetherX6100Buildroot's newest commit at
every build, so upstream changes (e.g. 2026-10: Bluetooth profiles, a
display panel kernel patch, PulseAudio settings) go into our images
without us choosing them. Pinning a commit (`ref:` in the checkout step)
would make builds repeatable; then updating = moving the pin on purpose
and reading this list. Not decided yet.

## 2. Shared GUI code (x6100_gui outside the JS8 app)

`git diff --stat upstream-main..main -- . ':!src/js8' ':!src/dialog_js8.*'
':!third-party' ':!docs' ':!tests' ':!tools'` lists them (2026-10-10:
38 files). The main ones, by what they're for:

- **USB and Bluetooth keyboards** (PR candidates, first in the agreed
  order with gdyuldin): `src/keyboard.c` (hot plug; since 2026-10-10 also
  a Bluetooth keyboard: it has no by-path link, so it's found in udev's
  database, `ID_INPUT_KEYBOARD` + `ID_BUS=bluetooth`, a USB one still
  first), `src/usb_devices.cpp` + `src/pubsub_ids.h` (the udev monitor
  also watches `input`, as `MSG_INPUT_DEVICE_CHANGED`: a Bluetooth keyboard
  connecting, or waking from sleep as a new node, has no USB event),
  `src/kbd_rollover.{c,h}` (keys
  lost while another is down), `src/textarea_window.c` (a held key leaking
  into the next field), `src/keypad.{c,h}` (hold time while JS8 is open).
- **JS8's transmit level**: `src/tx_level.{c,h}`, `src/tx_player.{c,h}`
  (also used by FT8: same audio path).
- **Radio helpers**: `src/radio.{c,h}` (TX filter, speaker play, RX DSP
  off while JS8 is open).
- **Waterfall widget**: `src/widgets/lv_waterfall.{c,h}` (fill rect for
  decode marks, exact invalidation).
- **Settings and database**: `src/cfg/*` (JS8 params, js8_db),
  `src/settings_manager.h`, `sql/digital_modes.csv` (JS8 and GhostNet
  frequencies), `src/adif.c` / `src/qso_log.h` (MODE JS8).
- **Hooking JS8 in**: `src/main_screen.c`, `src/buttons.cpp`,
  `src/settings_types.h`, `src/CMakeLists.txt`, `CMakeLists.txt`.
- Also changed (check before a merge): `src/cat/civ_processor.cpp`,
  `src/cfg/db.cpp`, `src/settings/settings_page_ui.cpp`.

Upgrade notes from the 1.0.2 move: [upgrade-1.0.2/](upgrade-1.0.2/README.md).

## 3. The JS8 engine (js8core)

Local patches, one commit each, with what to send upstream:
[third-party/js8core/UPSTREAM.md](../third-party/js8core/UPSTREAM.md)
(15 as of 2026-10-10). Bugs reported to the JS8Call-improved team:
[js8core-bug-reports.md](js8core-bug-reports.md).
