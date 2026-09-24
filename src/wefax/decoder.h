#pragma once

#include <stdint.h>
#include <stdbool.h>


#define WEFAX_SAMPLE_RATE     11025
#define WEFAX_CENTER_HZ        1500
#define WEFAX_SHIFT_HZ          850
#define WEFAX_LPM               120
#define WEFAX_IOC               576
#define WEFAX_WIDTH            1809
#define WEFAX_MAX_ROWS         4000


typedef enum {
    WEFAX_MODE_IDLE = 0,
    WEFAX_MODE_CONT,
    WEFAX_MODE_AUTO
} wefax_mode_t;


typedef struct wefax_decoder wefax_decoder_t;


typedef void (*wefax_row_callback_t)(
    const uint8_t *row,
    unsigned int width,
    unsigned int row_number,
    void *user_data
);


wefax_decoder_t *wefax_decoder_create(void);

void wefax_decoder_destroy(
    wefax_decoder_t *decoder
);

void wefax_decoder_reset(
    wefax_decoder_t *decoder
);

void wefax_decoder_set_mode(
    wefax_decoder_t *decoder,
    wefax_mode_t mode
);

wefax_mode_t wefax_decoder_get_mode(
    const wefax_decoder_t *decoder
);

void wefax_decoder_process(
    wefax_decoder_t *decoder,
    const float *samples,
    unsigned int count
);

void wefax_decoder_set_row_callback(
    wefax_decoder_t *decoder,
    wefax_row_callback_t callback,
    void *user_data
);


uint64_t wefax_decoder_sample_count(
    const wefax_decoder_t *decoder
);

uint64_t wefax_decoder_block_count(
    const wefax_decoder_t *decoder
);

uint32_t wefax_decoder_row_count(
    const wefax_decoder_t *decoder
);

uint8_t wefax_decoder_gray_min(
    const wefax_decoder_t *decoder
);

uint8_t wefax_decoder_gray_max(
    const wefax_decoder_t *decoder
);

float wefax_decoder_gray_average(
    const wefax_decoder_t *decoder
);

bool wefax_decoder_auto_start_detected(
    const wefax_decoder_t *decoder
);

float wefax_decoder_auto_apt_frequency(
    const wefax_decoder_t *decoder
);


bool wefax_decoder_auto_stop_detected(
    const wefax_decoder_t *decoder
);
