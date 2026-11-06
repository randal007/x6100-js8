**Draft, experimental.** The xara modem package for the XARA app (OpenX6100/x6100_gui#345). This is the groundwork, so we can work on XARA here from now on.

## What it adds

- **xara** (github.com/x0rro/xara, MIT, by x0rro): a VARA HF compatible modem. Installs `/usr/bin/xara` and `/usr/bin/xara-monitor`. On in `X6100_defconfig` (`BR2_PACKAGE_XARA=y`); one line to switch it off.
- **Free speed levels only:** built `--no-default-features --features pulse`, never with xara's `registered` feature.
- VARA is EA5HVK's modem; xara is x0rro's own, independent program (said in the package help, the .mk and the app).

## Where the source comes from

For now **randal007/xara 919f5dd**: x0rro's 0.1.1 plus the fixes that made it work on the X6100. x0rro reviews and merges fixes himself, so they're offered to him rather than sent as PRs:
- an answer gate (plain xara answers every call to its callsign from the moment it starts; with the gate, the app decides, so nothing transmits on its own)
- the busy detector bypass (for APRS broadcasts)
- PSK frames built and searched on several threads (the Cortex-A7 sent them too late for VARA)
- catching up when the receiver falls behind, separate send and receive buffers, the send buffer counted as TX latency
- the poll/NACK loop fix

919f5dd is the xara that passed my bench tests on 2026-11-02. We move to x0rro's own release once his libxara (0.1.4, announced) has these.

## Why its own Rust

xara needs Rust 1.85 or newer (edition 2024), and Buildroot 2023.02 has 1.67. **host-xara-rust** installs the official Rust 1.97.1 binaries (hashes from static.rust-lang.org, x86_64 and aarch64 hosts) in `$(HOST_DIR)/opt/xara-rust`, for xara only, so Buildroot's own host-rustc is untouched. The crates are vendored into the download the same way Buildroot's cargo packages do it, so the build itself runs offline.

Checked on my PC: vendoring and the offline `--locked` build of 919f5dd with Rust 1.97.1. Full image with both branches: test image run https://github.com/randal007/x6100-js8/actions/runs/42272617883 (building now; both branches on OpenX6100's current main).

## Still to do (why it's a draft)

- Radio test of this exact image (free speeds only; my earlier bench tests used test builds).
- Move to x0rro's release when libxara is out.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
