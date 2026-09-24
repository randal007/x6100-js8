/*
 * DWD WeFax decoder
 *
 * Fixed format:
 *
 *   Sample rate : 11025 Hz
 *   Center      : 1500 Hz
 *   Shift       : 850 Hz
 *   IOC         : 576
 *   LPM         : 120
 *   Width       : 1809 pixels
 */

#include "decoder.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>


#define WEFAX_PI       3.14159265358979323846f
#define WEFAX_LPF_HZ   600.0f

/*
 * Complex baseband FIR low-pass.
 *
 * 63 taps, Hamming-windowed sinc, generated once when the
 * decoder is created.  Cutoff remains 600 Hz: this experiment
 * changes only the filter shape, not the nominal bandwidth.
 */
#define WEFAX_FIR_TAPS  63


/*
 * Tone histogram used only for diagnostics.
 *
 * We are interested in the useful WEFAX audio
 * region. 500..2500 Hz gives plenty of margin
 * around the nominal 1075..1925 Hz range.
 *
 * One bin = 1 Hz.
 */
#define TONE_HIST_MIN_HZ     500
#define TONE_HIST_MAX_HZ    2500
#define TONE_HIST_SIZE       \
    (TONE_HIST_MAX_HZ - TONE_HIST_MIN_HZ + 1)


/*
 * DWD IOC 576 APT START detector.
 */
#define WEFAX_APT_START_HZ          300.0f
#define WEFAX_APT_START_TOLERANCE     30.0f
#define WEFAX_APT_WINDOW_SAMPLES    (WEFAX_SAMPLE_RATE / 2)
#define WEFAX_APT_WHITE_THRESHOLD   215
#define WEFAX_APT_BLACK_THRESHOLD    40
#define WEFAX_APT_START_WINDOWS       2
#define WEFAX_APT_STOP_HZ           450.0f
#define WEFAX_APT_STOP_TOLERANCE      30.0f // 6.0f
#define WEFAX_APT_STOP_WINDOWS        1


/* Confirm a provisional START only after a sustained black band. */
#define WEFAX_CONFIRM_BLACK_THRESHOLD  50
#define WEFAX_CONFIRM_BLACK_PERCENT    90
#define WEFAX_CONFIRM_BLACK_ROWS        4


struct wefax_decoder {

    wefax_mode_t mode;

    uint64_t sample_count;
    uint64_t block_count;


    /*
     * Complex mixer.
     */
    float oscillator_phase;


    /*
     * Complex baseband FIR low-pass.
     */
    float fir_coeff[WEFAX_FIR_TAPS];
    float fir_i[WEFAX_FIR_TAPS];
    float fir_q[WEFAX_FIR_TAPS];

    unsigned int fir_pos;


    /*
     * Previous complex sample for
     * phase discriminator.
     */
    float previous_i;
    float previous_q;

    bool previous_valid;


    /*
     * Grayscale diagnostics.
     */
    uint8_t gray_min;
    uint8_t gray_max;

    double gray_sum;
    uint64_t gray_count;


    /*
     * Tone-frequency diagnostics.
     */
    double tone_sum;
    uint64_t tone_count;

    unsigned int tone_print_samples;

    uint32_t tone_histogram[TONE_HIST_SIZE];


    /*
     * WEFAX line generator.
     */
    double line_position;

    unsigned int current_pixel;

    double pixel_sum;
    unsigned int pixel_samples;

    uint8_t row[WEFAX_WIDTH];

    uint32_t row_count;


    /*
     * Completed-row callback.
     */
    /* AUTO / APT START detector. */
    bool apt_is_white;
    bool apt_level_valid;
    unsigned int apt_window_samples;
    unsigned int apt_transitions;
    unsigned int apt_start_windows;
    unsigned int apt_stop_windows;
    float apt_frequency;
    bool apt_start_detected;
    uint32_t apt_start_generation;
    bool apt_fax_confirmed;
    unsigned int apt_black_rows;
    bool apt_stop_detected;


    wefax_row_callback_t row_callback;
    void *row_callback_data;
};


/*
 * Return a percentile from the current
 * one-second tone histogram.
 */
static float tone_percentile(
    const wefax_decoder_t *decoder,
    float percentile)
{
    uint64_t total = 0;


    for (
        unsigned int i = 0;
        i < TONE_HIST_SIZE;
        i++
    ) {

        total += decoder->tone_histogram[i];
    }


    if (total == 0) {
        return 0.0f;
    }


    uint64_t target =
        (uint64_t)(
            percentile *
            (double)(total - 1)
        );


    uint64_t accumulated = 0;


    for (
        unsigned int i = 0;
        i < TONE_HIST_SIZE;
        i++
    ) {

        accumulated +=
            decoder->tone_histogram[i];


        if (accumulated > target) {

            return
                (float)(
                    TONE_HIST_MIN_HZ + i
                );
        }
    }


    return (float)TONE_HIST_MAX_HZ;
}


/*
 * Clear only the one-second tone
 * diagnostic window.
 */
static void reset_tone_window(
    wefax_decoder_t *decoder)
{
    decoder->tone_sum = 0.0;
    decoder->tone_count = 0;

    decoder->tone_print_samples = 0;


    memset(
        decoder->tone_histogram,
        0,
        sizeof(decoder->tone_histogram)
    );
}


/*
 * Build a unity-DC-gain low-pass FIR using a windowed sinc.
 */
static void init_fir(
    wefax_decoder_t *decoder)
{
    const int middle =
        (WEFAX_FIR_TAPS - 1) / 2;

    const float fc =
        WEFAX_LPF_HZ /
        (float)WEFAX_SAMPLE_RATE;

    float sum = 0.0f;


    for (
        int n = 0;
        n < WEFAX_FIR_TAPS;
        n++
    ) {

        const int k =
            n - middle;

        float sinc;


        if (k == 0) {

            sinc =
                2.0f * fc;
        }
        else {

            sinc =
                sinf(
                    2.0f *
                    WEFAX_PI *
                    fc *
                    (float)k
                ) /
                (
                    WEFAX_PI *
                    (float)k
                );
        }


        const float window =
            0.54f -
            0.46f *
            cosf(
                2.0f *
                WEFAX_PI *
                (float)n /
                (float)(
                    WEFAX_FIR_TAPS - 1
                )
            );


        decoder->fir_coeff[n] =
            sinc * window;

        sum +=
            decoder->fir_coeff[n];
    }


    /*
     * Normalize DC gain to exactly 1.
     */
    if (sum != 0.0f) {

        for (
            int n = 0;
            n < WEFAX_FIR_TAPS;
            n++
        ) {

            decoder->fir_coeff[n] /=
                sum;
        }
    }
}


static void reset_dsp(
    wefax_decoder_t *decoder)
{
    decoder->oscillator_phase = 0.0f;

    memset(
        decoder->fir_i,
        0,
        sizeof(decoder->fir_i)
    );

    memset(
        decoder->fir_q,
        0,
        sizeof(decoder->fir_q)
    );

    decoder->fir_pos = 0;

    decoder->previous_i = 0.0f;
    decoder->previous_q = 0.0f;

    decoder->previous_valid = false;


    decoder->gray_min = 255;
    decoder->gray_max = 0;

    decoder->gray_sum = 0.0;
    decoder->gray_count = 0;


    reset_tone_window(decoder);

    decoder->apt_is_white = false;
    decoder->apt_level_valid = false;
    decoder->apt_window_samples = 0;
    decoder->apt_transitions = 0;
    decoder->apt_start_windows = 0;
    decoder->apt_stop_windows = 0;
    decoder->apt_frequency = 0.0f;
    decoder->apt_start_detected = false;
    decoder->apt_start_generation = 0;
    decoder->apt_fax_confirmed = false;
    decoder->apt_black_rows = 0;
    decoder->apt_stop_detected = false;


    decoder->line_position = 0.0;

    decoder->current_pixel = 0;

    decoder->pixel_sum = 0.0;
    decoder->pixel_samples = 0;


    memset(
        decoder->row,
        0,
        sizeof(decoder->row)
    );


    decoder->row_count = 0;
}


wefax_decoder_t *wefax_decoder_create(void)
{
    wefax_decoder_t *decoder =
        calloc(
            1,
            sizeof(wefax_decoder_t)
        );


    if (!decoder) {
        return NULL;
    }


    decoder->mode = WEFAX_MODE_IDLE;


    init_fir(decoder);


    reset_dsp(decoder);


    return decoder;
}


void wefax_decoder_destroy(
    wefax_decoder_t *decoder)
{
    if (!decoder) {
        return;
    }


    free(decoder);
}


void wefax_decoder_reset(
    wefax_decoder_t *decoder)
{
    if (!decoder) {
        return;
    }


    decoder->sample_count = 0;
    decoder->block_count = 0;


    reset_dsp(decoder);
}


void wefax_decoder_set_mode(
    wefax_decoder_t *decoder,
    wefax_mode_t mode)
{
    if (!decoder) {
        return;
    }


    decoder->mode = mode;


    wefax_decoder_reset(decoder);
}


wefax_mode_t wefax_decoder_get_mode(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return WEFAX_MODE_IDLE;
    }


    return decoder->mode;
}


void wefax_decoder_set_row_callback(
    wefax_decoder_t *decoder,
    wefax_row_callback_t callback,
    void *user_data)
{
    if (!decoder) {
        return;
    }


    decoder->row_callback = callback;
    decoder->row_callback_data = user_data;
}


void wefax_decoder_process(
    wefax_decoder_t *decoder,
    const float *samples,
    unsigned int count)
{
    if (
        !decoder ||
        !samples ||
        count == 0
    ) {

        return;
    }


    if (decoder->mode == WEFAX_MODE_IDLE) {
        return;
    }


    const float phase_step =
        2.0f *
        WEFAX_PI *
        (float)WEFAX_CENTER_HZ /
        (float)WEFAX_SAMPLE_RATE;


    const float deviation_ratio =
        (
            (float)WEFAX_SAMPLE_RATE /
            (float)WEFAX_SHIFT_HZ
        ) /
        (2.0f * WEFAX_PI);


    const double line_step =
        (double)WEFAX_LPM /
        (
            60.0 *
            (double)WEFAX_SAMPLE_RATE
        );


    for (
        unsigned int n = 0;
        n < count;
        n++
    ) {

        const float sample =
            samples[n];


        /*
         * Mix center frequency to complex DC.
         */
        const float c =
            cosf(
                decoder->oscillator_phase
            );

        const float s =
            sinf(
                decoder->oscillator_phase
            );


        const float i =
            sample * c;

        const float q =
            -sample * s;


        decoder->oscillator_phase +=
            phase_step;


        if (
            decoder->oscillator_phase >=
            2.0f * WEFAX_PI
        ) {

            decoder->oscillator_phase -=
                2.0f * WEFAX_PI;
        }


        /*
         * Complex FIR low-pass.
         *
         * Store newest I/Q pair in the circular buffer, then
         * convolve newest -> oldest with the symmetric FIR.
         */
        decoder->fir_i[
            decoder->fir_pos
        ] = i;

        decoder->fir_q[
            decoder->fir_pos
        ] = q;


        float filtered_i = 0.0f;
        float filtered_q = 0.0f;

        unsigned int fir_index =
            decoder->fir_pos;


        for (
            unsigned int tap = 0;
            tap < WEFAX_FIR_TAPS;
            tap++
        ) {

            const float coefficient =
                decoder->fir_coeff[tap];


            filtered_i +=
                coefficient *
                decoder->fir_i[fir_index];

            filtered_q +=
                coefficient *
                decoder->fir_q[fir_index];


            if (fir_index == 0) {

                fir_index =
                    WEFAX_FIR_TAPS - 1;
            }
            else {

                fir_index--;
            }
        }


        decoder->fir_pos++;

        if (
            decoder->fir_pos >=
            WEFAX_FIR_TAPS
        ) {

            decoder->fir_pos = 0;
        }


        if (decoder->previous_valid) {

            /*
             * arg(conj(previous) * current)
             */
            const float real =
                decoder->previous_i *
                filtered_i +
                decoder->previous_q *
                filtered_q;


            const float imag =
                decoder->previous_i *
                filtered_q -
                decoder->previous_q *
                filtered_i;


            const float phase_difference =
                atan2f(
                    imag,
                    real
                );


            /*
             * ------------------------------------------------
             * Actual tone frequency
             * ------------------------------------------------
             */
            const float baseband_hz =
                phase_difference *
                (float)WEFAX_SAMPLE_RATE /
                (2.0f * WEFAX_PI);


            const float tone_hz =
                (float)WEFAX_CENTER_HZ -
                baseband_hz;


            /*
             * ------------------------------------------------
             * Tone diagnostic
             * ------------------------------------------------
             *
             * Store only values inside a sensible
             * WEFAX audio range. This prevents large
             * discriminator glitches from destroying
             * the percentile measurement.
             */
            if (
                tone_hz >=
                    (float)TONE_HIST_MIN_HZ &&
                tone_hz <=
                    (float)TONE_HIST_MAX_HZ
            ) {

                unsigned int bin =
                    (unsigned int)(
                        tone_hz -
                        (float)TONE_HIST_MIN_HZ +
                        0.5f
                    );


                if (bin >= TONE_HIST_SIZE) {
                    bin = TONE_HIST_SIZE - 1;
                }


                decoder->tone_histogram[bin]++;


                decoder->tone_sum +=
                    tone_hz;

                decoder->tone_count++;
            }


            decoder->tone_print_samples++;


            /*
             * Print approximately once per second.
             */
            if (
                decoder->tone_print_samples >=
                WEFAX_SAMPLE_RATE
            ) {

                if (
                    decoder->tone_count > 0
                ) {

                    const double average_tone =
                        decoder->tone_sum /
                        (double)
                            decoder->tone_count;


                    const float p01 =
                        tone_percentile(
                            decoder,
                            0.01f
                        );

                    const float p05 =
                        tone_percentile(
                            decoder,
                            0.05f
                        );

                    const float p50 =
                        tone_percentile(
                            decoder,
                            0.50f
                        );

                    const float p95 =
                        tone_percentile(
                            decoder,
                            0.95f
                        );

                    const float p99 =
                        tone_percentile(
                            decoder,
                            0.99f
                        );


                    /*
                    printf(
                        "WEFAX: tone avg=%.1f "
                        "P01=%.0f P05=%.0f "
                        "P50=%.0f P95=%.0f "
                        "P99=%.0f Hz\n",

                        average_tone,
                        p01,
                        p05,
                        p50,
                        p95,
                        p99
                    );
                    */
                }


                reset_tone_window(
                    decoder
                );
            }


            /*
             * ------------------------------------------------
             * WEFAX grayscale demodulation
             * ------------------------------------------------
             *
             * Polarity corrected for DWD:
             * low tone -> white,
             * high tone -> black.
             */
            float gray_f =
                255.0f *
                (
                    0.5f +
                    deviation_ratio *
                    phase_difference
                );


            if (gray_f < 0.0f) {
                gray_f = 0.0f;
            }


            if (gray_f > 255.0f) {
                gray_f = 255.0f;
            }


            const uint8_t gray =
                (uint8_t)(
                    gray_f + 0.5f
                );


            /*
             * Grayscale diagnostics.
             */
            if (
                gray <
                decoder->gray_min
            ) {

                decoder->gray_min =
                    gray;
            }


            if (
                gray >
                decoder->gray_max
            ) {

                decoder->gray_max =
                    gray;
            }


            decoder->gray_sum +=
                gray;

            decoder->gray_count++;


            /*
             * AUTO: DWD APT detector.
             *
             * Before START we look for two consecutive 0.5 s windows
             * around 300 Hz.  After START has been acquired, the same
             * hysteretic transition counter looks for two consecutive
             * windows around 450 Hz to identify APT STOP.
             *
             * Thresholds and counting follow the fldigi WEFAX approach:
             * white > 215, black < 40, count black->white transitions.
             */
            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                !decoder->apt_stop_detected
            ) {
                bool transition = false;

                if (!decoder->apt_level_valid) {
                    if (gray > WEFAX_APT_WHITE_THRESHOLD) {
                        decoder->apt_is_white = true;
                        decoder->apt_level_valid = true;
                    }
                    else if (gray < WEFAX_APT_BLACK_THRESHOLD) {
                        decoder->apt_is_white = false;
                        decoder->apt_level_valid = true;
                    }
                }
                else if (
                    !decoder->apt_is_white &&
                    gray > WEFAX_APT_WHITE_THRESHOLD
                ) {
                    decoder->apt_is_white = true;
                    transition = true;
                }
                else if (
                    decoder->apt_is_white &&
                    gray < WEFAX_APT_BLACK_THRESHOLD
                ) {
                    decoder->apt_is_white = false;
                }

                if (transition) {
                    decoder->apt_transitions++;
                }

                decoder->apt_window_samples++;

                if (
                    decoder->apt_window_samples >=
                    WEFAX_APT_WINDOW_SAMPLES
                ) {
                    decoder->apt_frequency =
                        (float)WEFAX_SAMPLE_RATE *
                        (float)decoder->apt_transitions /
                        (float)decoder->apt_window_samples;

                    
                    printf(
                        "WEFAX AUTO: APT %.1f Hz, transitions=%u\n",
                        decoder->apt_frequency,
                        decoder->apt_transitions
                    );
                    

                    if (!decoder->apt_fax_confirmed) {
                        /*
                         * Until the black confirmation band has been seen,
                         * keep looking for START continuously.  Noise may
                         * create false STARTs; every new valid START simply
                         * discards the provisional image and starts again.
                         */
                        if (
                            fabsf(
                                decoder->apt_frequency -
                                WEFAX_APT_START_HZ
                            ) <= WEFAX_APT_START_TOLERANCE
                        ) {
                            decoder->apt_start_windows++;

                            /*
                            printf(
                                "WEFAX AUTO: APT START candidate "
                                "%.1f Hz (%u/%u)\n",
                                decoder->apt_frequency,
                                decoder->apt_start_windows,
                                WEFAX_APT_START_WINDOWS
                            );
                            */
                        }
                        else {
                            decoder->apt_start_windows = 0;
                        }

                        if (
                            decoder->apt_start_windows >=
                            WEFAX_APT_START_WINDOWS
                        ) {
                            decoder->apt_start_detected = true;
                            decoder->apt_start_generation++;
                            decoder->apt_start_windows = 0;
                            decoder->apt_stop_windows = 0;
                            decoder->apt_black_rows = 0;

                            /*
                             * Start (or restart) provisional image
                             * acquisition exactly at this START.
                             */
                            decoder->line_position = 0.0;
                            decoder->current_pixel = 0;
                            decoder->pixel_sum = 0.0;
                            decoder->pixel_samples = 0;
                            decoder->row_count = 0;

                            memset(
                                decoder->row,
                                0,
                                sizeof(decoder->row)
                            );

                            printf(
                                "WEFAX AUTO: APT START DETECTED - "
                                "provisional fax (%.1f Hz)\n",
                                decoder->apt_frequency
                            );
                        }
                    }
                    else {
                        /*
                         * Once the fax is confirmed, keep watching BOTH
                         * control tones:
                         *
                         *   450 Hz -> normal APT STOP
                         *   300 Hz -> a new fax has started, therefore the
                         *             previous END was missed.
                         *
                         * A new START is deliberately turned into a fresh
                         * provisional fax here, but the GUI/full-resolution
                         * buffer is NOT touched in the audio thread.  The GUI
                         * notices apt_start_generation, saves the previous fax,
                         * then clears its buffer while this new fax remains
                         * provisional until the black confirmation band.
                         */
                        if (
                            fabsf(
                                decoder->apt_frequency -
                                WEFAX_APT_START_HZ
                            ) <= WEFAX_APT_START_TOLERANCE
                        ) {
                            decoder->apt_start_windows++;
                        }
                        else {
                            decoder->apt_start_windows = 0;
                        }

                        if (
                            decoder->apt_start_windows >=
                            WEFAX_APT_START_WINDOWS
                        ) {
                            decoder->apt_start_detected = true;
                            decoder->apt_start_generation++;
                            decoder->apt_start_windows = 0;
                            decoder->apt_stop_windows = 0;
                            decoder->apt_black_rows = 0;
                            decoder->apt_fax_confirmed = false;

                            decoder->line_position = 0.0;
                            decoder->current_pixel = 0;
                            decoder->pixel_sum = 0.0;
                            decoder->pixel_samples = 0;
                            decoder->row_count = 0;

                            memset(
                                decoder->row,
                                0,
                                sizeof(decoder->row)
                            );

                            printf(
                                "WEFAX AUTO: NEW APT START DETECTED - "
                                "previous END missed (%.1f Hz)\n",
                                decoder->apt_frequency
                            );
                        }
                        else {
                            if (
                                fabsf(
                                    decoder->apt_frequency -
                                    WEFAX_APT_STOP_HZ
                                ) <= WEFAX_APT_STOP_TOLERANCE
                            ) {
                                decoder->apt_stop_windows++;
                            }
                            else {
                                decoder->apt_stop_windows = 0;
                            }

                            if (
                                decoder->apt_stop_windows >=
                                WEFAX_APT_STOP_WINDOWS
                            ) {
                                decoder->apt_stop_detected = true;

                                printf(
                                    "WEFAX AUTO: APT STOP DETECTED "
                                    "(%.1f Hz)\n",
                                    decoder->apt_frequency
                                );
                            }
                        }
                    }

                    decoder->apt_window_samples = 0;
                    decoder->apt_transitions = 0;
                }
            }


            /*
             * ------------------------------------------------
             * WEFAX line generation
             * ------------------------------------------------
             *
             * CONT starts immediately.
             * AUTO must remain completely idle visually until APT START
             * has actually been detected.  The demodulator above keeps
             * running so the START detector can do its job.
             */
            if (
                decoder->mode == WEFAX_MODE_AUTO &&
                !decoder->apt_start_detected
            ) {
                continue;
            }

            decoder->line_position +=
                line_step;


            bool line_finished =
                false;


            unsigned int pixel =
                (unsigned int)(
                    decoder->line_position *
                    (double)WEFAX_WIDTH
                );


            if (
                pixel >=
                WEFAX_WIDTH
            ) {

                pixel =
                    WEFAX_WIDTH - 1;

                line_finished =
                    true;
            }


            if (
                pixel ==
                decoder->current_pixel
            ) {

                decoder->pixel_sum +=
                    gray;

                decoder->pixel_samples++;
            }
            else {

                if (
                    decoder->pixel_samples > 0 &&
                    decoder->current_pixel <
                        WEFAX_WIDTH
                ) {

                    decoder->row[
                        decoder->current_pixel
                    ] =
                        (uint8_t)(
                            decoder->pixel_sum /
                            decoder->pixel_samples +
                            0.5
                        );
                }


                while (
                    decoder->current_pixel + 1 <
                        pixel &&
                    decoder->current_pixel + 1 <
                        WEFAX_WIDTH
                ) {

                    decoder->current_pixel++;


                    decoder->row[
                        decoder->current_pixel
                    ] = gray;
                }


                decoder->current_pixel =
                    pixel;

                decoder->pixel_sum =
                    gray;

                decoder->pixel_samples =
                    1;
            }


            if (line_finished) {

                if (
                    decoder->pixel_samples > 0
                ) {

                    decoder->row[
                        decoder->current_pixel
                    ] =
                        (uint8_t)(
                            decoder->pixel_sum /
                            decoder->pixel_samples +
                            0.5
                        );
                }


                /*
                 * AUTO provisional START confirmation.
                 *
                 * The DWD fax START sequence is followed by a long black
                 * band.  A completed row is considered black when at least
                 * 90% of its pixels are below gray level 50.  Four such
                 * consecutive rows (2 seconds at 120 LPM) confirm that the
                 * START was real.  Until then the START detector remains
                 * armed and another START can restart acquisition.
                 */
                if (
                    decoder->mode == WEFAX_MODE_AUTO &&
                    decoder->apt_start_detected &&
                    !decoder->apt_fax_confirmed
                ) {
                    unsigned int black_pixels = 0;

                    for (
                        unsigned int x = 0;
                        x < WEFAX_WIDTH;
                        x++
                    ) {
                        if (
                            decoder->row[x] <
                            WEFAX_CONFIRM_BLACK_THRESHOLD
                        ) {
                            black_pixels++;
                        }
                    }

                    if (
                        black_pixels * 100U >=
                        WEFAX_WIDTH * WEFAX_CONFIRM_BLACK_PERCENT
                    ) {
                        decoder->apt_black_rows++;

                        /*
                        printf(
                            "WEFAX AUTO: BLACK %u/%u\n",
                            decoder->apt_black_rows,
                            WEFAX_CONFIRM_BLACK_ROWS
                        );
                        */

                        if (
                            decoder->apt_black_rows >=
                            WEFAX_CONFIRM_BLACK_ROWS
                        ) {
                            decoder->apt_fax_confirmed = true;
                            decoder->apt_start_windows = 0;
                            decoder->apt_stop_windows = 0;

                            printf(
                                "WEFAX AUTO: FAX CONFIRMED\n"
                            );
                        }
                    }
                    else {
                        decoder->apt_black_rows = 0;
                    }
                }

                if (
                    decoder->row_callback
                ) {

                    decoder->row_callback(
                        decoder->row,
                        WEFAX_WIDTH,
                        decoder->row_count,
                        decoder->row_callback_data
                    );
                }


                decoder->row_count++;


                /*
                 * Keep the fractional part.
                 */
                decoder->line_position -=
                    1.0;


                decoder->current_pixel =
                    (unsigned int)(
                        decoder->line_position *
                        (double)WEFAX_WIDTH
                    );


                if (
                    decoder->current_pixel >=
                    WEFAX_WIDTH
                ) {

                    decoder->current_pixel =
                        0;
                }


                decoder->pixel_sum =
                    0.0;

                decoder->pixel_samples =
                    0;


                memset(
                    decoder->row,
                    0,
                    sizeof(decoder->row)
                );
            }
        }


        decoder->previous_i =
            filtered_i;

        decoder->previous_q =
            filtered_q;

        decoder->previous_valid =
            true;
    }


    decoder->sample_count +=
        count;

    decoder->block_count++;
}


uint64_t wefax_decoder_sample_count(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return 0;
    }


    return decoder->sample_count;
}


uint64_t wefax_decoder_block_count(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return 0;
    }


    return decoder->block_count;
}


uint32_t wefax_decoder_row_count(
    const wefax_decoder_t *decoder)
{
    if (!decoder) {
        return 0;
    }


    return decoder->row_count;
}


uint8_t wefax_decoder_gray_min(
    const wefax_decoder_t *decoder)
{
    if (
        !decoder ||
        decoder->gray_count == 0
    ) {

        return 0;
    }


    return decoder->gray_min;
}


uint8_t wefax_decoder_gray_max(
    const wefax_decoder_t *decoder)
{
    if (
        !decoder ||
        decoder->gray_count == 0
    ) {

        return 0;
    }


    return decoder->gray_max;
}


float wefax_decoder_gray_average(
    const wefax_decoder_t *decoder)
{
    if (
        !decoder ||
        decoder->gray_count == 0
    ) {

        return 0.0f;
    }


    return
        (float)(
            decoder->gray_sum /
            (double)
                decoder->gray_count
        );
}

bool wefax_decoder_auto_start_detected(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_start_detected : false;
}


float wefax_decoder_auto_apt_frequency(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_frequency : 0.0f;
}


bool wefax_decoder_auto_stop_detected(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_stop_detected : false;
}


uint32_t wefax_decoder_auto_start_generation(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_start_generation : 0;
}


bool wefax_decoder_auto_fax_confirmed(
    const wefax_decoder_t *decoder)
{
    return decoder ? decoder->apt_fax_confirmed : false;
}
