#include "decoder.h"
#include "navtex_rx.h"

#include <cstdio>
#include <new>

struct navtex_decoder {
    navtex_rx *rx;
};

navtex_decoder_t *navtex_decoder_create(int reverse)
{
    navtex_decoder_t *decoder = new (std::nothrow) navtex_decoder_t;
    if (!decoder)
        return nullptr;

    /*
     * 11025 Hz is already produced by the X6100 DSP audio path.
     * only_sitor_b=true keeps this first integration stage simple:
     * every decoded character is emitted immediately to stdout.
     */
    decoder->rx = new (std::nothrow) navtex_rx(
        11025,
        true,
        reverse != 0,
        nullptr,
        nullptr,
        stderr
    );

    if (!decoder->rx) {
        delete decoder;
        return nullptr;
    }

    return decoder;
}

void navtex_decoder_set_char_callback(navtex_decoder_t *decoder,
                                      navtex_char_cb_t cb,
                                      void *userdata)
{
    if (!decoder || !decoder->rx)
        return;

    decoder->rx->set_char_callback(cb, userdata);
}

void navtex_decoder_destroy(navtex_decoder_t *decoder)
{
    if (!decoder)
        return;

    delete decoder->rx;
    delete decoder;
}

void navtex_decoder_process(navtex_decoder_t *decoder,
                            const float *samples,
                            unsigned int n)
{
    if (!decoder || !decoder->rx || !samples || n == 0)
        return;

    decoder->rx->process_data(samples, (int)n);
}
