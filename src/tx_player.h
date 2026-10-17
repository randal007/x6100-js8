/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - digital-mode TX player
 *
 *  Plays a prepared TX waveform with the radio keyed, setting the audio
 *  gain from the radio's ALC and power readback (tx_level.c: ramped up to
 *  the ALC's onset, then held). JS8's; FT8 on R1CBU 1.0 has its
 *  own (src/ft8/tx_worker.c). They share the learned gain offset.
 *
 *  The waveform is synthesised around TX_PLAYER_AUDIO_HZ; while it plays
 *  the VFO is shifted so the signal lands at the requested audio offset,
 *  and restored afterwards. Blocks for the length of the waveform, so call
 *  it from a worker thread.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Audio tone the waveform is centred on. */
#define TX_PLAYER_AUDIO_HZ 1325

/* Digital modes are capped at this power. */
#define TX_PLAYER_MAX_PWR_W 5.0f

/* Audio sent per pass; the ALC and power are read between passes. */
#define TX_PLAYER_BLOCK 2048

/* Abort-check callback. Return true to stop TX after the current block. */
typedef bool (*tx_abort_fn_t)(void *ctx);

/* Gives the next audio: up to n samples into buf, returns how many, 0 at
 * the end. */
typedef size_t (*tx_fill_fn_t)(int16_t *buf, size_t n, void *ctx);

/* Starting gain offset (dB) for the radio's firmware and power setting. */
float tx_player_base_gain_offset(void);

/* Key the radio and play n_samples of int16 audio at AUDIO_PLAY_RATE,
 * shifted to tx_offset_hz (samples not modified). Returns false if
 * abort_check stopped it early. */
bool tx_player_play(int16_t      *samples,
                    uint32_t      n_samples,
                    int32_t       tx_offset_hz,
                    float         base_gain_offset,
                    tx_abort_fn_t abort_check,
                    void         *abort_check_ctx);

/* The same with the audio asked for block by block from fill (int16 at
 * AUDIO_PLAY_RATE), for transmissions too long to keep in memory (WeFax,
 * SSTV). */
bool tx_player_stream(tx_fill_fn_t  fill,
                      void         *fill_ctx,
                      int32_t       tx_offset_hz,
                      float         base_gain_offset,
                      tx_abort_fn_t abort_check,
                      void         *abort_check_ctx);

#ifdef __cplusplus
}
#endif
