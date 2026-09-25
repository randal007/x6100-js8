/* C side of the harness: everything that touches firmware headers. */
#include "buttons.h"
#include "dialog.h"
#include "dialog_js8.h"
#include "events.h"
#include "keyboard.h"
#include "params/params.h"
#include "styles.h"

#include <string.h>

extern buttons_page_t *stub_page;

void ui_init(void) {
    EVENT_BAND_UP   = lv_event_register_id();
    EVENT_BAND_DOWN = lv_event_register_id();
    styles_init(THEME_SIMPLE);
    lv_obj_set_style_bg_color(lv_scr_act(), bg_color, 0);
    keyboard_group = lv_group_create();
    strcpy(params.callsign.x, "K2XYZ");
}
void ui_open(void) { dialog_construct(dialog_js8, lv_scr_act()); }
void ui_press(int i) { button_data_t *b = stub_page->items[i]; b->press(b); }
void ui_band_up(void) { lv_event_send(dialog_js8->obj, (lv_event_code_t)EVENT_BAND_UP, NULL); }
void ui_key(uint32_t key) { lv_event_send(lv_group_get_focused(keyboard_group), LV_EVENT_KEY, &key); }
int  ui_running(void) { return dialog_js8->run; }

#include "textarea_window.h"

void ui_compose_append(const char *text) { lv_textarea_add_text(textarea_window_text(), text); }
const char *ui_compose_text(void) { return textarea_window_get(); }
void ui_compose_enter(void) {
    uint32_t key = LV_KEY_ENTER;
    lv_event_send(textarea_window_text(), LV_EVENT_KEY, &key);
}
void ui_select_row_from(const char *call) {
    /* Step the MFK up from the newest row until the selected text starts with call. */
    lv_obj_t *table = lv_group_get_focused(keyboard_group);
    for (int i = 0; i < 20; i++) {
        uint16_t row, col;
        lv_table_get_selected_cell(table, &row, &col);
        const char *v = lv_table_get_cell_value(table, row, 0);
        if (v && strstr(v, call)) return;
        ui_key(LV_KEY_LEFT);
    }
}
