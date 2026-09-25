/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI
 *
 *  Copyright (c) 2022-2023 Belousov Oleg aka R1CBU
 */

#pragma once

#include <gps.h>
#include <stdbool.h>

typedef enum {
    GPS_STATUS_WAITING=0,
    GPS_STATUS_WORKING,
    GPS_STATUS_RESTARTING,
    GPS_STATUS_EXITED,
} gps_status_t;

void gps_init();

gps_status_t gps_status();

/* Latest 2D/3D fix from gpsd and its age in seconds; false if none yet. */
bool gps_last_fix(double *lat, double *lon, int *age_s);
