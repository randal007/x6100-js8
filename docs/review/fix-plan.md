# Fix plan

A second pass over every finding: the 27 bugs and 25 improvements of this
review ([bugs.md](bugs.md), [improvements.md](improvements.md)) and the
earlier bug hunt ([bug-hunt-2026-09-28.md](../bug-hunt-2026-09-28.md),
`BH-n`, its "smaller things" numbered `BH-S1`…`BH-S7` here). For each:
is it really worth fixing, and how. **Planning only: no code changed.**
Written 2026-09-28 against `main` at `8896823`.

- **Verdicts:** **Fix** (real, worth it, in a package), **Fix, small**
  (real, low impact, trivial: bundled with its package), **Later** (real,
  not worth it yet), **Won't fix** (with the reason), **Decide** (your
  call; my recommendation given), **Done**.
- **Effort** (Claude's time with tests): **S** under an hour, **M** a few
  hours, **L** a day or more.
- Every package ends with the unit tests, the ASan harness, a CI build and
  a flash; the first build also carries `9391f80` (Reply by SMS, Stations
  unlock), which hasn't been built yet.

## Decisions needed

Nothing in WP1, WP2 or WP7 waits on these; WP3 does. D1 is decided.

| # | Question | My recommendation |
|---|---|---|
| D1 | BH-7: when does HB pause, and does it come back? | **Decided 2026-09-28** (relayed by the builder session): HB and HB ACK **pause** (they stay switched on; the button shows "HB: paused") only when you send something **by hand** that isn't a heartbeat (Reply, Send..., a Query item, CQ, HW CPY?). Nothing heard pauses them, and nothing AUTO sends counts (SNR answers, MSG ACKs, relays, HB ACKs). They resume by themselves 10 min after your last hand-sent message (each new one restarts the 10 min). Not on deselect or unlock (unlike desktop). |
| D2 | BH-12: may automatic replies go out while a **list** is open (Log popup, Inbox, Settings)? Desktop only holds them while you're typing. | Yes: only the keyboard holds them. |
| D3 | B-04: while a message to us is still arriving, **hold** our automatic replies until it ends, or **drop** them as desktop does? | Hold (up to the usual 2 minutes); HB ACKs dropped, as desktop. |
| D4 | B-05: HB ACK / @ALLCALL cooldown 55 min like desktop? Keep it across power-off (desktop does)? | 55 min yes; across power-off not now. |
| D5 | B-08: heartbeats on desktop's fixed schedule? Auto CQ: keep "N minutes after each CQ ends" (your choice) but count only from our CQs, not every transmission? | Yes to both. |
| D6 | B-06: keep our 5-minute "don't answer the same question twice" guard (fixed), or drop it like desktop? | Keep it, keyed by the real asker, AGN? left out. |
| D7 | BH-5: remember one offered answer per station (so Reply works for each), or keep desktop's single outgoing box? | One per station: small, and our Reply is per station. |
| D8 | B-14: an Inbox file that can't be read: move it aside, or refuse to save? | **Decided 2026-09-28: A**, move it aside (`<name>.unreadable-<date>`) and say so; refuse to save only if it can't even be moved. |
| D9 | I-21: pin the buildroot commit CI builds with? (Updates become deliberate.) | Yes. |
| D10 | I-03 / I-19: our migration numbers and `MODE_JS8 = 8` will collide with upstream's future ones. Renumber now, or write the rule down for the next upstream merge? | Write the rule down; renumbering needs database surgery on your card. |

## Work packages, in order

### WP1: Safe transmitting — done 2026-09-28

Anything that can leave the radio keyed or on the wrong frequency.

| Item | Verdict | The fix |
|---|---|---|
| **B-01** crash when the app closes mid-frame | **Fix** | `js8_tx_destroy()`: `stop()`, then join the TX thread (a new `Transmitter::join()`), *then* `reset()`. Give `tx_abort_check` the handle through `ctx` instead of the global `tx`. |
| **I-01** test for it | **Fix** | Catch2 test: a `play` that polls `js8_tx_stopping()`, `js8_tx_destroy()` mid-play (ASan catches today's crash). Harness `ONLY_CLOSETX`: queue a message, wait for `[radio] PTT on`, `dialog_destruct()`, check PTT off and the app survived. |
| **BH-10** band keys mid-transmission | **Fix** | `band_cb()`: the Freq popup's "Not while sending - Stop TX first" check. |
| **BH-11** a waiting reply goes out on the new band | **Fix** | `retuned()` clears the waiting reply(s), the offer and `deliver_pending`. |
| **B-07** no WSPR guard | **Won't fix** (your call, 2026-09-28) | Custom frequencies stay free anywhere. For the record: our 17 m preset (18.104) puts JS8 on 17 m WSPR (18.1060–18.1062) at offsets of about 1950–2200 Hz, as desktop JS8Call does. |
| **B-02** power off skips the app's close | **Won't fix** (your call, 2026-09-28) | No changes to the power-off path; the README's known issue will say that holding POWER does the same as a power cut (WP8). |
| **BH-S5** garbage error text | **Fix, small** | `char err[...] = ""`, and `tx_queue_at()` says "transmitter not running" when `tx` is NULL. |

**On the radio after the build:** into the dummy load with the amp on
standby: GEN during a frame (app closes, PTT drops at once), a band key
during a message (refused), a heartbeat and a reply still going out.

**Status:** B-01, I-01, BH-10, BH-11, BH-S5 done (see the commit that
marks them fixed in bugs.md); B-02 and B-07 dropped by your decision.

### WP2: No lost data — done 2026-09-28

| Item | Verdict | The fix |
|---|---|---|
| **BH-8** full Inbox: new messages not saved or announced | **Fix** | `Inbox::add()` / `HeldMessages::add()` report whether they added (`std::pair<int, bool>`), not a size comparison. |
| **I-12** one safe file writer | **Fix** | `write_file_atomically(path, text)`: write `.tmp`, `fsync`, `rename`, `fsync` the directory. Used by the Inbox, the held messages and `js8_texts.txt`. |
| **BH-16** power cut leaves only `.tmp` | **Fix** | `load()`: no file but a `.tmp`: load that (and rename it back). |
| **B-15** `js8_texts.txt` rewritten in place | **Fix** | Through I-12's writer, errors reported. |
| **B-14** unreadable Inbox overwritten | **Fix** (D8) | Open: if the file exists and doesn't load, rename it aside and say so; if even that fails, no saving ("read only") until the next open. Same for held messages. |
| **B-17** ACK sent though not saved | **Fix** | The stores keep a "saved" flag; `keep()` retries a failed save on a resend; `js8_process()` turns the ACK into Ignore when the message isn't on the card, so their station tries again. |
| **I-13** tests for damaged files | **Fix** | Unreadable file not overwritten; `.tmp`-only recovery; cut-short texts file; full Inbox still saves and announces. |
| **I-11** groups setting too short for ten groups | **Fix, small** | `groups_text[160]`, and `js8_groups_normalise()` stops at the last group that fits whole. |

**On the radio:** nothing special; the harness covers it. Keep the DATA
backup the flash script makes.

**Status:** all eight done (D8 = A, your call); see the commit that marks
them fixed in bugs.md. `src/js8/datafile.cpp` holds the safe reader and
writer; unit tests `[files]`, harness `ONLY_BADFILES`.

### WP3: The unattended station, as desktop (effort L; needs D1–D7)

One rework of the automatic-reply path, which fixes most of these at once.
Today a reply is decided when it's decoded, then waits in one slot
(`pending_auto`) and is checked again in `auto_send()`. Instead:

- a small **reply queue** (8, oldest first, same text not twice, dropped
  after 2 min) replacing `pending_auto` (BH-1);
- **one** `auto_try_send()` that runs `AutoPolicy::decide()` at send time
  (I-08) and checks, in one place: our own `tx_active` flag (set when
  queued, cleared in `ui_tx_done`, fixing BH-S6's race), the keyboard (and
  lists, per D2), an open incoming message to us (B-04), the switches, the
  idle watchdog, Turbo for HB ACKs;
- deliveries tracked per sent text, a few at a time (BH-2).

| Item | Verdict | The fix |
|---|---|---|
| **BH-1** one waiting reply | **Fix** | The reply queue. |
| **BH-2** one delivery tracked | **Fix** | A short list of in-flight deliveries keyed by sent text. |
| **BH-S6** busy cleared before the UI hears "done" | **Fix** | The dialog's own `tx_active` flag for queuing decisions. |
| **I-08** decide once, at send time | **Fix** | As above. |
| **B-04** replies while a message to us is arriving | **Fix** (D3) | Track open partials (`msg_id`, to us?, last frame time) in `add_message()`; closed by their final message or 60 s. |
| **B-05** HB ACK cooldown 15 → 55 min | **Fix** (D4) | `HB_ACK_REPEAT_MS = 55 min`; unit test. |
| **I-09** prune the rate-limit map | **Fix, small** | Drop entries older than 55 min in `sent()`. |
| **BH-7** any message to us switches HB off | **Fix** (D1, decided) | Drop the `handle_incoming()` → `js8_starts_qso()` → `qso_started()` path for HB; pause HB and HB ACK (not off) from the hand-sent paths (`tx_queue_at(..., automatic=false)` for anything but a heartbeat); resume 10 min after the last one; the HB button shows "HB: paused". **Fixed** (see the commit after 3a2f878; harness ONLY_HBPAUSE). Pressing HB while paused resumes at once. |
| **BH-12** Log prompt holds up auto TX | **Fix** (D2) | Automatic transmissions don't count as our side of a QSO (`js8_qsos_sent` only for yours); lists don't hold replies. |
| **B-06** our 5-min guard | **Fix, small** (D6) | Key by the reply's addressee (the asker or its relay path); AGN? exempt. |
| **B-10** `@APRSIS MSG` without `TO:` | **Fix, small** | Return nothing when the pattern doesn't match. |
| **B-18** QUERY CALL for relay-only stations | **Fix, small** | Answer `YES (5m)` without the SNR, as desktop. |
| **B-21** AGN? repeats the last queued text | **Fix, small** | Set `last_tx_text` when the first frame keys; clear it in `retuned()`. |
| **B-03** heartbeat offset rules | **Fix, small** | Your own offset if ≤ 1000 Hz; your offset and your QSO partner's count as free; activity from all recent decodes (the history ring), not just Stations. |
| **B-08** HB / auto CQ timing | **Fix** (D5) | HB: next = last scheduled + N min, on the slot grid. Auto CQ: restart only after our CQ. |
| **B-09** CQ hold while sending | **Fix, small** | A "first CQ after this message" flag honoured in `ui_tx_done()`. |
| **BH-5** one offered answer | **Fix, small** (D7) | Offers kept per station (8, 5 min each). |
| **I-07** desktop-parity test table | **Fix** | Each fix above adds its rows (incoming text, switches → desktop's answer). |

**On the air afterwards** (your to-do list already has it): with a desktop
JS8Call station: heartbeats and HB ACKs, a query while a long MSG to you is
arriving, QUERY MSGS / relays.

### WP4: Smoother screen, lighter work (effort M–L)

| Item | Verdict | The fix |
|---|---|---|
| **I-14** TX bar restyles every 250 ms | **Fix** | First a harness case that runs `lv_timer_handler()` while timing (so it can see it); then set the TX bar's colour/text and the waterfall frame only when they change. |
| **B-25** 64-item scheduler queue drops messages | **Fix** | Waterfall rows go straight into the dialog's own ring under a mutex (no scheduler); partial updates coalesced (latest per message); `QUEUE_MAX_SIZE` 64 → 256 (one line in firmware code). |
| **I-16** cheaper waterfall rows | **Fix, small** | While there: preallocated buffers, `nth_element` for the floor, direct pixel writes, drop `line_buf`. |
| **I-17** station lists never forget | **Fix** | Erase expired stations in `add()`; `js8_stations_find(call)` by key; `heard_stations()` / `free_hb_offset()` without a full copy per message. |
| **B-20** "new station" alerts again | **Fix, small** | A per-power-on set of calls heard, for "new". |
| **BH-17** 200 log lookups every 5 s | **Fix** | Worked-before cached per call and band, refreshed after a log save. |
| **I-15** per-row work on every redraw | **Fix** | Command span stored with each history slot; station fields computed at each rebuild. Only if I-14's measurement says it's still worth it. |

### WP5: Screen and keyboard fixes (effort L, many small)

| Item | Verdict | The fix |
|---|---|---|
| **I-05 + I-18 + B-19** popups | **Fix** | One popup table (open/close/key handling/volume) and one `popup_to_keyboard()` helper; every popup's key callback handles the VOL knob. |
| **B-16** stale edit mode, no keyboard without a callsign | **Fix** | Reset `edit_target` in `construct_cb` and when the keyboard refuses; require the callsign only for sending (inside the helper above). |
| **B-22** ten sendable characters refused | **Fix, small** | Accept all printable ASCII (the font has all 95 glyphs, checked). |
| **B-23** too-long message freezes the count | **Fix, small** | Show `pv.error`. |
| **B-24** unlogged QSO stays first | **Fix, small** | ESC clears it too; Log QSO takes the selected station when it differs; expires with the QSO tracker. |
| **B-26** alert words and punctuation | **Fix, small** | Strip punctuation around tokens (keep `@` and `/`). |
| **BH-13** "73" anywhere ends the QSO | **Fix, small** | Last two words only. |
| **BH-15** Hold Speed uses the cursor | **Fix, small** | The selected station's speed (`js8_stations_find`). |
| **BH-3** Inbox "Reply to HOME" | **Fix, small** | Require a valid callsign (js8core `is_valid_callsign`). |
| **BH-4** `VE7NHW/P` counts as us | **Fix, small** | `to_me` = our call or our base call exactly, as desktop. |
| **BH-6** grids from any grid-shaped word | **Fix** | Grids only from heartbeats, CQs, `GRID` and `@APRSIS GRID`, in Stations and the QSO tracker; tests. |
| **BH-S3** typed log grid unchecked | **Fix, small** | `is_grid()` check in the Log popup. |
| **BH-9** Inbox list shows 50 of 200 | **Fix, small** | All unread first, then the newest, up to 200. |
| **BH-18** info rows vanish on rebuild | **Fix** | Info rows kept in the history ring (a flag), so they survive rebuilds and age like messages. |
| **B-12** partial rows reuse ids | **Fix, small** | A process-wide message counter; partial slots marked final on retune and close. |
| **B-13** "CQ" prefix | **Fix, small** | `first == "CQ"`. |

### WP6: APRS and SMS (effort M)

| Item | Verdict | The fix |
|---|---|---|
| **BH-S7** SMS gateway receipt lands in the Inbox | **Fix** (your "Still to do") | Remember our `{NN}` APRS sends (id, addressee, time); an incoming `ACKnn}` (or `REJnn}`) marks that one delivered (info row "SMS 04 delivered") instead of going to the Inbox. |
| **BH-S1** APRS length counts the `{NN}` ID | **Fix, small** | 67 characters of text, the ID extra (APRS spec). |
| **BH-S2** long relay paths cut at 48 | **Fix, small** | `JS8_PATH_LEN` 96. |
| **BH-S4** RETRIEVE MSG notice not retried | **Fix, small** | Mark it told only after `tx_queue_at()` succeeds. |

**On the air:** an SMS through NR4U and its receipt.

### WP7: Tools and CI (effort M; any time, no radio needed)

| Item | Verdict | The fix |
|---|---|---|
| **I-20** tests not in CI | **Fix** | A job on push: `run_tests.sh` (already builds `test_js8`) with `~[.slow]`, then the harness; the image build stays manual. |
| **I-21** unpinned buildroot and actions; tags never build | **Fix** (D9) | `ref:` on the buildroot checkout; actions pinned to SHAs; one test tag to see whether the tag trigger works, else remove it from the workflow and the README. |
| **B-27** `x6100-flash` default build | **Fix, small** | No default: it lists the newest successful builds and asks. |
| **I-24** flash checks | **Fix, small** | Check the image's partition table (two partitions ending at DATA's start); a unique backup folder. |
| **I-23** console safety | **Fix** | `cmd`/`send` refuse at a login or password prompt or the Xiegu banner; stale PID detected; default port `ttyACM0`; log rotated, `tail` from the end. |
| **I-22** WAV test mode in the firmware; warnings | **Later** | Harmless dead code; warnings would first need a clean-up pass. |

### WP8: Docs (with the release)

| Item | Verdict | The fix |
|---|---|---|
| **I-25** README | **Fix** | Stale button rows, 150 not 200, all popups listed, link `docs/review/`, the power-off note, beta 4 notes for what the packages fixed. |
| bugs.md / bug hunt | **Fix** | Mark each item fixed with its commit as the packages land. |

## Not now

| Item | Verdict | Why |
|---|---|---|
| **I-02** 16 MB TX synthesis buffers | **Later** | Works; first time the synthesis on the radio (a log line per frame) and only rework it if a Slow or Turbo frame is ever late. |
| **I-04** planning each message twice | **Later** | A few ms per message; do it if I-02 is done. |
| **I-06** learned TX gain saved every frame | **Won't fix** | About four small writes a minute while sending, shared with FT8. |
| **I-03** migration numbers | **Won't fix now** (D10) | Existing cards are already at version 5; write the rule into `docs/UPSTREAM_README.md`: upstream's migrations go after ours when merging. |
| **I-19** `MODE_JS8 = 8` | **Won't fix now** (D10) | Same: renumbering needs an update of the QSO database on your card; note it for the merge. |
| **I-10** stall-path test, **B-11** stall re-decode | **Later** | Needs a 5 s stall of a thread that does no I/O; do both together if the receiver is ever reworked. |
| **BH-14** | **Done** | 9391f80: opening Stations ends the lock. |

## All findings at a glance

| ID | Verdict | Package | Effort |
|---|---|---|---|
| B-01 | Fix | WP1 | S |
| B-02 | Won't fix (your call) | — | — |
| B-03 | Fix, small | WP3 | S |
| B-04 | Fix (D3) | WP3 | M |
| B-05 | Fix (D4) | WP3 | S |
| B-06 | Fix, small (D6) | WP3 | S |
| B-07 | Won't fix (your call) | — | — |
| B-08 | Fix (D5) | WP3 | S |
| B-09 | Fix, small | WP3 | S |
| B-10 | Fix, small | WP3 | S |
| B-11 | Later | — | S |
| B-12 | Fix, small | WP5 | S |
| B-13 | Fix, small | WP5 | S |
| B-14 | Fix (D8) | WP2 | M |
| B-15 | Fix | WP2 | S |
| B-16 | Fix | WP5 | S |
| B-17 | Fix | WP2 | S |
| B-18 | Fix, small | WP3 | S |
| B-19 | Fix | WP5 | S |
| B-20 | Fix, small | WP4 | S |
| B-21 | Fix, small | WP3 | S |
| B-22 | Fix, small | WP5 | S |
| B-23 | Fix, small | WP5 | S |
| B-24 | Fix, small | WP5 | S |
| B-25 | Fix | WP4 | M |
| B-26 | Fix, small | WP5 | S |
| B-27 | Fix, small | WP7 | S |
| I-01 | Fix | WP1 | S |
| I-02 | Later | — | M |
| I-03 | Won't fix now | — | — |
| I-04 | Later | — | S |
| I-05 | Fix | WP5 | M |
| I-06 | Won't fix | — | — |
| I-07 | Fix | WP3 | M |
| I-08 | Fix | WP3 | M |
| I-09 | Fix, small | WP3 | S |
| I-10 | Later | — | S |
| I-11 | Fix, small | WP2 | S |
| I-12 | Fix | WP2 | M |
| I-13 | Fix | WP2 | S |
| I-14 | Fix | WP4 | S |
| I-15 | Fix (if I-14 says so) | WP4 | M |
| I-16 | Fix, small | WP4 | S |
| I-17 | Fix | WP4 | M |
| I-18 | Fix | WP5 | M |
| I-19 | Won't fix now | — | — |
| I-20 | Fix | WP7 | M |
| I-21 | Fix (D9) | WP7 | S |
| I-22 | Later | — | M |
| I-23 | Fix | WP7 | M |
| I-24 | Fix, small | WP7 | S |
| I-25 | Fix | WP8 | S |
| BH-1 | Fix | WP3 | M |
| BH-2 | Fix | WP3 | S |
| BH-3 | Fix, small | WP5 | S |
| BH-4 | Fix, small | WP5 | S |
| BH-5 | Fix, small (D7) | WP3 | S |
| BH-6 | Fix | WP5 | M |
| BH-7 | Fix (D1 decided) | WP3 | S |
| BH-8 | Fix | WP2 | S |
| BH-9 | Fix, small | WP5 | S |
| BH-10 | Fix | WP1 | S |
| BH-11 | Fix | WP1 | S |
| BH-12 | Fix (D2) | WP3 | S |
| BH-13 | Fix, small | WP5 | S |
| BH-14 | Done | — | — |
| BH-15 | Fix, small | WP5 | S |
| BH-16 | Fix | WP2 | S |
| BH-17 | Fix | WP4 | S |
| BH-18 | Fix | WP5 | M |
| BH-S1 APRS length counts the ID | Fix, small | WP6 | S |
| BH-S2 relay paths cut at 48 | Fix, small | WP6 | S |
| BH-S3 typed log grid unchecked | Fix, small | WP5 | S |
| BH-S4 RETRIEVE MSG not retried | Fix, small | WP6 | S |
| BH-S5 garbage error text | Fix, small | WP1 | S |
| BH-S6 busy before "done" | Fix | WP3 | S |
| BH-S7 SMS receipt as a message | Fix | WP6 | M |

**In numbers:** 77 findings (27 + 25 from this review, 18 + 7 from the
bug hunt): 66 to fix in 8 packages, 5 later, 5 not now (B-02 and B-07
by your decision), 1 done. 10 decisions for you, each with a
recommendation.
