/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 (receive)
 */

#pragma once

#include "dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern dialog_t *dialog_js8;

/* The callsign the list selection points at, if any (used by
 * tools/js8_ui_harness). */
bool dialog_js8_selected_call(char *call, unsigned len);

#ifdef __cplusplus
}
#endif
