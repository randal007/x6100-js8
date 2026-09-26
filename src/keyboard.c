/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#include "keyboard.h"

#include "pubsub_ids.h"
#include "usb_devices.h"

#include "kbd_rollover.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_drivers/indev/xkb.h"

#include <linux/input.h>
#include <unistd.h>

#include <glob.h>


lv_group_t *keyboard_group;

static lv_indev_drv_t       indev_drv_2;
static bool                 ready = false;
static kbd_rollover_t       rollover;

extern int evdev_fd; /* lv_drivers/indev/evdev.c, set by evdev_set_file() */

/* The next key event from the USB keyboard, through xkb (layout, Shift). */
static bool kbd_next(void *ctx, uint16_t *scancode, uint32_t *key, int *value) {
    (void)ctx;
    struct input_event in;
    while (read(evdev_fd, &in, sizeof(in)) > 0) {
        if (in.type != EV_KEY) continue;
        *scancode = in.code;
        *value    = in.value;
        *key      = xkb_process_key(in.code, in.value); /* also keeps Shift state */
        return true;
    }
    return false;
}

static void kbd_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    kbd_rollover_read(&rollover, kbd_next, NULL, data);
}

static char* search_kbd_device() {
    glob_t globbuf;
    globbuf.gl_offs = 1;
    char *path = NULL;

    int res = glob("/dev/input/by-path/*-kbd", 0, NULL, &globbuf);
    if (res == 0) {
        path = globbuf.gl_pathv[0];
    }
    return path;
}

static void setup_kbd(char *path) {
    if (!evdev_set_file(path)) {
        return;
    }
    rollover = (kbd_rollover_t){0};

    if (indev_drv_2.type == LV_INDEV_TYPE_NONE) {
        lv_indev_drv_init(&indev_drv_2);
        indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv_2.read_cb = kbd_read;

        lv_indev_t *keyboard_indev = lv_indev_drv_register(&indev_drv_2);

        lv_indev_set_group(keyboard_indev, keyboard_group);
    }

    ready = true;
}

static void on_usb_device_change(void * s, lv_msg_t * msg) {
    enum usb_devices_event_t event = (enum usb_devices_event_t)msg->payload;
    char *dev_path = search_kbd_device();
    if (dev_path && !ready) {
        setup_kbd(dev_path);
    } else if (!dev_path && ready) {
        ready = false;
    }
}

void keyboard_init() {
    keyboard_group = lv_group_create();

    lv_msg_subscribe(MSG_USB_DEVICE_CHANGED, on_usb_device_change, NULL);

    char *path = search_kbd_device();
    if (!path)
        return;

    setup_kbd(path);
}

bool keyboard_ready() {
    return ready;
}
