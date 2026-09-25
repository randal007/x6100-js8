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

5. **Value-initialised decoder arrays.** `DecodeMode` reads some of its
   arrays before writing them. Upstream gets away with this because its
   instances live in static storage, which is always zeroed. Patch 4 moved
   them to the heap, where they started with whatever the memory held
   before. In one reproducible case (a GCC -O2/-O3 build, with the decoder
   built right after large buffers were freed) the sync search returned only
   garbage candidates and nothing decoded. Zeroing the storage before
   construction did not help, because GCC's lifetime dead-store elimination
   removes that `memset()`. The fix gives every array member a `{}`
   initialiser, so construction zeroes them wherever the object lives. The
   failure depends on exact heap history, so there is no reliable
   regression test. The investigation is in the commit message.

6. **Callsign validation matches desktop.** `is_valid_callsign()` was a
   simplified rewrite that accepted almost any short word ("JUST",
   "HELLO", "THE") as a callsign. Desktop JS8Call requires a letter-digit
   pair, or a real compound call or @group. One visible effect:
   `build_message_frames()` took the first word of free text for a
   recipient, so forced identification never happened and free text went
   out without the sender's callsign, where desktop prepends it. This is a
   straight port of desktop's `isValidCallsign()` and
   `isValidCompoundCallsign()`. The upstream varicode round-trip test
   still passes (31/31).

7. **SNR format matches desktop.** `format_snr()` printed "-8" and "+5"
   where desktop's `Varicode::formatSNR()` prints "-08" and "+05", and it
   didn't return empty outside -60..+60 as desktop does. Only affects how
   received reports are displayed.

8. **Ready decode windows are never dropped.** `enqueue_decode()` returned
   without queuing while a decode was running, but `isDecodeReady()` had
   already marked the window done. With several speeds on (Turbo re-checks
   every second), a Normal or Fast slot falling due during a decode was
   silently never decoded; a test that switches Fast on mid-run lost a
   Normal frame every time. Desktop JS8Call queues ready windows. Now at
   most one snapshot waits behind the running decode; a newer snapshot
   replaces it and inherits any speed's window only the older one had
   (the 60 s ring still holds that audio).

9. **Decode range and QSO offset are settable.** The engine searched a fixed
   200-2500 Hz with `nfqso` 1500 Hz. Desktop searches its waterfall filter's
   edges (0-5000 Hz without a filter) and passes its own offset as `nfqso`,
   which the decoder uses to try candidates near it first.
   `set_decode_range()` and `set_qso_offset()` feed the next decode.

All nine are candidates to send upstream. Patch 5 matters to upstream only
if they ever move decoders off static storage; patch 6 affects them
today.

A related cost of upstream's approach, for the record: five `static
thread_local` decoders reserve about 31 MB of address space in every thread
(A 7.3, B 4.8, C 2.9, E 14.5, I 2.0 MB). That is significant in a 32-bit
process and is part of why patch 4 exists.
