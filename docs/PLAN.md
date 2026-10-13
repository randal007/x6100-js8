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
1. Typing-lag radio-tested OK (2026-10-13): PR #9 waits for VE7NHW's merge click.
2. Bluetooth security: #297 approved; Buildroot #6 review fixes pushed 2026-10-13; test image run 38448410652 building, then a radio retest.
3. JS8 into OpenX6100: 1/4 (#290) waits for gdyuldin; licence settled.
4. Workflow clean-up: one status list, PLAN_DONE archive, buildroot_ref (done 2026-10-13); CLAUDE.local.md backup place to decide.
5. Ticked items move to [PLAN_DONE.md](PLAN_DONE.md) at each offload.

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
  so). The add-on idea is off. **Next:** 2/4 (js8-engine, with that note
  and local patch 16) once #290 is merged; 3/4 will carry
  JS8_RX_TEXT_LEN 640 (typing-lag).
- [ ] **JS8 typing and logging, branch `typing-lag`** (5 commits on main; all JS8 app / library, nothing shared). Radio-tested
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
  pass; **merge waits for VE7NHW** (Claude isn't allowed to merge
  unreviewed), then delete `typing-lag`. Was:
  PR `typing-lag` into main (self-merge OK), delete it. README already
  describes all of it on the branch (Send…, Logging).
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
  TEST-ONLY commit any more): *Build image* run **38448410652** with
  `buildroot_ref=bt-security`. **Next:** flash it, radio retest (keyboard
  pair; a phone that forgot the radio re-pairs from Scan; Scan twice,
  still pairable after the first 3 min; from the unpaired PC hidden right
  after power-on; FT8CN CAT), post the results on #6; when both are
  merged, delete `bt-security-js8` and add
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
