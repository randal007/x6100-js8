/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - JS8 map (C API)
 *
 *  Where stations go on the Show Map view and which part of the world it
 *  shows (docs/MAP_PLAN.md). Everything here is maths and lookups: no LVGL.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AD1C's country file (cty.dat). Loaded once; the lookups below fall back
 * to "unknown" without it. False if the file can't be read or parsed. */
bool js8_map_load_countries(const char *path);
bool js8_map_countries_loaded(void);

/* Where a station goes: its grid if it has one (the square's centre), else
 * its callsign's region or country (`approx` set: drawn hollow). */
typedef struct {
    double lat, lon;
    bool   approx;        /* placed by callsign, not grid */
    char   continent[3];  /* "NA" ... "" unknown */
    char   where[40];     /* the grid, else "British Columbia", "Canada" ... */
} js8_map_place_t;

bool js8_map_place(const char *call, const char *grid, js8_map_place_t *out);

/* Your continent: from your callsign, else the country nearest your grid.
 * False (empty out) if neither is known. */
bool js8_map_my_continent(const char *call, const char *grid, char out[3]);

/* The view button: Auto switches between your continent and the world. */
typedef enum {
    JS8_MAP_AUTO,
    JS8_MAP_CLOSE,
    JS8_MAP_WORLD,
} js8_map_mode_t;

/* A view: the centre, the scale and the size in pixels. */
typedef struct {
    double lon_c, merc_c, px_deg;
    int    width, height;
} js8_map_view_t;

/* One station for choosing the view. */
typedef struct {
    double lat, lon;
    char   continent[3];
} js8_map_point_t;

/* The view for the stations shown:
 *   Auto  - close-in while every station is on `my_continent` (or it's
 *           unknown), else fitted to them all ("world");
 *   Close - fitted to you and the stations on your continent only;
 *   World - the whole world, centred on you.
 * Fitted views keep a margin and are never closer in than about 40 x 18
 * degrees. Returns true when the result is a world view (for the legend). */
bool js8_map_choose_view(js8_map_mode_t mode, double my_lat, double my_lon, const char *my_continent,
                         const js8_map_point_t *pts, unsigned n, int width, int height, js8_map_view_t *out);

/* Screen position (pixels, (0,0) top left) of a point in a view. */
void js8_map_project(const js8_map_view_t *v, double lat, double lon, float *x, float *y);

/* The screen rectangle of a locator's area (its 4 characters for a
 * station's square): x0 < x1, y0 < y1. False if `grid` isn't a locator. */
bool js8_map_grid_rect(const js8_map_view_t *v, const char *grid, float *x0, float *y0, float *x1, float *y1);

/* The curved path from 1 to 2 on the screen: `n` + 1 points into xs/ys
 * (both `n` + 1 long). */
void js8_map_path(const js8_map_view_t *v, double lat1, double lon1, double lat2, double lon2, int n, float *xs,
                  float *ys);

/* Great-circle distance, km. */
double js8_map_distance_km(double lat1, double lon1, double lat2, double lon2);

#ifdef __cplusplus
}
#endif
