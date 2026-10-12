# cty.dat (vendored)

AD1C's amateur radio country file, the list WSJT-X, JTDX, JS8Call and the
contest loggers use, **Big CTY** edition, from
<https://www.country-files.com/bigcty/cty.dat>: version `VER20260915`
(CTY-3631, 15 September 2026). MIT licence: see `LICENSE` (the copyright
page at <https://www.country-files.com/copyright/>).

Installed once, as `/usr/share/x6100/cty.dat`, and read by:

- `src/dxcc.c`: the country shown in the FT8/FT4 table;
- the JS8 map: places stations heard without a grid (their country,
  continent and centre; `src/js8/callsign_place.cpp` adds regions for
  Canada, the US call areas, Australia and Japan) and finds your continent
  from your callsign.

A newer list from <https://www.country-files.com/> can be put on the SD
card as `/mnt/cty.dat`; both use it instead of the one in the image.

To update the one in the image: download the new `cty.dat` over this one
and change the version above. The format is described at
<https://www.country-files.com/cty-dat-format/> (longitudes are positive
**west**).
