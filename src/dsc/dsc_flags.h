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
 * Xiegu X6100 - country flags for the DSC detail card (see make_flags_db.py)
 *
 * The file is memory mapped read-only; the images point into it, so they
 * cost no RAM and no decoding.
 */
#pragma once

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maps the flags file; returns the number of flags (0 = none). */
int  dsc_flags_open(void);
void dsc_flags_close(void);

/* Flag of an ISO 3166 alpha-2 code ("it"), NULL if there is none. */
const lv_img_dsc_t *dsc_flag(const char *iso);

#ifdef __cplusplus
}
#endif
