/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 receive, C interface for the dialog
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JS8_RX_CALL_LEN 16
#define JS8_RX_TEXT_LEN 256

/* Submode bits for js8_rx_create(). */
#define JS8_SUBMODE_NORMAL (1 << 0)
#define JS8_SUBMODE_FAST   (1 << 1)
#define JS8_SUBMODE_TURBO  (1 << 2)
#define JS8_SUBMODE_SLOW   (1 << 3)

/* Frame-type bits in js8_rx_msg_t.type. */
#define JS8_FRAME_FIRST 0x1
#define JS8_FRAME_LAST  0x2
#define JS8_FRAME_DATA  0x4

/* One decoded frame or one assembled message. Plain value type: safe to
 * memcpy, e.g. through scheduler_put(). */
typedef struct {
    int32_t utc;      /* HHMMSS */
    int16_t snr;
    float   dt;
    float   freq_hz;  /* audio offset */
    uint8_t type;     /* JS8_FRAME_* bits of the (last) frame */
    uint8_t submode;  /* 0 Normal, 1 Fast, 2 Turbo, 4 Slow */
    bool    low_confidence;
    bool    heartbeat;
    bool    cq;
    bool    to_me;
    bool    to_group;
    int8_t  checksum; /* buffered command (MSG etc.): 0 none, 1 valid, -1 bad */
    bool    tx;       /* set by the app for its own transmissions */
    bool    alert;    /* set by the app: matched an alert word */
    bool     partial; /* a message still arriving: the text so far (on_message) */
    uint32_t msg_id;  /* partials and the final message share it */
    char    from[JS8_RX_CALL_LEN];
    char    to[JS8_RX_CALL_LEN];
    char    text[JS8_RX_TEXT_LEN];
} js8_rx_msg_t;

/* All callbacks run on receiver worker threads, never the caller's. */
typedef struct {
    void (*on_frame)(const js8_rx_msg_t *msg, void *ctx);   /* every decode */
    /* Assembled messages; also, with msg->partial set, the text so far of a
     * multi-frame message after each of its frames (the final one follows
     * with the same msg_id). */
    void (*on_message)(const js8_rx_msg_t *msg, void *ctx);
    void (*on_cycle_done)(unsigned decodes, void *ctx);
    /* Input-rate audio off the audio thread, e.g. for a waterfall. */
    void (*on_audio)(const float *samples, unsigned n, void *ctx);
    void *ctx;
} js8_rx_cb_t;

typedef struct js8_rx js8_rx_t;

/* Returns NULL on failure. `my_call` may be NULL or empty. */
js8_rx_t *js8_rx_create(int input_rate, int submodes, const char *my_call, const js8_rx_cb_t *cb);

/* Queue float audio in [-1, 1] at input_rate. Safe from the audio thread. */
void js8_rx_feed(js8_rx_t *rx, const float *samples, unsigned n);

/* Drop partially received messages, e.g. after a band change. */
void js8_rx_clear(js8_rx_t *rx);
/* Change which speeds are decoded (JS8_SUBMODE_* bits), from any thread. */
void js8_rx_set_submodes(js8_rx_t *rx, int submodes);
/* The audio range searched (low..high Hz) and our TX offset, whose
 * neighbours are decoded first. From any thread. */
void js8_rx_set_decode_range(js8_rx_t *rx, int low_hz, int high_hz);
void js8_rx_set_qso_offset(js8_rx_t *rx, int offset_hz);

/* Test mode: play a 16-bit PCM WAV (any rate; resampled as needed) into the
 * decoder in real time, starting at the next 30 s boundary (a slot start for
 * every speed), so a file whose first sample is a slot start decodes like
 * live audio. Live audio from js8_rx_feed() is ignored while it plays.
 * Returns the seconds until playback starts, or -1 with a message in err. */
float js8_rx_play_wav(js8_rx_t *rx, const char *path, char *err, unsigned err_len);
void  js8_rx_stop_wav(js8_rx_t *rx);
bool  js8_rx_wav_active(js8_rx_t *rx);

/* Stops threads; no callbacks fire after this returns. NULL is ignored. */
void js8_rx_destroy(js8_rx_t *rx);

#ifdef __cplusplus
}
#endif
