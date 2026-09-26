# Beta 2: urgent fixes from the first on-air QSOs

VE7NHW's list after the first QSOs on beta 1 (2026-09-25). These come
before the rest of the beta 2 list. One item per commit, tested in the UI
harness and then on the radio.

Suggested order: 1, 3, 5, 6, 4, 7, 2. Auto CQ (2) comes last because it
builds on 1 and 4.

## Status (2026-09-26): all done, tested in the UI harness, not yet on the radio

| # | Result |
|---|---|
| 1 | Done. |
| 2 | Done, with VE7NHW's choice: a **fixed 1-minute** interval (start to start), no knob; the first CQ goes at once. Off on an answer, a press of CQ, a reply you send, Stop TX, band change, 1 h idle. |
| 3 | Done. |
| 4 | Done as planned: `sel_call` separate from the cursor, green bar, `selected:` in the TX bar. |
| 5, 6 | Same root cause, found with real input devices in the harness: **LVGL 8.3's indev reset** (the keyboard window deletes the focused object mid-press) sets the press time to 0, so the still-held key counts as a long press and **repeats every 100 ms** into the next focused object: the list (ESC → stop TX, then close) or the Log's Save (Enter from a USB keyboard). Fix: `lv_indev_wait_release()` in `textarea_window`. The Log also reopens on the edited field. |
| 7 | Done: growing rows keep the list scrolled to the end; while typing, the text box moves to the top of the screen and, with the on-screen keyboard, the list moves up into the space above it (no separate strip needed). |
| 8 | Added: Show *No HB* hides SNR reports (`classify()` → `snr_report`). |
| 9 | Added: USB keyboard lost keys. LVGL's keypad ignores a press while another key is down (fast typing overlaps keys): `src/kbd_rollover.c` releases the old key first. Lowercase was also dropped by the text boxes; now typed as capitals. |

---

## 1. CQ switches heartbeats off

**Problem.** If HB is on and you call CQ, heartbeats keep going. They go
out in the middle of the CQ and the QSO that follows it.

**Today.** `qso_started()` turns HB and HB ACK off, but only for a
directed message: one you send (`tx_queue_at` → `starts_with_call`) or
one sent to you (`handle_incoming` → `js8_starts_qso`). `cq_cb()` never
calls it, because "CQ CQ CQ" isn't a directed message.

**Plan.**
- Split the HB-off part of `qso_started()` into `hb_pause(const char *why)`.
  `qso_started(call)` then becomes `hb_pause("QSO with CALL")`.
- `cq_cb()`: once the CQ is queued, call `hb_pause("CQ")`.
  Screen: "HB and HB ACK off: CQ".
- Also when auto CQ starts (item 2).
- As today, heartbeats stay off until you turn them back on. They are never
  switched back on by themselves.

**Test.** Harness: HB on → CQ → HB button shows Off and the info row
appears. Radio: HB on, CQ, and no heartbeat after the interval.

---

## 2. Auto CQ (hold CQ)

**Desktop JS8Call** (`mainwindow.cpp`, `buildRepeatMenu` / `sendCQ`):
- The CQ interval defaults to **0, meaning "on demand / do not repeat"**.
  The menu offers 1, 5, 10 and 15 minutes plus a custom interval, so there
  is no 2–3 minute preset.
- The timer is `m_nextCQ = next TX cycle + interval`. Any transmission
  restarts it, so the gap is counted from your last TX.
- Auto CQ stops when a directed message **to you** arrives (someone
  answered), and when you select a callsign.
- The button shows the countdown: "CQ (42)" or "CQ (now)".

**Plan.**
- New param `js8_cq_interval` (minutes, 1–15, **default 3**), remembered
  like the HB interval. Auto CQ itself always starts **off** when the app
  opens, like AUTO and HB, so nothing transmits on its own.
- **Hold CQ** → auto CQ on:
  - It sends a CQ now. Desktop waits one interval first, but holding CQ is
    a clear "go".
  - The button changes to `CQ auto\n< 3 min >` and the main knob sets the
    interval, the same way the HB button does (`hb_adjust_*`).
  - It turns heartbeats off (item 1).
- **Press CQ** while auto CQ is on → auto CQ off. A plain press with auto
  CQ off still sends one CQ, as today.
- The button label counts down: `CQ auto\n2:41`.
- The status line and TX bar show when the next CQ is due, like the HB line.
- A 1 s tick (`hb_tick`, renamed `auto_tick`) sends a CQ when it's due:
  - skipped while TX is busy, the keyboard is open or a list is open, and
    tried again next second;
  - due time = end of our last TX + interval (desktop behaviour), so a
    reply you send also pushes it back.
- **Auto CQ stops by itself** when:
  - a directed message to you arrives (`handle_incoming` with
    `js8_starts_qso`), with the screen message "Auto CQ off: CALL
    answered";
  - you send a directed message yourself (Reply / HW CPY?);
  - you press Stop TX (ESC / top knob);
  - you change band;
  - the idle watchdog `js8_auto_idle` fires, so an unattended radio doesn't
    call CQ for hours;
  - the app closes.

**Decided:** fixed 1-minute interval, first CQ at once (see Status).

**Test.** Unit test for the due-time rule. Harness: hold CQ, the countdown,
and a directed message stopping it. Radio: on air.

---

## 3. Hold the page button to go back a page

**Today.** The main screen already does this: page buttons use
`.press = button_next_page_cb`, `.hold = button_prev_page_cb`, and
`buttons.cpp` fills in the `.prev` pointers. The JS8 page buttons only
have `.press = js8_next_page_cb` and `.next`.

**Plan.**
- Add `js8_prev_page_cb` to close any open list (as `js8_next_page_cb`
  does) and then call `button_prev_page_cb`.
- Set `.hold = js8_prev_page_cb` and `.prev = &page_N` on btn_p1..btn_p6:
  page 1's prev is page 6, and so on around the ring.
- The same behaviour as the main screen, so nothing new to learn.

**Test.** Harness: hold on page 1 → page 6, hold on page 3 → page 2.

---

## 4. The selected station doesn't stay selected

**Root cause.** The list's cursor (the lv_table selected row) is *both* the
selected station *and* the thing that auto-scrolls. `at_bottom()` is true
when the cursor sits on the last row, and then every new row makes
`follow()` move the cursor onto the new row. So:
1. You select WB8PLB's message, and it's the newest row.
2. Your own TX row, an info row ("Auto: …", "Logged …") or anyone else's
   decode arrives.
3. The cursor moves to that row. Reply now answers someone else, or says
   "Select a station first" (TX and info rows have no station).

`rebuild_rows()` (the Show filter, the list filling up, a history slot
wrapping) also calls `follow()` and throws the selection away. The green
line (`qso_freq`) stays on the old offset while the cursor has moved,
which is confusing.

**Plan: a selected station that's separate from the scroll position.**
- Add `sel_call` / `sel_freq` / `sel_snr`: the station you picked.
  - It is set when the MFK cursor lands on a row with a station (a user
    move, not `auto_selecting`), and by a press on a row.
  - It is **not** changed by new rows, rebuilds or the auto-scroll.
  - It is cleared by Clear, a band change, or picking another station.
- `selected_station()` returns `sel_*`. Reply, HW CPY?, Query, Log and
  `dialog_js8_selected_call()` all use it. If the station is heard again
  at a new offset, `sel_freq` follows it through `js8_stations`.
- Auto-scroll without moving the selection: scroll the table's view to the
  bottom (`lv_obj_scroll_to_y` to the end) and leave `row_act` alone.
  This is the base for item 7.
- Rows from the selected station, or to you from it, get a marker such as
  a coloured left bar in `table_draw_end_cb`. You can then see who is
  selected even when the cursor row is off screen.
- Status / TX bar: "Selected: WB8PLB 1574 Hz".
- `rebuild_rows()` puts the cursor back on the selected station's newest
  row if it still shows, instead of always on the last row.

**Test.** Harness scenario: select a station on the last row, feed 5 more
messages including our TX and an info row. Reply must still prefill that
call. Radio: select, wait through a few decodes, then Reply.

---

## 5. Log QSO: Enter in a field logs the QSO

**Root cause.** Enter's *press* closes the keyboard (`text_cb` → `ok()` →
`log_edit_done` → `log_list_open()`), and the reopened list focuses
**"Save to log"**. LVGL's keypad then sends `CLICKED` to whatever is
focused when the *same* Enter is *released*. That is Save, so the QSO is
logged straight away.

**Plan.**
- `log_list_open(focus)`: after an edit, reopen with the edited field
  focused (Name → Name), not Save. You then carry on to Grid / Comment /
  Save with the knob.
- Ignore the stray release: list items only act on `CLICKED` if they got
  the `PRESSED` of that key themselves (a `pressed_here` flag set on
  `LV_EVENT_PRESSED`, cleared on `LV_EVENT_DEFOCUSED`). Without this,
  focusing Name would just reopen the keyboard. Put it in `list_add_item()`
  so every list gets it.
- Check the other lists that come back after the keyboard for the same
  trap: Texts… (INFO/STATUS), Alerts > (alert words), Activation (park /
  summit ref), APRS.

**Test.** Harness: open Log, edit Name, Enter. The list is back with Name
focused, nothing is logged, and Save still logs. The same for Grid and
Comment.

---

## 6. ESC in a text field leaves the JS8 app

**What we know.** ESC is the VOL knob press (`keypad.c`,
`BTN_TRIGGER_HAPPY21` → `LV_KEY_ESC`). LVGL sends it as a KEY to the
focused object and then a CANCEL to the same object.
`compose_cancel_cb()` → `compose_close()` puts the **table** back in the
group and focuses it *during that same ESC*. The table's `key_cb` treats
ESC as "close JS8" (`dialog_destruct()`). The exact second delivery isn't
visible from reading the code. Candidates are LVGL's key repeat on a
longer press (the long press time is 1 s, then a repeat every 100 ms, which
sends ESC again to the now-focused table), or the CANCEL/KEY order when
the textarea rather than the keyboard has focus.

**Plan.**
1. **Reproduce first.** Log every ESC in `key_cb` and `text_cb` (tick,
   focused object, composing/popup state) to /tmp/x6100_log.txt, then
   press ESC in Reply, Send…, the Log fields, Texts, Alerts and Park ref
   on the radio while I watch the console.
2. **Fix whatever the path turns out to be, plus a guard.**
   - The key that closes a keyboard or list must not also close the app:
     remember the tick when a keyboard/list was closed by ESC, and in
     `key_cb` ignore ESC until that key has been *released* (track
     press/release, not a time window).
   - `key_cb` closes JS8 only on a fresh ESC press that started while the
     table had focus.
3. The rule everywhere: ESC closes the innermost thing (keyboard → list →
   the app), one level per press.

**Test.** Radio: ESC out of each text box closes only the box. A second
ESC closes JS8.

---

## 7. See the latest text while you pre-type a reply

**Problem.**
- (a) The list only follows new lines while the cursor is on the last row
  (same cause as item 4), so during a QSO the newest text slides out of
  view.
- (b) While you type a reply, the keyboard window (`textarea_window` at
  y = 80 with a keyboard on the bottom half) covers the list. The other
  station's long message keeps growing underneath and you can't read it
  without sending or closing.

**Plan.**
- (a) Always keep the view on the newest row when new text arrives. This
  comes free with item 4: scroll without moving the selection. If you
  moved the cursor up to read older lines, auto-scroll pauses and resumes
  after about 30 s of no knob movement, or at once when you move back to
  the bottom.
- (b) While composing, show a **live strip** between the text field and
  the keyboard: the tail (the last two lines) of the newest message from
  the selected station, or failing that the newest message to you,
  updated as partial frames arrive (`update_slot`). It is a label drawn
  over the waterfall area, which is hidden behind the keyboard anyway.
  The text field stays open and keeps what you typed. Send when you're
  ready, or ESC (after item 6: closes only the keyboard) to keep reading.
- Alternative, if the strip is too small: move the text field to the top
  and shrink the list into the gap above the keyboard. I'll try the strip
  first, send a harness screenshot, and pick one with you.

**Test.** Harness PARTIAL scenario with the keyboard open: the strip grows
as frames arrive. Radio: pre-type during a live long message.

---

## Also noticed

- WB8PLB's long message at 04:38:43 ended mid-word. VE7NHW: not a bug to
  chase; the app was being closed by the ESC bug around then.
