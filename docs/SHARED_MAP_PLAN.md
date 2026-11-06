# Shared map: one map for every app (started 2026-11-05)

VE7NHW asked for a map in FT8/FT4/FT2 and WSPR, built so that every app
uses the same map instead of each getting its own. Point 11 of the review
report to Kris (11-03); decided then: the JS8 map made shared, for WSPR and
FT8/FT4/FT2 (not #287's map, not JTTY: no grids in its exchanges). Kris told
we take it (note drafted 11-05).

Branch **`shared-map`** (from OpenX6100 main 2dd8d1e, worktree
`~/Work/x6100/wt-shared-map`), going to OpenX6100 as its own PRs.

## What exists (in the JS8 app)

1. **No LVGL, already shared-ready** (`src/js8/`): `js8_map.cpp/.h` (place a
   station by grid or callsign/cty.dat, choose the view Auto/Close/World,
   project, grid rectangles, curved paths, bearing, worked-before lookups),
   `map_render.cpp` (the base map from `js8_map.bin`, GridTracker Dark Gray),
   `geo.cpp`.
2. **The screen side, inside `dialog_js8.c`** (~1,300 lines, 59 `map_*`
   functions): the box/canvas with the border's corners, the base and pixel
   buffers, labels that keep off each other (`map_take`/`map_overlaps`), station
   squares (`map_station_mark`), paths, CQ and count tags (badges), the legend,
   the stats and status labels, QRZ line, the last-two-messages strip, the
   moving dots, pop-ups and rings, and `map_update()`, which decides what to
   draw from JS8's station list.

## Design: two layers

**A. `map_widget` (shared, LVGL, C):** the drawing toolkit, moved out of
`dialog_js8.c` as it is:
- create in a parent at x, y, w, h (the fitted part's height for the legend);
  loads the map data and cty.dat on first use, frees them on close;
- `set_view(mode, home, points)`: the view, the base map redrawn only when it
  changed;
- `begin()` (base copied in, label areas cleared) ... drawing calls ...
  `end()` (corners, invalidate): station mark, path (great circle, colour,
  width, opacity), label (keeps off what's taken), badge (CQ / count tags),
  ring, rect, legend items, the top-right status and top-left stats labels;
- the change hash (nothing redrawn when nothing changed).

**B. A simple scene on top, for FT8/WSPR-style apps:** the app hands a list
`{call, grid, colour, label or tag, path from home (colour), selected}`, the
legend items and the two text lines; the scene does the rest the JS8 way:
counts where stations share a square, the selected station's red box and
info label (where, dB, distance, azimuth), CQ tags. MFK selection stays the
app's (it owns the list).

JS8 keeps its own `map_update()` logic (pop-ups, dots, strip, QRZ, Follow)
but draws through layer A.

## Steps

1. [ ] Layer A out of `dialog_js8.c` into `src/map_widget.c/.h`; JS8 drawn
   through it. **Check:** the JS8 UI harness (our repo,
   `tools/js8_ui_harness`) on the branch, its map scenarios (MAP, MAPSTACK,
   STQRZ, HISTORY's map) **pixel-identical** before and after.
2. [ ] Layer B (scene) + PC test pictures.
3. [ ] FT8/FT4/FT2: the map (layout chosen by VE7NHW, mock-ups below), MFK
   picks a station, press calls it; test image, radio test.
4. [ ] WSPR: stations decoded here with their dB (Kris's app: he may hook it
   in himself). "Heard you" lines need WSPRnet spots over WiFi: after the
   WSPRnet upload work.
5. [ ] OpenX6100 PRs: the widget (with JS8 moved onto it) first, then one per
   app. A friendly note on #287 (Yuli Dai's FT8 world map, open since 10-11).

## Decisions for VE7NHW

- **FT8 layout** (mock-ups `~/Pictures/x6100-shared-map-mockups/round2-2026-11-05/`,
  drawn, not screenshots; the FT8 list from `dialog_ft8.c`'s layout):
  1 full map with "Show: List/Map" (button 5, page 2), MFK picks a station,
  press calls it; or 2 the map on top (170 px) over a 7-row decode list,
  "Map: On/Off".
- WSPR: full map, "Show: List/Map" on page 2 (button 5).
- Where Auto/Close/World goes in FT8/WSPR (JS8 uses its Map: button).

## Notes

- Memory: map data ~1 MB + two 792 x 290 x 4 B buffers (~1.8 MB), only while
  an app with the map is open.
- Mock-up script: `~/Work/x6100/research/shared-map/mock2.py` (+ `helpers.py`,
  `basemap`; `.venv` has Pillow).
