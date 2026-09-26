/* C side of the harness: everything that touches firmware headers. */
#include "buttons.h"
#include "dialog.h"
#include "dialog_js8.h"
#include "events.h"
#include "keyboard.h"
#include "params/params.h"
#include "styles.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern buttons_page_t *stub_page;

void ui_init(void) {
    EVENT_BAND_UP   = lv_event_register_id();
    EVENT_BAND_DOWN = lv_event_register_id();
    styles_init(THEME_SIMPLE);
    lv_obj_set_style_bg_color(lv_scr_act(), bg_color, 0);
    keyboard_group = lv_group_create();
    strcpy(params.callsign.x, "K2XYZ");
    strcpy(params.qth.x, "FN42AB");
    params.js8_hold_offset.x = false; /* the firmware defaults */
    params.js8_hb_interval.x = 30;
    params.js8_log_prompt.x  = true;
    params.js8_log_activation.x = 0;
    params.js8_alerts.x = 7;
    params.js8_speed.x  = 0;
    params.js8_rx_all.x = true;
}
void ui_open(void) { dialog_construct(dialog_js8, lv_scr_act()); }
void ui_press(int i) {
    button_data_t *b = stub_page->items[i];
    if (!b) {
        printf("[harness] no button %d on this page\n", i);
        return;
    }
    b->press(b);
}
const char *ui_focus_desc(void) {
    lv_obj_t *f = lv_group_get_focused(keyboard_group);
    if (!f) return "nothing";
    if (lv_obj_check_type(f, &lv_keyboard_class)) return lv_group_get_editing(keyboard_group) ? "keyboard (editing)" : "keyboard";
    if (lv_obj_check_type(f, &lv_textarea_class)) return "textarea";
    if (lv_obj_check_type(f, &lv_table_class)) return "message list";
    if (lv_obj_check_type(f, &lv_btn_class)) return "a list button";
    return "something else";
}
/* Is `text` in any row of the dialog's list? */
int ui_list_has(const char *text) {
    lv_obj_t *t = lv_group_get_focused(keyboard_group);
    if (!t || !lv_obj_check_type(t, &lv_table_class)) return -1;
    for (uint16_t r = 0; r < lv_table_get_row_cnt(t); r++) {
        const char *v = lv_table_get_cell_value(t, r, 0);
        if (v && strstr(v, text)) return 1;
    }
    return 0;
}
/* How many rows of the dialog's list contain `text`. */
int ui_list_count(const char *text) {
    lv_obj_t *t = lv_group_get_focused(keyboard_group);
    if (!t || !lv_obj_check_type(t, &lv_table_class)) return -1;
    int n = 0;
    for (uint16_t r = 0; r < lv_table_get_row_cnt(t); r++) {
        const char *v = lv_table_get_cell_value(t, r, 0);
        if (v && strstr(v, text)) n++;
    }
    return n;
}
/* Text of the focused list item, or "" */
const char *ui_focused_text(void) {
    lv_obj_t *f = lv_group_get_focused(keyboard_group);
    if (!f || !lv_obj_check_type(f, &lv_list_btn_class)) return "";
    return lv_list_get_btn_text(lv_obj_get_parent(f), f);
}
void ui_rotary(int32_t diff) { dialog_js8->rotary_cb(diff); }
/* Does any item or title in the focused list contain `text`? */
int ui_popup_has(const char *text) {
    lv_obj_t *f = lv_group_get_focused(keyboard_group);
    if (!f || !lv_obj_check_type(f, &lv_list_btn_class)) return -1;
    lv_obj_t *list = lv_obj_get_parent(f);
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(list); i++) {
        lv_obj_t  *c = lv_obj_get_child(list, i);
        lv_obj_t  *l = lv_obj_has_class(c, &lv_label_class) ? c : lv_obj_get_child(c, 0);
        const char *t = l && lv_obj_has_class(l, &lv_label_class) ? lv_label_get_text(l) : NULL;
        if (t && strstr(t, text)) return 1;
    }
    return 0;
}
/* Long-press of a bottom button. */
void ui_hold(int i) {
    button_data_t *b = stub_page->items[i];
    if (b && b->hold) b->hold(b);
}
const char *ui_button_label(int i) {
    button_data_t *b = stub_page->items[i];
    return b->type == BTN_TEXT_FN ? b->label_fn() : b->label;
}
void ui_band_up(void) { lv_event_send(dialog_js8->obj, (lv_event_code_t)EVENT_BAND_UP, NULL); }
void ui_key(uint32_t key) { lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_KEY, &key); }
int  ui_running(void) { return dialog_js8->run; }
int  ui_focus_is_table(void) {
    lv_obj_t *f = lv_group_get_focused(keyboard_group);
    return f && lv_obj_check_type(f, &lv_table_class);
}

#include "textarea_window.h"

void ui_compose_append(const char *text) { lv_textarea_add_text(textarea_window_text(), text); }
void ui_compose_clear(void) { lv_textarea_set_text(textarea_window_text(), ""); }
const char *ui_compose_text(void) {
    /* textarea_window keeps its pointer after closing; don't read a dead one. */
    lv_obj_t *t = textarea_window_text();
    return (t && lv_obj_is_valid(t)) ? textarea_window_get() : "(no compose window)";
}
void ui_compose_enter(void) {
    uint32_t key = LV_KEY_ENTER;
    lv_event_send(textarea_window_text(), LV_EVENT_KEY, &key);
}
/* ESC as the text box sees it (the on-screen keyboard has the focus). */
void ui_compose_cancel(void) {
    uint32_t key = LV_KEY_ESC;
    lv_event_send(textarea_window_text(), LV_EVENT_KEY, &key);
}
/* Move the selection with MFK steps to the row for `call`, searching down
 * from the top. Station rows are drawn, not stored in the cell, so ask the
 * dialog what's selected. */
bool dialog_js8_selected_call(char *call, unsigned len); /* test hook */
void ui_select_row_from(const char *call) {
    for (int i = 0; i < 60; i++) ui_key(LV_KEY_LEFT); /* to the top */
    for (int i = 0; i < 60; i++) {
        char sel[32];
        if (dialog_js8_selected_call(sel, sizeof(sel)) && strcmp(sel, call) == 0) return;
        ui_key(LV_KEY_RIGHT);
    }
    printf("[harness] could not select %s\n", call);
}
void ui_click_focused(void) { lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_CLICKED, NULL); }
/* Press the page button until page n ("(JS8 n:4)") is showing. */
void ui_page(int n) {
    char want[16];
    snprintf(want, sizeof(want), "(JS8 %d:", n);
    for (int i = 0; i < 8; i++) {
        if (stub_page && stub_page->items[0] && strncmp(stub_page->items[0]->label, want, strlen(want)) == 0) return;
        ui_press(0);
    }
    printf("[harness] could not reach page %d\n", n);
}

/* Real input devices, as on the radio: the MFK knob is an LVGL encoder
 * (turn + press) and ESC is the VOL knob's press on a keypad (keypad.c),
 * both in keyboard_group. Unlike ui_key() these go through LVGL's indev
 * code, so press and release are separate events, as on the radio. */
static int32_t  mfk_diff;
static bool     mfk_down;
static uint32_t kp_key = LV_KEY_ESC;
static bool     kp_down;

static void mfk_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    data->enc_diff = (int16_t)mfk_diff;
    mfk_diff       = 0;
    data->state    = mfk_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void kp_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    data->key   = kp_key;
    data->state = kp_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void ui_indevs_init(void) {
    static lv_indev_drv_t mfk, kp;
    lv_indev_drv_init(&mfk);
    mfk.type    = LV_INDEV_TYPE_ENCODER;
    mfk.read_cb = mfk_read;
    lv_indev_set_group(lv_indev_drv_register(&mfk), keyboard_group);
    lv_indev_drv_init(&kp);
    kp.type            = LV_INDEV_TYPE_KEYPAD;
    kp.read_cb         = kp_read;
    kp.long_press_time = 1000; /* keypad.c */
    lv_indev_set_group(lv_indev_drv_register(&kp), keyboard_group);
}
void ui_mfk_turn(int32_t diff) { mfk_diff += diff; }
void ui_mfk_set(bool down) { mfk_down = down; }
void ui_keypad_set(uint32_t key, bool down) {
    kp_key  = key;
    kp_down = down;
}
/* Put the on-screen keyboard's cursor on its OK (tick) key, as turning
 * MFK would. Returns 0 if there's no keyboard. */
int ui_kb_select_ok(void) {
    lv_obj_t *f = lv_group_get_focused(keyboard_group);
    if (!f || !lv_obj_check_type(f, &lv_keyboard_class)) return 0;
    for (uint16_t i = 0; i < 64; i++) {
        const char *t = lv_btnmatrix_get_btn_text(f, i);
        if (t && strcmp(t, LV_SYMBOL_OK) == 0) {
            lv_btnmatrix_set_selected_btn(f, i);
            return 1;
        }
    }
    return 0;
}

/* A USB keyboard through the firmware's kbd_rollover filter, fed from a
 * script of evdev-like events. */
#include "kbd_rollover.h"

typedef struct {
    uint16_t code;
    uint32_t key;
    int      value;
} usb_ev_t;
static usb_ev_t       usb_q[512];
static unsigned       usb_head, usb_tail;
static kbd_rollover_t usb_ro;

static bool usb_next(void *ctx, uint16_t *code, uint32_t *key, int *value) {
    (void)ctx;
    if (usb_head == usb_tail) return false;
    usb_ev_t e = usb_q[usb_head++ % 512];
    *code      = e.code;
    *key       = e.key;
    *value     = e.value;
    return true;
}
static void usb_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    if (getenv("USB_NAIVE")) { /* the old evdev_read: every event as it comes */
        static uint32_t last_key;
        static bool     last_down;
        uint16_t        code;
        uint32_t        key;
        int             value;
        if (usb_next(NULL, &code, &key, &value)) {
            last_key               = key;
            last_down              = value != 0;
            data->continue_reading = true;
        }
        data->key   = last_key;
        data->state = last_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        return;
    }
    kbd_rollover_read(&usb_ro, usb_next, NULL, data);
}
void ui_usb_init(void) {
    static lv_indev_drv_t d;
    lv_indev_drv_init(&d);
    d.type    = LV_INDEV_TYPE_KEYPAD;
    d.read_cb = usb_read;
    lv_indev_set_group(lv_indev_drv_register(&d), keyboard_group);
}
/* Queue one event; the scancode is the character itself here. */
void ui_usb_event(uint32_t key, int value) { usb_q[usb_tail++ % 512] = (usb_ev_t){(uint16_t)key, key, value}; }
