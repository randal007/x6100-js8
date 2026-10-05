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
#include <sys/stat.h>
#include <unistd.h>

#include <glob.h>
#include <stdio.h>

#ifndef KBD_GLOB
#define KBD_GLOB "/dev/input/by-path/*-kbd" /* tests point it elsewhere */
#endif

/* After a USB device comes or goes, look for the keyboard this often, this
 * many times: udev's "add" for the USB device comes before the keyboard's
 * /dev/input node and its by-path link exist, so looking once at once
 * finds nothing (1.0's USB thread passes the events on without the old
 * half-second pause between them). */
#define KBD_RESCAN_MS    500
#define KBD_RESCAN_TIMES 10


lv_group_t *keyboard_group;

static lv_indev_drv_t       indev_drv_2;
static bool                 ready = false;
static kbd_rollover_t       rollover;
static dev_t                kbd_rdev;  /* the keyboard opened: a new one (unplugged and back) opens again */
static ino_t                kbd_ino;
static lv_timer_t          *rescan_timer;
static int                  rescans_left;

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
    static char path[256];
    glob_t      globbuf;
    bool        found = glob(KBD_GLOB, 0, NULL, &globbuf) == 0;
    if (found) snprintf(path, sizeof(path), "%s", globbuf.gl_pathv[0]);
    globfree(&globbuf);
    return found ? path : NULL;
}

static void setup_kbd(char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || !evdev_set_file(path)) {
        return;
    }
    kbd_rdev = st.st_rdev;
    kbd_ino  = st.st_ino;
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

/* A keyboard there and not open (or a different one: unplugged and back):
 * open it; none there: not ready. */
static void check_kbd(void) {
    char       *dev_path = search_kbd_device();
    struct stat st;
    if (dev_path && stat(dev_path, &st) == 0) {
        if (!ready || st.st_rdev != kbd_rdev || st.st_ino != kbd_ino) setup_kbd(dev_path);
        return;
    }
    ready = false;
}

/* All the checks, also once one finds it: an unplugged keyboard's link can
 * outlive the "remove" for a moment. */
static void rescan_cb(lv_timer_t *t) {
    check_kbd();
    if (--rescans_left <= 0) {
        lv_timer_del(t);
        rescan_timer = NULL;
    }
}

static void on_usb_device_change(void * s, lv_msg_t * msg) {
    (void)s;
    (void)msg;
    check_kbd();
    /* Look again for a few seconds: the keyboard's node comes later. */
    rescans_left = KBD_RESCAN_TIMES;
    if (!rescan_timer) rescan_timer = lv_timer_create(rescan_cb, KBD_RESCAN_MS, NULL);
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
