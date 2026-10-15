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
#pragma once

#include <stdbool.h>

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_navtex;
bool dialog_navtex_is_active(void);

/* Microphone while NAVTEX is open: V/M opens / closes the frequency list,
 * UP (dir < 0) / DOWN (dir > 0) step through it.  false when not open. */
bool dialog_navtex_vm_toggle(void);
bool dialog_navtex_mic_step(int dir);

#ifdef __cplusplus
}
#endif
