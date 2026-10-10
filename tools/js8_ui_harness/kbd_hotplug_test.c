/* tools/js8_ui_harness: src/keyboard.c's hot plug, on the PC (built by
 * the harness CMake as kbd_hotplug_test). As on the radio, the keyboard's
 * by-path link appears after udev's USB "add", goes on unplugging and
 * comes back. R1CBU 1.0 looked once, at the "add", and missed it. A
 * Bluetooth keyboard has no by-path link and no USB event: its node comes
 * and goes (asleep, awake) with an input event only. */
#include "lvgl/lvgl.h"
#include "keyboard.h"
#include "pubsub_ids.h"
#include "usb_devices.h"
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int  evdev_fd = -1;
int  opens;
char opened[256];
bool evdev_set_file(char *p) {
    snprintf(opened, sizeof(opened), "%s", p);
    if (evdev_fd >= 0) close(evdev_fd);
    evdev_fd = open(p, O_RDONLY | O_NONBLOCK);
    if (evdev_fd >= 0) opens++;
    return evdev_fd >= 0;
}
uint32_t xkb_process_key(uint32_t c, uint8_t v) { (void)c; (void)v; return 0; }

static void run_ms(int ms) {
    for (int i = 0; i < ms / 10; i++) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
}
static void plug(bool in) {
    if (in) close(open(DIR "/platform-usb-0:1:1.0-event-kbd", O_CREAT | O_WRONLY, 0644));
    else unlink(DIR "/platform-usb-0:1:1.0-event-kbd");
}
/* A Bluetooth keyboard: no by-path link, a plain event node (found in
 * udev's database on the radio), and only an input event, no USB one. */
static void bt(bool in, int n) {
    char p[256];
    snprintf(p, sizeof(p), BTDIR "/event%d", n);
    if (in) close(open(p, O_CREAT | O_WRONLY, 0644));
    else unlink(p);
}
static void input_event(void) { lv_msg_send(MSG_INPUT_DEVICE_CHANGED, NULL); }
static void usb_event(bool add) { lv_msg_send(MSG_USB_DEVICE_CHANGED, (void *)(intptr_t)(add ? USB_DEV_ADDED : USB_DEV_REMOVED)); }
static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *c) { (void)a; (void)c; lv_disp_flush_ready(d); }
static void screen(void) { /* LVGL registers input devices only with a display */
    static lv_color_t         buf[800 * 10];
    static lv_disp_draw_buf_t db;
    static lv_disp_drv_t      dd;
    lv_disp_draw_buf_init(&db, buf, NULL, 800 * 10);
    lv_disp_drv_init(&dd);
    dd.hor_res  = 800;
    dd.ver_res  = 480;
    dd.draw_buf = &db;
    dd.flush_cb = flush;
    lv_disp_drv_register(&dd);
}

int main(void) {
    int fails = 0;
#define CHECK(c, what) do { bool ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", what); fails += !ok_; } while (0)
    mkdir(DIR, 0755);
    mkdir(BTDIR, 0755);
    plug(false);
    bt(false, 5);
    bt(false, 6);
    lv_init();
    screen();
    keyboard_init();
    CHECK(!keyboard_ready(), "no keyboard at start");

    usb_event(true);               /* udev's "add", the link not there yet */
    run_ms(700);
    plug(true);                    /* the link comes 0.7 s later */
    run_ms(600);
    CHECK(keyboard_ready(), "plugged in while running: found within a second of its link appearing");
    CHECK(opens == 1, "opened once");

    plug(false);                   /* unplugged: the link may outlive the event */
    usb_event(false);
    run_ms(100);
    CHECK(true, "(remove event)");
    run_ms(800);
    CHECK(!keyboard_ready(), "unplugged: not ready");

    plug(true);                    /* back in: a new device */
    usb_event(true);
    run_ms(200);
    CHECK(keyboard_ready() && opens == 2, "plugged back in: opened again");

    plug(false);                   /* out and straight back in, the link never seen missing */
    plug(true);
    usb_event(false);
    usb_event(true);
    run_ms(600);
    CHECK(keyboard_ready() && opens == 3, "out and back fast: the new one opened");

    run_ms(6000);                  /* the checks stop after 5 s */
    int before = opens;
    plug(false);
    plug(true);                    /* a new node, but no USB event: nothing looks */
    run_ms(2000);
    CHECK(opens == before, "no looking without a USB event (the checks stopped)");
    plug(false);
    usb_event(false);
    run_ms(6000);
    CHECK(!keyboard_ready(), "(USB keyboard gone)");

    before = opens;
    bt(true, 5);                   /* a Bluetooth keyboard connects */
    input_event();
    run_ms(600);
    CHECK(keyboard_ready() && opens == before + 1 && strstr(opened, "/kbd-bt/event5"),
          "Bluetooth keyboard connected: found from its input event alone");

    bt(false, 5);                  /* it goes to sleep: its node goes */
    input_event();
    run_ms(800);
    CHECK(!keyboard_ready(), "Bluetooth keyboard asleep: not ready");

    bt(true, 6);                   /* a key wakes it: back as a new node */
    input_event();
    run_ms(600);
    CHECK(keyboard_ready() && opens == before + 2 && strstr(opened, "/kbd-bt/event6"),
          "Bluetooth keyboard awake again as a new node: opened");

    plug(true);                    /* a USB keyboard as well: it comes first */
    usb_event(true);
    run_ms(600);
    CHECK(keyboard_ready() && strstr(opened, "-kbd"), "USB and Bluetooth: the USB keyboard is used");

    plug(false);
    usb_event(false);
    run_ms(600);
    CHECK(keyboard_ready() && strstr(opened, "/kbd-bt/event6"), "USB unplugged: back to the Bluetooth one");
    bt(false, 6);
    printf("%s\n", fails ? "FAILED" : "all passed");
    return fails != 0;
}
