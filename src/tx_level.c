/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - the drive level of a JS8 transmission
 */

#include "tx_level.h"

#include <math.h>

/* Below the setting by less than this (or reading it): no change. */
#define NEAR_W 0.05f
/* Short but within one reading step: creep up this much a transmission,
 * until the ALC starts to show (it then holds the power at the setting). */
#define CREEP_DB 0.3f

void tx_level_start(tx_level_t *l, float target_w) {
    *l          = (tx_level_t){0};
    l->target_w = target_w;
}

float tx_level_block(tx_level_t *l, bool have, float pwr_w, float alc) {
    l->blocks++;
    if (l->blocks <= TX_LEVEL_SETTLE_BLOCKS || !have) return 0.0f;
    if (alc < 0.0f) alc = 0.0f;
    if (alc > 10.0f) alc = 10.0f;
    l->n++;
    l->sum_pwr += pwr_w;
    l->sum_alc += alc;
    l->all_n++;
    l->all_pwr += pwr_w;
    l->all_alc += alc;
    if (l->n < TX_LEVEL_WINDOW_BLOCKS) return 0.0f;

    float p = l->sum_pwr / l->n, a = l->sum_alc / l->n;
    l->n = 0;
    l->sum_pwr = l->sum_alc = 0.0f;

    float d = 0.0f;
    if (a > TX_LEVEL_ALC_HIGH) {
        /* Overdriven: half the old formula a window (ALC 1: -0.2 dB,
         * 3: -0.6, 5: -1.2, 8 or more: the 1.5 dB limit). */
        d = (log10f(log10f(11.1f - a)) * 20.0f - 0.38f) * 0.5f;
    } else if (l->target_w > 0.0f && p > l->target_w * 1.25f + TX_LEVEL_PWR_STEP_W) {
        d = -10.0f * log10f(p / l->target_w) * 0.5f; /* well over the setting */
    }
    if (d > 0.0f) d = 0.0f;
    if (d < -TX_LEVEL_DOWN_MAX_DB) d = -TX_LEVEL_DOWN_MAX_DB;
    l->down_db += d;
    return d;
}

float tx_level_end(const tx_level_t *l) {
    /* Too short to judge, or it had to come down: it's at the edge now. */
    if (l->all_n < TX_LEVEL_WINDOW_BLOCKS || l->down_db < 0.0f || l->target_w <= 0.0f) return 0.0f;
    float p = l->all_pwr / l->all_n, a = l->all_alc / l->all_n;
    if (a >= TX_LEVEL_ALC_IDLE || p >= l->target_w - NEAR_W) return 0.0f;
    if (p >= l->target_w * 0.8f - TX_LEVEL_PWR_STEP_W) return CREEP_DB; /* within a reading step */
    float up = 10.0f * log10f(l->target_w / (p + 0.01f)) * 0.5f;
    if (up < CREEP_DB) up = CREEP_DB;
    return up > TX_LEVEL_UP_MAX_DB ? TX_LEVEL_UP_MAX_DB : up;
}
