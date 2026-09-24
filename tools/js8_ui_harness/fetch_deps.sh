#!/bin/bash
# Fetch the host-side dependencies of the JS8 UI harness into ./deps, at the
# versions the X6100 image uses (AetherX6100Buildroot / buildroot 2022.11).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p deps/include deps/src
cd deps

# LVGL: stock v8.3.11, the version the firmware's NEON fork is based on. The
# fork's non-NEON fallback does not compile, and only ARM builds use NEON.
[ -d src/lvgl ] || git clone -q --depth 1 --branch v8.3.11 https://github.com/lvgl/lvgl src/lvgl

# ft8_lib headers: params.h includes ft8lib/constants.h.
if [ ! -d include/ft8lib ]; then
    git clone -q https://github.com/gdyuldin/ft8_lib src/ft8_lib
    git -C src/ft8_lib checkout -q 73b0db861dc41c7ca9f3dedc73ee148daa06dd0e
    mkdir -p include/ft8lib && cp src/ft8_lib/ft8/*.h include/ft8lib/
fi

# X6100Control headers, plus its CMake-generated api.h.
if [ ! -f include/aether_radio/x6100_control/api.h ]; then
    git clone -q --depth 1 --branch v0.16.1 https://github.com/gdyuldin/X6100Control src/X6100Control
    cmake -S src/X6100Control -B src/X6100Control/build >/dev/null
    cp -r src/X6100Control/include/aether_radio include/
    cp src/X6100Control/build/include/aether_radio/x6100_control/api.h include/aether_radio/x6100_control/
fi

# liquid-dsp 1.4.0, static, for the waterfall spectrogram.
if [ ! -f lib/libliquid.a ]; then
    git clone -q --depth 1 --branch v1.4.0 https://github.com/jgaeddert/liquid-dsp src/liquid-dsp
    (cd src/liquid-dsp && ./bootstrap.sh >/dev/null && ./configure --prefix="$PWD/../.." >/dev/null && make -j"$(nproc)" >/dev/null && make install >/dev/null)
fi

echo "deps ready in $PWD"
