For JS8 (2/4), OpenX6100/x6100_gui#301, from Kris's review there:

- **Dependencies:** JS8's engine (`third-party/js8core`) needs Boost headers and single-precision FFTW. `X6100_GUI_DEPENDENCIES` gets `boost fftw-single`, and `BR2_PACKAGE_BOOST=y` goes in X6100_defconfig (no Boost libraries selected, so it installs headers only). `fftw-single` is already built for liquid-dsp; this only orders it before x6100-gui. Our own image builds have added exactly these two lines on the fly since beta 1, so this is what the JS8 images were built with.
- **Licence:** `X6100_GUI_LICENSE` said `GPLv2`. The GUI's own code is LGPL-2.1+ (its `LICENSE`), and with js8core (GPL-3.0) the program is GPL-3.0 as a whole, so `legal-info` now reports `LGPL-2.1+, GPL-3.0 (JS8 engine, third-party/js8core)` with `LICENSE` as the licence file. `third-party/js8core/LICENSE` joins `X6100_GUI_LICENSE_FILES` once `X6100_GUI_VERSION` points at a release that has it (v0.23.0-rc.3 doesn't, and `legal-info` would fail on a missing file).

Best merged before or together with #301, so the image build can't break when JS8 lands. `legal-info` in the image workflow will follow as its own PR, as Georgy asked.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_017kh9JtieivDQgN5JvFUSZr
