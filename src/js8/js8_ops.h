/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 one-press messages, heartbeats and the
 *  station list; C interface for the dialog.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "js8_rx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JS8_Q_SNR_Q,     /* CALL SNR?      */
    JS8_Q_SEND_SNR,  /* CALL SNR -12   */
    JS8_Q_GRID_Q,    /* CALL GRID?     */
    JS8_Q_MY_GRID,   /* CALL GRID FN42AB */
    JS8_Q_INFO_Q,    /* CALL INFO?     */
    JS8_Q_STATUS_Q,  /* CALL STATUS?   */
    JS8_Q_HEARING_Q, /* CALL HEARING?  */
    JS8_Q_AGN_Q,     /* CALL AGN?      */
    JS8_Q_RR,        /* CALL RR        */
    JS8_Q_73,        /* CALL 73        */
    JS8_Q_COUNT,
} js8_query_t;

/* Menu label for a query, e.g. "SNR?" or "Send SNR". */
const char *js8_query_label(js8_query_t q);

/* Text to send. Returns false (empty out) if something needed is missing. */
bool js8_query_text(js8_query_t q, const char *to_call, int their_snr, const char *my_grid, char *out,
                    unsigned out_len);

/* "CALL: HEARTBEAT FN42", as desktop JS8Call sends it. */
void js8_heartbeat_text(const char *my_call, const char *my_grid, char *out, unsigned out_len);

/* A free heartbeat offset in 500-1000 Hz: at least 50 Hz from anything
 * heard in the last 30 s (desktop JS8Call's rule). */
int js8_heartbeat_offset(const float *offsets_hz, const int64_t *heard_ms, unsigned n, int64_t now_ms);

/* ---- Station list ---------------------------------------------------- */

typedef struct {
    char    call[JS8_RX_CALL_LEN];
    char    grid[8];
    int64_t heard_ms;
    int16_t snr;          /* how we hear them */
    float   freq_hz;
    bool    heard_me;     /* ★: they've sent us something */
    int64_t heard_me_ms;
    bool    has_reported_snr;
    int16_t reported_snr; /* how they hear us */
} js8_station_t;

typedef struct js8_stations js8_stations_t;

js8_stations_t *js8_stations_create(void);
void            js8_stations_destroy(js8_stations_t *s);
void            js8_stations_add(js8_stations_t *s, const js8_rx_msg_t *msg, const char *my_call, int64_t now_ms);
/* Fill up to max stations, ★ first then most recent; returns the count. */
int             js8_stations_list(js8_stations_t *s, int64_t now_ms, js8_station_t *out, int max);
void            js8_stations_clear(js8_stations_t *s);

#ifdef __cplusplus
}
#endif
