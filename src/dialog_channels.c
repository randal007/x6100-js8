/*
 * Channel Memory Manager
 */

#include "dialog_channels.h"
#include "channels.h"
#include "keyboard.h"
#include "voice.h"
#include "textarea_window.h"
#include "cfg/cfg_api.h"
#include "styles.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void close_cb(button_data_t *data);
static void add_cb(button_data_t *data);
static void delete_cb(button_data_t *data);
static void edit_cb(button_data_t *data);
static void drag_cb(button_data_t *data);
static void drop_cb(button_data_t *data);
static bool edit_ok_cb(void);
static bool edit_cancel_cb(void);
static void rebuild_after_edit(void *data);
static void channel_pressed_cb(lv_event_t *e);
static void channel_changed_cb(lv_event_t *e);
static void channel_draw_cb(lv_event_t *e);
static const char *mode_to_string(x6100_mode_t mode);
static void refresh_table(void);
static void build_table(void);
static void select_row(int16_t row, bool center);
static bool recall_row(int16_t row, bool close_dialog);

static lv_obj_t *table = NULL;
static lv_obj_t *channels_frame = NULL;


/*
 * Last channel actually recalled with MFK.
 *
 * This is intentionally kept in RAM only:
 * reopening the dialog restores the selection,
 * while restarting x6100-gui starts fresh.
 */
static int16_t last_recalled_row = LV_TABLE_CELL_NONE;


/*
 * Channel currently being edited.
 */
static int16_t edit_row = LV_TABLE_CELL_NONE;
static int16_t edit_restore_row = LV_TABLE_CELL_NONE;


/*
 * Drag & Drop state.
 */
static bool drag_active = false;

/* Add a new channel immediately after the dialog is opened. */
static bool add_on_construct = false;
static int16_t drag_row = LV_TABLE_CELL_NONE;


/*
 * Soft-key F1: Close
 */
static button_data_t btn_close = {
    .type = BTN_TEXT,
    .label = "Close",
    .press = close_cb
};


/*
 * Soft-key F2: Add
 */
static button_data_t btn_add = {
    .type = BTN_TEXT,
    .label = "Add",
    .press = add_cb
};


/*
 * Soft-key F3: Delete
 */
static button_data_t btn_delete = {
    .type = BTN_TEXT,
    .label = "Delete",
    .press = delete_cb
};


/*
 * Soft-key F4: Edit
 */
static button_data_t btn_edit = {
    .type = BTN_TEXT,
    .label = "Edit",
    .press = edit_cb
};


/*
 * Soft-key F5: Drag
 */
static button_data_t btn_drag = {
    .type = BTN_TEXT,
    .label = "Drag",
    .press = drag_cb
};


/*
 * Soft-key F5 while dragging: Drop
 */
static button_data_t btn_drop = {
    .type = BTN_TEXT,
    .label = "Drop",
    .press = drop_cb
};


static buttons_page_t btn_page = {
    .items = {
        &btn_close,
        &btn_add,
        &btn_delete,
        &btn_edit,
        &btn_drag
    }
};


static dialog_t dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = NULL,
    .key_cb = NULL,
    .btn_page = &btn_page
};


dialog_t *dialog_channels = &dialog;


/*
 * Convert mode enum to text for display.
 */
static const char *mode_to_string(x6100_mode_t mode) {

    switch (mode) {

        case x6100_mode_lsb:
            return "LSB";

        case x6100_mode_usb:
            return "USB";

        case x6100_mode_lsb_dig:
            return "LSB-D";

        case x6100_mode_usb_dig:
            return "USB-D";

        case x6100_mode_cw:
            return "CW";

        case x6100_mode_cwr:
            return "CWR";

        case x6100_mode_am:
            return "AM";

        case x6100_mode_nfm:
            return "NFM";

        default:
            return "?";
    }
}


/*
 * Rebuild channel list from current database.
 */
static void refresh_table(void) {

    if (!table) {
        return;
    }


    uint16_t count = channels_count();


    /*
     * The table intentionally contains only one
     * logical column.
     *
     * Each cell contains a single blank character:
     * the visible row is drawn by channel_draw_cb().
     * Keeping one real LVGL cell per channel preserves
     * the existing MFK navigation and Drag/Drop logic.
     */
    for (uint16_t i = 0; i < count; i++) {

        lv_table_set_cell_value(
            table,
            i,
            0,
            " "
        );
    }


    lv_table_set_row_cnt(
        table,
        count
    );
}


/*
 * Custom row drawing.
 *
 * The LVGL table still has exactly one selectable cell
 * per channel, but the visible contents are drawn as
 * three independent fields:
 *
 *   name       left aligned
 *   frequency  right aligned
 *   mode       right aligned
 *
 * This gives pixel-accurate alignment without changing
 * encoder navigation or Drag/Drop behaviour.
 */
static void channel_draw_cb(lv_event_t *e) {

    lv_obj_draw_part_dsc_t *dsc =
        lv_event_get_draw_part_dsc(e);


    if (!dsc ||
        dsc->part != LV_PART_ITEMS) {

        return;
    }


    lv_obj_t *obj =
        lv_event_get_target(e);


    uint32_t col_count =
        lv_table_get_col_cnt(obj);


    if (col_count == 0) {
        return;
    }


    uint32_t row =
        dsc->id / col_count;


    if (row >= channels_count()) {
        return;
    }


    const channel_t *channel =
        channels_get((uint16_t)row);


    if (!channel) {
        return;
    }


    /*
     * LVGL has already drawn the cell background and
     * selection state.  The normal cell text is only a
     * blank character, so we now draw the real contents.
     */
    lv_draw_label_dsc_t label_dsc =
        *dsc->label_dsc;


    label_dsc.opa = LV_OPA_COVER;


    const lv_coord_t pad_top =
        lv_obj_get_style_pad_top(
            obj,
            LV_PART_ITEMS
        );


    const lv_coord_t pad_bottom =
        lv_obj_get_style_pad_bottom(
            obj,
            LV_PART_ITEMS
        );


    const lv_coord_t pad_left =
        lv_obj_get_style_pad_left(
            obj,
            LV_PART_ITEMS
        );


    lv_area_t area;


    area.y1 =
        dsc->draw_area->y1 + pad_top;

    area.y2 =
        dsc->draw_area->y2 - pad_bottom;


    /*
     * Channel name.
     * Leave enough room on the right for frequency
     * and mode so a long name cannot overlap them.
     */
    area.x1 =
        dsc->draw_area->x1 + pad_left;

    area.x2 =
        dsc->draw_area->x2 - 340;


    label_dsc.align =
        LV_TEXT_ALIGN_LEFT;


    lv_draw_label(
        dsc->draw_ctx,
        &label_dsc,
        &area,
        channel->name,
        NULL
    );


    /*
     * Frequency.
     */
    char freq[24];


    uint32_t mhz =
        channel->freq / 1000000;

    uint32_t khz =
        (channel->freq / 1000) % 1000;

    uint32_t hz =
        channel->freq % 1000;


    snprintf(
        freq,
        sizeof(freq),
        "%u.%03u.%03u",
        mhz,
        khz,
        hz
    );


    area.x1 =
        dsc->draw_area->x2 - 335;

    area.x2 =
        dsc->draw_area->x2 - 110;


    label_dsc.align =
        LV_TEXT_ALIGN_RIGHT;


    lv_draw_label(
        dsc->draw_ctx,
        &label_dsc,
        &area,
        freq,
        NULL
    );


    /*
     * Operating mode.
     */
    area.x1 =
        dsc->draw_area->x2 - 105;

    area.x2 =
        dsc->draw_area->x2 - 12;


    label_dsc.align =
        LV_TEXT_ALIGN_RIGHT;


    lv_draw_label(
        dsc->draw_ctx,
        &label_dsc,
        &area,
        mode_to_string(channel->mode),
        NULL
    );
}


/*
 * Create the channel table from scratch.
 */
static void build_table(void) {

    /*
     * Remove old table if present.
     */
    if (table) {

        lv_group_remove_obj(
            table
        );

        lv_obj_del(
            table
        );

        table = NULL;
    }


    /*
     * Create new table.
     */
    table = lv_table_create(
        dialog.obj
    );


    lv_obj_set_style_border_width(
        table,
        0,
        LV_PART_MAIN
    );


    lv_obj_set_size(
        table,
        700,
        270
    );


    lv_obj_align(
        table,
        LV_ALIGN_TOP_MID,
        0,
        50
    );


    /*
     * One selectable item per row.
     */
    lv_table_set_col_cnt(
        table,
        1
    );


    lv_table_set_col_width(
        table,
        0,
        690
    );


    /*
     * Table background.
     */
    lv_obj_set_style_bg_color(
        table,
        lv_color_hex(0x27313a),
        LV_PART_MAIN
    );


    lv_obj_set_style_bg_opa(
        table,
        LV_OPA_COVER,
        LV_PART_MAIN
    );


    /*
     * Normal channels:
     * white text.
     */
    lv_obj_set_style_border_width(
        table,
        0,
        LV_PART_ITEMS
    );


    lv_obj_set_style_bg_opa(
        table,
        LV_OPA_TRANSP,
        LV_PART_ITEMS
    );


    lv_obj_set_style_text_color(
        table,
        lv_color_white(),
        LV_PART_ITEMS
    );


    lv_obj_set_style_text_font(
        table,
        &sony_24,
        LV_PART_ITEMS
    );


    /*
     * Row spacing.
     */
    lv_obj_set_style_pad_top(
        table,
        5,
        LV_PART_ITEMS
    );


    lv_obj_set_style_pad_bottom(
        table,
        5,
        LV_PART_ITEMS
    );


    lv_obj_set_style_pad_left(
        table,
        12,
        LV_PART_ITEMS
    );


    /*
     * Selected channel:
     * grey text, same background.
     */
    lv_obj_set_style_text_color(
        table,
        lv_color_hex(0x808080),
        LV_PART_ITEMS | LV_STATE_EDITED
    );


    lv_obj_set_style_bg_opa(
        table,
        LV_OPA_TRANSP,
        LV_PART_ITEMS | LV_STATE_EDITED
    );


    /*
     * Populate table AFTER styles have
     * been applied.
     */
    refresh_table();


    /*
     * MFK PRESS:
     * activate selected channel.
     */
    lv_obj_add_event_cb(
        table,
        channel_pressed_cb,
        LV_EVENT_PRESSED,
        NULL
    );


    /*
     * MFK rotation changes the selected row.
     *
     * During Drag mode this event is used
     * to physically move the selected channel
     * inside the in-memory database.
     */
    lv_obj_add_event_cb(
        table,
        channel_changed_cb,
        LV_EVENT_VALUE_CHANGED,
        NULL
    );


    /*
     * Draw name, frequency and mode independently
     * while keeping a single logical table column.
     */
    lv_obj_add_event_cb(
        table,
        channel_draw_cb,
        LV_EVENT_DRAW_PART_END,
        NULL
    );


    /*
     * Give the table to the MFK encoder.
     */
    lv_group_add_obj(
        keyboard_group,
        table
    );


    lv_group_focus_obj(
        table
    );


    lv_group_set_editing(
        keyboard_group,
        true
    );
}



/*
 * Select a table row using the same LVGL encoder
 * mechanism used by the MFK.
 *
 * This avoids relying on lv_table_set_selected_cell(),
 * which is not available in the LVGL version used
 * by this firmware.
 *
 * If center is true, try to place the selected row
 * approximately in the middle of the visible table.
 */
static void select_row(int16_t row, bool center) {

    if (!table ||
        row < 0 ||
        row >= channels_count()) {

        return;
    }


    /*
     * build_table() leaves the one-column table focused
     * with the first row selected. Move down until the
     * requested row is reached.
     */
    for (int16_t i = 0; i < row; i++) {

        lv_group_send_data(
            keyboard_group,
            LV_KEY_DOWN
        );
    }


    /*
     * Let LVGL calculate the final row geometry before
     * applying our optional centering scroll.
     */
    lv_obj_update_layout(
        table
    );


    if (center) {

        const lv_font_t *font =
            lv_obj_get_style_text_font(
                table,
                LV_PART_ITEMS
            );


        lv_coord_t row_height =
            lv_font_get_line_height(font) +
            lv_obj_get_style_pad_top(
                table,
                LV_PART_ITEMS
            ) +
            lv_obj_get_style_pad_bottom(
                table,
                LV_PART_ITEMS
            );


        lv_coord_t visible_height =
            lv_obj_get_content_height(
                table
            );


        lv_coord_t target_y =
            (row * row_height) +
            (row_height / 2) -
            (visible_height / 2);


        if (target_y < 0) {
            target_y = 0;
        }


        lv_obj_scroll_to_y(
            table,
            target_y,
            LV_ANIM_OFF
        );
    }
}


/*
 * Dialog destruction callback.
 */
static void destruct_cb(void) {

    table = NULL;
    channels_frame = NULL;

    edit_row = LV_TABLE_CELL_NONE;

    drag_active = false;
    drag_row = LV_TABLE_CELL_NONE;


    /*
     * Restore F5 for the next opening
     * of the dialog.
     */
    btn_page.items[4] =
        &btn_drag;
}


/*
 * F1 - Close
 */
static void close_cb(button_data_t *data) {

    dialog_destruct();
}


/*
 * F2 - Add
 */
static void add_cb(button_data_t *data) {

    /*
     * Database modifications are disabled
     * while a channel is being dragged.
     */
    if (drag_active) {
        return;
    }


    if (!channels_add_current()) {

        voice_say_text_fmt(
            "Unable to add channel"
        );

        return;
    }


    /*
     * Recreate the table completely.
     */
    build_table();


    /*
     * Select the newly added channel.
     *
     * channels_add_current() appends it to the end,
     * so the new row is count - 1.
     */
    int16_t new_row =
        (int16_t)channels_count() - 1;


    select_row(
        new_row,
        false
    );


    /*
     * Keep the existing ADD behaviour:
     * show the bottom of the list.
     */
    lv_obj_scroll_to_y(
        table,
        LV_COORD_MAX,
        LV_ANIM_OFF
    );


    voice_say_text_fmt(
        "Channel added"
    );
}


/*
 * F3 - Delete
 */
static void delete_cb(button_data_t *data) {

    if (drag_active) {
        return;
    }


    if (!table) {
        return;
    }


    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;


    lv_table_get_selected_cell(
        table,
        &row,
        &col
    );


    if (row == LV_TABLE_CELL_NONE) {
        return;
    }


    if (row < 0 ||
        row >= channels_count()) {

        return;
    }


    int16_t select_after_delete =
    (row > 0) ? row - 1 : 0;


    if (!channels_delete((uint16_t)row)) {

        voice_say_text_fmt(
            "Unable to delete channel"
        );

        return;
    }


    /*
     * Keep the remembered recalled channel coherent
     * with the compacted database.
     */
    if (last_recalled_row == row) {

        last_recalled_row =
            LV_TABLE_CELL_NONE;

    } else if (last_recalled_row != LV_TABLE_CELL_NONE &&
               last_recalled_row > row) {

        last_recalled_row--;
    }


    /*
     * Recreate complete table so there are
     * no empty rows and spacing stays correct.
     */
    build_table();

    if (channels_count() > 0) {

    if (select_after_delete >= channels_count()) {
        select_after_delete =
            (int16_t)channels_count() - 1;
    }

    select_row(
        select_after_delete,
        true
    );
}

    voice_say_text_fmt(
        "Channel deleted"
    );
}


/*
 * F4 - Edit
 */
static void edit_cb(button_data_t *data) {

    if (drag_active) {
        return;
    }


    if (!table) {
        return;
    }


    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;


    /*
     * Get selected channel BEFORE opening
     * the text-entry window.
     */
    lv_table_get_selected_cell(
        table,
        &row,
        &col
    );


    if (row == LV_TABLE_CELL_NONE) {
        return;
    }


    if (row < 0 ||
        row >= channels_count()) {

        return;
    }


    const channel_t *channel =
        channels_get((uint16_t)row);


    if (!channel) {
        return;
    }


    /*
     * Remember which database record
     * is being edited.
     */
    edit_row = row;


    /*
     * Temporarily remove the table from
     * the MFK encoder group.
     */
    lv_group_remove_obj(
        table
    );


    /*
     * Open standard R1CBU keyboard.
     */
    textarea_window_open(
        edit_ok_cb,
        edit_cancel_cb
    );


    /*
     * Channel names are limited by
     * CHANNEL_NAME_MAX.
     */
    lv_textarea_set_max_length(
        textarea_window_text(),
        CHANNEL_NAME_MAX - 1
    );


    lv_textarea_set_placeholder_text(
        textarea_window_text(),
        "Channel name"
    );


    /*
     * Load current name into editor.
     */
    textarea_window_set(
        channel->name
    );
}


/*
 * F5 - Drag
 *
 * Start moving the currently selected channel.
 */
static void drag_cb(button_data_t *data) {

    if (!table ||
        channels_count() == 0) {

        return;
    }


    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;


    lv_table_get_selected_cell(
        table,
        &row,
        &col
    );


    if (row == LV_TABLE_CELL_NONE ||
        row < 0 ||
        row >= channels_count()) {

        return;
    }


    /*
     * Remember current position.
     */
    drag_row = row;
    drag_active = true;


    /*
     * Change F5 from Drag to Drop.
     */
    btn_page.items[4] =
        &btn_drop;

    buttons_load(
        4,
        &btn_drop
    );


    voice_say_text_fmt(
        "Drag channel"
    );
}


/*
 * F5 - Drop
 *
 * Save the final in-memory order to JSON.
 */
static void drop_cb(button_data_t *data) {

    if (!drag_active) {
        return;
    }


    /*
     * Write the final channel order.
     */
    if (!channels_save()) {

        /*
         * Saving failed.
         *
         * Reload the last valid database
         * from disk so RAM and JSON remain
         * consistent.
         */
        channels_load();

        drag_active = false;
        drag_row = LV_TABLE_CELL_NONE;


        btn_page.items[4] =
            &btn_drag;

        buttons_load(
            4,
            &btn_drag
        );


        build_table();


        voice_say_text_fmt(
            "Unable to save channel order"
        );

        return;
    }


    /*
     * Drag operation completed.
     */
    drag_active = false;
    drag_row = LV_TABLE_CELL_NONE;


    /*
     * Restore normal F5 button.
     */
    btn_page.items[4] =
        &btn_drag;

    buttons_load(
        4,
        &btn_drag
    );


    voice_say_text_fmt(
        "Channel moved"
    );
}


/*
 * Called asynchronously after the Edit
 * callback has finished.
 */
static void rebuild_after_edit(void *data) {

    if (!dialog.run ||
        !dialog.obj) {

        edit_restore_row = LV_TABLE_CELL_NONE;
        return;
    }


    build_table();


    /*
     * Restore selection to the channel
     * that has just been edited.
     */
    if (edit_restore_row != LV_TABLE_CELL_NONE &&
        edit_restore_row >= 0 &&
        edit_restore_row < channels_count()) {

        select_row(
            edit_restore_row,
            true
        );
    }


    edit_restore_row = LV_TABLE_CELL_NONE;
}

/*
 * Edit keyboard - OK
 */
static bool edit_ok_cb(void) {

    if (edit_row == LV_TABLE_CELL_NONE ||
        edit_row < 0 ||
        edit_row >= channels_count()) {

        edit_row = LV_TABLE_CELL_NONE;


        lv_group_add_obj(
            keyboard_group,
            table
        );


        lv_group_focus_obj(
            table
        );


        lv_group_set_editing(
            keyboard_group,
            true
        );


        return true;
    }


    const char *name =
        textarea_window_get();


    /*
     * Update channel name and JSON database.
     */
    if (!channels_set_name(
            (uint16_t)edit_row,
            name)) {

        voice_say_text_fmt(
            "Unable to save channel"
        );

        return false;
    }


    /*
     * Remember the edited row so it can be
     * restored after rebuilding the table.
     */

    edit_restore_row = edit_row;
    edit_row = LV_TABLE_CELL_NONE;

    /*
     * Rebuild after textarea_window has
     * completed processing its OK event.
     */
    lv_async_call(
        rebuild_after_edit,
        NULL
    );


    voice_say_text_fmt(
        "Channel saved"
    );


    return true;
}


/*
 * Edit keyboard - Cancel
 */
static bool edit_cancel_cb(void) {

    edit_row = LV_TABLE_CELL_NONE;


    /*
     * Give control back to channel table.
     */
    lv_group_add_obj(
        keyboard_group,
        table
    );


    lv_group_focus_obj(
        table
    );


    lv_group_set_editing(
        keyboard_group,
        true
    );


    return true;
}


/*
 * Selected table row changed.
 *
 * Normally LVGL simply changes the selection.
 *
 * During Drag mode, however, the newly selected
 * row becomes the new position of the channel
 * being dragged.
 */
static void channel_changed_cb(lv_event_t *e) {

    if (!drag_active ||
        !table) {

        return;
    }


    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;


    lv_table_get_selected_cell(
        table,
        &row,
        &col
    );


    if (row == LV_TABLE_CELL_NONE ||
        row < 0 ||
        row >= channels_count()) {

        return;
    }


    /*
     * No actual movement.
     */
    if (row == drag_row) {
        return;
    }


    /*
     * Move the dragged database record
     * to the newly selected position.
     *
     * This modifies RAM only.
     */
    int16_t old_drag_row =
        drag_row;


    if (!channels_move(
            (uint16_t)old_drag_row,
            (uint16_t)row)) {

        return;
    }


    /*
     * Keep last_recalled_row attached to the same
     * logical channel while records are reordered.
     */
    if (last_recalled_row != LV_TABLE_CELL_NONE) {

        if (last_recalled_row == old_drag_row) {

            last_recalled_row = row;

        } else if (old_drag_row < row &&
                   last_recalled_row > old_drag_row &&
                   last_recalled_row <= row) {

            last_recalled_row--;

        } else if (old_drag_row > row &&
                   last_recalled_row >= row &&
                   last_recalled_row < old_drag_row) {

            last_recalled_row++;
        }
    }


    /*
     * The dragged record now lives at
     * this position.
     */
    drag_row = row;


    /*
     * Update displayed strings.
     *
     * Do NOT recreate the table here:
     * keeping the same LVGL object preserves
     * encoder selection and automatic scrolling.
     */
    refresh_table();
}


/*
 * MFK PRESS
 */
static bool recall_row(int16_t row, bool close_dialog) {

    if (row < 0 ||
        row >= channels_count()) {

        return false;
    }


    const channel_t *channel =
        channels_get((uint16_t)row);


    if (!channel) {
        return false;
    }


    /*
     * Remember the channel actually recalled.
     */
    last_recalled_row = row;


    /* Frequency */
    cparam_i_set(
        cfg_fg_freq,
        channel->freq
    );


    /* Operating mode */
    cparam_i_set(
        cfg_cur_mode,
        channel->mode
    );


    /* AGC */
    cparam_i_set(
        cfg_cur_agc,
        channel->agc
    );


    /* PRE */
    cparam_i_set(
        cfg_cur_pre,
        channel->pre
            ? x6100_pre_on
            : x6100_pre_off
    );


    /* ATT */
    cparam_i_set(
        cfg_cur_att,
        channel->att
            ? x6100_att_on
            : x6100_att_off
    );


    /* Voice confirmation */
    if (channel->name[0] != '\0') {

        voice_say_text_fmt(
            "Channel %s",
            channel->name
        );

    } else {

        voice_say_text_fmt(
            "Channel"
        );
    }


    if (close_dialog) {
        dialog_destruct();
    }


    return true;
}


/*
 * Recall previous/next channel without opening
 * the Channel Memory dialog.
 *
 * direction < 0 : previous channel (FIL)
 * direction > 0 : next channel     (GENE)
 *
 * Wrap-around is intentional.
 * If no channel has been recalled yet:
 *   FIL  -> last channel
 *   GENE -> first channel
 */

const char *dialog_channels_last_recalled_name(void) {

    static char fallback_name[32];

    if (last_recalled_row == LV_TABLE_CELL_NONE ||
        last_recalled_row < 0 ||
        last_recalled_row >= channels_count()) {

        return "";
    }

    const channel_t *channel =
        channels_get((uint16_t)last_recalled_row);

    if (!channel) {
        return "";
    }

    if (channel->name[0] != '\0') {
        return channel->name;
    }

    snprintf(
        fallback_name,
        sizeof(fallback_name),
        "CHANNEL %d",
        last_recalled_row + 1
    );

    return fallback_name;
}


static void sync_table_to_recalled_channel(void) {

    if (!dialog.run ||
        !dialog.obj ||
        !table ||
        last_recalled_row == LV_TABLE_CELL_NONE) {

        return;
    }

    build_table();

    select_row(
        last_recalled_row,
        true
    );
}


bool dialog_channels_recall_relative(int8_t direction) {

    if (direction == 0) {
        return false;
    }


    /*
     * Reload the database so FIL/GENE also work
     * before the Channel Memory dialog has ever
     * been opened, and see manual JSON changes.
     */
    if (!channels_load()) {
        return false;
    }


    int16_t count =
        (int16_t)channels_count();


    if (count <= 0) {
        voice_say_text_fmt(
            "No channels"
        );
        return false;
    }


    int16_t row;


    if (last_recalled_row == LV_TABLE_CELL_NONE ||
        last_recalled_row < 0 ||
        last_recalled_row >= count) {

        row = (direction < 0)
            ? count - 1
            : 0;

    } else if (direction < 0) {

        row = last_recalled_row - 1;

        if (row < 0) {
            row = count - 1;
        }

    } else {

        row = last_recalled_row + 1;

        if (row >= count) {
            row = 0;
        }
    }



    /*
    return recall_row(
        row,
        false
    ); */

    bool recalled = recall_row(
        row,
        false
    );

    if (recalled) {
        sync_table_to_recalled_channel();
    }


return recalled;


}


/*
 * MFK PRESS
 */
static void channel_pressed_cb(lv_event_t *e) {

    if (drag_active) {
        return;
    }


    int16_t row = LV_TABLE_CELL_NONE;
    int16_t col = LV_TABLE_CELL_NONE;


    lv_table_get_selected_cell(
        table,
        &row,
        &col
    );


    if (row == LV_TABLE_CELL_NONE) {
        return;
    }


    recall_row(
        row,
        true
    );
}


/*
 * Request that the next opening of the Channel Memory dialog
 * immediately adds the current radio settings as a new channel.
 */
void dialog_channels_add_on_open(void) {

    add_on_construct = true;
}


/*
 * Build Channel Manager window
 */
static void construct_cb(lv_obj_t *parent) {

    channels_load();


    /*
     * Always start in normal mode.
     */
    drag_active = false;
    drag_row = LV_TABLE_CELL_NONE;


    /*
     * F5 must always start as Drag.
     */
    btn_page.items[4] =
        &btn_drag;


    dialog.obj = dialog_init(
        parent
    );


    /*
     * Use the same outer geometry and frame as WEFAX BIG/FULL.
     * dialog_init()'s background image is disabled because its own outline
     * would otherwise remain visible underneath the new frame.
     */
    lv_obj_set_size(
        dialog.obj,
        795,
        350
    );

    lv_obj_center(
        dialog.obj
    );

    lv_obj_set_style_bg_img_src(
        dialog.obj,
        NULL,
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        dialog.obj,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        dialog.obj,
        0,
        LV_PART_MAIN
    );


    /*
     * Same frame used by WEFAX BIG/FULL.
     */
    channels_frame = lv_obj_create(
        dialog.obj
    );

    lv_obj_remove_style_all(
        channels_frame
    );

    lv_obj_set_size(
        channels_frame,
        795,
        350
    );

    lv_obj_center(
        channels_frame
    );

    lv_obj_set_style_bg_color(
        channels_frame,
        lv_color_hex(0x182028),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        channels_frame,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_width(
        channels_frame,
        1,
        LV_PART_MAIN
    );

    lv_obj_set_style_border_color(
        channels_frame,
        lv_color_white(),
        LV_PART_MAIN
    );

    lv_obj_set_style_border_opa(
        channels_frame,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_set_style_radius(
        channels_frame,
        8,
        LV_PART_MAIN
    );

    lv_obj_clear_flag(
        channels_frame,
        LV_OBJ_FLAG_SCROLLABLE
    );

    lv_obj_clear_flag(
        channels_frame,
        LV_OBJ_FLAG_CLICKABLE
    );

    lv_obj_move_background(
        channels_frame
    );


    /*
     * Title
     */
    lv_obj_t *title =
        lv_label_create(
            dialog.obj
        );


    lv_label_set_text(
        title,
        "CHANNELS"
    );


    lv_obj_align(
        title,
        LV_ALIGN_TOP_MID,
        0,
        12
    );


    /*
     * Build channel list.
     */
    build_table();


    /*
     * A long press of the microphone V/M key requests
     * an immediate Add after opening the dialog.
     * Reuse the exact same code path as F2 Add.
     */
    if (add_on_construct) {

        add_on_construct = false;
        add_cb(NULL);
        return;
    }


    /*
     * Restore the last channel recalled with MFK.
     * When possible, place it approximately in the
     * middle of the visible list.
     */
    if (last_recalled_row != LV_TABLE_CELL_NONE &&
        last_recalled_row >= 0 &&
        last_recalled_row < channels_count()) {

        select_row(
            last_recalled_row,
            true
        );
    }
}
