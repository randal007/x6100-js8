#ifndef X6100_NAVTEX_DECODER_H
#define X6100_NAVTEX_DECODER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct navtex_decoder navtex_decoder_t;
typedef void (*navtex_char_cb_t)(int c, void *userdata);

/*
 * Create the validated NAVTEX/SITOR-B decoder.
 * Input audio is mono float at 11025 Hz, same stream used by WeFax.
 * reverse=false matches normal polarity; pass true when RV is required.
 * For this first integration stage decoded characters are written to stdout.
 */
navtex_decoder_t *navtex_decoder_create(int reverse);
void navtex_decoder_destroy(navtex_decoder_t *decoder);
void navtex_decoder_set_char_callback(navtex_decoder_t *decoder, navtex_char_cb_t cb, void *userdata);
void navtex_decoder_process(navtex_decoder_t *decoder,
                            const float *samples,
                            unsigned int n);

#ifdef __cplusplus
}
#endif

#endif
