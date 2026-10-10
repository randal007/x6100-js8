/* ---- Saved messages --------------------------------------------------- */

/* Desktop JS8Call's Saved Messages: texts you send often, with macros
 * (<CALL>, <SNR>, <MYGRID4> ...) filled in as they go. Query > Saved
 * messages >: press one to send it at once (desktop's default), hold the
 * MFK on it to edit it. Desktop starts with "TNX 73 GL". */

static void load_saved(void) {
    memset(saved_msgs, 0, sizeof(saved_msgs));
    char buf[SAVED_N * (TX_TEXT_MAX + 2) + 64], notice[160];
    saved_writable = js8_file_read(JS8_SAVED_PATH, buf, sizeof(buf), notice, sizeof(notice));
    data_file_notice(notice);
    if (saved_writable && access(JS8_SAVED_PATH, F_OK) != 0) { /* none saved yet */
        snprintf(saved_msgs[0], sizeof(saved_msgs[0]), "TNX 73 GL");
        return;
    }
    /* One a line, empty lines for empty slots; capitals, as typed here. */
    char *p = buf;
    for (int i = 0; i < SAVED_N && *p; i++) {
        char *nl = strchr(p, '\n');
        if (nl) *nl = '\0';
        p[strcspn(p, "\r")] = '\0';
        snprintf(saved_msgs[i], sizeof(saved_msgs[i]), "%s", p);
        for (char *c = saved_msgs[i]; *c; c++)
            if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 'a' + 'A');
        if (!nl) break;
        p = nl + 1;
    }
}

static void save_saved(void) {
    if (!saved_writable) {
        msg_update_text_fmt("Not saved: %s can't be read or moved (check the SD card)", JS8_SAVED_PATH);
        return;
    }
    char   buf[SAVED_N * (TX_TEXT_MAX + 2) + 64];
    size_t n = 0;
    buf[0]   = '\0';
    for (int i = 0; i < SAVED_N; i++) n += snprintf(buf + n, sizeof(buf) - n, "%s\n", saved_msgs[i]);
    if (!js8_file_write(JS8_SAVED_PATH, buf)) msg_update_text_fmt("Can't write %s", JS8_SAVED_PATH);
}

/* Into the keyboard with saved message i; Enter saves it, ESC leaves it. */
static void saved_edit_open(int i) {
    saved_edit = i;
    if (popup_to_keyboard(&query_list, EDIT_SAVED, saved_msgs[i]))
        msg_update_text_fmt("Message %d: Enter saves, empty clears. Macros: <CALL> <SNR> <TDELTA> <MYCALL> "
                            "<MYGRID4> <MYGRID12> <MYINFO> <MYSTATUS> <MYIDLE> <MYVERSION>",
                            i + 1);
}

/* Press: send it now, macros filled in. An empty one: write it. */
static void saved_item_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    user_touch();
    if (!saved_msgs[i][0]) {
        saved_edit_open(i);
        return;
    }
    char  call[JS8_RX_CALL_LEN];
    float freq;
    int   snr;
    bool  have = selected_station(call, sizeof(call), &freq, &snr);
    /* Desktop would send it with <CALL> left out: not to nobody here. */
    if (!have && js8_macros_need_station(saved_msgs[i])) {
        msg_update_text_fmt("Select a station first: message %d has <CALL>, <SNR> or <TDELTA>", i + 1);
        return;
    }
    char text[TX_TEXT_MAX * 2];
    macros_fill(saved_msgs[i], true, text, sizeof(text));
    char *t = text + strspn(text, " ");
    for (char *end = t + strlen(t); end > t && end[-1] == ' ';) *--end = '\0';
    if (!*t) {
        msg_update_text_fmt("Nothing to send: message %d has only macros that are empty now", i + 1);
        return;
    }
    query_close();
    if (have) apply_hold(freq);
    tx_queue(t);
}

/* Hold the MFK: edit it. */
static void saved_hold_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    user_touch();
    /* The MFK's release would otherwise land in the keyboard as Enter. */
    lv_indev_t *indev = lv_indev_get_act();
    if (indev) lv_indev_wait_release(indev);
    saved_edit_open(i);
}

/* The message line says what a press would send. */
static void saved_focus_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (!saved_msgs[i][0]) {
        msg_update_text_fmt("Message %d is empty: press or hold to write one", i + 1);
        return;
    }
    if (!sel_call[0] && js8_macros_need_station(saved_msgs[i])) {
        msg_update_text_fmt("%d: %s (select a station first)", i + 1, saved_msgs[i]);
        return;
    }
    char text[TX_TEXT_MAX * 2];
    macros_fill(saved_msgs[i], true, text, sizeof(text));
    msg_update_text_fmt("%d: %s", i + 1, text);
}

static void saved_back_cb(lv_event_t *e) {
    (void)e;
    popup_leave(&query_list);
    query_open();
}

/* The ten in the Query list's place (wider: messages are long), focus on
 * `focus`; < Back to the Query list. */
static void saved_open(int focus) {
    query_list = query_list_create(560, "Saved messages: press sends, hold edits");
    lv_obj_t *items[SAVED_N];
    for (int i = 0; i < SAVED_N; i++) {
        char label[TX_TEXT_MAX * 2 + 8], text[TX_TEXT_MAX * 2];
        if (saved_msgs[i][0]) {
            macros_fill(saved_msgs[i], false, text, sizeof(text)); /* as desktop's menu shows them */
            snprintf(label, sizeof(label), "%d  %s", i + 1, text);
        } else {
            snprintf(label, sizeof(label), "%d  (empty)", i + 1);
        }
        lv_obj_t *b = items[i] = list_add_item(query_list, label);
        lv_label_set_long_mode(lv_obj_get_child(b, -1), LV_LABEL_LONG_DOT); /* one line, never scrolling */
        if (!saved_msgs[i][0]) lv_obj_set_style_text_color(b, lv_color_hex(0x909090), 0);
        lv_obj_add_event_cb(b, saved_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, saved_hold_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, saved_focus_cb, LV_EVENT_FOCUSED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, query_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
    }
    query_list_end_item("< Back", saved_back_cb);
    query_list_end_item("Close", query_close_cb);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(items[focus >= 0 && focus < SAVED_N ? focus : 0]);
}

/* For tools/js8_ui_harness: Waterfall: Sharp (0) .. Calm (3), as Settings. */
void dialog_js8_wf_avg(int level) {
    param_i_set(cfg.js8.wf_avg(), level);
    atomic_store(&wf_avg_level, level % WF_AVG_LEVELS);
}

/* For tools/js8_ui_harness: the waterfall's place on the screen, and its
 * palette (WF_MIN_DB..WF_MAX_DB over 256 colours). */
bool dialog_js8_wf_area(lv_area_t *a, const lv_color_t **palette) {
    if (!wf_box) return false;
    lv_obj_get_coords(wf_box, a);
    *palette = (const lv_color_t *)style.wf_palette;
    return true;
}

/* For tools/js8_ui_harness: the red band's offset, and the green band's
 * place and width in pixels (false when it isn't shown). */
int dialog_js8_finder_hz(void) {
    return finder_shown;
}
bool dialog_js8_cursor_band(int *x, int *w) {
    if (!cursor_box || lv_obj_has_flag(cursor_box, LV_OBJ_FLAG_HIDDEN)) return false;
    *x = lv_obj_get_x(cursor_box);
    *w = lv_obj_get_width(cursor_box);
    return true;
}

/* For tools/js8_ui_harness: the station the selection points at. */
bool dialog_js8_selected_call(char *call, unsigned len) {
    float freq;
    int   snr;
    return table && selected_station(call, len, &freq, &snr);
}

/* For tools/js8_ui_harness: the decode marks drawn lately (frequency and
 * JS8_MARK_* level of each bracket), up to `max`. */
unsigned dialog_js8_marks(float *freq_hz, uint8_t *level, unsigned max) {
    unsigned n = 0;
    for (unsigned i = 0; i < MARK_MEMORY && n < max; i++) {
        if (!marks_drawn[i].used) continue;
        freq_hz[n] = marks_drawn[i].freq_hz;
        level[n]   = marks_drawn[i].level;
        n++;
    }
    return n;
}

/* For tools/js8_ui_harness: the Stations view's QRZ line if it shows, else "". */
const char *dialog_js8_st_qrz(void) {
    return st_qrz_label && !lv_obj_has_flag(st_qrz_label, LV_OBJ_FLAG_HIDDEN) ? lv_label_get_text(st_qrz_label) : "";
}

/* For tools/js8_ui_harness: Auto time sync on or off. Scenarios that move
 * JS8's time on by minutes with a drift switch it off: Auto would put the
 * drift back within a slot, as desktop's does. */
void dialog_js8_time_auto(bool on) {
    param_i_set(cfg.js8.tsync_auto(), on);
    js8_rx_set_auto_sync(rx, on);
}

/* For tools/js8_ui_harness: the Stations view's calls in row order, the
 * cursor's with a '>'; how many stations are listed. */
int dialog_js8_station_rows(char *out, unsigned len) {
    out[0] = '\0';
    if (!table || !view_stations) return 0;
    uint16_t cur = 0, col = 0;
    lv_table_get_selected_cell(table, &cur, &col);
    size_t n = 0;
    for (uint16_t r = 0; r < rows && n < len; r++)
        if (row_hist[r] >= 0)
            n += snprintf(out + n, len - n, "%s%s%s", n ? " " : "", r == cur ? ">" : "", st_rows[row_hist[r]].call);
    return st_count;
}

/* For tools/js8_ui_harness: the station history (NULL if not open). */
js8_history_t *dialog_js8_history(void) {
    return history_db;
}

/* For tools/js8_ui_harness: the Stations view's star column for `call`
 * ('@', '*' or ' '), or 0 if it isn't listed. */
char dialog_js8_station_star(const char *call) {
    for (int i = 0; i < st_count; i++)
        if (strcasecmp(st_rows[i].call, call) == 0) {
            station_fields_t f;
            station_fields(&st_rows[i], now_wall_ms(), &f);
            return f.star[0];
        }
    return 0;
}

/* For tools/js8_ui_harness: the status line (top right), and our grid now. */
const char *dialog_js8_status_text(void) {
    update_status();
    return status ? lv_label_get_text(status) : "";
}
const char *dialog_js8_my_grid(void) {
    return my_grid();
}

/* For tools/js8_ui_harness: a decoded message, handed over as the
 * receiver's thread does. */
void dialog_js8_test_message(const js8_rx_msg_t *m) {
    on_message(m, NULL);
}

/* For tools/js8_ui_harness: how far the list is scrolled past its end, in
 * pixels (blank space under the last row); 0 when it isn't. */
int dialog_js8_list_overscroll(void) {
    if (!table) return -1;
    lv_obj_update_layout(table);
    lv_coord_t below = lv_obj_get_scroll_bottom(table), y = lv_obj_get_scroll_y(table);
    if (below >= 0 || y <= 0) return 0;
    return -below < y ? -below : y;
}

/* For tools/js8_ui_harness: does message-list row `row` have the green bar? */
bool dialog_js8_row_marked(unsigned row) {
    return table && row < rows && row_is_selected_station(row_hist[row]);
}

/* ---- T4: auto-reply, heartbeats ---------------------------------------- */

/* Any key, button or knob: resets the idle watchdog. */
static void user_touch(void) {
    js8_auto_user_activity(autop, now_wall_ms());
}

/* Recently heard stations, most recent first: HEARING?, QUERY CALL and
 * RETRIEVE MSG look here. */
static unsigned heard_stations(js8_heard_t *out, unsigned max) {
    static js8_station_t list[MAX_ROWS];
    if (max > MAX_ROWS) max = MAX_ROWS;
    int n = js8_stations_recent(stations, now_wall_ms(), list, (int)max);
    for (int i = 0; i < n; i++) {
        out[i].call     = list[i].call;
        out[i].snr      = list[i].snr;
        out[i].heard_ms = list[i].heard_ms;
    }
    return (unsigned)n;
}

/* The switches as they are now, for deciding again at send time. */
static js8_auto_settings_t auto_settings(void) {
    /* Their macros filled in, as desktop answers INFO? and STATUS?. */
    static char info_now[TX_TEXT_MAX + 1], status_now[TX_TEXT_MAX + 1];
    macros_fill(info_text, true, info_now, sizeof(info_now));
    macros_fill(status_text, true, status_now, sizeof(status_now));
    js8_auto_settings_t st = {
        .autoreply = param_i_get(cfg.js8.auto_mode()),
        .heartbeat = param_i_get(cfg.js8.hb()) && !hb_paused(),
        .hb_ack    = param_i_get(cfg.js8.hb_ack()) && !hb_paused(),
        .relay     = param_i_get(cfg.js8.relay()),
        .my_call   = my_call(),
        .my_grid   = my_grid(),
        .info      = info_now,
        .status    = status_now,
        .groups    = groups_text,
        .held      = held,
    };
    return st;
}

static void reply_drop(int i) {
    memmove(&replies[i], &replies[i + 1], (size_t)(n_replies - i - 1) * sizeof(replies[0]));
    n_replies--;
}

/* An automatic reply: in the queue, then out at the first chance. */
static void auto_queue(const js8_auto_result_t *r) {
    for (int i = 0; i < n_replies; i++)
        if (strcmp(replies[i].r.text, r->text) == 0) return; /* already waiting */
    if (n_replies == REPLIES) {
        add_info_row("Auto: too many replies waiting, not sent: %s", r->text);
        return;
    }
    replies[n_replies].r       = *r;
    replies[n_replies].ms      = now_wall_ms();
    replies[n_replies].cycle   = cycles;
    replies[n_replies].checked = false;
    n_replies++;
}

/* The oldest reply that may go now, decided again as things are now: the
 * switches, the idle watchdog and the @ALLCALL cooldown. Waits for our own
 * TX and the keyboard (not a list, your choice); while a message to us is
 * still arriving nothing goes, and while any is arriving no HB ACK. */
static void auto_try_send(void) {
    int64_t now = now_wall_ms();
    for (int i = 0; i < n_replies;) {
        if (now - replies[i].ms > REPLY_WAIT_MS) {
            reply_drop(i);
            continue;
        }
        /* Desktop answers once all of a cycle's decodes are in, and drops
         * the answer if a message to us is still arriving then (an HB ACK
         * if any message is): we can't hear its next frames while we send
         * (D3: as desktop). */
        bool ended = replies[i].cycle != cycles || now - replies[i].ms > CYCLE_WAIT_MS;
        if (!replies[i].checked && ended) {
            replies[i].checked = true;
            if (message_open(true) || (replies[i].r.hb_ack && message_open(false))) {
                add_info_row("Auto: %s not sent, a message is still arriving", replies[i].r.text);
                reply_drop(i);
                continue;
            }
        }
        i++;
    }
    if (!n_replies || tx_active || composing || message_open(true)) return;

    js8_auto_settings_t st = auto_settings();
    for (int i = 0; i < n_replies;) {
        js8_auto_result_t r = replies[i].r;
        if (!replies[i].checked || (r.hb_ack && message_open(false))) {
            i++; /* it waits; the others may go */
            continue;
        }
        reply_drop(i);
        if (js8_auto_decide(autop, &r, &st, now) != JS8_AUTO_SEND) continue; /* switched off meanwhile */
        if (r.hb_ack && !js8_speed_heartbeats(cur_speed())) continue;        /* desktop: no HB ACKs in Turbo or Ultra */
        int offset = r.hb_ack ? free_hb_offset(false) : param_i_get(cfg.js8.tx_freq());
        LV_LOG_USER("JS8 auto: '%s' at %d Hz", r.text, offset);
        if (!tx_queue_at(r.text, offset, true)) continue;
        js8_auto_sent(autop, &r, now);
        if (r.deliver_id) deliver_start(r.deliver_id, r.deliver_group_call, r.text);
        if (r.kind == JS8_REPLY_RELAY) msg_update_text_fmt("Relaying %s's message", r.to);
        add_info_row("Auto: %s", r.text);
        return; /* one at a time: the next when this one ends */
    }
}

/* AUTO off: the answer offered to a station, if it's still fresh. */
static int offer_find(const char *call) {
    int64_t now = now_wall_ms();
    for (int i = 0; i < OFFERS; i++)
        if (offers[i].call[0] && strcmp(offers[i].call, call) == 0 && now - offers[i].ms < OFFER_MS) return i;
    return -1;
}

/* One per station: a newer answer to the same station replaces its old
 * one; with all taken, the oldest goes. */
static void offer_add(const js8_auto_result_t *r) {
    int slot = 0;
    for (int i = 0; i < OFFERS; i++) {
        if (offers[i].call[0] && strcmp(offers[i].call, r->to) == 0) {
            slot = i;
            break;
        }
        if (!offers[slot].call[0]) continue;
        if (!offers[i].call[0] || offers[i].ms < offers[slot].ms) slot = i;
    }
    snprintf(offers[slot].call, sizeof(offers[slot].call), "%s", r->to);
    snprintf(offers[slot].text, sizeof(offers[slot].text), "%s", r->text);
    offers[slot].ms         = now_wall_ms();
    offers[slot].deliver_id = r->deliver_id;
    snprintf(offers[slot].deliver_group_call, sizeof(offers[slot].deliver_group_call), "%s", r->deliver_group_call);
}

/* Everything that answered stations on this frequency: waiting replies,
 * offers, the offer taken into the keyboard, messages still arriving and
 * the band activity. */
static void auto_clear(void) {
    n_replies = 0;
    memset(offers, 0, sizeof(offers));
    deliver_pending.id = 0;
    memset(open_msgs, 0, sizeof(open_msgs));
    memset(activity, 0, sizeof(activity));
}

/* Heartbeats and HB ACKs pause while you're busy: anything you send by
 * hand except a heartbeat (Reply, Send..., Query, CQ ...) pauses them until
 * HB_PAUSE_MS after the last one; they resume by themselves (the user's
 * choice: nothing heard or sent automatically pauses them, and selecting
 * or unlocking a station doesn't resume them, unlike desktop). The
 * switches stay on. */
static bool hb_paused(void) {
    return hb_paused_until && now_wall_ms() < hb_paused_until;
}

static void hb_pause(const char *why) {
    if (!param_i_get(cfg.js8.hb()) && !param_i_get(cfg.js8.hb_ack())) return;
    bool was = hb_paused();
    hb_paused_until = now_wall_ms() + HB_PAUSE_MS;
    if (hb_adjusting) hb_adjust_end();
    if (btn_hb.disp_btn) buttons_refresh(&btn_hb);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    if (!was) add_info_row("HB and HB ACK paused %d min: %s", HB_PAUSE_MS / 60000, why);
    update_status();
}

/* The pause ran out (or HB was pressed): heartbeats carry on. One that fell
 * due meanwhile goes out at the next chance. */
static void hb_resume(void) {
    hb_paused_until = 0;
    if (btn_hb.disp_btn) buttons_refresh(&btn_hb);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    update_status();
}

static void handle_incoming(const js8_rx_msg_t *m) {
    if (js8_starts_qso(m)) {
        char why[JS8_RX_CALL_LEN + 12];
        snprintf(why, sizeof(why), "%s answered", m->from);
        auto_cq_stop(why);
        /* Someone calling you (an answer to your CQ or not; not a heartbeat
         * ACK, nor a doubtful decode) pauses heartbeats too. */
        if (!m->low_confidence) {
            snprintf(why, sizeof(why), "%s called", m->from);
            hb_pause(why);
        }
    }

    js8_heard_t heard[32];
    unsigned    n = heard_stations(heard, 32);

    js8_auto_settings_t st = auto_settings();
    js8_stored_t        kept;
    js8_auto_result_t   r;
    js8_process(autop, m, &st, heard, n, last_tx_text, now_wall_ms(), inbox, &kept, &r);
    stored_received(&kept);

    switch (r.action) {
    case JS8_AUTO_SEND:
        auto_queue(&r); /* goes when this decode cycle ends (auto_try_send) */
        break;
    case JS8_AUTO_OFFER:
        offer_add(&r);
        if (r.kind == JS8_REPLY_RELAY) {
            char dest[JS8_RX_CALL_LEN];
            snprintf(dest, sizeof(dest), "%.*s", (int)strcspn(r.text, ">"), r.text);
            msg_update_text_fmt("%s asks you to relay to %s - select it and press Reply to pass it on", r.to, dest);
            add_info_row("%s: Reply passes on \"%s\" (AUTO does it by itself)", r.to, r.text);
        } else if (strcmp(r.command, ">") == 0) {
            msg_update_text_fmt("Relayed message via %s - select it and press Reply to send ACK", r.to);
            add_info_row("%s: Reply sends \"%s\" (AUTO sends it by itself)", r.to, r.text);
        } else if (strcmp(r.command, "MSG") == 0 || strcmp(r.command, "MSG TO:") == 0) {
            msg_update_text_fmt("Message from %s - select it and press Reply to send ACK", r.to);
            add_info_row("%s: Reply sends \"%s\" (AUTO sends it by itself)", r.to, r.text);
        } else if (r.deliver_id) {
            msg_update_text_fmt("%s asks for the message held for them - select it and press Reply to send it", r.to);
            add_info_row("%s asks for held message %d: Reply sends it", r.to, r.deliver_id);
        } else if (strcmp(r.command, "HW CPY?") == 0) {
            msg_update_text_fmt("%s asks how you copy - select it and press Reply", r.to);
            add_info_row("%s asked HW CPY?: Reply has \"%s\"", r.to, r.text);
        } else if (strcmp(r.command, "MSG ID") == 0) {
            msg_update_text_fmt("%s holds a message for you - select it and press Reply to fetch it", r.to);
            add_info_row("%s holds a message for you: Reply sends \"%s\"", r.to, r.text);
        } else {
            msg_update_text_fmt("%s asked %s - select it and press Reply to answer", r.to, r.command);
            add_info_row("%s asked %s: Reply sends \"%s\"", r.to, r.command, r.text);
        }
        break;
    default:
        break;
    }
}

/* Desktop's stored-message notices (pushNotificationHandler): every 15
 * minutes, with AUTO on and nothing else going out, "W1ABC RETRIEVE MSG 3"
 * to a station we hold a message for, heard in the last 15 minutes and not
 * told in 8 hours. One per look. */
#define PUSH_INTERVAL_MS (15 * 60 * 1000)

static void push_tick(void) {
    int64_t now = now_wall_ms();
    if (!push_next_ms) push_next_ms = now + PUSH_INTERVAL_MS;
    if (now < push_next_ms) return;
    push_next_ms = now + PUSH_INTERVAL_MS;
    if (!param_i_get(cfg.js8.auto_mode()) || !held || js8_auto_idle(autop, now) || !my_call()[0]) return;
    if (tx_active || n_replies || composing) return;
    js8_heard_t heard[32];
    unsigned    n = heard_stations(heard, 32);
    char        text[64];
    int         id;
    if (!js8_held_push_due(held, heard, n, now, text, sizeof(text), &id)) return;
    LV_LOG_USER("JS8 auto: '%s'", text);
    /* Told only once it's queued: one that couldn't go out is tried at the
     * next look, not in 8 hours (bug hunt S4). */
    if (!tx_queue_at(text, param_i_get(cfg.js8.tx_freq()), true)) return;
    js8_held_push_sent(held, id, now);
    add_info_row("Auto: %s", text);
}

/* Messages kept (Settings): every 30 s, drop rows past their time, unless
 * you're scrolled up reading. */
static void msg_age_tick(void) {
    static int64_t next;
    int64_t        now = now_wall_ms(), keep = msg_keep_ms();
    if (now < next) return;
    next = now + 30000;
    if (!keep || view_stations || !table || !at_bottom()) return;
    for (uint16_t r = 0; r < rows; r++) {
        if (row_hist[r] < 0) continue;
        if (now - hist_ms[row_hist[r]] > keep) rebuild_rows();
        return; /* the oldest message row decides */
    }
}

/* Once a second: send a heartbeat when one is due. */
/* GPS time into the battery clock (rtc1), so the radio keeps it after the
 * GPS is unplugged: once a power-on, when the system clock is synced
 * (ntpd, from the GPS: the radio has no network) and there's a current
 * fix, never while sending. rtc1 shares i2c-0 with the amplifier's band
 * data: one write, on its own thread (hwclock takes ~0.1 s). */
#ifndef JS8_RTC_FLAG_PATH
#define JS8_RTC_FLAG_PATH "/tmp/js8_rtc_saved" /* /tmp is cleared at boot */
#endif
static bool        rtc_done;
static atomic_bool rtc_busy;
static atomic_int  rtc_result; /* from the save thread for the list: 1 set, -1 failed, 0 nothing new */

static void *rtc_save_thread(void *arg) {
    (void)arg;
    bool ok = js8_clock_save_rtc();
    LV_LOG_USER("JS8: battery clock %s from GPS time", ok ? "set" : "not set");
    if (ok) {
        FILE *f = fopen(JS8_RTC_FLAG_PATH, "w");
        if (f) fclose(f);
    }
    atomic_store(&rtc_result, ok ? 1 : -1);
    atomic_store(&rtc_busy, false);
    return NULL;
}

static void rtc_tick(void) {
    /* The list says what happened once the write is done, not when it
     * starts. A failed write isn't tried again this power-on: rtc1 shares
     * i2c-0 with the amplifier's band data. */
    int result = atomic_exchange(&rtc_result, 0);
    if (result > 0) add_info_row("Battery clock set from GPS time (kept when the GPS is unplugged)");
    if (result < 0) add_info_row("Battery clock not set: writing it failed (the time is right until power-off)");

    static int64_t next;
    int64_t        now = now_mono_ms();
    if (rtc_done || atomic_load(&rtc_busy) || now < next) return;
    next = now + 30000;
    if (access(JS8_RTC_FLAG_PATH, F_OK) == 0) { /* done since power-on */
        rtc_done = true;
        return;
    }
    if (!gps_fix_current() || tx_active || !js8_clock_synced()) return;
    rtc_done = true;
    atomic_store(&rtc_busy, true);
    pthread_t t;
    if (pthread_create(&t, NULL, rtc_save_thread, NULL) != 0) {
        atomic_store(&rtc_busy, false);
        return;
    }
    pthread_detach(t);
}

static void hb_tick(void) {
    beep_log_level();
    rtc_tick();
    if (hb_adjusting && now_wall_ms() - hb_adjust_ms > HB_ADJUST_MS) hb_adjust_end();
    /* The transmitter is idle but ui_tx_done never came (its message was
     * lost): don't wait for it for ever. */
    if (tx_active && !js8_tx_busy(tx)) {
        if (!tx_quiet_ms) tx_quiet_ms = now_wall_ms();
        else if (now_wall_ms() - tx_quiet_ms > TX_DONE_LOST_MS) {
            tx_active = false;
            finder_sync();
        }
    } else {
        tx_quiet_ms = 0;
    }
    /* Replies that waited for the keyboard or a message still arriving
     * (after a transmission, ui_tx_done does the same). */
    auto_try_send();
    push_tick();
    msg_age_tick();
    if (hb_paused_until && !hb_paused()) hb_resume();
    if (!param_i_get(cfg.js8.hb())) {
        hb_next_ms = 0;
        return;
    }
    if (btn_hb.disp_btn && !hb_adjusting) buttons_refresh(&btn_hb); /* its countdown */
    int64_t now = now_wall_ms();
    if (hb_paused()) return;
    if (js8_auto_idle(autop, now) || !js8_speed_heartbeats(cur_speed())) return;
    /* As on desktop, the first one comes an interval after switching on;
     * page 1's Heartbeat sends one now. */
    if (hb_next_ms == 0) {
        hb_next_ms = hb_first_ms();
        update_status();
    }
    if (now < hb_next_ms - 5000) return;   /* desktop prepares it 5 s early */
    /* Lists don't hold it up, only the keyboard. Waiting replies went
     * first (auto_try_send above). */
    if (tx_active || composing || !my_call()[0]) return;
    LV_LOG_USER("JS8 auto: heartbeat (due %lld)", (long long)hb_next_ms);
    if (send_heartbeat(true)) {
        /* On desktop's fixed schedule: an interval after the one due, not
         * after this one went (later if it waited). */
        hb_next_ms = js8_following_heartbeat_ms(hb_next_ms, now, param_i_get(cfg.js8.hb_interval()));
        update_status();
    }
}

static const char *auto_label_getter(void) {
    return param_i_get(cfg.js8.auto_mode()) ? "AUTO:\nOn" : "AUTO:\nOff";
}

static void auto_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    param_i_set(cfg.js8.auto_mode(), !param_i_get(cfg.js8.auto_mode()));
    buttons_refresh(btn);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    msg_update_text_fmt(param_i_get(cfg.js8.auto_mode()) ? "AUTO on: answers SNR? GRID? INFO? STATUS? HEARING? AGN?"
                                          : "AUTO off: answers are offered on Reply");
    update_status();
}

static const char *hb_label_getter(void) {
    static char buf[24];
    if (hb_adjusting) {
        snprintf(buf, sizeof(buf), "HB: knob\n< %u min >", param_i_get(cfg.js8.hb_interval()));
        return buf;
    }
    if (!param_i_get(cfg.js8.hb())) return "Heart-\nbeat";
    if (hb_paused()) return "HB auto:\npaused";
    if (!js8_speed_heartbeats(cur_speed())) {
        snprintf(buf, sizeof(buf), "HB auto:\nnot %s", js8_speed_name(cur_speed()));
        return buf;
    }
    if (!hb_next_ms) return "HB auto:\nsoon";
    int secs = (int)((hb_next_ms - now_wall_ms() + 999) / 1000);
    if (secs < 1) return "HB auto:\nnow";
    if (secs < 60) snprintf(buf, sizeof(buf), "HB auto:\n%d s", secs);
    else snprintf(buf, sizeof(buf), "HB auto:\n%d:%02d", secs / 60, secs % 60);
    return buf;
}

/* Setting the interval with the main knob, as for auto CQ: press Heartbeat
 * (or wait) to finish. */
static void hb_adjust_start(button_data_t *btn) {
    if (cq_adjusting) cq_adjust_end();
    hb_adjusting    = true;
    hb_adjust_ms    = now_wall_ms();
    buttons_refresh(btn);
    update_tx_bar();
}

static void hb_adjust_end(void) {
    if (!hb_adjusting) return;
    hb_adjusting = false;
    if (btn_hb.disp_btn) buttons_refresh(&btn_hb);
    update_tx_bar();
    if (param_i_get(cfg.js8.hb()))
        msg_update_text_fmt("Auto HB every %u min (press Heartbeat to stop)", param_i_get(cfg.js8.hb_interval()));
}

static void hb_auto_stop(const char *why) {
    if (!param_i_get(cfg.js8.hb())) return;
    param_i_set(cfg.js8.hb(), false);
    hb_adjusting    = false;
    hb_next_ms      = 0;
    hb_paused_until = 0;
    if (btn_hb.disp_btn) buttons_refresh(&btn_hb);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    msg_update_text_fmt("Auto HB off: %s", why);
    add_info_row("Auto HB off: %s", why);
    update_status();
}

static const char *hb_ack_label_getter(void) {
    if (!param_i_get(cfg.js8.hb_ack())) return "HB ACK:\nOff";
    if (hb_paused() && param_i_get(cfg.js8.hb())) return "HB ACK:\npaused";
    return (param_i_get(cfg.js8.auto_mode()) && param_i_get(cfg.js8.hb())) ? "HB ACK:\nOn" : "HB ACK:\nOn (idle)";
}

static void hb_ack_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    param_i_set(cfg.js8.hb_ack(), !param_i_get(cfg.js8.hb_ack()));
    buttons_refresh(btn);
    if (param_i_get(cfg.js8.hb_ack()) && !(param_i_get(cfg.js8.auto_mode()) && param_i_get(cfg.js8.hb()))) {
        msg_update_text_fmt("HB ACK acts only while AUTO and auto HB (hold Heartbeat) are on");
    }
    update_status();
}

/* ---- INFO / STATUS texts -------------------------------------------- */

/* js8_texts.txt couldn't be read or moved aside: never write over it. */
static bool texts_writable = true;

/* A data file was moved aside (or can't be written): say so on the screen
 * and in the log. */
static void data_file_notice(const char *notice) {
    if (!notice || !notice[0]) return;
    LV_LOG_USER("JS8: %s", notice);
    msg_update_text_fmt("%s", notice);
    add_info_row("%s", notice);
}

/* Desktop's default STATUS (Configuration.cpp "MyStatus"), the user's
 * choice (2026-10-09): what STATUS? is answered with until you change it. */
#define STATUS_DEFAULT "IDLE <MYIDLE> VERSION <MYVERSION>"

static void load_texts(void) {
    info_text[0] = status_text[0] = last_pota[0] = last_sota[0] = alert_words[0] = spot_note[0] = '\0';
    groups_text[0] = operator_call[0] = '\0';
    bool status_def = false; /* STATUSDEF=1: the default was given once */
    char buf[2048], notice[160];
    texts_writable = js8_file_read(JS8_TEXTS_PATH, buf, sizeof(buf), notice, sizeof(notice));
    data_file_notice(notice);
    char *save = NULL;
    for (char *line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        line[strcspn(line, "\r")] = '\0';
        if (strncmp(line, "INFO=", 5) == 0) snprintf(info_text, sizeof(info_text), "%s", line + 5);
        if (strncmp(line, "STATUS=", 7) == 0) snprintf(status_text, sizeof(status_text), "%s", line + 7);
        if (strcmp(line, "STATUSDEF=1") == 0) status_def = true;
        if (strncmp(line, "POTA=", 5) == 0) snprintf(last_pota, sizeof(last_pota), "%s", line + 5);
        if (strncmp(line, "SOTA=", 5) == 0) snprintf(last_sota, sizeof(last_sota), "%s", line + 5);
        if (strncmp(line, "ALERTS=", 7) == 0) js8_alert_words_normalise(line + 7, alert_words, sizeof(alert_words));
        if (strncmp(line, "GROUPS=", 7) == 0) js8_groups_normalise(line + 7, groups_text, sizeof(groups_text));
        if (strncmp(line, "OPERATOR=", 9) == 0 && !js8_operator_call_valid(line + 9, operator_call, sizeof(operator_call)))
            operator_call[0] = '\0';
        if (strncmp(line, "SPOTMODE=", 9) == 0 && line[9]) snprintf(spot_mode, sizeof(spot_mode), "%s", line + 9);
        if (strncmp(line, "SPOTHZ=", 7) == 0) spot_typed_hz = atoi(line + 7);
        if (strncmp(line, "SPOTTYPED=", 10) == 0) spot_use_typed = atoi(line + 10) != 0;
        if (strncmp(line, "SPOTNOTE=", 9) == 0) snprintf(spot_note, sizeof(spot_note), "%s", line + 9);
    }
    if (!spot_typed_hz) spot_use_typed = false;
    /* Every card so far saved an empty STATUS= without anyone choosing it:
     * the default goes in once (STATUSDEF=1 marks that); a STATUS cleared
     * after that stays empty, as on desktop. */
    if (!status_def) {
        if (!status_text[0]) snprintf(status_text, sizeof(status_text), "%s", STATUS_DEFAULT);
        if (texts_writable) save_texts();
    }
}

/* Written safely (js8_file_write): a power cut during a save used to leave
 * the file empty, losing every setting in it. */
static void save_texts(void) {
    if (!texts_writable) {
        msg_update_text_fmt("Not saved: %s can't be read or moved (check the SD card)", JS8_TEXTS_PATH);
        return;
    }
    char buf[2048];
    snprintf(buf, sizeof(buf),
             "INFO=%s\nSTATUS=%s\nPOTA=%s\nSOTA=%s\nALERTS=%s\n"
             "SPOTMODE=%s\nSPOTHZ=%d\nSPOTTYPED=%d\nSPOTNOTE=%s\n"
             "GROUPS=%s\nOPERATOR=%s\nSTATUSDEF=1\n",
             info_text, status_text, last_pota, last_sota, alert_words, spot_mode, (int)spot_typed_hz,
             spot_use_typed ? 1 : 0, spot_note, groups_text, operator_call);
    if (!js8_file_write(JS8_TEXTS_PATH, buf)) msg_update_text_fmt("Can't write %s", JS8_TEXTS_PATH);
}

static void texts_close(void) {
    hclear_btn = NULL;
    if (!texts_list) return;
    lv_obj_del_async(texts_list);
    texts_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void texts_close_cb(lv_event_t *e) {
    (void)e;
    texts_close();
}

/* The Settings list's items (user data): the texts are edit_t values. */
#define SETTINGS_RELAY    100
#define SETTINGS_ST_KEEP  101
#define SETTINGS_MSG_KEEP 102
#define SETTINGS_MILES    103
#define SETTINGS_MARKS    104
#define SETTINGS_TRESET   106 /* 105 was Time Sync now: the Time button (page 4) now */
#define SETTINGS_WF       107
#define SETTINGS_DECODE   108
#define SETTINGS_HCLEAR   109 /* clear the station history: press, then again within HCLEAR_MS */
#define SETTINGS_FREQ     110 /* Frequencies: JS8Call's / GhostNet / custom kHz (page 6's Freq before) */
#define SETTINGS_ULTRA    111 /* Ultra on the Speed button (experimental) */
#define HCLEAR_MS         5000

static int64_t   hclear_armed_ms; /* the first press (monotonic), 0: not armed */

/* Five seconds on without the second press: the line as it was. */
static void hclear_tick(void) {
    if (!hclear_armed_ms || now_mono_ms() - hclear_armed_ms < HCLEAR_MS) return;
    hclear_armed_ms = 0;
    if (texts_list && hclear_btn) lv_label_set_text(lv_obj_get_child(hclear_btn, 0), "Clear station history...");
}

static const char *relay_label(void) {
    return param_i_get(cfg.js8.relay()) ? "Relay: On" : "Relay: Off";
}

/* The label of a line that changes in place. */
static const char *settings_label(int which) {
    static char buf[40];
    switch (which) {
    case SETTINGS_RELAY: return relay_label();
    case SETTINGS_ST_KEEP:
        snprintf(buf, sizeof(buf), "Stations kept: %s",
                 st_keep_opts[param_i_get(cfg.js8.st_keep()) < ST_KEEP_N ? param_i_get(cfg.js8.st_keep()) : 2].label);
        return buf;
    case SETTINGS_MSG_KEEP:
        snprintf(buf, sizeof(buf), "Messages kept: %s",
                 msg_keep_opts[param_i_get(cfg.js8.msg_keep()) < MSG_KEEP_N ? param_i_get(cfg.js8.msg_keep()) : 0].label);
        return buf;
    case SETTINGS_MILES: return param_i_get(cfg.js8.miles()) ? "Distance: miles" : "Distance: km";
    case SETTINGS_MARKS: return param_i_get(cfg.js8.decode_marks()) ? "Decode marks: On" : "Decode marks: Off";
    case SETTINGS_DECODE: return param_i_get(cfg.js8.rx_all()) ? "Decode: All speeds" : "Decode: My speed";
    case SETTINGS_ULTRA: return param_i_get(cfg.js8.ultra()) ? "Ultra on Speed button: On" : "Ultra on Speed button: Off";
    case SETTINGS_FREQ: snprintf(buf, sizeof(buf), "Frequencies: %s", freq_name()); return buf;
    case SETTINGS_HCLEAR:
        if (hclear_armed_ms && now_mono_ms() - hclear_armed_ms < HCLEAR_MS) {
            int n = js8_history_station_count(history_db);
            snprintf(buf, sizeof(buf), "Press again: clear %d station%s", n, n == 1 ? "" : "s");
            return buf;
        }
        return "Clear station history...";
    case SETTINGS_WF:
        snprintf(buf, sizeof(buf), "Waterfall: %s", wf_avg_name[param_i_get(cfg.js8.wf_avg()) % WF_AVG_LEVELS]);
        return buf;
    case SETTINGS_TRESET:
        if (js8_drift_ms()) snprintf(buf, sizeof(buf), "Reset time drift (%+.1f s)", js8_drift_ms() / 1000.0);
        else snprintf(buf, sizeof(buf), "Reset time drift");
        return buf;
    }
    return "";
}

static void texts_item_cb(lv_event_t *e) {
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which >= SETTINGS_RELAY) { /* switched in place; the list stays */
        switch (which) {
        case SETTINGS_RELAY:
            /* Desktop's "Disable message relay (>)": it stops holding MSG
             * TO: messages for others too. */
            param_i_set(cfg.js8.relay(), !param_i_get(cfg.js8.relay()));
            msg_update_text_fmt(param_i_get(cfg.js8.relay())
                                    ? "Relay on: relays (>) are passed on and MSG TO: messages held, as desktop JS8Call"
                                    : "Relay off: relays (>) and MSG TO: messages for others are ignored");
            break;
        case SETTINGS_ST_KEEP:
            param_i_set(cfg.js8.st_keep(), (param_i_get(cfg.js8.st_keep()) + 1) % ST_KEEP_N);
            apply_station_keep();
            if (view_stations) rebuild_rows();
            msg_update_text_fmt("Stations stay listed %s after they were last heard",
                                st_keep_opts[param_i_get(cfg.js8.st_keep())].min ? st_keep_opts[param_i_get(cfg.js8.st_keep())].label
                                                                       : "until the radio is switched off, however long");
            break;
        case SETTINGS_MSG_KEEP:
            param_i_set(cfg.js8.msg_keep(), (param_i_get(cfg.js8.msg_keep()) + 1) % MSG_KEEP_N);
            if (!view_stations) rebuild_rows();
            if (param_i_get(cfg.js8.msg_keep()))
                msg_update_text_fmt("Messages leave the list %s after they arrived",
                                    msg_keep_opts[param_i_get(cfg.js8.msg_keep())].label);
            else msg_update_text_fmt("Messages stay in the list (the newest %d)", KEEP_ROWS);
            break;
        case SETTINGS_MILES:
            param_i_set(cfg.js8.miles(), !param_i_get(cfg.js8.miles()));
            if (view_stations) rebuild_rows();
            break;
        case SETTINGS_MARKS:
            param_i_set(cfg.js8.decode_marks(), !param_i_get(cfg.js8.decode_marks()));
            js8_rx_set_sync_marks(rx, param_i_get(cfg.js8.decode_marks()));
            if (!param_i_get(cfg.js8.decode_marks())) marks_reset();
            msg_update_text_fmt(param_i_get(cfg.js8.decode_marks())
                                    ? "Decode marks on: where the decoder is trying (cyan, white) and what it decoded (yellow)"
                                    : "Decode marks off");
            break;
        case SETTINGS_WF:
        {
            static const char *const what[WF_AVG_LEVELS] = {
                "every row as heard, as before",
                "rows lightly averaged",
                "rows averaged",
                "rows averaged most: least speckle and flicker",
            };
            int level = (param_i_get(cfg.js8.wf_avg()) + 1) % WF_AVG_LEVELS; /* Sharp > Light > Medium > Calm > Sharp */
            param_i_set(cfg.js8.wf_avg(), level);
            atomic_store(&wf_avg_level, level);
            msg_update_text_fmt("Waterfall: %s (%s)", wf_avg_name[level], what[level]);
        }
            break;
        case SETTINGS_TRESET:
            time_sync_reset();
            break;
        case SETTINGS_DECODE:
            decode_toggle();
            break;
        case SETTINGS_ULTRA:
            ultra_toggle();
            break;
        case SETTINGS_FREQ:
            /* Its own list, as page 6's Freq button opened it. */
            texts_close();
            freq_show();
            return;
        case SETTINGS_HCLEAR:
            /* Everything, every band, for good: a second press within 5 s. */
            if (!history_db) {
                msg_update_text_fmt("No station history: js8_history.db on the SD card couldn't be opened");
            } else if (hclear_armed_ms && now_mono_ms() - hclear_armed_ms < HCLEAR_MS) {
                hclear_armed_ms = 0;
                js8_history_clear(history_db);
                if (st_all_time) {
                    ever_load();
                    if (view_stations) rebuild_station_rows();
                }
                msg_update_text_fmt("Station history cleared: every station, INFO, STATUS and QSO, every band");
                add_info_row("Station history cleared");
            } else {
                hclear_armed_ms = now_mono_ms();
                msg_update_text_fmt("Clear the whole station history (every band)? Press again within 5 s; it can't be undone");
            }
            break;
        }
        lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), settings_label(which));
        return;
    }
    /* Straight into the keyboard. */
    popup_to_keyboard(&texts_list, which,
                      which == EDIT_INFO     ? info_text
                      : which == EDIT_STATUS ? status_text
                      : which == EDIT_GROUPS ? groups_text
                                             : operator_call);
}

static void texts_key_cb(lv_event_t *e) {
    popup_key(e, texts_close);
}

static lv_obj_t *settings_add(const char *label, int which) {
    lv_obj_t *b = list_add_item(texts_list, label);
    lv_obj_add_event_cb(b, texts_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)which);
    lv_obj_add_event_cb(b, texts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

/* Settings: INFO and STATUS (what AUTO sends for INFO? and STATUS?), Relay
 * on/off, and the groups we're in, like desktop's settings. */
static void texts_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (texts_list) { /* Settings... again closes it */
        texts_close();
        return;
    }
    if (popup_guard()) return;
    if (query_list || aprs_list || composing) return;
    lv_group_remove_obj(table);
    texts_list = lv_list_create(dialog.obj);
    lv_obj_set_size(texts_list, 560, 290);
    lv_obj_align(texts_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(texts_list, &sony_24, 0);
    lv_obj_set_style_bg_color(texts_list, lv_color_hex(0x202020), 0);
    lv_obj_t *title = lv_list_add_text(texts_list, "Settings");
    lv_obj_set_style_text_font(title, &sony_22, 0);

    /* Time Sync is the Time button now (page 4); its reset stays here. */
    lv_obj_t *first = settings_add(settings_label(SETTINGS_TRESET), SETTINGS_TRESET);
    settings_add(settings_label(SETTINGS_FREQ), SETTINGS_FREQ);

    char label[TEXT_MAX + 16];
    snprintf(label, sizeof(label), "INFO: %s", info_text[0] ? info_text : "(not set)");
    settings_add(label, EDIT_INFO);
    snprintf(label, sizeof(label), "STATUS: %s", status_text[0] ? status_text : "(not set)");
    settings_add(label, EDIT_STATUS);
    settings_add(relay_label(), SETTINGS_RELAY);
    snprintf(label, sizeof(label), "Groups: %s", groups_text[0] ? groups_text : "(none)");
    settings_add(label, EDIT_GROUPS);
    settings_add(settings_label(SETTINGS_ST_KEEP), SETTINGS_ST_KEEP);
    settings_add(settings_label(SETTINGS_MSG_KEEP), SETTINGS_MSG_KEEP);
    settings_add(settings_label(SETTINGS_MILES), SETTINGS_MILES);
    settings_add(settings_label(SETTINGS_DECODE), SETTINGS_DECODE);
    settings_add(settings_label(SETTINGS_ULTRA), SETTINGS_ULTRA);
    settings_add(settings_label(SETTINGS_MARKS), SETTINGS_MARKS);
    settings_add(settings_label(SETTINGS_WF), SETTINGS_WF);
    hclear_armed_ms = 0;
    hclear_btn      = settings_add(settings_label(SETTINGS_HCLEAR), SETTINGS_HCLEAR);
    snprintf(label, sizeof(label), "Operator: %s", operator_call[0] ? operator_call : "(the station call)");
    settings_add(label, EDIT_OPERATOR);

    lv_obj_t *close = list_add_item(texts_list, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_obj_add_event_cb(close, texts_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(close, texts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, close);
    lv_group_focus_obj(first);
    lv_group_set_editing(keyboard_group, false);
}

/* ---- APRS via @APRSIS -------------------------------------------------- */

/* JS8Call stations with "spot to APRS" on forward these to APRS-IS; formats
 * as desktop JS8Call and KF7MIX's JS8Spotter send them. Raw packets are
 * "@APRSIS CMD :<addressee padded to 9>:<text>"; APRS allows 67 characters
 * of text. The gateway sends you as your plain callsign (no SSID). */
#define APRS_CMD      "@APRSIS CMD :"
#define APRS_CMD_RAW  "@APRSIS CMD " /* any APRS packet body, e.g. a position */
#define APRS_TEXT_MAX 67

/* The Echo test is gone (VE7NHW, 2026-10-09): the ECHO service answers
 * (findu.com shows it), but relay stations don't bring its answer back
 * over JS8. The @ badge comes from any relayed answer (SMS, MPAD...). */
typedef enum {
    APRS_GRID,
    APRS_GPS,
    APRS_POTA,
    APRS_SOTA,
    APRS_SMS,
    APRS_EMAIL,
    APRS_WL_START,
    APRS_WL_TEXT,
    APRS_WL_SEND,
    APRS_MORE,
    APRS_COUNT
} aprs_item_t;

static const char *const aprs_labels[APRS_COUNT] = {
    "Spot my grid",   "Spot GPS position", "POTA spot",       "SOTA spot", "SMS text", "Email",
    "Winlink: start", "Winlink: text",     "Winlink: send",   "More services >",
};

/* More services >: APRS information services, each message filled in
 * (docs/feature-ideas.md: the ones that answered on 2026-09-28). A "%s"
 * in the text is your 6-character grid. MPAD's commands other than
 * weather use your last APRS position (MPAD docs), so Spot my grid
 * first. Answers come back over JS8 from an inbound relay station and
 * land in the Inbox from the service. */
#define APRS_POS_HINT "uses your last APRS position (Spot my grid first)"
static const struct {
    const char *label, *to, *text, *hint;
} aprs_svcs[] = {
    {"Weather today", "MPAD", "grid %s today", "MPAD weather for your grid, worldwide"},
    {"Weather tomorrow", "MPAD", "grid %s tomorrow", "MPAD weather for your grid, worldwide"},
    {"US forecast", "WXBOT", "grid %s brief", "WXBOT: US National Weather Service only"},
    {"Nearest wx station", "WXNOW", "N 1", "WXNOW: the nearest weather station; " APRS_POS_HINT},
    {"Sunrise / sunset", "MPAD", "riseset", "MPAD " APRS_POS_HINT},
    {"Nearest repeater", "MPAD", "repeater 2m", "MPAD " APRS_POS_HINT "; 2m, 70cm, c4fm, dmr..."},
    {"Next ISS pass", "MPAD", "satpass iss", "MPAD " APRS_POS_HINT},
    {"Nearest hospital", "MPAD", "osm hospital", "MPAD " APRS_POS_HINT},
    {"Nearest fuel", "MPAD", "osm fuel", "MPAD " APRS_POS_HINT},
    {"Drinking water", "MPAD", "osm drinking_water", "MPAD " APRS_POS_HINT},
    {"Where am I", "MPAD", "whereami", "MPAD " APRS_POS_HINT},
    {"Email my position", "MPAD", "posmsg ", "MPAD emails a map link: type the address; " APRS_POS_HINT},
    {"Airport weather", "MPAD", "metar", "MPAD: METAR of the nearest airport; " APRS_POS_HINT},
    {"Callsign lookup", "WHO-IS", "", "WHO-IS: type a callsign"},
    {"Magic 8-ball", "MPAD", "magic8ball", "MPAD's magic 8-ball"},
    {"Joke", "JOKE", "joke", "A joke from JOKE"},
};
#define APRS_SVCS (int)(sizeof(aprs_svcs) / sizeof(aprs_svcs[0]))

static unsigned aprs_msg_id;

/* APRS messages we sent with an id ("{04}"), so the gateway's receipt
 * ("ACK04}" / "REJ04}", relayed back over JS8) marks the right one instead
 * of landing in the Inbox as a message (bug hunt S7). Kept while the radio
 * is on (your choice): receipts come within minutes, and the ids restart
 * at power-on. */
#define APRS_SENT 16
typedef struct {
    char    id[6];     /* "04" */
    char    to[10];    /* the addressee: SMS, EMAIL-2, WLNK-1 ... */
    char    dest[48];  /* the message's first word: "@6045551234", an address */
    int64_t ms;
    bool    answered;  /* its receipt came */
} aprs_sent_t;
static aprs_sent_t aprs_sent[APRS_SENT];
static int         aprs_sent_next;
static aprs_sent_t aprs_pending; /* prepared, not yet queued; id "" = none */

/* "{04}" or "{AB12" at the end of an APRS message: its id (1-5 letters or
 * digits, "}" optional), or NULL. */
static const char *aprs_id_at_end(const char *body, char *id, size_t len) {
    const char *brace = strrchr(body, '{');
    if (!brace) return NULL;
    size_t n = 0;
    while (n < 5 && isalnum((unsigned char)brace[1 + n])) n++;
    const char *end = brace + 1 + n;
    if (!n || (*end == '}' ? end[1] : *end)) return NULL;
    snprintf(id, len, "%.*s", (int)n, brace + 1);
    return brace;
}

static void aprs_sent_commit(void) {
    if (!aprs_pending.id[0]) return;
    aprs_pending.ms          = now_wall_ms();
    aprs_sent[aprs_sent_next] = aprs_pending;
    aprs_sent_next            = (aprs_sent_next + 1) % APRS_SENT;
    aprs_pending.id[0]        = '\0';
}

static const char *aprs_kind(const char *to) {
    return !strcmp(to, "SMS") ? "SMS" : !strcmp(to, "EMAIL-2") ? "Email" : !strcmp(to, "WLNK-1") ? "Winlink" : to;
}

/* A gateway's receipt: the message it answers, marked. */
static void aprs_receipt(const js8_stored_t *k) {
    aprs_sent_t *s = NULL;
    for (int i = 1; i <= APRS_SENT && !s; i++) { /* newest first */
        aprs_sent_t *c = &aprs_sent[(aprs_sent_next - i + APRS_SENT) % APRS_SENT];
        if (c->id[0] && !strcasecmp(c->id, k->text)) s = c;
    }
    const char *what = k->rejected ? "rejected by the gateway" : "delivered";
    if (!s) { /* sent before a restart, or by another radio */
        msg_update_text_fmt("APRS: message {%s} %s (receipt from %s)", k->text, what, k->from);
        add_info_row("APRS receipt from %s: message {%s} %s", k->from, k->text, what);
        return;
    }
    if (s->answered) return; /* the same receipt again */
    s->answered = true;
    msg_update_text_fmt("%s {%s} to %s %s", aprs_kind(s->to), s->id, s->dest, what);
    add_info_row("%s {%s} to %s %s (receipt from %s)", aprs_kind(s->to), s->id, s->dest, what, k->from);
}

/* Before sending anything typed: APRS CMDs get checked, SMS/email/Winlink a
 * message ID like JS8Spotter's "{01}", and POTA/SOTA refs are remembered.
 * Anything else passes through unchanged. */
static bool aprs_prepare(const char *in, char *out, size_t size) {
    snprintf(out, size, "%s", in);
    aprs_pending.id[0] = '\0';
    size_t pre = strlen(APRS_CMD);
    if (strncmp(in, APRS_CMD, pre) != 0) return true;
    if (strlen(in) < pre + 10 || in[pre + 9] != ':') {
        msg_update_text_fmt("APRS: the addressee is 9 characters, then ':'");
        return false;
    }
    char to[10];
    memcpy(to, in + pre, 9);
    to[9] = '\0';
    for (int i = 8; i >= 0 && to[i] == ' '; i--) to[i] = '\0';
    const char *body = in + pre + 10;
    while (*body == ' ') body++;
    if (!*body) {
        msg_update_text_fmt("APRS: nothing to send to %s", to);
        return false;
    }

    /* Not Winlink: WLNK-1 answers each message with its own reply, so an
     * id (which asks for an ACK too) only brought a second acknowledgement
     * (VE7NHW on the air, beta 4). Plain APRS messages may go without. */
    bool want_id = !strcmp(to, "SMS") || !strcmp(to, "EMAIL-2");
    if (want_id && !strchr(body, '{')) {
        if (!aprs_msg_id) aprs_msg_id = (unsigned)(time(NULL) % 90);
        aprs_msg_id = aprs_msg_id % 99 + 1;
        snprintf(out, size, "%s{%02u}", in, aprs_msg_id);
    }
    /* 67 characters of text; the id "{04}" comes on top (APRS spec, bug
     * hunt S1: counting it cut SMS/email/Winlink to 63). */
    const char *text_out = out + pre + 10;
    size_t      text_len = strlen(text_out);
    char        id[6];
    const char *id_at    = aprs_id_at_end(text_out, id, sizeof(id));
    if (id_at) text_len = (size_t)(id_at - text_out);
    if (id_at && want_id) { /* its receipt will come back: remember it */
        snprintf(aprs_pending.id, sizeof(aprs_pending.id), "%s", id);
        snprintf(aprs_pending.to, sizeof(aprs_pending.to), "%s", to);
        snprintf(aprs_pending.dest, sizeof(aprs_pending.dest), "%.*s", (int)strcspn(body, " "), body);
        aprs_pending.answered = false;
    }
    if (text_len > APRS_TEXT_MAX) {
        msg_update_text_fmt("APRS allows %d characters of text (the {id} is extra), this is %u: shorten it",
                            APRS_TEXT_MAX, (unsigned)text_len);
        return false;
    }

    /* "! POTA PARK MHZ MODE ..." / "SUMMIT FREQ MODE ..." / old POTAGW "CALL PARK KHZ MODE ..." */
    char w1[24] = "", w2[24] = "", w3[24] = "";
    sscanf(body, "%23s %23s %23s", w1, w2, w3);
    if (!strcmp(to, "APSPOT") && !strcmp(w1, "!") && w3[0] && (!strcmp(w2, "POTA") || !strcmp(w2, "SOTA"))) {
        if (w2[0] == 'P') snprintf(last_pota, sizeof(last_pota), "%s", w3);
        else snprintf(last_sota, sizeof(last_sota), "%s", w3);
        save_texts();
    } else if (!strcmp(to, "POTAGW") && w2[0]) {
        snprintf(last_pota, sizeof(last_pota), "%s", w2);
        save_texts();
    } else if ((!strcmp(to, "APRS2SOTA") || !strcmp(to, "SOTA")) && w1[0]) {
        snprintf(last_sota, sizeof(last_sota), "%s", w1);
        save_texts();
    }
    return true;
}

/* The keyboard with `head` + `tail`, the cursor between them. */
static void aprs_compose(const char *head, const char *tail) {
    char text[TX_TEXT_MAX + 1];
    snprintf(text, sizeof(text), "%s%s", head, tail);
    compose_open(text);
    if (composing) lv_textarea_set_cursor_pos(textarea_window_text(), (int32_t)strlen(head));
}

/* Your QTH grid (6 characters) and its centre. False, with the reason
 * shown, if it isn't set. Any out-pointer may be NULL. */
static bool aprs_grid(char grid[8], double *lat, double *lon) {
    char g[8];
    snprintf(g, sizeof(g), "%.6s", my_grid());
    if (strlen(g) < 4) {
        msg_update_text_fmt("Set your grid first: APP > QTH");
        return false;
    }
    if (grid) memcpy(grid, g, sizeof(g));
    if (lat && lon) qth_str_to_pos(g, lat, lon);
    return true;
}

/* GPS: R1CBU 1.0 announces each gpsd report (MSG_GPS, on the GUI thread);
 * a 2D/3D fix and when it came are kept while JS8 is open. */
static void gps_msg_cb(void *s, lv_msg_t *m) {
    (void)s;
    (void)m;
    struct gps_data_t d;
    gps_get_snapshot(&d);
    gps_seen = time(NULL);
    if (d.fix.mode < MODE_2D || !isfinite(d.fix.latitude) || !isfinite(d.fix.longitude)) return;
    gps_lat  = d.fix.latitude;
    gps_lon  = d.fix.longitude;
    gps_when = time(NULL);
}

/* The latest fix and its age in seconds; false if none since JS8 opened. */
static bool gps_last_fix(double *lat, double *lon, int *age_s) {
    if (!gps_when) return false;
    *lat   = gps_lat;
    *lon   = gps_lon;
    *age_s = (int)(time(NULL) - gps_when);
    return true;
}

/* A current GPS fix, and its 10-character grid (about 20 x 35 m). False,
 * with the reason shown, if there is none. */
static bool aprs_gps(char grid[12], double *lat_out, double *lon_out) {
    double lat, lon;
    int    age;
    if (!gps_last_fix(&lat, &lon, &age)) {
        msg_update_text_fmt("No GPS fix: plug in a GPS and wait for a fix (APP > GPS shows it)");
        return false;
    }
    if (age > 120) {
        msg_update_text_fmt("No current GPS fix (last one %d min ago)", age / 60);
        return false;
    }
    char g[12];
    if (!js8_latlon_to_grid(lat, lon, 10, g, sizeof(g))) {
        msg_update_text_fmt("GPS position out of range");
        return false;
    }
    if (grid) memcpy(grid, g, sizeof(g));
    if (lat_out) *lat_out = lat;
    if (lon_out) *lon_out = lon;
    return true;
}

/* An APRS position report: "=4916.9 N/12307.2 WG MESSAGE". To about
 * 185 m: minutes to one decimal, the last digit a space (APRS position
 * ambiguity), which JS8 sends in far fewer bits than digits; the space
 * before the message lets JS8's word compression take it whole (6 frames
 * down to 5 for "MADE IT TO CAMP"). "/G" is the grid-square symbol, the
 * one JS8Call's own grid spots use. */
static void aprs_position(double lat, double lon, const char *comment, char *out, size_t size) {
    double alat = fabs(lat), alon = fabs(lon);
    int    lat_d = (int)alat, lon_d = (int)alon;
    double lat_m = (alat - lat_d) * 60.0, lon_m = (alon - lon_d) * 60.0;
    if (lat_m >= 59.95) lat_d++, lat_m = 0; /* don't print 60.0 */
    if (lon_m >= 59.95) lon_d++, lon_m = 0;
    snprintf(out, size, "=%02d%04.1f %c/%03d%04.1f %cG %s", lat_d, lat_m, lat < 0 ? 'S' : 'N', lon_d, lon_m,
             lon < 0 ? 'W' : 'E', comment);
}

/* What Spot my grid / GPS position sends. No message: "@APRSIS GRID
 * <grid>" as desktop JS8Call sends it (the gateway adds frequency and
 * SNR). With one: the GRID command can't carry it, so a position report
 * with the message as its comment goes through CMD (the gateway passes it
 * on as it is). False, with the reason shown, without a position. */
static bool beacon_text(bool gps, const char *message, char *out, size_t size, char grid[12]) {
    double lat, lon;
    if (gps ? !aprs_gps(grid, &lat, &lon) : !aprs_grid(grid, &lat, &lon)) return false;
    while (*message == ' ') message++;
    if (!*message) {
        snprintf(out, size, "@APRSIS GRID %s", grid);
        return true;
    }
    char pos[80];
    aprs_position(lat, lon, message, pos, sizeof(pos));
    snprintf(out, size, APRS_CMD_RAW "%s", pos);
    return true;
}

static void aprs_beacon(bool gps, const char *message) {
    char text[112], grid[12];
    if (!beacon_text(gps, message, text, sizeof(text), grid)) return;
    while (*message == ' ') message++;
    if (!tx_queue(text)) return;
    if (!*message)
        add_info_row("APRS: spotting %s at %s%s", my_call(), grid, gps ? " (GPS)" : "");
    else
        add_info_row("APRS: position %s (%s%s) with \"%s\"", my_call(), grid, gps ? ", GPS" : "", message);
}

/* While typing a beacon message: how long it will be on the air. */
static void beacon_changed_cb(lv_event_t *e) {
    (void)e;
    char text[112], grid[12];
    if (!beacon_text(edit_target == EDIT_BEACON_GPS, textarea_window_get(), text, sizeof(text), grid)) return;
    js8_tx_preview_t pv;
    js8_tx_preview(my_call(), my_grid(), text, cur_speed(), &pv);
    if (!pv.ok) return;
    const char *msg = textarea_window_get();
    while (*msg == ' ') msg++;
    if (*msg) msg_update_text_fmt("Position + message: %d frames, %.0f s", pv.frames, pv.seconds);
    else msg_update_text_fmt("Plain position: %d frames, %.0f s - type a message, or Enter", pv.frames, pv.seconds);
}

static void aprs_close(void) {
    if (!aprs_list) return;
    lv_obj_del_async(aprs_list); /* often called from one of its buttons */
    aprs_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void aprs_item_cb(lv_event_t *e) {
    aprs_item_t item = (aprs_item_t)(intptr_t)lv_event_get_user_data(e);
    /* Straight into the keyboard for most items: take the buttons out of
     * the group first, as Texts... does, or the keyboard opens unfocused. */
    popup_leave(&aprs_list);

    switch (item) {
    case APRS_MORE:
        aprs_open(true);
        return;
    case APRS_GRID:
    case APRS_GPS:
        /* A message to go with it, or just Enter. GPS: a fix first, so
         * nothing is typed for nothing. */
        if (item == APRS_GRID ? !aprs_grid(NULL, NULL, NULL) : !aprs_gps(NULL, NULL, NULL)) break;
        if (popup_to_keyboard(NULL, item == APRS_GRID ? EDIT_BEACON_GRID : EDIT_BEACON_GPS, NULL))
            beacon_changed_cb(NULL); /* the plain beacon's length, until you type */
        break;
    case APRS_POTA:
    case APRS_SOTA:
        spot_show(item == APRS_SOTA, (item == APRS_SOTA ? last_sota : last_pota)[0] ? SP_SEND : SP_REF);
        return;
    case APRS_SMS:
        aprs_compose(APRS_CMD "SMS      :@", "");
        msg_update_text_fmt("SMS (NA7Q gateway): 10-digit number, space, message");
        break;
    case APRS_EMAIL:
        aprs_compose(APRS_CMD "EMAIL-2  :", "");
        msg_update_text_fmt("Email: address, space, message");
        break;
    case APRS_WL_START:
        aprs_compose(APRS_CMD "WLNK-1   :SP ", "");
        msg_update_text_fmt("Winlink 1/3: address (or call), space, subject. Spot your grid first");
        break;
    case APRS_WL_TEXT:
        aprs_compose(APRS_CMD "WLNK-1   :", "");
        msg_update_text_fmt("Winlink 2/3: one line of the message; repeat for more lines");
        break;
    case APRS_WL_SEND:
        if (tx_queue(APRS_CMD "WLNK-1   :/EX")) msg_update_text_fmt("Winlink 3/3: sending /EX");
        break;
    default:
        break;
    }
    aprs_item_done();
}

/* After an item: the keyboard if it opened, else the knob back to the table. */
static void aprs_item_done(void) {
    if (composing) lv_group_set_editing(keyboard_group, true);
    else if (table) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* A More services item: its message in the keyboard, Enter sends it. */
static void aprs_svc_cb(lv_event_t *e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    popup_leave(&aprs_list);
    char grid[8] = "", text[96], head[TX_TEXT_MAX + 1];
    if (strstr(aprs_svcs[i].text, "%s") && !aprs_grid(grid, NULL, NULL)) {
        aprs_item_done();
        return;
    }
    snprintf(text, sizeof(text), aprs_svcs[i].text, grid);
    snprintf(head, sizeof(head), APRS_CMD "%-9s:%s", aprs_svcs[i].to, text);
    aprs_compose(head, "");
    msg_update_text_fmt("%s; the answer comes to the Inbox", aprs_svcs[i].hint);
    aprs_item_done();
}

static void aprs_back_cb(lv_event_t *e) {
    (void)e;
    popup_leave(&aprs_list);
    aprs_open(false);
}

static void aprs_close_cb(lv_event_t *e) {
    (void)e;
    aprs_close();
}

static void aprs_key_cb(lv_event_t *e) {
    popup_key(e, aprs_close);
}

static void aprs_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (aprs_list) { /* APRS > again closes it */
        aprs_close();
        return;
    }
    if (popup_guard()) return;
    if (query_list || texts_list || composing) return;
    if (!my_call()[0]) {
        msg_update_text_fmt("Set your callsign first: APP > Callsign");
        return;
    }
    aprs_open(false);
}

/* The APRS list, or (`more`) the More services list in its place. */
static void aprs_open(bool more) {
    lv_group_remove_obj(table);
    aprs_list = lv_list_create(dialog.obj);
    lv_obj_set_size(aprs_list, 300, WF_HEIGHT - 10);
    lv_obj_align(aprs_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(aprs_list, &sony_24, 0);
    lv_obj_set_style_bg_color(aprs_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(aprs_list, lv_color_white(), 0);
    lv_obj_t *t = lv_list_add_text(aprs_list, more ? "More APRS services" : "APRS via @APRSIS");
    lv_obj_set_style_text_font(t, &sony_22, 0);

    lv_obj_t *first = NULL;
    for (int i = 0; i < (more ? APRS_SVCS : APRS_COUNT); i++) {
        lv_obj_t *b = list_add_item(aprs_list, more ? aprs_svcs[i].label : aprs_labels[i]);
        lv_obj_add_event_cb(b, more ? aprs_svc_cb : aprs_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, aprs_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        if (!first) first = b;
    }
    if (more) {
        lv_obj_t *back = list_add_item(aprs_list, "< Back");
        lv_obj_set_style_text_color(back, lv_color_hex(0xffc040), 0);
        lv_obj_add_event_cb(back, aprs_back_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_add_event_cb(back, aprs_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, back);
    }
    lv_obj_t *close = list_add_item(aprs_list, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_obj_add_event_cb(close, aprs_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(close, aprs_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, close);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

/* ---- A station's History page ------------------------------------------
 *
 * An MFK press on a station in the Stations view or on the map: what the
 * station history knows of them. Their grid and distance, when you first
 * and last exchanged messages on this band and how they heard you, their
 * latest INFO and STATUS with how old they are, then the QSOs, newest
 * first (heartbeat-only exchanges left out); a QSO opens to its messages,
 * time-stamped. */

#define HPAGE_QSOS  64
#define HPAGE_LINES 120

static char    hpage_call[JS8_RX_CALL_LEN];
static int64_t hpage_ids[HPAGE_QSOS];
static int64_t hpage_back_qso; /* focused again when back from reading it */

/* "Oct 4 19:57Z"; another year: "Oct 4 2025". */
static void format_when(int64_t ms, char *buf, size_t size) {
    static const char *const mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    time_t    t = (time_t)(ms / 1000), now = (time_t)(now_wall_ms() / 1000);
    struct tm tm, tn;
    gmtime_r(&t, &tm);
    gmtime_r(&now, &tn);
    if (tm.tm_year == tn.tm_year) snprintf(buf, size, "%s %d %02d:%02dZ", mon[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min);
    else snprintf(buf, size, "%s %d %d", mon[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);
}

static void hpage_close(void) {
    if (!hpage_list) return;
    lv_obj_del_async(hpage_list);
    hpage_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void hpage_close_cb(lv_event_t *e) {
    (void)e;
    hpage_close();
}

static void hpage_key_cb(lv_event_t *e) {
    popup_key(e, hpage_close);
}

static void hpage_qso_cb(lv_event_t *e) {
    int i          = (int)(intptr_t)lv_event_get_user_data(e);
    hpage_back_qso = hpage_ids[i];
    char call[JS8_RX_CALL_LEN];
    snprintf(call, sizeof(call), "%s", hpage_call);
    hpage_close();
    hpage_open(call, hpage_back_qso);
}

static void hpage_back_cb(lv_event_t *e) {
    (void)e;
    char call[JS8_RX_CALL_LEN];
    snprintf(call, sizeof(call), "%s", hpage_call);
    hpage_close();
    hpage_open(call, 0);
}

/* A line of text, wrapped, not a button. */
static lv_obj_t *hpage_text(const char *text, uint32_t color) {
    lv_obj_t *t = lv_list_add_text(hpage_list, text);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(t, &sony_22, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_ver(t, 3, 0);
    return t;
}

static lv_obj_t *hpage_item(const char *text, lv_event_cb_t cb, void *data, bool wrap) {
    lv_obj_t *b = list_add_item(hpage_list, text);
    lv_obj_set_style_text_font(b, &sony_22, 0);
    if (wrap) lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_WRAP);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, data);
    lv_obj_add_event_cb(b, hpage_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    return b;
}

/* `qso_id` 0: the page; else that QSO's messages. */
static void hpage_open(const char *call, int64_t qso_id) {
    if (!history_db) {
        msg_update_text_fmt("No station history: js8_history.db on the SD card couldn't be opened");
        return;
    }
    snprintf(hpage_call, sizeof(hpage_call), "%s", call);
    lv_group_remove_obj(table);
    hpage_list = lv_list_create(dialog.obj);
    lv_obj_set_size(hpage_list, 740, WF_HEIGHT - 10);
    lv_obj_align(hpage_list, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_style_text_font(hpage_list, &sony_22, 0);
    lv_obj_set_style_bg_color(hpage_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(hpage_list, lv_color_white(), 0);

    char      line[JS8_RX_TEXT_LEN + 64], when[24], age[8];
    lv_obj_t *first = NULL, *focus = NULL;
    int64_t   now   = now_wall_ms();
    if (qso_id) {
        /* Reading one QSO, oldest message first. */
        static js8_hist_qso_t  q[HPAGE_QSOS];
        static js8_hist_line_t l[HPAGE_LINES];
        int                    nq = js8_history_qsos(history_db, call, q, HPAGE_QSOS);
        const char            *band = "";
        for (int i = 0; i < nq; i++)
            if (q[i].id == qso_id) {
                format_when(q[i].start_ms, when, sizeof(when));
                band = q[i].band;
            }
        snprintf(line, sizeof(line), "%s  %s  %s", call, when, band_name(band));
        lv_obj_t *t = lv_list_add_text(hpage_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        int n = js8_history_lines(history_db, qso_id, l, HPAGE_LINES);
        for (int i = 0; i < n; i++) {
            time_t    ts = (time_t)(l[i].ms / 1000);
            struct tm tm;
            gmtime_r(&ts, &tm);
            snprintf(line, sizeof(line), "%02d:%02d  %s", tm.tm_hour, tm.tm_min, l[i].text);
            lv_obj_t *b = hpage_item(line, NULL, NULL, true);
            /* Yours in the red of your own rows, heartbeat ACKs grey. */
            if (l[i].tx) lv_obj_set_style_text_color(b, lv_color_hex(0xff9a9a), 0);
            if (l[i].heartbeat) lv_obj_set_style_text_color(b, lv_color_hex(0x9a9a9a), 0);
            if (!first) first = b;
        }
        lv_obj_t *back = hpage_item("< Back", hpage_back_cb, NULL, false);
        lv_obj_set_style_text_color(back, lv_color_hex(0xffc040), 0);
        if (!first) first = back;
    } else {
        lv_obj_t *t = lv_list_add_text(hpage_list, call);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        const char        *band = dial_band();
        js8_hist_contact_t c;
        bool               have = js8_history_contact(history_db, call, band, &c);
        js8_station_t      st;
        memset(&st, 0, sizeof(st));
        for (int i = 0; i < st_count; i++)
            if (strcasecmp(st_rows[i].call, call) == 0) st = st_rows[i];
        if (!st.grid[0] && have) snprintf(st.grid, sizeof(st.grid), "%s", c.grid);
        station_fields_t f;
        station_fields(&st, now, &f);
        if (st.grid[0]) {
            /* "az 58" as the map's label: this font has no degree sign. */
            snprintf(line, sizeof(line), "%s   %s   az %.*s", st.grid, f.dist, (int)strcspn(f.az, "\xC2"), f.az);
            hpage_text(line, 0xffffff);
        }
        if (have) {
            char first_s[24], last_s[24];
            format_when(c.first_ms, first_s, sizeof(first_s));
            format_when(c.last_ms, last_s, sizeof(last_s));
            int len = snprintf(line, sizeof(line), "%s: first %s, last %s", band_name(band), first_s, last_s);
            if (c.heard_us_ms && c.has_reported_snr)
                snprintf(line + len, sizeof(line) - len, "; heard you %+03d", c.reported_snr);
            hpage_text(line, 0xc8c8c8);
        } else {
            snprintf(line, sizeof(line), "No messages exchanged on %s yet", band_name(band));
            hpage_text(line, 0xc8c8c8);
        }
        for (int kind = 0; kind < 2; kind++) {
            js8_hist_info_t info;
            const char     *what = kind ? "STATUS" : "INFO";
            if (js8_history_info(history_db, call, kind, &info)) {
                format_age(now - info.ms, age, sizeof(age));
                snprintf(line, sizeof(line), strcmp(age, "now") ? "%s (%s ago): %s" : "%s (%s): %s", what,
                         strcmp(age, "now") ? age : "just now", info.text);
                hpage_text(line, 0xffd24a);
            } else {
                snprintf(line, sizeof(line), "%s: none heard yet", what);
                hpage_text(line, 0x9a9a9a);
            }
        }
        static js8_hist_qso_t q[HPAGE_QSOS];
        int                   nq = js8_history_qsos(history_db, call, q, HPAGE_QSOS);
        snprintf(line, sizeof(line), nq ? "QSOs, newest first:" : "No QSOs yet (heartbeats aren't listed)");
        hpage_text(line, 0xc8c8c8);
        for (int i = 0; i < nq; i++) {
            hpage_ids[i] = q[i].id;
            format_when(q[i].start_ms, when, sizeof(when));
            snprintf(line, sizeof(line), "%s   %s   %d message%s%s", when, band_name(q[i].band), q[i].lines,
                     q[i].lines == 1 ? "" : "s", q[i].logged ? "   logged" : "");
            lv_obj_t *b = hpage_item(line, hpage_qso_cb, (void *)(intptr_t)i, false);
            if (!first) first = b;
            if (q[i].id == hpage_back_qso) focus = b;
        }
    }
    lv_obj_t *close = hpage_item("Close", hpage_close_cb, NULL, false);
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(focus ? focus : first ? first : close);
    if (!qso_id) lv_obj_scroll_to_y(hpage_list, 0, LV_ANIM_OFF); /* the top lines in view */
}

/* ---- POTA / SOTA spot form (docs/SPOTS_PLAN.md) ------------------------ */

/* POTA goes to APSPOT, which posts to pota.app (needs a pota.app account);
 * SOTA to APRS2SOTA (needs registering with sotaspots.co.uk). Both take
 * MHz. The frequency is the JS8 dial, or one you typed (your SSB run),
 * each remembered with the mode and comment. Gateway replies come back
 * over APRS only: the radio never sees them. */

static const char *const spot_modes_pota[] = {"DATA", "SSB", "CW", "FM", "AM", "FT8"}; /* APSPOT's */
static const char *const spot_modes_sota[] = {"DATA", "SSB", "CW", "FM", "AM", "DV"};  /* APRS2SOTA's */
#define SPOT_MODES 6

static lv_obj_t *spot_items[SP_COUNT];
static lv_obj_t *spot_preview;

static const char *const *spot_modes(void) {
    return spot_sota ? spot_modes_sota : spot_modes_pota;
}

/* The mode to send: the remembered one if this gateway takes it. */
static const char *spot_mode_now(void) {
    for (int i = 0; i < SPOT_MODES; i++)
        if (!strcmp(spot_modes()[i], spot_mode)) return spot_modes()[i];
    return "DATA";
}

static int32_t spot_hz(void) {
    return spot_use_typed && spot_typed_hz ? spot_typed_hz : cparam_i_get(cfg.cur.fg_freq());
}

/* MHz with at least 3 decimals, more only when needed: "7.078", "7.1855". */
static void format_mhz(int32_t hz, char *buf, size_t size) {
    snprintf(buf, size, "%d.%06d", (int)(hz / 1000000), (int)(hz % 1000000));
    char *end = buf + strlen(buf) - 1;
    while (*end == '0' && end[-3] != '.') *end-- = '\0';
}

/* No comment typed: "JS8" while spotting the JS8 dial. */
static const char *spot_comment(void) {
    if (spot_note[0]) return spot_note;
    return spot_use_typed && spot_typed_hz ? "" : "JS8";
}

/* The APRS text after "@APRSIS CMD :<addressee>:". */
static void spot_body(char *out, size_t size) {
    char mhz[16];
    format_mhz(spot_hz(), mhz, sizeof(mhz));
    const char *note = spot_comment();
    if (spot_sota)
        snprintf(out, size, "%s %s %s %s%s%s", last_sota[0] ? last_sota : "?", mhz, spot_mode_now(), my_call(),
                 note[0] ? " " : "", note);
    else
        snprintf(out, size, "! POTA %s %s %s%s%s", last_pota[0] ? last_pota : "?", mhz, spot_mode_now(), note[0] ? " " : "",
                 note);
}

static bool spot_freq_empty(const char *text) {
    while (*text == ' ') text++;
    return !*text;
}

/* kHz ("7185.5"), or MHz below 1000 ("144.2"); 1.8 MHz to 1.3 GHz. False,
 * with the reason shown, keeps the keyboard open to fix it. */
static bool spot_parse_freq(const char *text, int32_t *out) {
    char  *end;
    double v = strtod(text, &end);
    if (end == text || v <= 0) {
        msg_update_text_fmt("Type the frequency in kHz, e.g. 7185");
        return false;
    }
    double hz = v < 1000 ? v * 1e6 : v * 1e3;
    if (hz < 1800000 || hz > 1300000000) {
        msg_update_text_fmt("Out of range: 1800 kHz - 1300 MHz");
        return false;
    }
    if (out) *out = (int32_t)(hz + 0.5);
    return true;
}

static void spot_close(void) {
    if (!spot_list) return;
    lv_obj_del_async(spot_list); /* often called from one of its buttons */
    spot_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void spot_label(spot_item_t item, char *line, size_t size) {
    char mhz[16];
    switch (item) {
    case SP_SEND:
        snprintf(line, size, "Send spot");
        break;
    case SP_REF:
        if (spot_sota) snprintf(line, size, "Summit: %s", last_sota[0] ? last_sota : "(type it)");
        else snprintf(line, size, "Park: %s", last_pota[0] ? last_pota : "(type it)");
        break;
    case SP_FREQ:
        format_mhz(spot_hz(), mhz, sizeof(mhz));
        snprintf(line, size, "Frequency: %s MHz (%s)", mhz, spot_use_typed && spot_typed_hz ? "typed" : "JS8 dial");
        break;
    case SP_MODE:
        snprintf(line, size, "Mode: %s", spot_mode_now());
        break;
    case SP_NOTE:
        if (spot_note[0]) snprintf(line, size, "Comment: %s", spot_note);
        else if (spot_comment()[0]) snprintf(line, size, "Comment: %s (auto)", spot_comment());
        else snprintf(line, size, "Comment: (none)");
        break;
    case SP_CLOSE:
        snprintf(line, size, "Close");
        break;
    default:
        line[0] = '\0';
    }
}

/* Labels and the message preview, in place. */
static void spot_refresh(void) {
    char line[96];
    for (int i = 0; i < SP_COUNT; i++) {
        if (!spot_items[i]) continue;
        spot_label((spot_item_t)i, line, sizeof(line));
        lv_label_set_text(lv_obj_get_child(spot_items[i], 0), line);
    }
    spot_body(line, sizeof(line));
    lv_label_set_text_fmt(spot_preview, "%s: %s", spot_sota ? "APRS2SOTA" : "APSPOT", line);
}

static void spot_send(void) {
    if (!(spot_sota ? last_sota : last_pota)[0]) {
        msg_update_text_fmt("Type the %s first", spot_sota ? "summit" : "park");
        return;
    }
    char body[96], text[TX_TEXT_MAX + 8], out[TX_TEXT_MAX + 8];
    spot_body(body, sizeof(body));
    snprintf(text, sizeof(text), APRS_CMD "%-9s:%s", spot_sota ? "APRS2SOTA" : "APSPOT", body);
    if (!aprs_prepare(text, out, sizeof(out))) return; /* too long: says so */
    if (!tx_queue(out)) return;
    add_info_row("%s spot: %s. A JS8Call APRS gateway must hear it; the reply goes to APRS, not here",
                 spot_sota ? "SOTA" : "POTA", body);
    spot_close();
}

static void spot_item_cb(lv_event_t *e) {
    spot_item_t item = (spot_item_t)(intptr_t)lv_event_get_user_data(e);
    int         edit = 0;
    switch (item) {
    case SP_SEND:
        spot_send();
        return;
    case SP_CLOSE:
        spot_close();
        return;
    case SP_MODE: { /* next mode, in place */
        const char *cur = spot_mode_now();
        int         i   = 0;
        while (i < SPOT_MODES && strcmp(spot_modes()[i], cur) != 0) i++;
        snprintf(spot_mode, sizeof(spot_mode), "%s", spot_modes()[(i + 1) % SPOT_MODES]);
        save_texts();
        spot_refresh();
        return;
    }
    case SP_FREQ: /* the keyboard, your last one filled in; empty = the JS8 dial */
        edit = EDIT_SPOT_FREQ;
        break;
    case SP_REF:
        edit = EDIT_SPOT_REF;
        break;
    case SP_NOTE:
        edit = EDIT_SPOT_NOTE;
        break;
    default:
        return;
    }
    /* A field: into the keyboard, then back here (see log_item_cb). */
    char khz[16] = ""; /* the last one typed, even while spotting the dial */
    if (spot_typed_hz) format_khz(spot_typed_hz, khz, sizeof(khz));
    popup_to_keyboard(&spot_list, edit,
                      edit == EDIT_SPOT_REF ? (spot_sota ? last_sota : last_pota) : edit == EDIT_SPOT_FREQ ? khz : spot_note);
    if (edit == EDIT_SPOT_REF)
        msg_update_text_fmt(spot_sota ? "Summit, e.g. VE7/LM-001" : "Park, e.g. CA-1234 (POTA uses US- and CA- now, not K- or VE-)");
    else if (edit == EDIT_SPOT_FREQ)
        msg_update_text_fmt("Frequency in kHz (7185) or MHz for VHF (144.2); clear it and Enter for the JS8 dial");
    else
        msg_update_text_fmt("Comment (optional; with none, a JS8 dial spot says JS8)");
}

static void spot_key_cb(lv_event_t *e) {
    popup_key(e, spot_close);
}

/* `focus`: the item to start on - Send, or the field just edited. */
static void spot_show(bool sota, spot_item_t focus) {
    spot_sota = sota;
    lv_group_remove_obj(table);
    spot_list = lv_list_create(dialog.obj);
    lv_obj_set_size(spot_list, 560, WF_HEIGHT - 10);
    lv_obj_align(spot_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(spot_list, &sony_24, 0);
    lv_obj_set_style_bg_color(spot_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(spot_list, lv_color_white(), 0);
    lv_obj_t *t = lv_list_add_text(spot_list, sota ? "SOTA spot via APRS2SOTA (registration needed)"
                                                   : "POTA spot via APSPOT (your pota.app account)");
    lv_obj_set_style_text_font(t, &sony_22, 0);
    spot_preview = lv_list_add_text(spot_list, "");
    lv_obj_set_style_text_font(spot_preview, &sony_22, 0);
    lv_obj_set_style_text_color(spot_preview, lv_color_hex(0x1030a0), 0); /* on the list's light title bar */

    char line[96];
    for (int i = 0; i < SP_COUNT; i++) {
        spot_label((spot_item_t)i, line, sizeof(line));
        lv_obj_t *b = list_add_item(spot_list, line);
        lv_obj_add_event_cb(b, spot_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(b, spot_key_cb, LV_EVENT_KEY, NULL);
        lv_group_add_obj(keyboard_group, b);
        lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
        spot_items[i] = b;
    }
    lv_obj_set_style_text_color(spot_items[SP_SEND], lv_color_hex(0x80ff80), 0);
    lv_obj_set_style_text_color(spot_items[SP_CLOSE], lv_color_hex(0xffc040), 0);
    spot_refresh();
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(spot_items[focus]);
    const char *ref = sota ? last_sota : last_pota;
    if (!sota && (!strncmp(ref, "K-", 2) || !strncmp(ref, "VE-", 3)))
        msg_update_text_fmt("POTA park numbers are US-/CA- now (was K-/VE-): check yours");
}

/* ---- Log QSO ----------------------------------------------------------- */

/* Like desktop JS8Call's Log QSO: the entry is filled in from the QSO and
 * goes to JS8_LOG_PATH (ADIF, on the SD card's DATA partition) and to the
 * radio's QSO database, which marks worked stations. A two-way QSO that
 * ends with 73 or SK opens it by itself (Log prompt), and nothing is
 * logged without Save. */

static bool any_popup(void) {
    for (size_t i = 0; i < POPUPS; i++)
        if (*popups[i].list) return true;
    return false;
}

static bool find_station(const char *call, js8_station_t *out) {
    return js8_stations_find(stations, call, now_wall_ms(), out);
}

/* 6-character grid from a current GPS fix (portable), else the QTH setting. */
static void my_log_grid(char *out, size_t size) {
    double lat, lon;
    int    age;
    if (gps_last_fix(&lat, &lon, &age) && age <= 120 && js8_latlon_to_grid(lat, lon, 6, out, size)) return;
    snprintf(out, size, "%s", my_grid());
}

/* Sent: the report we gave them, else how we heard them. Rcvd: the report
 * they gave us (a message or a heartbeat ack). */
static void log_reports_fill(const js8_qso_t *q, const js8_station_t *st) {
    char *sent = log_entry.rst_sent, *rcvd = log_entry.rst_rcvd;
    if (q && q->has_sent_snr) snprintf(sent, sizeof(log_entry.rst_sent), "%+03d", q->sent_snr);
    else if (q && q->has_heard_snr) snprintf(sent, sizeof(log_entry.rst_sent), "%+03d", q->heard_snr);
    else if (st) snprintf(sent, sizeof(log_entry.rst_sent), "%+03d", st->snr);
    if (q && q->has_rcvd_snr) snprintf(rcvd, sizeof(log_entry.rst_rcvd), "%+03d", q->rcvd_snr);
    else if (st && st->has_reported_snr) snprintf(rcvd, sizeof(log_entry.rst_rcvd), "%+03d", st->reported_snr);
}

static void log_prepare(const char *call) {
    int64_t       now = now_wall_ms();
    js8_qso_t     q;
    js8_station_t st;
    bool          have_q  = js8_qsos_get(qsos, call, now, &q);
    bool          have_st = find_station(call, &st);

    memset(&log_entry, 0, sizeof(log_entry));
    log_grid_typed = false;
    snprintf(log_entry.call, sizeof(log_entry.call), "%s", have_q ? q.call : call);
    snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", have_q && q.grid[0] ? q.grid : have_st ? st.grid : "");

    log_reports_fill(have_q ? &q : NULL, have_st ? &st : NULL);

    log_entry.on_ms   = have_q ? q.start_ms : now;
    log_entry.off_ms  = now;
    log_entry.freq_hz = (uint64_t)cparam_i_get(cfg.cur.fg_freq()) + param_i_get(cfg.js8.tx_freq());
    snprintf(log_entry.my_call, sizeof(log_entry.my_call), "%s", my_call());
    snprintf(log_entry.op_call, sizeof(log_entry.op_call), "%s", operator_call);
    my_log_grid(log_entry.my_grid, sizeof(log_entry.my_grid));
    float pwr          = param_f_get(cfg.pwr());
    log_entry.tx_pwr_w = pwr > TX_PLAYER_MAX_PWR_W ? TX_PLAYER_MAX_PWR_W : pwr;
    if (param_i_get(cfg.js8.log_activation()) == 1) snprintf(log_entry.pota_ref, sizeof(log_entry.pota_ref), "%s", last_pota);
    if (param_i_get(cfg.js8.log_activation()) == 2) snprintf(log_entry.sota_ref, sizeof(log_entry.sota_ref), "%s", last_sota);
}

static void log_close(void) {
    if (!log_list) return;
    lv_obj_del_async(log_list); /* often called from one of its buttons */
    log_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void log_save(void) {
    if (!log_entry.call[0] || !log_entry.my_call[0]) {
        msg_update_text_fmt("Nothing to log");
        return;
    }
    log_entry.off_ms = now_wall_ms();
    char err[96];
    if (!js8_log_append(JS8_LOG_PATH, &log_entry, err, sizeof(err))) {
        msg_update_text_fmt("Not logged: %s", err);
        return;
    }
    /* The radio's QSO database too: worked-before marks here and in FT8. */
    char            *canon = util_canonize_callsign(log_entry.call, false);
    qso_log_record_t rec   = qso_log_record_create(
        log_entry.my_call, canon ? canon : log_entry.call, (time_t)(log_entry.on_ms / 1000), MODE_JS8,
        atoi(log_entry.rst_sent), atoi(log_entry.rst_rcvd), log_entry.freq_hz, log_entry.name[0] ? log_entry.name : NULL,
        NULL, log_entry.my_grid, log_entry.grid);
    free(canon);
    qso_log_record_save(rec);
    worked_forget();
    js8_map_worked_add(log_entry.call, log_entry.grid); /* no more NEW outline on the map */

    js8_qsos_logged(qsos, log_entry.call);
    js8_history_logged(history_db, log_entry.call, log_entry.freq_hz, now_wall_ms());
    if (strcmp(log_pending, log_entry.call) == 0) log_pending[0] = '\0';
    const char *band = js8_log_band(log_entry.freq_hz);
    msg_update_text_fmt("Logged %s%s%s", log_entry.call, band[0] ? " on " : "", band);
    add_info_row("Logged %s %s %s/%s%s%s", log_entry.call, band, log_entry.rst_sent[0] ? log_entry.rst_sent : "-",
                 log_entry.rst_rcvd[0] ? log_entry.rst_rcvd : "-", log_entry.pota_ref[0] ? " POTA " : "",
                 log_entry.pota_ref[0] ? log_entry.pota_ref : log_entry.sota_ref);
    if (view_stations) rebuild_station_rows();
}

static void log_item_cb(lv_event_t *e) {
    log_item_t item = (log_item_t)(intptr_t)lv_event_get_user_data(e);
    if (item == LOG_SAVE || item == LOG_CANCEL) {
        if (item == LOG_SAVE) log_save();
        else log_pending[0] = '\0';
        log_close();
        return;
    }
    /* A field: into the keyboard, then back here (see texts_item_cb). */
    popup_to_keyboard(&log_list, item == LOG_GRID ? EDIT_LOG_GRID : item == LOG_NAME ? EDIT_LOG_NAME : EDIT_LOG_NOTE,
                      item == LOG_GRID ? log_entry.grid : item == LOG_NAME ? log_entry.name : log_entry.comment);
}

/* ESC on the prompt for a QSO that ended means "not now": Log QSO then
 * takes the selected station again (B-24, your choice). */
static void log_esc(void) {
    if (log_pending[0] && strcmp(log_entry.call, log_pending) == 0) log_pending[0] = '\0';
    log_close();
}

static void log_key_cb(lv_event_t *e) {
    popup_key(e, log_esc);
}

static lv_obj_t *log_add(log_item_t item, const char *label) {
    lv_obj_t *b = list_add_item(log_list, label);
    lv_obj_add_event_cb(b, log_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    lv_obj_add_event_cb(b, log_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    return b;
}

static void log_reports_line(char *line, size_t size) {
    snprintf(line, size, "Sent %s  Rcvd %s  %.3f MHz  %.0f W", log_entry.rst_sent[0] ? log_entry.rst_sent : "-",
             log_entry.rst_rcvd[0] ? log_entry.rst_rcvd : "-", log_entry.freq_hz / 1e6, log_entry.tx_pwr_w);
}

/* They sent something while the popup is open (often their 73 after ours
 * opened it): take in a new report or grid, in place, focus unchanged. */
static void log_refresh(void) {
    js8_qso_t q;
    if (!log_list || !js8_qsos_get(qsos, log_entry.call, now_wall_ms(), &q)) return;
    log_reports_fill(&q, NULL);
    char line[160];
    log_reports_line(line, sizeof(line));
    lv_label_set_text(log_reports, line);
    if (!log_grid_typed && q.grid[0] && strcmp(q.grid, log_entry.grid) != 0) {
        snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", q.grid);
        snprintf(line, sizeof(line), "Grid: %s", log_entry.grid);
        lv_label_set_text(lv_obj_get_child(log_grid_btn, 0), line);
    }
}

/* `focus`: the item to start on - Save, or the field just edited. */
static void log_list_open(log_item_t focus) {
    lv_group_remove_obj(table);
    log_list = lv_list_create(dialog.obj);
    lv_obj_set_size(log_list, 560, WF_HEIGHT - 10);
    lv_obj_align(log_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(log_list, &sony_24, 0);
    lv_obj_set_style_bg_color(log_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(log_list, lv_color_white(), 0);

    char      line[160], on[8], off[8];
    time_t    t_on = (time_t)(log_entry.on_ms / 1000), t_off = (time_t)(log_entry.off_ms / 1000);
    struct tm tm;
    strftime(on, sizeof(on), "%H:%M", gmtime_r(&t_on, &tm));
    strftime(off, sizeof(off), "%H:%M", gmtime_r(&t_off, &tm));
    snprintf(line, sizeof(line), "Log %s  %s  %s-%s UTC", log_entry.call, js8_log_band(log_entry.freq_hz), on, off);
    lv_obj_t *t = lv_list_add_text(log_list, line);
    lv_obj_set_style_text_font(t, &sony_22, 0);
    log_reports_line(line, sizeof(line));
    log_reports = lv_list_add_text(log_list, line);
    lv_obj_set_style_text_font(log_reports, &sony_22, 0);

    lv_obj_t *items[LOG_CANCEL + 1];
    items[LOG_SAVE] = log_add(LOG_SAVE, "Save to log");
    lv_obj_set_style_text_color(items[LOG_SAVE], lv_color_hex(0x80ff80), 0);
    snprintf(line, sizeof(line), "Grid: %s", log_entry.grid[0] ? log_entry.grid : "(none)");
    log_grid_btn = items[LOG_GRID] = log_add(LOG_GRID, line);
    snprintf(line, sizeof(line), "Name: %s", log_entry.name[0] ? log_entry.name : "(none)");
    items[LOG_NAME] = log_add(LOG_NAME, line);
    snprintf(line, sizeof(line), "Comment: %s", log_entry.comment[0] ? log_entry.comment : "(none)");
    items[LOG_NOTE] = log_add(LOG_NOTE, line);
    if (log_entry.pota_ref[0] || log_entry.sota_ref[0]) {
        snprintf(line, sizeof(line), "Activating %s %s", log_entry.pota_ref[0] ? "POTA" : "SOTA",
                 log_entry.pota_ref[0] ? log_entry.pota_ref : log_entry.sota_ref);
        t = lv_list_add_text(log_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
    }
    if (log_entry.op_call[0]) { /* Settings: operator, not the station call */
        snprintf(line, sizeof(line), "Operator %s (station %s)", log_entry.op_call, log_entry.my_call);
        t = lv_list_add_text(log_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
    }
    items[LOG_CANCEL] = log_add(LOG_CANCEL, "Cancel");
    lv_obj_set_style_text_color(items[LOG_CANCEL], lv_color_hex(0xffc040), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(items[focus]);
}

/* Back from the keyboard: keep the value (NULL: cancelled), reopen. */
static void log_edit_done(const char *text) {
    /* Copy first: closing the window frees the text. */
    char  buf[sizeof(alert_words)];
    char *value = NULL;
    if (text) value = strncpy(buf, text, sizeof(buf) - 1), buf[sizeof(buf) - 1] = '\0', buf;
    int target  = edit_target;
    edit_target = 0;
    compose_close();
    if (value) {
        switch (target) {
        case EDIT_LOG_GRID: { /* checked in compose_ok_cb; in capitals, as logs have them */
            char *g = value + strspn(value, " ");
            size_t n = strcspn(g, " ");
            g[n]     = '\0';
            for (char *c = g; *c; c++) *c = (char)toupper((unsigned char)*c);
            snprintf(log_entry.grid, sizeof(log_entry.grid), "%s", g);
            log_grid_typed = true;
            break;
        }
        case EDIT_LOG_NAME:
            snprintf(log_entry.name, sizeof(log_entry.name), "%s", value);
            break;
        case EDIT_LOG_NOTE:
            snprintf(log_entry.comment, sizeof(log_entry.comment), "%s", value);
            break;
        case EDIT_POTA_REF:
            snprintf(last_pota, sizeof(last_pota), "%s", value);
            save_texts();
            break;
        case EDIT_SOTA_REF:
            snprintf(last_sota, sizeof(last_sota), "%s", value);
            save_texts();
            break;
        case EDIT_ALERT_WORDS:
            js8_alert_words_normalise(value, alert_words, sizeof(alert_words));
            save_texts();
            if (view_stations) rebuild_station_rows();
            break;
        case EDIT_FREQ:
            tune_custom(value);
            return;
        case EDIT_SPOT_REF:
            if (spot_sota) snprintf(last_sota, sizeof(last_sota), "%s", value);
            else snprintf(last_pota, sizeof(last_pota), "%s", value);
            save_texts();
            if (btn_act.disp_btn) buttons_refresh(&btn_act);
            break;
        case EDIT_SPOT_FREQ: /* a frequency: spot it (remembered); empty: the JS8 dial */
            if (spot_freq_empty(value)) spot_use_typed = false;
            else spot_use_typed = spot_parse_freq(value, &spot_typed_hz);
            save_texts();
            break;
        case EDIT_SPOT_NOTE:
            snprintf(spot_note, sizeof(spot_note), "%s", value);
            save_texts();
            break;
        case EDIT_BEACON_GRID:
        case EDIT_BEACON_GPS:
            aprs_beacon(target == EDIT_BEACON_GPS, value);
            return;
        }
    }
    if (target >= EDIT_BEACON_GRID) { /* cancelled: nothing sent */
        msg_update_text_fmt("Position not sent");
        return;
    }
    if (target == EDIT_FREQ) return; /* cancelled */
    if (target >= EDIT_SPOT_REF) { /* back on the field just edited */
        spot_show(spot_sota, target == EDIT_SPOT_REF ? SP_REF : target == EDIT_SPOT_FREQ ? SP_FREQ : SP_NOTE);
        return;
    }
    if (target == EDIT_ALERT_WORDS) {
        alerts_show();
        return;
    }
    if (target == EDIT_POTA_REF || target == EDIT_SOTA_REF) {
        if (btn_act.disp_btn) buttons_refresh(&btn_act);
        return;
    }
    /* Back on the field just edited: Save is a separate, deliberate step. */
    log_list_open(target == EDIT_LOG_GRID ? LOG_GRID : target == EDIT_LOG_NAME ? LOG_NAME : LOG_NOTE);
}

static void log_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (log_list) { /* Log QSO again closes it */
        log_close();
        return;
    }
    if (popup_guard()) return;
    if (composing) return;
    if (!my_call()[0]) {
        msg_update_text_fmt("Set your callsign first: APP > Callsign");
        return;
    }
    /* A QSO that just ended, unless you've selected another station since
     * (your choice): the ended one is still there when you select it. It's
     * forgotten with the QSO (30 min), or once logged. */
    char      call[JS8_RX_CALL_LEN];
    float     freq;
    int       snr;
    js8_qso_t q;
    bool      selected = selected_station(call, sizeof(call), &freq, &snr);
    if (log_pending[0] && !js8_qsos_get(qsos, log_pending, now_wall_ms(), &q)) log_pending[0] = '\0';
    if (log_pending[0] && (!selected || strcmp(call, log_pending) == 0)) {
        snprintf(call, sizeof(call), "%s", log_pending);
    } else if (!selected) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }
    log_prepare(call);
    log_list_open(LOG_SAVE);
}

/* A two-way QSO ended with 73 / SK. */
static void log_offer(const char *call) {
    snprintf(log_pending, sizeof(log_pending), "%s", call);
    if (!param_i_get(cfg.js8.log_prompt())) {
        add_info_row("QSO with %s ended: Log QSO (page 5) to log it", call);
        return;
    }
    if (composing || any_popup()) {
        msg_update_text_fmt("QSO with %s ended: Log QSO (page 5) to log it", call);
        add_info_row("QSO with %s ended: Log QSO (page 5) to log it", call);
        return;
    }
    log_prepare(call);
    log_list_open(LOG_SAVE);
    msg_update_text_fmt("QSO with %s ended - Save to log?", call);
}

static const char *prompt_label_getter(void) {
    return param_i_get(cfg.js8.log_prompt()) ? "Log\nprompt: On" : "Log\nprompt: Off";
}

static void prompt_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    param_i_set(cfg.js8.log_prompt(), !param_i_get(cfg.js8.log_prompt()));
    buttons_refresh(btn);
    msg_update_text_fmt(param_i_get(cfg.js8.log_prompt()) ? "Offer to log when a QSO ends with 73 or SK"
                                                : "No log prompt: use Log QSO");
}

/* Activating a park or summit: MY_SIG / MY_SOTA_REF in the log. The
 * reference is the one last spotted via APRS, or set by holding this. */
static const char *act_label_getter(void) {
    static char label[40];
    switch (param_i_get(cfg.js8.log_activation())) {
    case 1:
        snprintf(label, sizeof(label), "POTA\n%s", last_pota[0] ? last_pota : "(hold: set)");
        break;
    case 2:
        snprintf(label, sizeof(label), "SOTA\n%s", last_sota[0] ? last_sota : "(hold: set)");
        break;
    default:
        snprintf(label, sizeof(label), "Activ.:\nOff");
        break;
    }
    return label;
}

static void act_cb(button_data_t *btn) {
    user_touch();
    if (popup_guard()) return;
    uint8_t v = (param_i_get(cfg.js8.log_activation()) + 1) % 3;
    param_i_set(cfg.js8.log_activation(), v);
    buttons_refresh(btn);
    const char *ref = v == 1 ? last_pota : last_sota;
    if (v == 0) msg_update_text_fmt("Not activating: no park or summit in the log");
    else if (!ref[0]) msg_update_text_fmt("Hold this button to set your %s", v == 1 ? "park" : "summit");
    else msg_update_text_fmt("Logging as %s %s (hold to change)", v == 1 ? "POTA" : "SOTA", ref);
}

static void act_hold_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard() || composing) return;
    uint8_t v = param_i_get(cfg.js8.log_activation());
    if (v == 0) {
        msg_update_text_fmt("Press to choose POTA or SOTA first");
        return;
    }
    popup_to_keyboard(NULL, v == 1 ? EDIT_POTA_REF : EDIT_SOTA_REF, v == 1 ? last_pota : last_sota);
}

/* ---- Inbox and messages -------------------------------------------------- */

/* Like desktop JS8Call: "CALL MSG text" to us (checksum good) goes to the
 * inbox, answered with "CALL ACK" (sent by AUTO, else offered on Reply).
 * The Query list sends messages: MSG for their inbox, MSG TO: to leave one
 * at their station for someone else, QUERY MSGS to ask what they hold. */

#define INBOX_ROWS 200 /* all of them (Inbox::MAX_MESSAGES): an older unread one couldn't be opened (bug hunt 9) */
#define HELD_ROWS  100 /* HeldMessages::MAX_MESSAGES */

static int inbox_view_id; /* the message shown, 0: the list */

/* Green, like a selected Settings tab, while there are unread messages. */
static void inbox_refresh_button(void) {
    buttons_mark(&btn_inbox, inbox && js8_inbox_unread(inbox) > 0);
}

static const char *inbox_label_getter(void) {
    static char label[24];
    int         unread = js8_inbox_unread(inbox);
    if (unread) snprintf(label, sizeof(label), "Inbox\n%d new", unread);
    else snprintf(label, sizeof(label), "Inbox");
    return label;
}

/* What js8_process() kept: a MSG for us (or our group) in the inbox, or a
 * "MSG TO:W1ABC ..." held here until W1ABC asks (QUERY MSGS, QUERY MSG n,
 * our HB ack's "MSG ID n", or our RETRIEVE MSG), as desktop does. */
static void stored_received(const js8_stored_t *k) {
    if (k->kind == JS8_STORED_APRS_RECEIPT) {
        aprs_receipt(k);
        return;
    }
    char from[JS8_PATH_LEN + 16];
    js8_path_display(k->path[0] ? k->path : k->from, from, sizeof(from));
    if (k->kind == JS8_STORED_INBOX) {
        bool group = k->to[0] == '@';
        if (k->id < 0) msg_update_text_fmt("Message from %s: can't save %s, so no ACK sent", from, JS8_INBOX_PATH);
        else if (!k->resend && group) msg_update_text_fmt("New message to %s from %s - Inbox on page 3", k->to, from);
        else if (!k->resend) msg_update_text_fmt("New message from %s - Inbox on page 3", from);
        if (!k->resend) add_info_row("Message from %s in the Inbox: %s", from, k->text);
        if (!k->resend && (param_i_get(cfg.js8.alerts()) & JS8_ALERT_INBOX)) alert_beep(2);
        inbox_refresh_button();
        update_status();
    } else if (k->kind == JS8_STORED_HELD && !k->resend) {
        if (k->id < 0) {
            msg_update_text_fmt("Message from %s for %s: can't save %s, so no ACK sent", from, k->to, JS8_HELD_PATH);
            return;
        }
        msg_update_text_fmt("Holding a message from %s for %s (Inbox)", from, k->to);
        add_info_row("Holding message %d from %s for %s: %s", k->id, from, k->to, k->text);
    }
}

/* Same message as the transmitter reports it: it sends upper case, trimmed. */
static bool same_tx_text(const char *a, const char *b) {
    while (*a == ' ') a++;
    while (*b == ' ') b++;
    size_t la = strlen(a), lb = strlen(b);
    while (la && a[la - 1] == ' ') la--;
    while (lb && b[lb - 1] == ' ') lb--;
    return la == lb && strncasecmp(a, b, la) == 0;
}

/* A held message's delivery was queued: it counts once it has all gone.
 * A few can be on their way; with all taken, the oldest is forgotten. */
static void deliver_start(int id, const char *group_call, const char *text) {
    int slot = 0;
    while (slot < DELIVERIES - 1 && deliveries[slot].id) slot++;
    if (deliveries[slot].id) { /* all taken */
        memmove(&deliveries[0], &deliveries[1], (DELIVERIES - 1) * sizeof(deliveries[0]));
        slot = DELIVERIES - 1;
    }
    deliveries[slot].id = id;
    snprintf(deliveries[slot].group_call, sizeof(deliveries[slot].group_call), "%s", group_call ? group_call : "");
    snprintf(deliveries[slot].text, sizeof(deliveries[slot].text), "%s", text);
}

/* A message ended. If it was a delivery, sent in full it's delivered;
 * stopped halfway, it stays held, and their next QUERY MSGS or heartbeat
 * is offered it again. */
static void deliver_end(const char *text, bool completed) {
    for (int i = 0; i < DELIVERIES; i++) {
        if (!deliveries[i].id || !same_tx_text(text, deliveries[i].text)) continue;
        int id = deliveries[i].id;
        if (completed) {
            if (deliveries[i].group_call[0]) js8_held_group_delivered(held, id, deliveries[i].group_call);
            else js8_held_delivered(held, id);
            add_info_row("Held message %d delivered", id);
        } else {
            add_info_row("Held message %d stopped before the end: still held", id);
        }
        deliveries[i].id = 0;
        return;
    }
}

static void inbox_close(void) {
    if (!inbox_list) return;
    lv_obj_del_async(inbox_list); /* often called from one of its buttons */
    inbox_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* Leave the popup from one of its own buttons, going somewhere else (the
 * keyboard, or another view): buttons out of the group now, list later. */
static void inbox_leave(void) {
    popup_leave(&inbox_list);
}

/* "CALL MSG " / "CALL MSG TO:" in the keyboard; kind is "MSG " or "MSG TO:".
 * `call` may be a relay path ("K2XYZ>N0XYZ"). */
static void msg_compose(const char *call, const char *kind) {
    char prefill[JS8_PATH_LEN + 12];
    snprintf(prefill, sizeof(prefill), "%s %s", call, kind);
    if (!popup_to_keyboard(NULL, 0, prefill)) return;
    if (strcmp(kind, "MSG TO:") == 0)
        msg_update_text_fmt("Left at %s for someone: type their call, a space, the message", call);
    else msg_update_text_fmt("Message for %s's inbox: type it and press Enter", call);
}

static void inbox_show(int id);

/* ESC in a message: back to the list; in the list: close. */
static void inbox_esc(void) {
    if (inbox_view_id) {
        inbox_leave();
        inbox_show(0);
    } else {
        inbox_close();
    }
}

static void inbox_key_cb(lv_event_t *e) {
    popup_key(e, inbox_esc);
}

typedef enum {
    INBOX_NEW        = -1,
    INBOX_CLOSE      = -2,
    INBOX_BACK       = -3,
    INBOX_REPLY      = -4, /* desktop's "Reply to <path>": PATH MSG ... */
    INBOX_DELETE     = -5,
    INBOX_REPLY_VIA  = -6, /* "Reply to <sender> via <path>": PATH MSG TO:SENDER ... */
    INBOX_REPLY_ORIG = -7, /* "Reply to <sender>": SENDER MSG ... */
    INBOX_FETCH_NEXT = -8, /* PATH QUERY MSG n, for its NEXT MSG ID n */
    INBOX_REPLY_APRS = -9, /* from an APRS gateway: an APRS message to its DE sender */
} inbox_action_t;

/* The sender of a message an APRS gateway passed on: "... DE N0CALL". */
static bool aprs_sender(const char *text, char *call, size_t size) {
    const char *de = NULL;
    for (const char *p = strstr(text, " DE "); p; p = strstr(p + 1, " DE ")) de = p;
    if (!de) return false;
    de += 4;
    while (*de == ' ') de++;
    size_t n = strcspn(de, " ");
    if (!n || de[n + strspn(de + n, " ")] != '\0' || n >= size) return false;
    snprintf(call, size, "%.*s", (int)n, de);
    return true;
}

/* A text from a phone, passed on by an SMS gateway (sender SMS, SMSGTE):
 * "@6045551234 TEXT DE SMS" starts with the number (or the alias) it came
 * from, which the gateway needs at the front of the reply. */
static bool aprs_sms_from(const char *text, const char *sender, char *out, size_t size) {
    if (strncmp(sender, "SMS", 3) != 0) return false;
    while (*text == ' ') text++;
    size_t n = strcspn(text, " ");
    if (text[0] != '@' || n < 2 || n >= size) return false;
    for (size_t i = 1; i < n; i++) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) return false;
    }
    snprintf(out, size, "%.*s", (int)n, text);
    return true;
}
#define INBOX_HELD 100000 /* list/view ids from here on are held messages (id - INBOX_HELD) */

static void inbox_item_cb(lv_event_t *e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    int viewed = inbox_view_id;
    switch (action) {
    case INBOX_CLOSE:
        inbox_close();
        return;
    case INBOX_BACK:
        inbox_leave();
        inbox_show(0);
        return;
    case INBOX_DELETE:
        if (viewed >= INBOX_HELD) js8_held_delete(held, viewed - INBOX_HELD);
        else js8_inbox_delete(inbox, viewed);
        msg_update_text_fmt("Message deleted");
        inbox_leave();
        inbox_show(0);
        inbox_refresh_button();
        return;
    case INBOX_REPLY:
    case INBOX_REPLY_VIA:
    case INBOX_REPLY_ORIG:
    case INBOX_FETCH_NEXT:
    case INBOX_REPLY_APRS: {
        js8_inbox_msg_t m;
        if (!js8_inbox_get(inbox, viewed, &m)) return;
        const char *path = m.path[0] ? m.path : m.from;
        char        orig[JS8_RX_CALL_LEN] = "";
        int         next                  = 0;
        js8_delivered_signature(m.text, orig, sizeof(orig), &next);
        char aprs_to[JS8_RX_CALL_LEN];
        if (action == INBOX_REPLY_APRS && !aprs_sender(m.text, aprs_to, sizeof(aprs_to))) return;
        if (action == INBOX_FETCH_NEXT) { /* sent at once: back to the messages */
            char text[JS8_PATH_LEN + 24];
            snprintf(text, sizeof(text), "%s QUERY MSG %d", path, next);
            inbox_close();
            tx_queue(text);
            return;
        }
        inbox_leave(); /* into the keyboard */
        if (action == INBOX_REPLY) {
            msg_compose(path, "MSG ");
        } else if (action == INBOX_REPLY_ORIG) {
            msg_compose(orig, "MSG ");
        } else if (action == INBOX_REPLY_VIA) {
            char prefill[JS8_PATH_LEN + JS8_RX_CALL_LEN + 12];
            snprintf(prefill, sizeof(prefill), "%s MSG TO:%s ", path, orig);
            if (popup_to_keyboard(NULL, 0, prefill))
                msg_update_text_fmt("Left at %s for %s: type the message and press Enter", path, orig);
        } else {
            /* An SMS: the number filled in, the cursor after it. */
            char head[64], sms[24];
            bool to_sms = aprs_sms_from(m.text, aprs_to, sms, sizeof(sms));
            snprintf(head, sizeof(head), "%s%-9.9s:%s%s", APRS_CMD, aprs_to, to_sms ? sms : "", to_sms ? " " : "");
            aprs_compose(head, "");
            lv_group_set_editing(keyboard_group, true);
            if (to_sms) msg_update_text_fmt("SMS to %s: type your message and press Enter", sms + 1);
            else msg_update_text_fmt("APRS message to %s: type it and press Enter", aprs_to);
        }
        return;
    }
    case INBOX_NEW: {
        char  call[JS8_RX_CALL_LEN];
        float freq;
        int   snr;
        inbox_leave();
        if (selected_station(call, sizeof(call), &freq, &snr)) {
            apply_hold(freq);
            msg_compose(call, "MSG ");
        } else if (popup_to_keyboard(NULL, 0, NULL)) {
            msg_update_text_fmt("Type their call, then MSG and the message");
        }
        return;
    }
    default: /* a message */
        inbox_leave();
        inbox_show(action);
        return;
    }
}

static lv_obj_t *inbox_add(const char *label, int action) {
    lv_obj_t *b = list_add_item(inbox_list, label);
    lv_obj_add_event_cb(b, inbox_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)action);
    lv_obj_add_event_cb(b, inbox_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    /* Long lines end in "..." rather than scrolling, which redraws the list. */
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

static void utc_label(int64_t ms, char *buf, size_t size, const char *fmt) {
    time_t    t = (time_t)(ms / 1000);
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(buf, size, fmt, &tm);
}

/* The list (id 0) or one message. */
static void inbox_show(int id) {
    inbox_view_id = id;
    lv_group_remove_obj(table);
    inbox_list = lv_list_create(dialog.obj);
    lv_obj_set_size(inbox_list, 600, WF_HEIGHT - 10);
    lv_obj_align(inbox_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(inbox_list, &sony_24, 0);
    lv_obj_set_style_bg_color(inbox_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(inbox_list, lv_color_white(), 0);

    char      line[JS8_RX_TEXT_LEN + 48];
    lv_obj_t *first = NULL;
    if (id >= INBOX_HELD) {
        /* A message held here for another station. */
        js8_held_msg_t m;
        if (!js8_held_get(held, id - INBOX_HELD, &m)) {
            inbox_view_id = 0;
            lv_obj_del(inbox_list);
            inbox_show(0);
            return;
        }
        char when[32], from[JS8_PATH_LEN + 16];
        utc_label(m.utc_ms, when, sizeof(when), "%d %b %H:%M UTC");
        js8_path_display(m.path, from, sizeof(from));
        snprintf(line, sizeof(line), "For %s from %s  %s", m.to, from, when);
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        lv_obj_t *body = lv_list_add_text(inbox_list, m.text);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_color(body, lv_color_white(), 0);
        lv_obj_set_style_pad_ver(body, 8, 0);
        if (m.group)
            snprintf(line, sizeof(line), "For any %s member to fetch for 2 days; fetched by %d", m.to, m.got);
        else snprintf(line, sizeof(line), m.delivered ? "Delivered to %s" : "Waiting for %s to ask (QUERY MSGS)", m.to);
        t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        first = inbox_add("Delete", INBOX_DELETE);
        lv_obj_t *back = inbox_add("Back", INBOX_BACK);
        lv_obj_set_style_text_color(back, lv_color_hex(0xffc040), 0);
    } else if (id) {
        js8_inbox_msg_t m;
        if (!js8_inbox_get(inbox, id, &m)) {
            inbox_view_id = 0;
            lv_obj_del(inbox_list);
            inbox_show(0);
            return;
        }
        js8_inbox_mark_read(inbox, id);
        inbox_refresh_button();
        update_status();
        char when[32], from[JS8_PATH_LEN + 16];
        utc_label(m.utc_ms, when, sizeof(when), "%d %b %H:%M UTC");
        js8_path_display(m.path[0] ? m.path : m.from, from, sizeof(from));
        if (m.to[0] == '@') snprintf(line, sizeof(line), "From %s to %s  %s", from, m.to, when);
        else snprintf(line, sizeof(line), "From %s  %s", from, when);
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);
        lv_obj_t *body = lv_list_add_text(inbox_list, m.text);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_bg_color(body, lv_color_hex(0x202020), 0);
        lv_obj_set_style_text_color(body, lv_color_white(), 0);
        lv_obj_set_style_pad_ver(body, 8, 0);
        /* Desktop's Reply choices: to the path it came by; and for a
         * delivered message ("... FROM N0XYZ"), to its sender through the
         * station that held it (MSG TO:) or straight. */
        const char *path = m.path[0] ? m.path : m.from;
        char        orig[JS8_RX_CALL_LEN] = "", aprs_from[JS8_RX_CALL_LEN];
        int         next                  = 0;
        bool        signed_ = js8_delivered_signature(m.text, orig, sizeof(orig), &next) && orig[0] != '@';
        if (strcmp(path, "APRS") == 0) {
            if (aprs_sender(m.text, aprs_from, sizeof(aprs_from))) {
                char sms[24];
                if (aprs_sms_from(m.text, aprs_from, sms, sizeof(sms)))
                    snprintf(line, sizeof(line), "Reply by SMS to %s", sms);
                else snprintf(line, sizeof(line), "Reply by APRS to %s", aprs_from);
                first = inbox_add(line, INBOX_REPLY_APRS);
            }
        } else {
            snprintf(line, sizeof(line), "Reply: MSG to %s", from);
            first = inbox_add(line, INBOX_REPLY);
            if (signed_ && strcmp(orig, path) != 0) {
                snprintf(line, sizeof(line), "Reply to %s via %s (MSG TO:)", orig, from);
                inbox_add(line, INBOX_REPLY_VIA);
                snprintf(line, sizeof(line), "Reply to %s", orig);
                inbox_add(line, INBOX_REPLY_ORIG);
            }
            if (next > 0) {
                snprintf(line, sizeof(line), "Fetch the next: QUERY MSG %d", next);
                inbox_add(line, INBOX_FETCH_NEXT);
            }
        }
        lv_obj_t *del = inbox_add("Delete", INBOX_DELETE);
        if (!first) first = del;
        lv_obj_t *back = inbox_add("Back", INBOX_BACK);
        lv_obj_set_style_text_color(back, lv_color_hex(0xffc040), 0);
    } else {
        static js8_inbox_msg_t rows[INBOX_ROWS];
        int                    n = js8_inbox_list(inbox, rows, INBOX_ROWS);
        snprintf(line, sizeof(line), "Inbox: %d message%s, %d new", js8_inbox_count(inbox),
                 js8_inbox_count(inbox) == 1 ? "" : "s", js8_inbox_unread(inbox));
        lv_obj_t *t = lv_list_add_text(inbox_list, line);
        lv_obj_set_style_text_font(t, &sony_22, 0);

        char  call[JS8_RX_CALL_LEN];
        float freq;
        int   snr;
        if (selected_station(call, sizeof(call), &freq, &snr)) snprintf(line, sizeof(line), "New message to %s...", call);
        else snprintf(line, sizeof(line), "New message...");
        first = inbox_add(line, INBOX_NEW);
        lv_obj_set_style_text_color(first, lv_color_hex(0x80ff80), 0);

        lv_obj_t *newest_new = NULL;
        for (int i = 0; i < n; i++) {
            char when[16];
            utc_label(rows[i].utc_ms, when, sizeof(when), "%d %b %H:%M");
            char from[JS8_PATH_LEN + 16];
            js8_path_display(rows[i].path[0] ? rows[i].path : rows[i].from, from, sizeof(from));
            snprintf(line, sizeof(line), "%s%s  %s  %s", rows[i].read ? "" : "* ", when, from, rows[i].text);
            lv_obj_t *b = inbox_add(line, rows[i].id);
            if (!rows[i].read) {
                lv_obj_set_style_text_color(b, lv_color_hex(0xffe080), 0);
                if (!newest_new) newest_new = b; /* newest first */
            }
        }
        if (n == 0) {
            t = lv_list_add_text(inbox_list, "No messages yet");
            lv_obj_set_style_text_font(t, &sony_22, 0);
        }
        /* Messages held here for others (MSG TO:), until they ask. */
        static js8_held_msg_t held_rows[HELD_ROWS];
        int                   nh = js8_held_list(held, held_rows, HELD_ROWS);
        if (nh > 0) {
            snprintf(line, sizeof(line), "Held for others: %d waiting", js8_held_waiting(held));
            t = lv_list_add_text(inbox_list, line);
            lv_obj_set_style_text_font(t, &sony_22, 0);
        }
        for (int i = 0; i < nh; i++) {
            char from[JS8_PATH_LEN + 16], got[16] = "";
            js8_path_display(held_rows[i].path, from, sizeof(from));
            if (held_rows[i].group) snprintf(got, sizeof(got), " (%d got it)", held_rows[i].got);
            snprintf(line, sizeof(line), "%sfor %s%s from %s  %s", held_rows[i].delivered ? "(sent) " : "",
                     held_rows[i].to, got, from, held_rows[i].text);
            lv_obj_t *b = inbox_add(line, INBOX_HELD + held_rows[i].id);
            if (held_rows[i].delivered) lv_obj_set_style_text_color(b, lv_color_hex(0x909090), 0);
        }
        lv_obj_t *close = inbox_add("Close", INBOX_CLOSE);
        lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
        if (newest_new) first = newest_new; /* straight to the newest unread (your choice) */
    }
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

static void inbox_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (inbox_list) { /* Inbox again closes it */
        inbox_close();
        return;
    }
    if (popup_guard()) return;
    if (composing || !inbox) return;
    inbox_show(0);
}

/* ---- Alerts ---------------------------------------------------------------- */

/* Like desktop JS8Call's notifications and highlight words: a beep through
 * the speaker for what's switched on in the Alerts list, and rows matching
 * an alert word (calls or words you type) in purple. Never while
 * transmitting: the speaker path is the TX audio path then.
 *
 * The base unit plays this app's audio on the speaker only in its play
 * mode (radio_speaker_play, as voice prompts and the recorder use it);
 * without it the beep went nowhere. In play mode the audio the base sends
 * us isn't the receiver's, so the beep is kept short and beep_guard feeds
 * the decoder silence meanwhile. */

#define BEEP_HZ       1000
#define BEEP_MS       120
#define BEEP_GAP_MS   100
#define BEEP_LEAD_MS  50   /* silence first: the base switches to play mode */
#define BEEP_TAIL_MS  300  /* guard after speaker-off: capture latency */
#define BEEP_AMPL     13000 /* about -8 dBFS */
#define BEEP_EVERY_MS 3000 /* at most one alert sound per 3 s */

static int64_t beep_last_ms;

enum {
    BEEP_TONE = AUDIO_PLAY_RATE * BEEP_MS / 1000,
    BEEP_GAP  = AUDIO_PLAY_RATE * BEEP_GAP_MS / 1000,
    BEEP_LEAD = AUDIO_PLAY_RATE * BEEP_LEAD_MS / 1000,
};
static int16_t         beep_buf[BEEP_TONE + BEEP_GAP];
static int16_t         beep_lead[BEEP_LEAD];
static int             beep_count;
static atomic_bool     beeping;

/* Beep thread: audio_play() waits for room in the stream, so it can't run
 * on the LVGL thread. */
/* audio_play() in parts of at most 2048 samples, as tx_player does: R1CBU
 * 0.34's hung on a bigger write (1.0's waits properly), and small parts let
 * the beep stop as soon as TX keys. */
static void beep_play(const int16_t *buf, size_t n) {
    for (size_t at = 0; at < n && !atomic_load(&keyed);) {
        size_t part = LV_MIN(2048, n - at);
        audio_play((int16_t *)buf + at, part);
        at += part;
    }
}

static void *beep_thread(void *arg) {
    (void)arg;
    pthread_setname_np(pthread_self(), "js8-beep");
    pthread_mutex_lock(&speaker_lock);
    if (atomic_load(&keyed)) goto out;
    /* The receiver's level just before, for the log. */
    atomic_store(&beep_guard_sq, 0);
    atomic_store(&beep_guard_n, 0);
    atomic_store(&beep_guard, true);
    radio_speaker_play(true);
    beep_play(beep_lead, BEEP_LEAD);
    for (int i = 0; i < beep_count; i++) beep_play(beep_buf, BEEP_TONE + BEEP_GAP);
    audio_play_wait();
    radio_speaker_play(false);
    atomic_store(&beep_guard_end, now_mono_ms() + BEEP_TAIL_MS);
    atomic_store(&beep_guard, false);
out:
    pthread_mutex_unlock(&speaker_lock);
    atomic_store(&beeping, false);
    return NULL;
}

/* Once the guard is over: what the capture carried while the speaker
 * played, against the receiver just before (tells whether play mode
 * really cuts the receiver on this base). */
static void beep_log_level(void) {
    int64_t n = atomic_load(&beep_guard_n);
    if (!n || beep_guard_active()) return;
    double rms = sqrt((double)atomic_load(&beep_guard_sq) / 1e9 / (double)n);
    LV_LOG_USER("JS8 beep: capture %.1f dBFS during the beep (%lld samples), %.1f dBFS before",
                20.0 * log10(rms + 1e-12), (long long)n, beep_level_before_db);
    atomic_store(&beep_guard_n, 0);
}

static void alert_beep(int count) {
    if (!(param_i_get(cfg.js8.alerts()) & JS8_ALERT_BEEP)) return;
    if (now_wall_ms() - beep_last_ms < BEEP_EVERY_MS) return;
    beep_start(count);
}

static int beeps_started; /* for tools/js8_ui_harness */

/* `count` beeps now, unless we're sending or a beep is playing (false). */
static bool beep_start(int count) {
    if (atomic_load(&keyed) || js8_tx_busy(tx)) return false;
    if (atomic_exchange(&beeping, true)) return false;
    beep_last_ms = now_wall_ms();

    static bool made;
    if (!made) {
        int ramp = AUDIO_PLAY_RATE / 200; /* 5 ms fades: no clicks */
        for (int i = 0; i < BEEP_TONE; i++) {
            float env = 1.0f;
            if (i < ramp) env = (float)i / ramp;
            if (i > BEEP_TONE - ramp) env = (float)(BEEP_TONE - i) / ramp;
            beep_buf[i] = (int16_t)(BEEP_AMPL * env * sinf(2.0f * (float)M_PI * BEEP_HZ * i / AUDIO_PLAY_RATE));
        }
        made = true;
    }
    beep_count = count;
    beeps_started++;
    pthread_t th;
    if (pthread_create(&th, NULL, beep_thread, NULL) == 0) pthread_detach(th);
    else atomic_store(&beeping, false);
    return true;
}

/* ---- High-SWR guard ---------------------------------------------------- */

/* For unattended stations (VE7NHW, beta 5): the SWR above 3:1 for half a
 * second while we transmit turns every automatic sender off (AUTO, auto
 * HB, HB ACK, auto CQ). The message on the air carries on. Three beeps
 * once the transmitter is done (the speaker is the TX audio path while
 * keyed), whatever the Alerts list says. The radio reports the SWR a few
 * times a second while keyed (tx_info, unfiltered in USB-D, capped at 5). */
#define SWR_TRIP    3.0f
#define SWR_TRIP_MS 500

static uint8_t swr_msg_id;
static int64_t swr_high_since; /* 0: the last reading was at or under the limit */
static float   swr_high_max;
static bool    swr_beep_due;

static void swr_guard_trip(float swr) {
    bool any = param_i_get(cfg.js8.auto_mode()) || param_i_get(cfg.js8.hb()) || param_i_get(cfg.js8.hb_ack()) || auto_cq;
    if (!any) return;
    js8_log("high SWR: %.1f:1 for %d ms while sending, AUTO / HB / HB ACK / auto CQ off", swr, SWR_TRIP_MS);
    param_i_set(cfg.js8.auto_mode(), false);
    param_i_set(cfg.js8.hb_ack(), false);
    hb_auto_stop("high SWR");
    auto_cq_stop("high SWR");
    if (btn_auto.disp_btn) buttons_refresh(&btn_auto);
    if (btn_hbackk.disp_btn) buttons_refresh(&btn_hbackk);
    add_info_row("High SWR %.1f:1: AUTO, HB, HB ACK and auto CQ off", swr);
    msg_update_text_fmt("High SWR %.1f:1: AUTO, HB, HB ACK and auto CQ off. Check the antenna", swr);
    update_status();
    swr_beep_due = true;
}

static void swr_guard_reset(void) {
    swr_high_since = 0;
    swr_beep_due   = false;
}

/* Every 250 ms (tx_timer). Three readings over the limit in a row, the
 * first and last 500 ms apart, trip it: a spike as the radio keys doesn't. */
static void swr_guard_tick(void) {
    if (!atomic_load(&keyed)) {
        swr_high_since = 0;
        tx_info_refresh(&swr_msg_id, NULL, NULL, NULL); /* the next TX starts fresh */
        if (swr_beep_due && beep_start(3)) swr_beep_due = false;
        return;
    }
    float swr;
    if (!tx_info_refresh(&swr_msg_id, NULL, NULL, &swr)) return;
    if (swr <= SWR_TRIP) {
        swr_high_since = 0;
        return;
    }
    int64_t now = now_mono_ms();
    if (!swr_high_since) {
        swr_high_since = now;
        swr_high_max   = swr;
        return;
    }
    swr_high_max = LV_MAX(swr_high_max, swr);
    if (now - swr_high_since < SWR_TRIP_MS) return;
    swr_high_since = 0;
    swr_guard_trip(swr_high_max);
}

/* For tools/js8_ui_harness: the automatic switches, and how many beep
 * sounds started (the last one's beep count). */
void dialog_js8_autos(bool *auto_mode, bool *hb, bool *hb_ack, bool *cq) {
    *auto_mode = param_i_get(cfg.js8.auto_mode());
    *hb        = param_i_get(cfg.js8.hb());
    *hb_ack    = param_i_get(cfg.js8.hb_ack());
    *cq        = auto_cq;
}

int dialog_js8_beeps(int *last_count) {
    *last_count = beep_count;
    return beeps_started;
}

/* Every decode but our own: alert words, then what beeps. */
static void alert_check(js8_rx_msg_t *m, bool new_station) {
    if (m->low_confidence) return;
    char hit[16];
    if (js8_alert_hit(m->text, m->from, alert_words, hit, sizeof(hit))) {
        m->alert = true;
        msg_update_text_fmt("Alert %s: %s", hit, m->text);
        alert_beep(2);
        return;
    }
    uint8_t a      = param_i_get(cfg.js8.alerts());
    bool    hb_ack = strstr(m->text, " HEARTBEAT SNR") != NULL;
    if ((a & JS8_ALERT_TO_ME) && m->to_me && !hb_ack) alert_beep(1);
    else if ((a & JS8_ALERT_CQ) && m->cq) alert_beep(1);
    else if ((a & JS8_ALERT_NEW) && new_station && m->from[0] && !worked_before(m->from)) {
        msg_update_text_fmt("New station: %s", m->from);
        alert_beep(1);
    }
}

typedef enum {
    AL_BEEP,
    AL_TO_ME,
    AL_INBOX,
    AL_CQ,
    AL_NEW,
    AL_WORDS,
    AL_TEST,
    AL_CLOSE,
} alerts_item_t;

static const struct {
    uint8_t     bit;
    const char *label;
} alert_switches[] = {
    [AL_BEEP]  = {JS8_ALERT_BEEP, "Beep"},
    [AL_TO_ME] = {JS8_ALERT_TO_ME, "Message to me"},
    [AL_INBOX] = {JS8_ALERT_INBOX, "Inbox message"},
    [AL_CQ]    = {JS8_ALERT_CQ, "Someone calls CQ"},
    [AL_NEW]   = {JS8_ALERT_NEW, "New station (not in log)"},
};

static void alerts_switch_label(alerts_item_t item, char *buf, size_t size) {
    snprintf(buf, size, "%s: %s", alert_switches[item].label,
             (param_i_get(cfg.js8.alerts()) & alert_switches[item].bit) ? "On" : "Off");
}

static void alerts_close(void) {
    if (!alerts_list) return;
    lv_obj_del_async(alerts_list); /* often called from one of its buttons */
    alerts_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

static void alerts_item_cb(lv_event_t *e) {
    alerts_item_t item = (alerts_item_t)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t     *btn  = lv_event_get_target(e);
    char          line[sizeof(alert_words) + 24];
    switch (item) {
    case AL_CLOSE:
        alerts_close();
        return;
    case AL_TEST: {
        int64_t last = beep_last_ms;
        beep_last_ms = 0;
        if (!(param_i_get(cfg.js8.alerts()) & JS8_ALERT_BEEP)) msg_update_text_fmt("Beep is off");
        else if (atomic_load(&keyed) || js8_tx_busy(tx)) msg_update_text_fmt("Not while transmitting");
        alert_beep(2);
        if (!beep_last_ms) beep_last_ms = last;
        return;
    }
    case AL_WORDS:
        /* Into the keyboard, then back here (see texts_item_cb). */
        if (popup_to_keyboard(&alerts_list, EDIT_ALERT_WORDS, alert_words[0] ? alert_words : NULL))
            msg_update_text_fmt("Calls or words to watch for, separated by spaces");
        return;
    default: /* a switch: flip it, relabel in place */
        param_i_set(cfg.js8.alerts(), param_i_get(cfg.js8.alerts()) ^ alert_switches[item].bit);
        alerts_switch_label(item, line, sizeof(line));
        lv_label_set_text(lv_obj_get_child(btn, 0), line);
        return;
    }
}

static void alerts_key_cb(lv_event_t *e) {
    popup_key(e, alerts_close);
}

static lv_obj_t *alerts_add(alerts_item_t item, const char *label) {
    lv_obj_t *b = list_add_item(alerts_list, label);
    lv_obj_add_event_cb(b, alerts_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    lv_obj_add_event_cb(b, alerts_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

static void alerts_show(void) {
    lv_group_remove_obj(table);
    alerts_list = lv_list_create(dialog.obj);
    lv_obj_set_size(alerts_list, 560, WF_HEIGHT - 10);
    lv_obj_align(alerts_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(alerts_list, &sony_24, 0);
    lv_obj_set_style_bg_color(alerts_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(alerts_list, lv_color_white(), 0);
    lv_obj_t *t = lv_list_add_text(alerts_list, "Alerts: beep, and alert words in purple");
    lv_obj_set_style_text_font(t, &sony_22, 0);

    char      line[sizeof(alert_words) + 24];
    lv_obj_t *first = NULL;
    for (int i = AL_BEEP; i <= AL_NEW; i++) {
        alerts_switch_label((alerts_item_t)i, line, sizeof(line));
        lv_obj_t *b = alerts_add((alerts_item_t)i, line);
        if (!first) first = b;
    }
    snprintf(line, sizeof(line), "Alert words: %s", alert_words[0] ? alert_words : "(none)");
    lv_obj_t *words = alerts_add(AL_WORDS, line);
    lv_obj_set_style_text_color(words, lv_color_hex(0xd0a0ff), 0);
    alerts_add(AL_TEST, "Test beep");
    lv_obj_t *close = alerts_add(AL_CLOSE, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(first);
}

static void alerts_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (alerts_list) { /* Alerts > again closes it */
        alerts_close();
        return;
    }
    if (popup_guard()) return;
    if (composing) return;
    alerts_show();
}

/* ---- Speeds (T6) ----------------------------------------------------------- */

/* As desktop JS8Call: everything we send goes at one speed (Normal, Fast,
 * Turbo, Slow, and Ultra with its Setting on), and the receiver decodes
 * every speed at once unless Decode is set to My speed. Turbo and Ultra send
 * no heartbeats or HB acks. */

/* The selected station's speed (the one Reply and the green bar are for,
 * not the row under the cursor: with a station locked they differ, bug
 * hunt 15); else the cursor row's (a message without a call). */
static bool selected_speed(js8_speed_t *out) {
    js8_station_t st;
    if (sel_call[0] && find_station(sel_call, &st)) {
        *out = js8_speed_from_submode(st.submode);
        return true;
    }
    uint16_t row, col;
    lv_table_get_selected_cell(table, &row, &col);
    if (row >= rows || row_hist[row] < 0) return false;
    uint8_t submode = view_stations ? st_rows[row_hist[row]].submode : history[row_hist[row]].submode;
    if (!view_stations && history[row_hist[row]].tx) return false;
    *out = js8_speed_from_submode(submode);
    return true;
}

/* Replying to someone heard on another speed: say so (desktop offers "Jump
 * to Fast speed"; here, hold Speed). */
static void speed_warn(void) {
    js8_speed_t their;
    char        call[JS8_RX_CALL_LEN];
    float       freq;
    int         snr;
    if (!selected_speed(&their) || their == cur_speed() || !selected_station(call, sizeof(call), &freq, &snr)) return;
    msg_update_text_fmt("%s was heard on %s, you send %s - hold Speed (page 6) to match", call, js8_speed_name(their),
                        js8_speed_name(cur_speed()));
}

static void set_speed(js8_speed_t s) {
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    param_i_set(cfg.js8.speed(), (uint8_t)s);
    /* The offset must leave room for the wider signal below 3000 Hz. */
    int max = js8_speed_max_offset_hz(s);
    if (param_i_get(cfg.js8.tx_freq()) > max) param_i_set(cfg.js8.tx_freq(), (uint16_t)max);
    js8_rx_set_qso_offset(rx, param_i_get(cfg.js8.tx_freq()));
    lv_finder_set_width(finder, js8_speed_bandwidth_hz(s));
    finder_shown = -1; /* another width: all of it again */
    finder_sync();
    lv_obj_invalidate(finder);
    if (!param_i_get(cfg.js8.rx_all())) js8_rx_set_submodes(rx, rx_speed_mask());
    /* A heartbeat that fell due while in Turbo or Ultra mustn't go out the moment we
     * leave it: start the interval again. */
    hb_next_ms = 0;
    if (btn_speed.disp_btn) buttons_refresh(&btn_speed);
    update_tx_bar();
    msg_update_text_fmt("Sending %s: %d s slots, %d Hz wide%s", js8_speed_name(s), js8_speed_period_s(s),
                        js8_speed_bandwidth_hz(s), js8_speed_heartbeats(s) ? "" : ", no heartbeats (as desktop)");
    add_info_row("Sending at %s speed", js8_speed_name(s));
}

static const char *speed_label_getter(void) {
    static char label[24];
    snprintf(label, sizeof(label), "Speed:\n%s", js8_speed_name(cur_speed()));
    return label;
}

static void speed_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard()) return;
    int at = 0;
    for (int i = 0; i < JS8_SPEED_COUNT; i++)
        if (speed_order[i] == cur_speed()) at = i;
    js8_speed_t next;
    do next = speed_order[at = (at + 1) % JS8_SPEED_COUNT];
    while (!speed_on_button(next));
    set_speed(next);
}

/* Hold: the selected station's speed. */
static void speed_hold_cb(button_data_t *btn) {
    (void)btn;
    user_touch();
    if (popup_guard()) return;
    js8_speed_t their;
    if (!selected_speed(&their)) {
        msg_update_text_fmt("Select a station first (MFK)");
        return;
    }
    if (their == cur_speed()) {
        msg_update_text_fmt("Already sending %s", js8_speed_name(their));
        return;
    }
    set_speed(their); /* Ultra too, even if it isn't on the button */
}

/* Decode: all speeds or only the one we send at (a Settings line; it was
 * page 6's button before Hold took its place). */
static void decode_toggle(void) {
    param_i_set(cfg.js8.rx_all(), !param_i_get(cfg.js8.rx_all()));
    js8_rx_set_submodes(rx, rx_speed_mask());
    if (param_i_get(cfg.js8.rx_all())) msg_update_text_fmt("Decoding every speed (Normal, Fast, Turbo, Ultra, Slow)");
    else msg_update_text_fmt("Decoding %s only", js8_speed_name(cur_speed()));
}

/* Ultra on the Speed button or not (Settings, experimental). It's decoded
 * either way; switched off while you send Ultra, you stay on it until the
 * next press of Speed. */
static void ultra_toggle(void) {
    bool on = !param_i_get(cfg.js8.ultra());
    param_i_set(cfg.js8.ultra(), on);
    msg_update_text_fmt(on ? "Ultra on the Speed button (page 6): after Turbo"
                           : cur_speed() == JS8_SPEED_ULTRA
                               ? "Ultra off the Speed button: you stay on Ultra until you press Speed"
                               : "Ultra off the Speed button (still decoded; hold Speed on an Ultra station to match)");
}

/* ---- Frequency: JS8Call's, GhostNet's, or your own ---------------------- */

/* The band keys step through JS8Call's usual dial frequencies, or with
 * GhostNet on through GhostNet's (3.575, 7.107, 14.107 MHz). A custom
 * frequency is remembered; the band keys leave it for the preset list. */

typedef enum {
    FQ_JS8,
    FQ_GHOSTNET,
    FQ_CUSTOM,
    FQ_CLOSE,
} freq_item_t;

/* Settings' Frequencies line: which list the band keys step through. */
static const char *freq_name(void) {
    static char buf[32];
    if (param_i_get(cfg.js8.custom_on())) {
        format_khz(param_i_get(cfg.js8.custom_hz()), buf, sizeof(buf));
        return buf;
    }
    return param_i_get(cfg.js8.ghostnet()) ? "GhostNet" : "JS8Call's";
}

static void freq_close(void) {
    if (!freq_list) return;
    lv_obj_del_async(freq_list); /* often called from one of its buttons */
    freq_list = NULL;
    if (table && !composing) {
        lv_group_add_obj(keyboard_group, table);
        lv_group_focus_obj(table);
        lv_group_set_editing(keyboard_group, true);
    }
}

/* To the closest preset of the chosen list. */
static void use_presets(bool ghostnet) {
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return;
    }
    param_i_set(cfg.js8.ghostnet(), ghostnet);
    param_i_set(cfg.js8.custom_on(), false);
    load_band(0);
    retuned();
}

/* From the keyboard: kHz ("7107.5"), or MHz when under 100 ("7.1075").
 * False, with the reason shown, keeps the keyboard open to fix it. */
static bool parse_custom(const char *text, int32_t *out) {
    char  *end;
    double v = strtod(text, &end);
    if (end == text || v <= 0) {
        msg_update_text_fmt("Type the dial frequency in kHz, e.g. 7107");
        return false;
    }
    int32_t hz = (int32_t)(v < 100 ? v * 1e6 + 0.5 : v * 1e3 + 0.5);
    if (hz < CUSTOM_MIN_HZ || hz > CUSTOM_MAX_HZ) {
        msg_update_text_fmt("Out of range: 1800 - 54000 kHz");
        return false;
    }
    if (js8_tx_busy(tx)) {
        msg_update_text_fmt("Not while sending - Stop TX first");
        return false;
    }
    if (out) *out = hz;
    return true;
}

static void tune_custom(const char *text) {
    int32_t hz;
    if (!parse_custom(text, &hz)) return;
    param_i_set(cfg.js8.custom_hz(), hz);
    param_i_set(cfg.js8.custom_on(), true);
    cparam_i_set(cfg.cur.fg_freq(), hz);
    js8_usb_dig();
    retuned();
    msg_update_text_fmt("%s", where_label());
}

static void freq_item_cb(lv_event_t *e) {
    freq_item_t item = (freq_item_t)(intptr_t)lv_event_get_user_data(e);
    switch (item) {
    case FQ_JS8:
    case FQ_GHOSTNET:
        freq_close();
        use_presets(item == FQ_GHOSTNET);
        return;
    case FQ_CUSTOM: {
        /* Into the keyboard with the last one filled in. */
        char khz[16] = "";
        if (param_i_get(cfg.js8.custom_hz())) format_khz(param_i_get(cfg.js8.custom_hz()), khz, sizeof(khz));
        if (popup_to_keyboard(&freq_list, EDIT_FREQ, khz)) msg_update_text_fmt("Dial frequency in kHz, then Enter");
        return;
    }
    case FQ_CLOSE:
        freq_close();
        return;
    }
}

static void freq_key_cb(lv_event_t *e) {
    popup_key(e, freq_close);
}

static lv_obj_t *freq_add(freq_item_t item, const char *label) {
    lv_obj_t *b = list_add_item(freq_list, label);
    lv_obj_add_event_cb(b, freq_item_cb, LV_EVENT_CLICKED, (void *)(intptr_t)item);
    lv_obj_add_event_cb(b, freq_key_cb, LV_EVENT_KEY, NULL);
    lv_group_add_obj(keyboard_group, b);
    lv_label_set_long_mode(lv_obj_get_child(b, 0), LV_LABEL_LONG_DOT);
    return b;
}

static void freq_show(void) {
    lv_group_remove_obj(table);
    freq_list = lv_list_create(dialog.obj);
    lv_obj_set_size(freq_list, 560, WF_HEIGHT - 10);
    lv_obj_align(freq_list, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_set_style_text_font(freq_list, &sony_24, 0);
    lv_obj_set_style_bg_color(freq_list, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_color(freq_list, lv_color_white(), 0);
    char line[64];
    if (param_i_get(cfg.js8.custom_on())) {
        char khz[16];
        format_khz(param_i_get(cfg.js8.custom_hz()), khz, sizeof(khz));
        snprintf(line, sizeof(line), "Now: JS8 Custom, %s kHz", khz);
    } else {
        snprintf(line, sizeof(line), "Now: %s", where_label());
    }
    lv_obj_t *t = lv_list_add_text(freq_list, line);
    lv_obj_set_style_text_font(t, &sony_22, 0);

    bool      custom = param_i_get(cfg.js8.custom_on()), ghost = param_i_get(cfg.js8.ghostnet());
    lv_obj_t *js8    = freq_add(FQ_JS8, "JS8Call frequencies (7.078, 14.078...)");
    lv_obj_t *gn     = freq_add(FQ_GHOSTNET, "GhostNet (3.575, 7.107, 14.107)");
    char      khz[16];
    if (param_i_get(cfg.js8.custom_hz())) {
        format_khz(param_i_get(cfg.js8.custom_hz()), khz, sizeof(khz));
        snprintf(line, sizeof(line), "Custom kHz... (last %s)", khz);
    } else {
        snprintf(line, sizeof(line), "Custom kHz...");
    }
    lv_obj_t *cu    = freq_add(FQ_CUSTOM, line);
    lv_obj_t *close = freq_add(FQ_CLOSE, "Close");
    lv_obj_set_style_text_color(close, lv_color_hex(0xffc040), 0);
    /* The one in use stands out and has the knob. */
    lv_obj_t *cur = custom ? cu : ghost ? gn : js8;
    lv_obj_set_style_text_color(cur, lv_color_hex(0x60ff60), 0);
    lv_group_set_editing(keyboard_group, false);
    lv_group_focus_obj(cur);
}

/* For tools/js8_ui_harness: the Frequencies setting as Settings shows it. */
const char *dialog_js8_freq_name(void) {
    return freq_name();
}
