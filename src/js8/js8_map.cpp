/*
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 *
 *  Xiegu X6100 LVGL GUI - JS8 map (C API)
 */

#include "js8_map.h"

#include "callsign_place.hpp"
#include "geo.hpp"

#include <cstdio>
#include <cstring>
#include <mutex>

using namespace x6100::js8;

namespace {

std::mutex  countries_mutex;
CountryFile countries;

void copy_str(char *dst, std::size_t cap, const std::string &src) {
    if (!dst || !cap) return;
    std::snprintf(dst, cap, "%s", src.c_str());
}

geo::View to_view(const js8_map_view_t *v) {
    geo::View g;
    g.lon_c  = v->lon_c;
    g.merc_c = v->merc_c;
    g.px_deg = v->px_deg;
    g.width  = v->width;
    g.height = v->height;
    return g;
}

void from_view(const geo::View &g, js8_map_view_t *v) {
    v->lon_c  = g.lon_c;
    v->merc_c = g.merc_c;
    v->px_deg = g.px_deg;
    v->width  = g.width;
    v->height = g.height;
}

} // namespace

extern "C" bool js8_map_load_countries(const char *path) {
    CountryFile f;
    if (!path || !f.load(path)) return false;
    std::lock_guard<std::mutex> lock(countries_mutex);
    countries = std::move(f);
    return true;
}

extern "C" bool js8_map_countries_loaded(void) {
    std::lock_guard<std::mutex> lock(countries_mutex);
    return countries.loaded();
}

extern "C" bool js8_map_place(const char *call, const char *grid, js8_map_place_t *out) {
    if (!out) return false;
    std::memset(out, 0, sizeof(*out));
    std::optional<CallsignPlace> pl;
    if (call && *call) {
        std::lock_guard<std::mutex> lock(countries_mutex);
        pl = countries.find(call);
    }
    if (pl) copy_str(out->continent, sizeof(out->continent), pl->continent);
    if (grid && *grid) {
        if (auto c = geo::grid_center(grid)) {
            out->lat = c->lat;
            out->lon = c->lon;
            copy_str(out->where, sizeof(out->where), grid);
            return true;
        }
    }
    if (!pl) return false;
    out->lat    = pl->pos.lat;
    out->lon    = pl->pos.lon;
    out->approx = true;
    copy_str(out->where, sizeof(out->where), pl->region.empty() ? pl->country : pl->region);
    return true;
}

extern "C" bool js8_map_my_continent(const char *call, const char *grid, char out[3]) {
    if (!out) return false;
    out[0] = '\0';
    std::lock_guard<std::mutex> lock(countries_mutex);
    if (call && *call) {
        if (auto pl = countries.find(call)) {
            copy_str(out, 3, pl->continent);
            return true;
        }
    }
    if (grid && *grid) {
        if (auto c = geo::grid_center(grid)) {
            if (auto cont = countries.continent_near(*c)) {
                copy_str(out, 3, *cont);
                return true;
            }
        }
    }
    return false;
}

extern "C" bool js8_map_choose_view(js8_map_mode_t mode, double my_lat, double my_lon, const char *my_continent,
                                    const js8_map_point_t *pts, unsigned n, int width, int height,
                                    js8_map_view_t *out) {
    if (!out) return false;
    geo::LatLon home{my_lat, my_lon};
    if (mode == JS8_MAP_WORLD) {
        geo::View v;
        v.width  = width;
        v.height = height;
        v.px_deg = width / 360.0;
        v.lon_c  = my_lon;
        v.merc_c = 0;
        from_view(v, out);
        return true;
    }
    bool                     know_mine = my_continent && my_continent[0];
    std::vector<geo::LatLon> all, near;
    bool                     outside = false;
    for (unsigned i = 0; pts && i < n; i++) {
        geo::LatLon p{pts[i].lat, pts[i].lon};
        all.push_back(p);
        // A station whose continent is unknown counts as near: it can't
        // prove there's DX, and it mustn't flip the view.
        bool on_mine = !know_mine || !pts[i].continent[0] || std::strcmp(pts[i].continent, my_continent) == 0;
        if (on_mine) near.push_back(p);
        else outside = true;
    }
    bool world = mode == JS8_MAP_AUTO && outside;
    from_view(geo::fit(home, world ? all : near, width, height), out);
    return world;
}

extern "C" void js8_map_project(const js8_map_view_t *v, double lat, double lon, float *x, float *y) {
    double dx = 0, dy = 0;
    if (v) to_view(v).project({lat, lon}, dx, dy);
    if (x) *x = (float)dx;
    if (y) *y = (float)dy;
}

extern "C" bool js8_map_grid_rect(const js8_map_view_t *v, const char *grid, float *x0, float *y0, float *x1,
                                  float *y1) {
    if (!v || !grid) return false;
    auto b = geo::grid_box(grid);
    if (!b) return false;
    geo::View g = to_view(v);
    // Project the south-west corner, then place the east edge by the box's
    // width so a square never splits across the date line.
    double ax, ay, bx, by;
    g.project(b->sw, ax, ay);
    g.project({b->ne.lat, b->sw.lon}, bx, by);
    double w = (b->ne.lon - b->sw.lon) * g.px_deg;
    if (x0) *x0 = (float)ax;
    if (x1) *x1 = (float)(ax + w);
    if (y0) *y0 = (float)by; // north edge: smaller y
    if (y1) *y1 = (float)ay;
    return true;
}

extern "C" void js8_map_path(const js8_map_view_t *v, double lat1, double lon1, double lat2, double lon2, int n,
                             float *xs, float *ys) {
    if (!v || !xs || !ys || n < 1) return;
    geo::View g   = to_view(v);
    auto      pts = geo::great_circle({lat1, lon1}, {lat2, lon2}, n);
    // Keep the path in one piece: project the first point normally, then
    // every next one from the previous point's unwrapped longitude.
    double x0 = 0, y0 = 0;
    g.project(pts[0], x0, y0);
    double base_lon = geo::unwrap(pts[0].lon, g.lon_c);
    for (int i = 0; i <= n; i++) {
        double lon = base_lon + (pts[i].lon - pts[0].lon); // pts are continuous
        xs[i]      = (float)((lon - g.lon_c) * g.px_deg + g.width / 2.0);
        ys[i]      = (float)(-(geo::mercator_y(pts[i].lat) - g.merc_c) * g.px_deg + g.height / 2.0);
    }
}

extern "C" double js8_map_distance_km(double lat1, double lon1, double lat2, double lon2) {
    return geo::distance_km({lat1, lon1}, {lat2, lon2});
}
