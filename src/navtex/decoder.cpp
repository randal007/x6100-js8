/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Hany El Imam - 1KO125 firmware for the Xiegu X6100
 *
 * Additional term under section 7(b) of the GNU GPL version 3: any work that
 * contains this file, or a work based on it, must preserve the author
 * attribution "Hany El Imam" and the credits of the WEFAX, NAVTEX/SITOR-B,
 * DSC and SSTV applications in the Appropriate Legal Notices displayed by
 * its user interface (in this program: the Info window of each application).
 * See NOTICE-1KO125.md.
 */
/*
 * Xiegu X6100 Sitor-B decoder
 *
 * (c) 2026 Hany El Imam
 * www.hanynet.com - www.murusfirewall.com - https://github.com/TheMurusTeam
 *
*/



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
     * 12000 Hz is requested from the X6100 DSP audio path.
     * only_sitor_b=true keeps this first integration stage simple:
     * every decoded character is emitted immediately to stdout.
     */
    decoder->rx = new (std::nothrow) navtex_rx(
        12000,
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

void navtex_decoder_set_char_q_callback(navtex_decoder_t *decoder,
                                        navtex_char_q_cb_t cb,
                                        void *userdata)
{
    if (!decoder || !decoder->rx)
        return;

    decoder->rx->set_char_q_callback(cb, userdata);
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
