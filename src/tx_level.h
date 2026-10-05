/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - the drive level of a JS8 transmission
 *
 *  tx_player.c plays a JS8 frame in blocks of 2048 samples (about 43 ms at
 *  48 kHz) and reads the radio's ALC and power after each. The old loop
 *  turned the gain after every block from that one reading: up to +0.6 dB
 *  whenever the power read short, down when the ALC rose. The radio reads
 *  power in 0.1 W steps, so at 0.3 W a true 0.28 W reads 0.2 W "short",
 *  and the drive swung by several dB a second (VE7NHW, 2026-10-04: an
 *  XPA125B behind it went 18-35 W and back every two seconds).
 *
 *  Now: readings are averaged over about a second; within a transmission
 *  the drive only goes down (the ALC shows overdrive, or the power is well
 *  over the setting), a limited step a second, so the level stays steady;
 *  it goes up only between transmissions, when a whole one averaged
 *  short of power with the ALC idle: 1 dB a time while the ALC reads
 *  zero (up to 2 dB when far short), 0.3 dB once it shows a little. Pure
 *  arithmetic: tests/test_js8.cpp runs it against a model radio.
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TX_LEVEL_SETTLE_BLOCKS 30   /* about 1.3 s: the PA and the ALC settle first */
#define TX_LEVEL_WINDOW_BLOCKS 24   /* about 1 s of readings per decision */
#define TX_LEVEL_ALC_HIGH      0.5f /* the radio's ALC (0-10) above this: overdriven */
#define TX_LEVEL_ALC_IDLE      0.25f /* below this: room to drive harder */
#define TX_LEVEL_DOWN_MAX_DB   1.5f /* the most the drive drops in one window */
#define TX_LEVEL_UP_MAX_DB     2.0f /* the most it rises from one transmission to the next */
#define TX_LEVEL_PWR_STEP_W    0.1f /* the radio's power reading resolution */

typedef struct {
    float target_w;   /* the power asked for */
    int   blocks;     /* blocks played so far */
    int   n;          /* readings in the current window */
    float sum_pwr;
    float sum_alc;
    int   all_n;      /* readings after settling, the whole transmission */
    float all_pwr;
    float all_alc;
    float down_db;    /* how far it went down this transmission */
} tx_level_t;

void tx_level_start(tx_level_t *l, float target_w);

/* One block played; `have` a new reading (power in W, ALC 0-10). Returns
 * the gain change to make now, in dB: 0, or negative (never up within a
 * transmission). */
float tx_level_block(tx_level_t *l, bool have, float pwr_w, float alc);

/* The transmission ended: the gain change for the next one, in dB (up when
 * it ran clearly short of power with the ALC idle; else 0). */
float tx_level_end(const tx_level_t *l);

#ifdef __cplusplus
}
#endif
