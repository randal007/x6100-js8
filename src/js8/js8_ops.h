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
    char    grid[8];          /* up to 6 characters */
    int64_t heard_ms;
    int16_t snr;          /* how we hear them */
    float   freq_hz;
    uint8_t submode;      /* speed last heard at: 0 Normal, 1 Fast, 2 Turbo, 4 Slow */
    bool    heard_me;     /* ★: they've sent us something */
    int64_t heard_me_ms;
    bool    has_reported_snr;
    int16_t reported_snr; /* how they hear us */
    char    via[JS8_RX_CALL_LEN]; /* only heard through this relay station, or "" */
} js8_station_t;

typedef struct js8_stations js8_stations_t;

js8_stations_t *js8_stations_create(void);
void            js8_stations_destroy(js8_stations_t *s);
void            js8_stations_add(js8_stations_t *s, const js8_rx_msg_t *msg, const char *my_call, int64_t now_ms);
/* Fill up to max stations, ★ first then most recent; returns the count. */
int             js8_stations_list(js8_stations_t *s, int64_t now_ms, js8_station_t *out, int max);
void            js8_stations_clear(js8_stations_t *s);
/* How long a station stays listed, for every list (0: always); default 1 h. */
void            js8_stations_set_expire_ms(int64_t ms);
/* A station heard only through `via` (a relay), as desktop lists them. */
void            js8_stations_add_via(js8_stations_t *s, const char *call, const char *via, float freq_hz,
                                     uint8_t submode, int64_t now_ms);
/* A relay that ends at my_call (desktop's rule: to us, no further hop, not
 * an ACK): the stations it came through, besides the one we heard. Their
 * calls go to out (up to max), the station heard to via; returns how many. */
int             js8_relay_stations(const js8_rx_msg_t *msg, const char *my_call, char (*out)[JS8_RX_CALL_LEN],
                                   int max, char *via, unsigned via_len);
/* The command in a decoded "FROM: TO CMD ..." (SNR?, MSG, ACK, >, CQ ...):
 * its offset and length in text; false if it has none. */
bool            js8_command_span(const char *text, unsigned *start, unsigned *len);

/* ---- Auto-reply, heartbeat acks, heartbeat timing (T4) --------------- */

typedef struct js8_held js8_held_t;
typedef struct js8_inbox js8_inbox_t;

#define JS8_PATH_LEN 48 /* a relay path, "K2XYZ>N0XYZ" */

typedef struct {
    bool        autoreply; /* AUTO */
    bool        heartbeat; /* HB   */
    bool        hb_ack;    /* HB ACK: acts only with AUTO and HB on */
    bool        relay;     /* pass relays on and hold MSG TO: (desktop's relay switch) */
    const char *my_call, *my_grid, *info, *status;
    const char *groups; /* "@GROUP1 @GROUP2": groups we're in, or NULL */
    js8_held_t *held;   /* messages held for others, or NULL */
} js8_auto_settings_t;

/* A station heard lately (HEARING?, QUERY CALL, RETRIEVE MSG). */
typedef struct {
    const char *call;
    int         snr;
    int64_t     heard_ms;
} js8_heard_t;

typedef enum {
    JS8_AUTO_IGNORE,
    JS8_AUTO_SEND,  /* queue it now */
    JS8_AUTO_OFFER, /* AUTO is off: suggest it, like desktop's outgoing box */
} js8_auto_action_t;

typedef enum {
    JS8_REPLY_QUERY,   /* an answer to a question */
    JS8_REPLY_HB_ACK,  /* a heartbeat ack */
    JS8_REPLY_ACK,     /* ACK for a message kept here, or a relay that ended here */
    JS8_REPLY_SUGGEST, /* only ever offered */
    JS8_REPLY_RELAY,   /* a relay passed on */
    JS8_REPLY_STORED,  /* YES MSG ID / NO, a held message, QUERY CALL */
} js8_reply_kind_t;

typedef struct {
    js8_auto_action_t action;
    js8_reply_kind_t  kind;
    bool              hb_ack;  /* a heartbeat ack: send in the HB sub-band */
    bool              allcall; /* answers an @ALLCALL (rate limit) */
    char              text[JS8_RX_TEXT_LEN];
    char              to[JS8_RX_CALL_LEN];
    char              command[16];
    int               deliver_id; /* a held message this delivers: js8_held_delivered() once sent */
    char              deliver_group_call[JS8_RX_CALL_LEN]; /* a group message: who it went to */
} js8_auto_result_t;

typedef enum { JS8_STORED_NONE, JS8_STORED_INBOX, JS8_STORED_HELD } js8_stored_kind_t;

/* What js8_process() kept from a message. */
typedef struct {
    js8_stored_kind_t kind;
    int               id;     /* -1: couldn't save the file (kept in memory) */
    bool              resend; /* the same message again: nothing new */
    char              from[JS8_RX_CALL_LEN];
    char              to[JS8_RX_CALL_LEN];
    char              path[JS8_PATH_LEN];
    char              text[JS8_RX_TEXT_LEN];
} js8_stored_t;

typedef struct js8_auto js8_auto_t;

js8_auto_t *js8_auto_create(void);
void        js8_auto_destroy(js8_auto_t *a);

/* A received message, as desktop JS8Call's processCommandActivity() takes
 * it: keeps a MSG for us in the inbox and a MSG TO: in s->held (stored),
 * and decides the answer (out): queries, heartbeat acks, ACKs, relays,
 * held messages. heard: recent stations, most recent first. last_tx: our
 * last message (for AGN?). Either output may be NULL. */
void js8_process(js8_auto_t *a, const js8_rx_msg_t *msg, const js8_auto_settings_t *s, const js8_heard_t *heard,
                 unsigned n_heard, const char *last_tx, int64_t now_ms, js8_inbox_t *inbox, js8_stored_t *stored,
                 js8_auto_result_t *out);
/* Record that a reply was queued (rate limits). */
void js8_auto_sent(js8_auto_t *a, const js8_auto_result_t *r, int64_t now_ms);
/* Any key, button or knob; automatic TX stops after an hour without one. */
void js8_auto_user_activity(js8_auto_t *a, int64_t now_ms);
bool js8_auto_idle(js8_auto_t *a, int64_t now_ms);

/* Does this message start a QSO with us? */
bool js8_starts_qso(const js8_rx_msg_t *msg);

/* Desktop's heartbeat schedule; interval clamped to 5-30 min. */
int64_t js8_next_heartbeat_ms(int64_t now_ms, int interval_min);

/* Time Sync from decodes, like desktop JS8Call's drift. One decode: its
 * station, when it was decoded, the drift it suggests (js8_rx_msg_t.drift_ms)
 * and its speed's slot length. */
typedef struct {
    char    call[JS8_RX_CALL_LEN];
    int64_t when_ms;
    int32_t drift_ms;
    int32_t period_ms;
} js8_sync_sample_t;

#define JS8_SYNC_MIN_DECODES    3
#define JS8_SYNC_MIN_STATIONS   3

/* The drift Time Sync should set. Each decode's suggested drift is worked
 * out from the drift in effect when its audio was captured, so decodes
 * finishing after a change still count right; it's taken the short way
 * round its slot from `current_ms`. Each station counts once (its latest
 * decode), so one busy station can't outvote the band: the median across
 * stations when at least JS8_SYNC_MIN_STATIONS were heard, else across all
 * decodes. Decodes older than `window_ms` (or empty samples) are skipped.
 * False with fewer than JS8_SYNC_MIN_DECODES; `decodes` and `stations`
 * (either may be NULL) say how many were used. */
bool js8_sync_drift(const js8_sync_sample_t *s, unsigned n, int64_t now_ms, int64_t window_ms, int64_t current_ms,
                    int64_t *drift_ms, unsigned *decodes, unsigned *stations);

/* Maidenhead locator for a position, `chars` long (4, 6, 8 or 10; 10 is
 * about 20 x 35 m). Letters upper case, as JS8 grids are. False for an
 * out-of-range position or length. */
bool js8_latlon_to_grid(double lat, double lon, int chars, char *out, unsigned size);

/* ---- QSO log --------------------------------------------------------- */

/* The QSO with one station, from the directed traffic seen so far. */
typedef struct {
    char    call[JS8_RX_CALL_LEN];
    char    grid[8];      /* the most precise they sent us (up to 6), or "" */
    int64_t start_ms;     /* first directed message either way */
    bool    has_sent_snr, has_rcvd_snr, has_heard_snr;
    int16_t sent_snr;     /* report we gave them */
    int16_t rcvd_snr;     /* report they gave us */
    int16_t heard_snr;    /* how we last heard them */
    bool    two_way;      /* both sides sent something (HB acks don't count) */
} js8_qso_t;

typedef struct js8_qsos js8_qsos_t;

js8_qsos_t *js8_qsos_create(void);
void        js8_qsos_destroy(js8_qsos_t *q);
/* A received message / one of ours as it goes out ("MYCALL: TO ..."). Both
 * return true, with the call in `ended`, when the message ends a two-way
 * QSO (73 or SK) that hasn't been offered for logging yet. */
bool js8_qsos_received(js8_qsos_t *q, const js8_rx_msg_t *msg, const char *my_call, int64_t now_ms, char *ended,
                       unsigned ended_len);
bool js8_qsos_sent(js8_qsos_t *q, const char *text, const char *my_call, int64_t now_ms, char *ended,
                   unsigned ended_len);
bool js8_qsos_get(js8_qsos_t *q, const char *call, int64_t now_ms, js8_qso_t *out);
void js8_qsos_logged(js8_qsos_t *q, const char *call);
void js8_qsos_clear(js8_qsos_t *q);

/* One log entry; empty strings and tx_pwr_w 0 are left out. */
typedef struct {
    char     call[JS8_RX_CALL_LEN];
    char     grid[12];
    char     name[64];
    char     comment[128];
    char     rst_sent[8], rst_rcvd[8];
    int64_t  on_ms, off_ms;
    uint64_t freq_hz;     /* dial + audio offset */
    char     my_call[JS8_RX_CALL_LEN];
    char     my_grid[12];
    float    tx_pwr_w;
    char     pota_ref[16]; /* MY_SIG POTA + MY_SIG_INFO */
    char     sota_ref[24]; /* MY_SOTA_REF */
} js8_log_entry_t;

/* Append to an ADIF file in desktop JS8Call's format (MODE MFSK, SUBMODE
 * JS8), with a header if the file is new. False with a message in err. */
bool js8_log_append(const char *path, const js8_log_entry_t *e, char *err, unsigned err_len);
/* "40m", or "" outside the bands. */
const char *js8_log_band(uint64_t freq_hz);

/* ---- Inbox ------------------------------------------------------------ */

typedef struct {
    int     id;
    int64_t utc_ms;
    char    from[JS8_RX_CALL_LEN];
    char    to[JS8_RX_CALL_LEN];  /* our call, or our group */
    char    path[JS8_PATH_LEN];   /* the way back: "K2XYZ>N0XYZ" if relayed, else from */
    char    text[JS8_RX_TEXT_LEN];
    bool    read;
} js8_inbox_msg_t;

/* Loads `path` (missing = empty); every change is saved back to it. */
js8_inbox_t *js8_inbox_open(const char *path);
void         js8_inbox_close(js8_inbox_t *b);
/* Save a message; returns its id (a resend within 30 min keeps the first),
 * or -1 if the file can't be written (the message is still in memory). */
int  js8_inbox_add(js8_inbox_t *b, const char *from, const char *text, int64_t utc_ms);
/* Up to max messages, newest first; returns the count. */
int  js8_inbox_list(js8_inbox_t *b, js8_inbox_msg_t *out, int max);
bool js8_inbox_get(js8_inbox_t *b, int id, js8_inbox_msg_t *out);
void js8_inbox_mark_read(js8_inbox_t *b, int id);
void js8_inbox_delete(js8_inbox_t *b, int id);
int  js8_inbox_unread(js8_inbox_t *b);
int  js8_inbox_count(js8_inbox_t *b);

/* Messages held here for other stations ("MSG TO:"), as desktop JS8Call
 * stores them until the station asks (QUERY MSGS, QUERY MSG n). */
typedef struct {
    int     id;
    int64_t utc_ms;
    char    from[JS8_RX_CALL_LEN];
    char    to[JS8_RX_CALL_LEN]; /* a base call, or a @GROUP */
    char    path[JS8_PATH_LEN];
    char    text[JS8_RX_TEXT_LEN];
    bool    delivered;
    bool    group; /* for a @GROUP: any member may fetch it for two days */
    int     got;   /* group message: how many have fetched it */
} js8_held_msg_t;

js8_held_t *js8_held_open(const char *path);
void        js8_held_close(js8_held_t *h);
/* Hold a message; returns its id (a resend keeps the first), -1 if unsaved. */
int  js8_held_add(js8_held_t *h, const char *from, const char *to, const char *text, int64_t utc_ms);
int  js8_held_list(js8_held_t *h, js8_held_msg_t *out, int max); /* newest first */
bool js8_held_get(js8_held_t *h, int id, js8_held_msg_t *out);
void js8_held_delivered(js8_held_t *h, int id);
/* A group message fetched by `call`. */
void js8_held_group_delivered(js8_held_t *h, int id, const char *call);
/* Desktop's stored-message notice: if a station we hold a message for was
 * heard in the last 15 min and not told in 8 h, "W1ABC RETRIEVE MSG 3" in
 * text and true (marked told). */
bool js8_held_push_due(js8_held_t *h, const js8_heard_t *heard, unsigned n_heard, int64_t now_ms, char *text,
                       unsigned text_len);
void js8_held_delete(js8_held_t *h, int id);
int  js8_held_waiting(js8_held_t *h); /* not yet delivered */
int  js8_held_count(js8_held_t *h);
/* "FROM: MYCALL MSG TO:W1ABC text" with a valid checksum: to and text out. */
bool js8_msg_to_for_me(const js8_rx_msg_t *msg, const char *my_call, char *to, unsigned to_len, char *text,
                       unsigned text_len);

/* A message for the inbox: "FROM: MYCALL MSG text" with a valid checksum.
 * The text goes to out. */
bool js8_msg_for_me(const js8_rx_msg_t *msg, const char *my_call, char *out, unsigned out_len);

/* "K2XYZ>N0XYZ" -> "N0XYZ via K2XYZ", as desktop's inbox shows a path. */
void js8_path_display(const char *path, char *out, unsigned out_len);
/* A delivered message's "... FROM N0XYZ [NEXT MSG ID 4 [+2]]": the original
 * sender, and the next id (0 if none). */
bool js8_delivered_signature(const char *text, char *from, unsigned from_len, int *next_id);
/* Groups as typed -> "@GROUP1 @GROUP2" (upper case, '@' added, at most 10). */
void js8_groups_normalise(const char *typed, char *out, unsigned out_len);

/* ---- Alerts ----------------------------------------------------------- */

/* Alert words as typed -> "VE7ABC @POTA SOTA" (upper case, deduplicated,
 * at most 20). */
void js8_alert_words_normalise(const char *typed, char *out, unsigned out_len);
/* The first of `words` (as normalised) in a decode, into hit; false if none. */
bool js8_alert_hit(const char *text, const char *from, const char *words, char *hit, unsigned hit_len);

#define JS8_HB_MIN_INTERVAL     5
#define JS8_HB_MAX_INTERVAL     30
#define JS8_HB_DEFAULT_INTERVAL 30

#ifdef __cplusplus
}
#endif
