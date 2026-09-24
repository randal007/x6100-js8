# JS8 test audio

`js8_test.wav` is a 45 s, 11025 Hz mono recording of a simulated band made
with `tools/js8_wavgen`. Sample 0 is a 15 s slot boundary.

## Trying it on the radio

1. Copy `js8_test.wav` to the root of the SD card's DATA partition, so the
   radio sees it as `/mnt/js8_test.wav`.
2. Open APP → JS8, go to page 2 (JS8 2:2) and press **Test WAV**.
3. Playback starts at the next slot boundary (up to 15 s). The radio's own
   audio is ignored until the file ends. Press **Stop Test** to end early.

## Expected messages

| offset | generated SNR | decoder SNR | message |
|---|---|---|---|
|  620 Hz | −12 dB | ≈ −20 | `W1ABC: @HB HEARTBEAT FN42` |
|  910 Hz |  −6 dB | ≈ −13 | `VE3KP: @ALLCALL CQ CQ CQ FN03` |
| 1320 Hz |  −4 dB | ≈ −11 | `N0XYZ: K2XYZ HELLO FROM THE TEST FILE` (3 frames) |
| 1760 Hz | −14 dB | ≈ −21 | `G4ABC: @ALLCALL ANYONE ON THE BAND FOR A CHAT` (3 frames) |
| 2150 Hz | −10 dB | ≈ −17 | `KN4CRD: K2XYZ MSG STORED MESSAGE FOR YOU` (3 frames, checksum verified) |
| 2380 Hz | −18 dB | ≈ −26 | `DL1XX: @HB HEARTBEAT JO62` |

"Generated SNR" is the true signal-to-noise ratio in 2500 Hz. The decoder's
estimate reads about 7 dB lower, as desktop JS8Call's does. With your
callsign set to K2XYZ, the two K2XYZ messages are highlighted in red.
Heartbeats are hidden under the default "Show: No HB" filter.

## Making your own

```sh
cd tools/js8_wavgen
cmake -S . -B build && cmake --build build
./build/js8_wavgen out.wav 'MYCALL,FN42,1200,-10,K2XYZ SNR?' 'OTHER,EM73,800,-15,CQ CQ CQ EM73'
```

Stations are `CALL,GRID,OFFSET_HZ,SNR_DB,TEXT`, with TEXT typed as you would
in JS8Call. `-r` sets the sample rate (the radio resamples anything else) and
`-n` sets the noise level.
