# 1KO125 – License notice

1KO125 is a firmware for the Xiegu X6100 based on R1CBU x6100_gui 1.0.2.

## Licenses

| Part | Author | License |
|---|---|---|
| R1CBU x6100_gui (the base firmware) | Belousov Oleg aka R1CBU and contributors | LGPL 2.1 or later (see `LICENSE`) |
| WEFAX, NAVTEX/SITOR-B, DSC and SSTV applications, Info window (`src/dialog_wefax*`, `src/wefax/`, `src/dialog_navtex*`, `src/navtex/decoder.*`, `src/navtex/bulletin.*`, `src/dialog_dsc*`, `src/dsc/`, `src/dialog_sstv*`, `src/sstv/`, `src/sstv_resources.*`, `src/app_info.*`) | Hany El Imam | GPL 3.0 or later, with the additional term below (see `COPYING`) |
| SITOR-B demodulator (`src/navtex/navtex_rx.*`, `fftfilt.*`, `complex.h`, `misc.h`, `gfft.h`) | fldigi (W1HKJ and contributors, after JNX by Paul Lutus), adapted by Franco Venturi | GPL 3.0 or later (`gfft.h`: LGPL 3.0 or later) |
| QR code generator (`src/dsc/dsc_qrcodegen.*`) | Project Nayuki | MIT |

Because it contains code under the GPL 3.0, the firmware as a whole (the
compiled program and the SD card image) is distributed under the terms of the
**GNU General Public License, version 3 or later**. The LGPL 2.1 of the R1CBU
files allows this (LGPL 2.1, section 3). Each source file keeps the license
written in its header.

## Additional term (GNU GPL v3, section 7(b))

The following additional term applies to the files written by Hany El Imam
listed above:

> Any work that contains these files, or a work based on them, must preserve
> the author attribution "Hany El Imam" and the credits of the WEFAX,
> NAVTEX/SITOR-B, DSC and SSTV applications in the Appropriate Legal Notices
> displayed by its user interface. In 1KO125 these notices are shown in the
> Info window of each application.

## Warranty

This program comes with ABSOLUTELY NO WARRANTY, to the extent permitted by
applicable law. See `COPYING`, sections 15 and 16.

## On the radio

The full text of the GNU GPL v3 is installed on the SD card as
`/usr/share/x6100/COPYING`.
