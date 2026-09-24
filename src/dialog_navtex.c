/*
 * NAVTEX / SITOR-B receiver
 */

#include "dialog_navtex.h"
#include "navtex/decoder.h"
#include "dialog.h"
#include "styles.h"
#include "buttons.h"
#include "keyboard.h"
#include "radio.h"
#include "mfk.h"
#include "events.h"

#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NAVTEX_TEST_WAV "/mnt/navtex_test.wav"
#define NAVTEX_TEST_CHUNK 256

#define NAVTEX_TEXT_MAX 65536 //32768
#define NAVTEX_PENDING_MAX 512
#define NAVTEX_UI_TIMER_MS 50

#define NAVTEX_SCROLL_STEP 36

static void construct_cb(lv_obj_t *parent);
static void destruct_cb(void);
static void audio_cb(unsigned int n, float *samples);
static void close_cb(button_data_t *data);
static void clear_cb(button_data_t *data);
static void test_cb(button_data_t *data);
static void *test_thread_cb(void *arg);
static bool start_test(void);
static void stop_test(void);
static bool reset_decoder(void);
static void ui_timer_cb(lv_timer_t *timer);
static void char_cb(int c, void *userdata);
static void key_cb(lv_event_t *e);

static navtex_decoder_t *decoder = NULL;
static lv_obj_t *scroll = NULL;
static lv_obj_t *label = NULL;
static lv_timer_t *ui_timer = NULL;

static pthread_mutex_t text_mutex = PTHREAD_MUTEX_INITIALIZER;
static char pending[NAVTEX_PENDING_MAX];
static size_t pending_len = 0;
static char text_buf[NAVTEX_TEXT_MAX];
static size_t text_len = 0;
static bool follow_tail = true;

static pthread_t test_thread;
static bool test_thread_valid = false;
static volatile bool test_stop_requested = false;
static volatile bool test_finished = false;
static volatile bool test_mode = false;

static button_data_t btn_page = {
    .type = BTN_TEXT,
    .label = "(NAVTEX 1:1)",
    .press = NULL
};

static button_data_t btn_close = {
    .type = BTN_TEXT,
    .label = "Close",
    .press = close_cb
};

static button_data_t btn_clear = {
    .type = BTN_TEXT,
    .label = "Clear",
    .press = clear_cb
};

static button_data_t btn_test = {
    .type = BTN_TEXT,
    .label = "Test",
    .press = test_cb
};

static buttons_page_t btn_page_1 = {
    .items = { &btn_page, &btn_close, &btn_clear, NULL, NULL }
};

static dialog_t dialog = {
    .run = false,
    .construct_cb = construct_cb,
    .destruct_cb = destruct_cb,
    .audio_cb = audio_cb,
    .key_cb = key_cb,
    .btn_page = &btn_page_1
};

dialog_t *dialog_navtex = &dialog;

bool dialog_navtex_is_active(void)
{
    return dialog.run;
}

static void append_text(const char *src, size_t n)
{
    if (n == 0)
        return;

    /*
     * Keep text_buf as a normal NUL-terminated string for LVGL.
     * When it fills up, discard complete oldest lines until there is
     * enough room for the incoming text.
     */
    if (n >= NAVTEX_TEXT_MAX) {
        src += n - (NAVTEX_TEXT_MAX - 1);
        n = NAVTEX_TEXT_MAX - 1;
        text_len = 0;
    }

    while (text_len + n >= NAVTEX_TEXT_MAX && text_len > 0) {
        char *newline = memchr(text_buf, '\n', text_len);
        size_t drop;

        if (newline)
            drop = (size_t)(newline - text_buf) + 1;
        else
            drop = text_len;

        memmove(text_buf, text_buf + drop, text_len - drop);
        text_len -= drop;
        text_buf[text_len] = '\0';
    }

    memcpy(text_buf + text_len, src, n);
    text_len += n;
    text_buf[text_len] = '\0';
}

static void char_cb(int c, void *userdata)
{
    (void)userdata;

    if (c == '\r')
        return;

    pthread_mutex_lock(&text_mutex);

    if (pending_len < NAVTEX_PENDING_MAX - 1)
        pending[pending_len++] = (char)c;

    pthread_mutex_unlock(&text_mutex);
}


static uint16_t read_le16(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_le32(const unsigned char *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool reset_decoder(void)
{
    if (decoder) {
        navtex_decoder_destroy(decoder);
        decoder = NULL;
    }

    decoder = navtex_decoder_create(0);
    if (!decoder) {
        fprintf(stderr, "NAVTEX: unable to create decoder\n");
        return false;
    }

    navtex_decoder_set_char_callback(decoder, char_cb, NULL);
    return true;
}

/* Locate the PCM data in a RIFF/WAVE file.  Do not assume a 44-byte
 * header: afconvert may place extra chunks before the audio data.
 */
static bool open_test_wav(FILE **out, uint32_t *data_bytes)
{
    unsigned char hdr[12];
    unsigned char chunk[8];
    bool fmt_ok = false;
    FILE *f = fopen(NAVTEX_TEST_WAV, "rb");

    if (!f) {
        fprintf(stderr, "NAVTEX TEST: cannot open %s\n", NAVTEX_TEST_WAV);
        return false;
    }

    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        memcmp(hdr, "RIFF", 4) != 0 ||
        memcmp(hdr + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "NAVTEX TEST: invalid WAV file\n");
        fclose(f);
        return false;
    }

    while (fread(chunk, 1, sizeof(chunk), f) == sizeof(chunk)) {
        uint32_t size = read_le32(chunk + 4);

        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char fmt[16];

            if (size < sizeof(fmt) ||
                fread(fmt, 1, sizeof(fmt), f) != sizeof(fmt)) {
                fprintf(stderr, "NAVTEX TEST: invalid fmt chunk\n");
                fclose(f);
                return false;
            }

            uint16_t format = read_le16(fmt);
            uint16_t channels = read_le16(fmt + 2);
            uint32_t rate = read_le32(fmt + 4);
            uint16_t bits = read_le16(fmt + 14);

            fmt_ok = (format == 1 && channels == 1 &&
                      rate == 11025 && bits == 16);

            if (size > sizeof(fmt))
                fseek(f, (long)(size - sizeof(fmt)), SEEK_CUR);
        } else if (memcmp(chunk, "data", 4) == 0) {
            if (!fmt_ok) {
                fprintf(stderr,
                        "NAVTEX TEST: WAV must be mono 11025 Hz PCM Int16\n");
                fclose(f);
                return false;
            }

            *out = f;
            *data_bytes = size;
            return true;
        } else {
            fseek(f, (long)size, SEEK_CUR);
        }

        if (size & 1)
            fseek(f, 1, SEEK_CUR);
    }

    fprintf(stderr, "NAVTEX TEST: data chunk not found\n");
    fclose(f);
    return false;
}

static void *test_thread_cb(void *arg)
{
    (void)arg;

    FILE *f = NULL;
    uint32_t bytes_left = 0;
    int16_t pcm[NAVTEX_TEST_CHUNK];
    float samples[NAVTEX_TEST_CHUNK];

    if (!open_test_wav(&f, &bytes_left)) {
        test_finished = true;
        return NULL;
    }

    fprintf(stderr, "NAVTEX TEST: playing %s\n", NAVTEX_TEST_WAV);

    while (!test_stop_requested && bytes_left >= 2) {
        size_t want = NAVTEX_TEST_CHUNK;
        size_t available = bytes_left / 2;

        if (want > available)
            want = available;

        size_t n = fread(pcm, sizeof(int16_t), want, f);
        if (n == 0)
            break;

        bytes_left -= (uint32_t)(n * sizeof(int16_t));

        for (size_t i = 0; i < n; ++i)
            samples[i] = (float)pcm[i] / 32768.0f;

        if (decoder)
            navtex_decoder_process(decoder, samples, (unsigned int)n);

        /* Feed the decoder at the same speed as live 11025 Hz audio. */
        usleep((useconds_t)((1000000ULL * n) / 11025ULL));
    }

    fclose(f);

    if (test_stop_requested)
        fprintf(stderr, "NAVTEX TEST: stopped\n");
    else
        fprintf(stderr, "NAVTEX TEST: finished\n");

    test_finished = true;
    return NULL;
}

static bool start_test(void)
{
    if (test_mode)
        return false;

    /* From this point audio_cb stops feeding live RX audio. */
    test_mode = true;
    test_stop_requested = false;
    test_finished = false;

    if (!reset_decoder()) {
        test_mode = false;
        return false;
    }

    int rc = pthread_create(&test_thread, NULL, test_thread_cb, NULL);
    if (rc != 0) {
        fprintf(stderr, "NAVTEX TEST: pthread_create failed: %d\n", rc);
        test_mode = false;
        reset_decoder();
        return false;
    }

    test_thread_valid = true;
    return true;
}

static void stop_test(void)
{
    if (!test_mode && !test_thread_valid)
        return;

    test_stop_requested = true;

    if (test_thread_valid) {
        pthread_join(test_thread, NULL);
        test_thread_valid = false;
    }

    test_finished = false;
    test_mode = false;
    reset_decoder();
}


static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    /* The worker never touches LVGL or destroys the decoder.  When a test
     * reaches EOF, finish the thread here in the UI context and return to
     * a fresh decoder for normal live reception.
     */
    if (test_mode && test_finished) {
        if (test_thread_valid) {
            pthread_join(test_thread, NULL);
            test_thread_valid = false;
        }
        test_finished = false;
        test_mode = false;
        reset_decoder();
    }
    char local[NAVTEX_PENDING_MAX];
    size_t n = 0;

    pthread_mutex_lock(&text_mutex);
    n = pending_len;
    if (n > 0) {
        memcpy(local, pending, n);
        pending_len = 0;
    }
    pthread_mutex_unlock(&text_mutex);

    if (n == 0 || !label)
        return;

    append_text(local, n);
    lv_label_set_text(label, text_buf);

    if (scroll && follow_tail)
        lv_obj_scroll_to_y(scroll, LV_COORD_MAX, LV_ANIM_OFF);
}

static void construct_cb(lv_obj_t *parent)
{
    pthread_mutex_lock(&text_mutex);
    pending_len = 0;
    pthread_mutex_unlock(&text_mutex);
    text_len = 0;
    text_buf[0] = '\0';
    follow_tail = true;

    test_mode = false;
    test_thread_valid = false;
    test_stop_requested = false;
    test_finished = false;

    reset_decoder();

    /*
     * Use exactly the same visual container as the built-in RTTY/CW panel.
     * panel_style supplies the same geometry, panel image for the selected
     * theme, padding and sony_24 font used by panel.cpp.
     *
     * Unlike the RTTY panel this object remains vertically scrollable,
     * because NAVTEX keeps a much longer text history.
     */
    scroll = lv_obj_create(parent);
    lv_obj_remove_style_all(scroll);
    lv_obj_add_style(scroll, &panel_style, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);
    dialog.obj = scroll;

    label = lv_label_create(scroll);
    /* panel_style has just been attached and LVGL may not have run a
     * layout pass yet.  lv_obj_get_content_width(scroll) can therefore
     * still be 0 here, making the label effectively invisible.
     * 100% means the panel's content width (same padding as RTTY).
     */
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_font(label, &sony_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, "NAVTEX ready - 100 baud / 170 Hz shift\n");

    /* MFK target.  LEFT scrolls towards older text, RIGHT towards
     * the newest text.  The volume encoder is handled in key_cb too.
     */
    lv_group_add_obj(keyboard_group, scroll);
    lv_obj_add_event_cb(scroll, key_cb, LV_EVENT_KEY, NULL);
    lv_group_focus_obj(scroll);
    lv_group_set_editing(keyboard_group, true);

    ui_timer = lv_timer_create(ui_timer_cb, NAVTEX_UI_TIMER_MS, NULL);
}

static void destruct_cb(void)
{
    /* Stop the WAV worker before destroying decoder/UI objects. */
    if (test_mode || test_thread_valid)
        stop_test();
    if (ui_timer) {
        lv_timer_del(ui_timer);
        ui_timer = NULL;
    }

    if (decoder) {
        navtex_decoder_destroy(decoder);
        decoder = NULL;
    }

    pthread_mutex_lock(&text_mutex);
    pending_len = 0;
    pthread_mutex_unlock(&text_mutex);

    label = NULL;
    scroll = NULL;
}

static void audio_cb(unsigned int n, float *samples)
{
    /* Never mix live receiver audio with the WAV test stream. */
    if (test_mode)
        return;

    if (decoder)
        navtex_decoder_process(decoder, samples, n);
}

static void key_cb(lv_event_t *e)
{
    uint32_t key = *((uint32_t *)lv_event_get_param(e));

    /* While a dialog is open the main-screen handler is disabled,
     * therefore NAVTEX must handle the volume encoder itself.
     */
    switch (key) {
        case KEY_VOL_LEFT_EDIT:
        case KEY_VOL_LEFT_SELECT:
            radio_change_vol(-1);
            return;

        case KEY_VOL_RIGHT_EDIT:
        case KEY_VOL_RIGHT_SELECT:
            radio_change_vol(1);
            return;

        default:
            break;
    }

    if (!scroll)
        return;

    switch (key) {
        case LV_KEY_LEFT: {
            /* MFK left: browse older text and stop following the tail. */
            lv_coord_t y = lv_obj_get_scroll_y(scroll);
            follow_tail = false;
            lv_obj_scroll_to_y(scroll, y - NAVTEX_SCROLL_STEP, LV_ANIM_OFF);
            break;
        }

        case LV_KEY_RIGHT: {
            /* MFK right: browse newer text.  Once another step cannot
             * move farther, we are at the tail and automatic following
             * is enabled again.
             */
            lv_coord_t old_y = lv_obj_get_scroll_y(scroll);
            lv_obj_scroll_to_y(scroll, old_y + NAVTEX_SCROLL_STEP, LV_ANIM_OFF);
            lv_coord_t new_y = lv_obj_get_scroll_y(scroll);

            if (new_y == old_y) {
                follow_tail = true;
                lv_obj_scroll_to_y(scroll, LV_COORD_MAX, LV_ANIM_OFF);
            }
            break;
        }

        default:
            break;
    }
}

static void close_cb(button_data_t *data)
{
    (void)data;
    dialog_destruct();
}

static void clear_cb(button_data_t *data)
{
    (void)data;

    pthread_mutex_lock(&text_mutex);
    pending_len = 0;
    pthread_mutex_unlock(&text_mutex);

    text_len = 0;
    text_buf[0] = '\0';
    follow_tail = true;

    if (label)
        lv_label_set_text(label, "");

    if (scroll)
        lv_obj_scroll_to_y(scroll, 0, LV_ANIM_OFF);
}


static void test_cb(button_data_t *data)
{
    (void)data;

    if (test_mode) {
        stop_test();
        return;
    }

    if (!start_test())
        fprintf(stderr, "NAVTEX TEST: unable to start\n");
}
