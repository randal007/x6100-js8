/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - digital-mode TX player
 *
 *  Copyright (c) 2026
 */

#include "tx_player.h"

#include <math.h>
#include <stddef.h>

#include "lvgl/lvgl.h"

#include "audio.h"
#include "cfg/cfg_api.h"
#include "radio.h"
#include "tx_info.h"
#include "tx_level.h"

#include <aether_radio/x6100_control/control.h>

#define GAIN_MIN_DB (-30.0f)
#define GAIN_MAX_DB 0.0f

static float clamp_gain(float g) {
    return g > GAIN_MAX_DB ? GAIN_MAX_DB : g < GAIN_MIN_DB ? GAIN_MIN_DB : g;
}

float tx_player_base_gain_offset(void) {
    float target_pwr = LV_MIN(param_f_get(cfg.pwr()), TX_PLAYER_MAX_PWR_W);
    if (x6100_control_get_base_ver().rev >= 3) {
        // patched firmware has a true power control
        return -9.4f;
    }
    return -16.4f + log10f(target_pwr) * 10.0f;
}

bool tx_player_play(int16_t      *samples,
                    uint32_t      n_samples,
                    int32_t       tx_offset_hz,
                    float         base_gain_offset,
                    tx_abort_fn_t abort_check,
                    void         *abort_check_ctx) {
    if (param_f_get(cfg.pwr()) > TX_PLAYER_MAX_PWR_W) {
        radio_set_pwr(TX_PLAYER_MAX_PWR_W);
    }

    float gain_offset      = base_gain_offset + param_f_get(cfg.ft8.output_gain_offset());
    float play_gain_offset = audio_set_play_vol(gain_offset + 6.0f);
    gain_offset           -= play_gain_offset;

    uint64_t radio_freq = cparam_i_get(cfg.cur.fg_freq());
    radio_set_freq((int32_t)radio_freq + tx_offset_hz - TX_PLAYER_AUDIO_HZ);
    radio_set_modem(true);

    /* The drive from the ALC and power readback (tx_level.c): ramped up
     * while the ALC reads zero, held once it shows; kept for the next. */
    static uint8_t msg_id = 0;
    tx_level_t     level;
    tx_level_start(&level, LV_MIN(param_f_get(cfg.pwr()), TX_PLAYER_MAX_PWR_W));
    tx_info_refresh(&msg_id, NULL, NULL, NULL); /* readings from before this one don't count */

    float    prev_gain_offset = gain_offset;
    int16_t *ptr              = samples;
    size_t   part;

    bool aborted = false;
    while (true) {
        float pwr = 0.0f, alc = 0.0f;
        bool  have = tx_info_refresh(&msg_id, &alc, &pwr, NULL);
        gain_offset = clamp_gain(gain_offset + tx_level_block(&level, have, pwr, alc));
        if (n_samples <= 0) {
            break;
        }
        if (abort_check && abort_check(abort_check_ctx)) {
            aborted = true;
            break;
        }
        part = LV_MIN(1024 * 2, n_samples);
        if (gain_offset == prev_gain_offset) {
            if (gain_offset != 0.0f) {
                audio_gain_db(ptr, part, gain_offset, ptr);
            }
        } else {
            audio_gain_db_transition(ptr, part, prev_gain_offset, gain_offset, ptr);
            prev_gain_offset = gain_offset;
        }
        audio_play(ptr, part); /* R1CBU 1.0: its default player, AUDIO_PLAY_RATE */
        n_samples -= part;
        ptr       += part;
    }

    /* The learned gain offset is shared by FT8 and JS8: same audio path. */
    param_f_set(cfg.ft8.output_gain_offset(), gain_offset - base_gain_offset + play_gain_offset);
    audio_play_wait();
    radio_set_modem(false);
    radio_set_freq((int32_t)radio_freq);
    audio_set_play_vol(param_f_get(cfg.audio.play_gain_db()));

    return !aborted;
}
