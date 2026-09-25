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
#include "params/params.h"
#include "radio.h"
#include "tx_info.h"

#include <aether_radio/x6100_control/control.h>

#define GAIN_MIN_DB (-30.0f)
#define GAIN_MAX_DB 0.0f

/* ALC-driven gain correction (legacy formula from dialog_ft8.c). */
static float get_correction(void) {
    static uint8_t msg_id = 0;
    float correction = 0.0f;
    float pwr        = 0.0f;
    float alc        = 0.0f;

    if (tx_info_refresh(&msg_id, &alc, &pwr, NULL)) {
        float target_pwr = LV_MIN(param_f_get(cfg_pwr), TX_PLAYER_MAX_PWR_W);
        if (alc > 0.5f) {
            correction = log10f(log10f(11.1f - alc)) * 20.0f - 0.38f;
        } else if (pwr < target_pwr * 0.8f && target_pwr - pwr > 0.1f) {
            /* Relative, not "0.5 W short": at 0.5-1 W a fixed 0.5 W margin
             * meant the drive could only ever go down, and the learned
             * (saved) offset stayed low until power was raised and lowered. */
            correction = log10f(target_pwr / (pwr + 0.01f)) * 10.0f;
            if (correction > 3.0f) correction = 3.0f;
        }
    }
    return correction;
}

float tx_player_base_gain_offset(void) {
    float target_pwr = LV_MIN(param_f_get(cfg_pwr), TX_PLAYER_MAX_PWR_W);
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
    if (param_f_get(cfg_pwr) > TX_PLAYER_MAX_PWR_W) {
        radio_set_pwr(TX_PLAYER_MAX_PWR_W);
    }

    float gain_offset      = base_gain_offset + params.ft8_output_gain_offset.x;
    float play_gain_offset = audio_set_play_vol(gain_offset + 6.0f);
    gain_offset           -= play_gain_offset;

    uint64_t radio_freq = cparam_i_get(cfg_fg_freq);
    radio_set_freq((int32_t)radio_freq + tx_offset_hz - TX_PLAYER_AUDIO_HZ);
    radio_set_modem(true);

    float    prev_gain_offset = gain_offset;
    size_t   counter          = 0;
    int16_t *ptr              = samples;
    size_t   part;

    bool aborted = false;
    while (true) {
        if (counter > 30) {
            gain_offset += get_correction() * 0.4f;
            if (gain_offset > GAIN_MAX_DB) {
                gain_offset = GAIN_MAX_DB;
            } else if (gain_offset < GAIN_MIN_DB) {
                gain_offset = GAIN_MIN_DB;
            }
        }
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
        audio_play(ptr, part);
        n_samples -= part;
        ptr       += part;
        counter++;
    }

    /* The learned gain offset is shared by FT8 and JS8: same audio path. */
    params_float_set(&params.ft8_output_gain_offset,
                     gain_offset - base_gain_offset + play_gain_offset);
    audio_play_wait();
    radio_set_modem(false);
    radio_set_freq((int32_t)radio_freq);
    audio_set_play_vol(params.play_gain_db_f.x);

    return !aborted;
}
