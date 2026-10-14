PR text for OpenX6100/x6100_gui, JS8 (2/4), branch `js8-engine` (drafted 2026-10-14)

Title: JS8 (2/4): the JS8 engine (js8core, GPL-3.0) and the licence note

---

Part 2 of 4, after #290. This adds the JS8 engine. Nothing uses it yet (the app comes in 4/4), so nothing changes on the radio.

**Three commits, easiest reviewed one at a time:**

1. **`third-party/js8core` exactly as published** by [JS8Call-improved/Android-port](https://github.com/JS8Call-improved/Android-port) at commit `1f8d390a` (`core/include`, `core/src`, `commons.h`, `vendor/Eigen`, `vendor/CRCpp` and its GPLv3 `LICENSE`). Nothing changed, so it can be checked against their repo.
2. **Our patches on top**: 6 source files, +354 −100, plus the folder's `CMakeLists.txt` and `UPSTREAM.md`, which describes each patch. They're fixes found by checking against desktop JS8Call (callsign check, SNR format, compound-call commands, dropped decode windows, uninitialised decoder arrays, drift estimate, missing includes) and what a small radio needs (no spectrum thread, decoders built only when needed, settable decode range, Ultra on its own thread). The bugs were reported to JS8Call-improved in Android-port#104: they fixed most in their #105, and the rest are offered in #108.
3. **Built with the GUI and the licence note.** `add_subdirectory(third-party/js8core)` (JS8 in the default build, as agreed in #23), and a *License* section in the README, as you asked in #23: the GUI's files stay LGPL-2.1-or-later, js8core is GPL-3.0, an image with JS8 is GPL-3.0 as a whole, and where its corresponding source is.

**Size:** big in lines, but little of it is code to read. About 8,000 lines are the engine itself, about 150,000 are the vendored Eigen and CRCpp, and about 524,000 are generated JSC tables. Commits 2 and 3 are the ones worth reading.

**Checked:** compiles with the image's GCC 12 for the X6100's Cortex-A7, with no warnings. Merges cleanly with main. The same engine has run in the x6100-js8 betas since late September, on the air with testers.

**One question:** the README says Buildroot's `legal-info` is published with the image, as you suggested in #23. *Build image* doesn't do that yet. Shall I add it to the workflow in a separate PR, or would you rather do it yourself?

73, Randal VE7NHW
