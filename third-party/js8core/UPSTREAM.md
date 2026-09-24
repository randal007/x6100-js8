# js8core (vendored)

Source: https://github.com/JS8Call-improved/Android-port
Commit: `1f8d390a52bbcdda98316060c479eb1a8d5a584c` (2026-09-23)
License: GPLv3 (see `LICENSE`)

Copied from that commit:

| here              | upstream            |
|-------------------|---------------------|
| `include/`, `src/`| `core/include`, `core/src` (minus `placeholder.cpp`) |
| `commons.h`       | `commons.h`         |
| `vendor/Eigen`    | `vendor/Eigen`      |
| `vendor/CRCpp`    | `vendor/CRCpp`      |

`CMakeLists.txt` and this file are ours.

## Local patches

Each one is its own commit on top of the pristine import, so
`git log -- third-party/js8core` shows them.

1. **Missing standard includes.** `varicode.hpp` uses `std::function` and
   `jsc.cpp` uses `std::strncmp` without including `<functional>` /
   `<cstring>`. Android's libc++ pulls those in transitively; GCC's
   libstdc++ does not.
2. **`EngineConfig::spectrum_enabled`.** When false, the engine does not
   start its spectrum thread or run a 4096-point double FFT every 100 ms.
   The X6100 draws its own waterfall from liquid-dsp.
3. **`Js8Engine::request_realign()`.** Re-snaps the 60 s RX ring to the
   wall clock on the next captured buffer. The engine otherwise aligns only
   at construction or when the drift setting changes, so audio gaps or a
   sound-card/system clock mismatch would slowly walk the decode windows.
4. **Lazy per-submode decoders.** `legacy_decode()` used to construct all
   five `DecodeMode` instances on first call. Now each is built on first use,
   which cut peak RSS from ~73 MB to ~44 MB in a Normal-only test and the
   first-decode setup time by ~4x.

All four are candidates to send upstream.
