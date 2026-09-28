# Feature check sheet

Every feature of the JS8 app, and everything we changed in the firmware
around it, split up for a feature-by-feature review. Each feature is read
for **bugs** (written up in [bugs.md](bugs.md)) and for **improvements**
(efficiency, simpler or safer code, tests: [improvements.md](improvements.md)).
The review reads code only: **no code is changed**.

- Started 2026-09-28 on `main` at `9391f80`.
- Done about 10 features at a time, in the batches below, riskiest first:
  anything that transmits or changes the radio, then automatic sending,
  then receiving, then stored data, then the screen.
- A ticked box means the feature has been read for both bugs and
  improvements; the finding IDs follow it.
- `BH-n` = finding n of the earlier read-only hunt,
  [bug-hunt-2026-09-28.md](../bug-hunt-2026-09-28.md). Those aren't
  repeated in bugs.md; a batch that covers one checks it's still there.
- Scope: `src/dialog_js8.c`, `src/js8/`, `src/tx_player.c`, our changes
  to the firmware (77 files differ from the `1ko125-beta6` import), the
  local patches in `third-party/js8core`, tests, tools, CI, and the
  helper scripts in `~/Work/bin`.

## Progress

| Batch | Area | Features | Done |
|---|---|---|---|
| 1 | Transmitting and the radio | F01–F10 | ✅ 2026-09-28: 3 bugs (1 high), 6 improvements |
| 2 | Automatic sending | F11–F20 | ✅ 2026-09-28: 7 bugs (2 medium), 3 improvements |
| 3 | Receiving and decoding | F21–F31 | ✅ 2026-09-28: 3 bugs (all low), 2 improvements |
| 4 | Inbox, saved data, settings | F32–F41 | ✅ 2026-09-28: 4 bugs (1 medium), 2 improvements |
| 5 | The screen | F42–F52 | ✅ 2026-09-28: no new bugs, 3 improvements (1 high) |
| 6 | Selecting, navigating, Stations | F53–F62 | ✅ 2026-09-28: 3 bugs (all low), 1 improvement |
| 7 | Sending by hand | F63–F72 | ✅ 2026-09-28: 3 bugs (all low), 1 improvement |
| 8 | Logging and APRS | F73–F82 | ✅ 2026-09-28: no new bugs, 1 improvement |
| 9 | Alerts, time, app life cycle, firmware hooks | F83–F92 | |
| 10 | Engine, build, tests, tools, docs | F93–F101 | |

## Batch 1: Transmitting and the radio

Anything that keys the transmitter, sets power, or changes the radio's
frequency, mode or filters.

- [x] **F01. TX player**: PTT and modem, ALC-driven gain correction,
  5 W cap, dial shifted for the audio tone and put back after, abort
  between parts. `src/tx_player.c`; also used by FT8 (`src/ft8/tx_worker.c`,
  `dialog_ft8.c`). Earlier: BH-10.
  → I-06. Fine: 5 W cap, dial shift and restore, ALC maths (no NaN: ALC is 0–10). BH-10 still there.
- [x] **F02. Transmitter**: message → frames (`plan_message`), tone
  synthesis, slot timing, Idle/Waiting/Keying states, status, stop.
  `src/js8/tx.cpp`, `src/js8/js8_tx.cpp`.
  → **B-01**, I-01, I-02. Fine: slot timing, frame gaps at every speed, status order.
- [x] **F03. The app's send queue**: `tx_queue_at`, `tx_queue`,
  `tx_start`, `tx_play`, `on_tx_status`/`ui_tx_status`,
  `on_tx_done`/`ui_tx_done`, `last_tx_text`.
  → I-04. Fine: busy check, list row on the first keyed frame. BH smaller item (busy cleared before on_done) still there.
- [x] **F04. Stop TX**: ESC, top knob, `stop_tx_cb`, `tx_stop_all`,
  `tx_abort_check`, `keyed`.
  → **B-01** (closing mid-frame). Fine: ESC and the VOL knob press both stop first, close second.
- [x] **F05. Radio set-up on open, put back on close**: USB-D, RX filter
  200–3000 Hz saved and restored, TX filter 160–3000 Hz, NR/NB/DNF off,
  5 W cap. `construct_cb`, `destruct_cb`, `radio_set_tx_filter`,
  `radio_set_rx_dsp_off` (`src/radio.c`).
  → **B-01**, B-02. Fine: USB-D, filter saved/restored (USB-D has its own filter group), TX filter, DSP off/on, power cap and restore.
- [x] **F06. Band keys and retuning**: `band_cb`, `load_band`,
  `retuned`, `stations_for_band`, `js8_usb_dig`. Earlier: BH-10, BH-11.
  → nothing new. BH-10 and BH-11 still there.
- [x] **F07. Freq popup**: JS8Call presets, GhostNet, custom kHz.
  `freq_show`, `use_presets`, `parse_custom`, `tune_custom`,
  `freq_item_cb`; params `js8_ghostnet`, `js8_custom_on`, `js8_custom_hz`.
  → nothing new. Fine: kHz/MHz parsing, 1.8–54 MHz is inside the radio's 0.5–55 MHz, busy checks.
- [x] **F08. TX offset**: where it comes from (`js8_tx_freq`), Hold
  (`js8_hold_offset`, `hold_cb`), the heartbeat's free offset
  (`free_hb_offset`, `js8_heartbeat_offset`, `find_free_offset`), the top
  limit per speed.
  → B-03. Fine: offset clamped per speed on open, on speed change and on the knob.
- [x] **F09. Alert beep and the TX interlock**: `beep_play`,
  `beep_thread`, `speaker_lock`, `keyed`, `beep_guard`,
  `radio_speaker_play`.
  → nothing new. Fine: the beep stops when TX keys and drains before TX takes the speaker lock.
- [x] **F10. Frequency presets in the database**: `sql/digital_modes.csv`,
  migrations 4 and 5 (`src/params/migrations.c`),
  `cfg/digital_modes.h` types 2 and 3, and whether FT8's band stepping
  can land on JS8 rows.
  → I-03. Fine: UNIQUE(freq, type) makes the inserts idempotent; FT8 only steps through its own types.

## Batch 2: Automatic sending

What an unattended station sends by itself.

- [x] **F11. AUTO and the auto-reply engine**: `process()`,
  `build_reply()`, `AutoPolicy` (`decide`, `sent`, `idle`),
  `js8_process()`. `src/js8/autoreply.cpp`, `js8_ops.cpp`.
  → B-04, B-07, B-10, I-07, I-08, I-09. Fine: stores MSG / MSG TO: whatever AUTO says, as desktop; checksum rule; own-call and low-confidence filters.
- [x] **F12. Queries answered**: `SNR?`, `GRID?`, `INFO?`, `STATUS?`,
  `HEARING?`, `QUERY CALL`, `AGN?`, the 5-minute repeat guard.
  → B-06. Fine: SNR?, INFO?, STATUS?, GRID?, HEARING?, AGN? texts and conditions match desktop (no macros in INFO/STATUS: known).
- [x] **F13. Heartbeats**: auto HB and its interval knob (`hb_tick`,
  `hb_cb`, `hb_hold_cb`, `hb_adjust_start`/`end`), `send_heartbeat`.
  → B-08. Fine: first HB an interval after switching on, not in Turbo, 5 s early, idle stop.
- [x] **F14. HB ACK**: heartbeat acknowledgements, the 15-minute
  @ALLCALL cache.
  → B-04, **B-05**. Fine: text "CALL HEARTBEAT SNR -08 MSG ID n +k" as desktop (minus its stray ")"), AUTO + HB + HB ACK all needed.
- [x] **F15. Auto CQ**: `cq_hold_cb`, the interval knob
  (`cq_adjust_*`), `auto_cq_tick`, `auto_cq_stop`.
  → B-08, B-09. Fine: stops on an answer, band change, Stop TX, idle hour, send failure.
- [x] **F16. QSO detection and the HB pause**: `starts_qso`,
  `qso_started`, `hb_pause`. Earlier: BH-7.
  → nothing new. BH-7 still there; desktop detail: it pauses HB and HB ACKs while a station is *selected* (HeartbeatQSOPause, default on) and brings HB back when deselected.
- [x] **F17. Relays**: passing `>` on, ACKs back along the path, the
  relayed command, the Relay switch (`js8_relay`). `src/js8/directed.cpp`,
  `autoreply.cpp`.
  → nothing new. Fine: next hop, *DE*, ACK back along the path, relayed commands re-dispatched, Relay switch, no relays for groups.
- [x] **F18. Store and forward**: `MSG TO:` holding, `QUERY MSGS`,
  `QUERY MSG n`, deliveries (`deliver_start`/`deliver_end`), group
  messages. Earlier: BH-2.
  → nothing new. Fine: QUERY MSG n / QUERY MSGS / YES MSG ID n +k / NO / NEXT MSG ID as desktop. BH-2 still there.
- [x] **F19. RETRIEVE MSG notices**: `push_tick`, `js8_held_push_due`.
  → nothing new. Fine: matches desktop's pushNotificationHandler (15 min heard, 8 h repeat, one per sweep, no groups, AUTO only).
- [x] **F20. Offers and the waiting reply**: `offer`, `OFFER_MS`,
  `pending_auto`, `PENDING_AUTO_MS`, `auto_send`. Earlier: BH-1, BH-5,
  BH-11.
  → I-08. BH-1, BH-5 and BH-11 still there. Inbox dedupe (same sender and text within 30 min) also covers duplicate APRS relays.

## Batch 3: Receiving and decoding

- [x] **F21. Audio in**: `audio_cb`, the resampler
  (`src/js8/resampler.cpp`), `js8_rx_feed`, silence during a beep.
  → nothing new. Fine: dropped while keyed, silence (same length) during a beep, level only for the log.
- [x] **F22. Receiver thread**: ring buffer, slot schedule, filling
  gaps, realign, `check_clock`. `src/js8/receiver.cpp`, `js8_rx.cpp`.
  → B-11, I-10. Fine: TX gap filled with silence, drift changes restart the clock check, realign threshold.
- [x] **F23. Speeds**: all four decoded together, the Decode button
  (`js8_rx_all`), `rx_speed_mask`, `src/js8/speeds.cpp`, `js8_speed.h`.
  → nothing new. Fine: table matches desktop JS8Submode.cpp (symbol samples, delays, periods, Costas, rxThreshold); desktop's rxSNRThreshold is unused there too.
- [x] **F24. Decode range and QSO offset**: `js8_rx_set_decode_range`,
  `js8_rx_set_qso_offset` (local patch 9).
  → nothing new. Fine: range from the filter, QSO offset updated on every offset change.
- [x] **F25. Duplicate filter**: `DuplicateFilter` (Turbo retries).
  → nothing new. Fine: same speed/frame/offset within one slot.
- [x] **F26. Frame assembler**: multi-frame messages, closing 60 s
  after the latest frame, partial rows growing in place.
  `src/js8/assembler.cpp`, `find_partial`.
  → B-12. Fine: grouping within rxThreshold, 60 s from the latest frame, first+last = complete, missed first frame handled.
- [x] **F27. Frame rendering**: frame bits, compound calls.
  `src/js8/render.cpp`.
  → nothing new. Fine: a faithful port of the Android renderer.
- [x] **F28. Message classification**: to me, groups, heartbeats, CQ,
  low confidence, checksums, grids, base calls. `src/js8/classify.cpp`.
  Earlier: BH-4, BH-6.
  → B-13. BH-4 and BH-6 still there. Fine: checksum rules including @APRSIS.
- [x] **F29. Directed-message parser**: `parse_directed`,
  `relay_next_hop`, `relay_path_calls`, `parse_callsigns`, `is_allcall`.
  `src/js8/directed.cpp`.
  → nothing new (checked in batch 2 against desktop's patterns).
- [x] **F30. Incoming messages in the app**: `on_message`,
  `ui_add_message`, `handle_incoming`, `process_message`,
  `add_message`, `on_cycle_done`.
  → B-12. Fine: acts only on final messages, own messages skipped, the audio callback can't outlive the receiver in practice.
- [x] **F31. Groups and @ALLCALL**: `GROUPS=`, `js8_groups_normalise`,
  group addressing.
  → I-11. Fine: @ added, upper case, duplicates dropped, exact match as desktop.

## Batch 4: Inbox, saved data, settings

- [x] **F32. Inbox store**: file format v1/v2, the 200 cap, ids,
  load/save. `src/js8/inbox.cpp` (`Inbox`). Earlier: BH-8, BH-16.
  → **B-14**, I-12, I-13. BH-8 and BH-16 still there. Fine: parsing in try/catch, v1 files, resends within 30 min (which also catches duplicate APRS relays).
- [x] **F33. Held-message store**: `HeldMessages`, group recipients,
  `notified_ms`. Earlier: BH-16.
  → **B-14**, I-12. Fine: stored by base call, 2-day group window, who fetched a group message, desktop's counts.
- [x] **F34. The C glue for the stores**: `keep()`, `js8_msg_for_me`,
  `js8_msg_to_for_me`, `js8_path_display`, `js8_delivered_signature`.
  `src/js8/js8_ops.cpp`. Earlier: BH-3, BH-8.
  → B-17. BH-3 and BH-8 still there.
- [x] **F35. Inbox window**: the list, one message, mark read, delete,
  Fetch the next, desktop's Reply choices. `inbox_show`,
  `inbox_item_cb`, `inbox_key_cb`. Earlier: BH-9.
  → nothing new. BH-9 still there. Fine: desktop's Reply choices, Fetch the next, held view, delete, focus on the oldest unread.
- [x] **F36. APRS messages in the Inbox**: `aprs_to_inbox`,
  `aprs_sender`, `aprs_sms_from`, Reply by APRS / SMS, gateway receipts
  (`ACKnn}`).
  → nothing new. Fine: the DE sender, the SMS number prefill. The ACKnn} receipt as a message is in the bug hunt's smaller things.
- [x] **F37. New-message notices**: `stored_received`,
  `inbox_refresh_button` (green button), `inbox_label_getter`.
  → nothing new. Fine: resends silent, group vs direct wording, green button.
- [x] **F38. The settings file `js8_texts.txt`**: `load_texts`,
  `save_texts` (INFO, STATUS, GROUPS, ALERTS, OPERATOR, spot form,
  POTA/SOTA refs).
  → B-15, I-12, I-13.
- [x] **F39. Params**: the `js8_*` entries in `src/params/params.c/h`,
  defaults, limits, saving; the harness's copy of the defaults.
  → nothing new. Fine: the harness's copy of the defaults matches; out-of-range saved values are clamped where used.
- [x] **F40. Settings popup**: `texts_cb`, `texts_item_cb`,
  `settings_label`, `relay_label`, INFO/STATUS/Groups/Operator editing,
  km/miles, Stations kept, Messages kept.
  → B-16. I-11 (groups length) from batch 3.
- [x] **F41. A missing, full or read-only DATA partition**: every
  read and write under `/mnt` (inbox, held, texts, log, `app_logs`).
  → **B-14**, B-15, B-17.

## Batch 5: The screen

- [x] **F42. Waterfall in the app**: PSD, noise floor, row queue,
  15 rows/s pacing. `wf_*`, `ui_waterfall_add`, `on_audio`.
  → I-16. Fine: rows paced by the monotonic clock, oldest dropped when far behind, freed on close, noise floor.
- [x] **F43. Waterfall widget**: ring buffer, `invalidate_exact`.
  `src/widgets/lv_waterfall.c` (shared with FT8 and the main screen).
  → I-16. Fine: ring window, exact invalidation, image freed on delete. Shared with FT8 (same behaviour there).
- [x] **F44. Finder and markers**: TX offset marker, the green QSO line.
  → nothing new. Fine: range, width per speed, green line follows the selected station.
- [x] **F45. Message list rows**: `format_row`, `append_row`, history
  ring, trimming at 200 → 150, `rebuild_rows`, `row_hist`.
  → I-15. Fine: history slots vs rows, trimming, rebuild when a shown slot is reused.
- [x] **F46. Row colours and marks**: own red, to-me blue, groups,
  alerts purple, yellow commands (`draw_recoloured`, `table_draw_cb`,
  `table_draw_end_cb`), the end mark `♢`, the `js8_marks_24` font.
  → I-15. Fine: '#' escaped for recolouring, colour priority (TX, alert, to me, CQ, HB, group).
- [x] **F47. Show filter**: All / No HB / Directed (`passes_filter`,
  `show_cb`).
  → nothing new. Fine: No HB keeps HB acks to us, Directed keeps the selected station's frequency.
- [x] **F48. Info rows**: `add_info_row`. Earlier: BH-18.
  → nothing new. BH-18 still there; info rows are cut at 127 characters (cosmetic).
- [x] **F49. Following new rows, reading back**: `follow`,
  `at_bottom`, `READ_PAUSE_MS`, `list_scroll_end`.
  → nothing new. Fine: 30 s read pause, scroll to the very end of a tall row.
- [x] **F50. Status line**: `update_status` (cycles, decodes, drift,
  dial, speed).
  → nothing new. Fine: buffer sizes, idle and Turbo flags.
- [x] **F51. TX bar**: `update_tx_bar`, `tx_timer_cb` (countdown, frame
  progress, selected/locked).
  → **I-14**.
- [x] **F52. Aging and Clear**: Messages kept / Stations kept
  (`msg_age_tick`, `msg_keep_ms`, `apply_station_keep`), `clear_cb`.
  → nothing new. Fine: Clear resets the list, this band's stations, the assembler and the waterfall; Messages kept only trims when you're at the bottom.

## Batch 6: Selecting, navigating, Stations

- [x] **F53. Selecting a station**: `select_row`, `select_at_cursor`,
  `table_select_cb`, `table_press_cb`, `show_selection`,
  `clear_selection`, `sel_call`.
  → nothing new. Fine: knob and press select, press announces, the green line follows the station's offset.
- [x] **F54. Lock (hold MFK)**: `table_hold_cb`, `sel_locked`,
  `press_on_locked`, `press_held`.
  → nothing new. Fine: lock, unlock, another station unlocks; BH-14 fixed (the Stations view unlocks).
- [x] **F55. Rows without a callsign, marked by frequency**:
  `callless_cursor_freq`, `row_on_freq`, `cursor_freq`.
  → nothing new. Fine: marks by the row speed's rxThreshold, no new selection.
- [x] **F56. Knob and keys**: `rotary_cb`, `key_cb`, `user_touch`.
  → B-19. Fine: MFK steps count as activity for the idle watchdog (the table gets the key events).
- [x] **F57. Button pages**: six pages, next/previous, hold time
  500 ms (`keypad_set_long_time`), label getters.
  → nothing new. Fine: popups closed on page change, hold = back, 500 ms hold only while open.
- [x] **F58. Stations store**: `StationList` (`src/js8/stations.cpp`),
  1 h expiry, `band_lists` per dial kHz and slot reuse.
  → I-17. Fine: per-dial lists, slot reuse, a relay-only entry never overwrites one heard directly.
- [x] **F59. Stations view**: `rebuild_station_rows`, `station_fields`
  (SNR, age, grid, km/miles, bearing). Earlier: BH-14 (fixed), BH-17.
  → I-15 (batch 5). BH-17 still there. Fine: the cursor stays on the selected station across rebuilds.
- [x] **F60. Worked-before ★ and alert marks in Stations**:
  `qso_log_search_worked`, `st_worked`, `st_alert`.
  → B-20. BH-17 still there.
- [x] **F61. Stations heard through relays**: `js8_relay_stations`,
  `add_via`.
  → nothing new. Fine: only relays ending here, Relay switch respected, SNR −64, as desktop.
- [x] **F62. The heard list for automatic replies**: `heard_stations`
  → `js8_heard_t`.
  → B-18, I-17. Fine: most recent first, per dial, includes relay-only stations as desktop's call activity does.

## Batch 7: Sending by hand

- [x] **F63. Compose window**: `compose_open`, `compose_ok_cb`,
  `compose_cancel_cb`, accepted characters, prefill, layout, live frame
  count (`compose_changed_cb`, `compose_insert_cb`, `compose_layout`).
  → B-16 (batch 4), B-22, B-23, I-18. Fine: lowercase taken as capitals, the window stays open when a send is refused.
- [x] **F64. Reply**: `reply_cb` (offer, selected station, prefill).
  → nothing new. BH-5 still there. Fine: the offer is used only for the same station within 5 min; an edited offer isn't counted as a delivery.
- [x] **F65. Send...**: `send_cb`.
  → nothing new.
- [x] **F66. HW CPY? and AGN?**: `hw_cpy_cb`, `last_tx_text`.
  → B-21.
- [x] **F67. Query list**: `query_cb`, `query_item_cb`, `query_msg_cb`,
  `js8_query_text` (SNR?, GRID?, INFO?, STATUS?, HEARING?, QUERY MSGS,
  Fetch message #, Relay via them, Can they reach).
  → nothing new. Fine: texts as desktop's call menu; Message / via / Fetch / Relay / QUERY CALL prefills.
- [x] **F68. CQ button**: `cq_cb`, `send_cq`.
  → nothing new (auto CQ: B-08, B-09). Fine: 4-character grid; a single CQ switches HB off, as desktop's does.
- [x] **F69. Heartbeat button**: `heartbeat_cb` (restarts the HB timer).
  → nothing new (offset: B-03). Fine: restarts the HB timer, refused in Turbo, not taken as a QSO start.
- [x] **F70. Speed**: `speed_cb`, `speed_hold_cb`, `selected_speed`,
  `speed_warn`, `set_speed`. Earlier: BH-15.
  → nothing new. BH-15 still there. Fine: refused while sending, offset clamped to the new speed, HB timer restarted.
- [x] **F71. Decode button**: `decode_cb` (this speed or all).
  → nothing new.
- [x] **F72. Frame preview and sendable characters**: `js8_tx_preview`,
  `js8_tx_sendable_char`, `tx_preview`.
  → B-23.

## Batch 8: Logging and APRS

- [x] **F73. QSO tracker**: `QsoTracker`, `js8_qsos_*` (reports, grids,
  end of QSO). `src/js8/qsolog.cpp`. Earlier: BH-6, BH-13.
  → nothing new. BH-6 and BH-13 still there. Fine: HB acks aren't QSOs, base calls, expiry, one offer per QSO.
- [x] **F74. Log popup**: `log_cb`, `log_prepare`, `log_list_open`,
  `log_item_cb`, `log_edit_done`, `log_save`, `my_log_grid`,
  `log_reports_*`.
  → nothing new. Fine: a typed grid is upper-cased by the keyboard (the bug hunt's "cn89" can't happen; "HOME" still can), reports refreshed while open, Save is a separate step.
- [x] **F75. ADIF file and log database**: `adif_record`,
  `adif_append`, `js8_log_append`, `js8_log_band`; `src/adif.c`
  (MODE_JS8, and the SSB fall-through fix), `qso_log.h`.
  → I-19. Fine: fsync'd append, header once, MFSK/JS8 written and read back, our SSB fall-through fix, OPERATOR = station call when unset (desktop).
- [x] **F76. Log prompt**: `log_offer`, `prompt_cb`, `js8_log_prompt`.
  Earlier: BH-12.
  → nothing new. BH-12 still there.
- [x] **F77. POTA/SOTA activation**: `act_cb`, `act_hold_cb`, MY_SIG
  fields.
  → nothing new. Fine: MY_SIG/MY_SIG_INFO for POTA, MY_SOTA_REF for SOTA.
- [x] **F78. Operator callsign**: `EDIT_OPERATOR`,
  `js8_operator_call_valid`.
  → nothing new. Fine: 3+ characters, letters/digits/'/', a digit.
- [x] **F79. APRS menu and message format**: `aprs_cb`,
  `aprs_item_cb`, `aprs_prepare`, `aprs_compose` (SMS, email, Winlink,
  67 characters, `{NN}` IDs).
  → nothing new. The bug hunt's "length check counts the {NN} ID" still there. Fine: IDs 1–99, 9-character addressee, gateway prefills.
- [x] **F80. Grid spot**: `aprs_grid` (`@APRSIS GRID`).
  → nothing new.
- [x] **F81. Position beacon (grid or GPS)**: `aprs_gps`,
  `aprs_position`, `beacon_text`, `aprs_beacon`, `beacon_changed_cb`,
  `js8_latlon_to_grid`, `gps_last_fix` (`src/gps.c`).
  → nothing new. Fine: 10-character GPS grid, APRS position ambiguity (8 + 9 characters), 2-minute fix age, live frame count.
- [x] **F82. POTA/SOTA spot form**: the `spot_*` functions.
  → nothing new. Fine: APSPOT and APRS2SOTA formats, MHz formatting to 1.3 GHz, the K-/VE- park hint.

## Batch 9: Alerts, time, app life cycle, firmware hooks

- [ ] **F83. Alert words and matching**: `src/js8/alerts.cpp`,
  `js8_alert_hit`, `alert_check`.
- [ ] **F84. Alerts popup**: `alerts_show`, `alerts_item_cb`,
  `alerts_switch_label`, the `JS8_ALERT_*` bits.
- [ ] **F85. Time Sync**: `time_sync_cb`, `time_sync_hold_cb`,
  `sync_samples`, `js8_sync_drift`.
- [ ] **F86. JS8 time and drift plumbing**: `wall_ms`/`set_drift_ms`
  (receiver), `js8_wall_ms`, `now_wall_ms`, the transmitter's clock.
- [ ] **F87. Opening and closing the app**: `construct_cb`,
  `destruct_cb` (threads, timers, popups, memory, files).
- [ ] **F88. Worker thread → screen hand-off**: `scheduler_put`
  callbacks (`on_message`, `on_cycle_done`, `on_tx_status`,
  `on_tx_done`, `on_audio`) and what they may touch.
- [ ] **F89. Popups**: `any_popup`, `popup_guard`, `close_popups`,
  `list_add_item`, `list_item_focused_cb`, deletion rules.
- [ ] **F90. Keyboard input**: `src/kbd_rollover.c`, `src/keyboard.c`,
  `swallow_key` in `src/textarea_window.c`.
- [ ] **F91. App launcher and keypad hooks**: `src/buttons.cpp`,
  `src/main_screen.c`, `dialog_settings.cpp` (long-press action),
  `ACTION_APP_JS8`, `src/keypad.c`.
- [ ] **F92. Radio helpers added to the firmware**: `src/radio.c`
  (TX filter, DSP off, speaker play) as used outside JS8.

## Batch 10: Engine, build, tests, tools, docs

- [ ] **F93. js8core local patches 1–11**: `third-party/js8core`,
  `UPSTREAM.md`.
- [ ] **F94. WAV playback and test signals**: `src/js8/wav.cpp`,
  `testsignal.cpp`, `js8_rx_play_wav`, `tools/js8_wavgen`.
- [ ] **F95. Unit tests**: `tests/test_js8.cpp`, `run_tests.sh`.
- [ ] **F96. UI harness**: `tools/js8_ui_harness` (`driver.c`,
  `stubs.c`, `main.cpp`).
- [ ] **F97. CI build and release**: `.github/workflows/main.yml`.
- [ ] **F98. Build files**: `CMakeLists.txt`, `src/js8/CMakeLists.txt`,
  js8core's CMake.
- [ ] **F99. Flash script**: `~/Work/bin/x6100-flash` (outside the repo).
- [ ] **F100. Console and screenshot helpers**: `~/Work/bin/x6100-console`,
  `x6100-screenshot` (outside the repo).
- [ ] **F101. README and docs against the code**: is the manual right?
