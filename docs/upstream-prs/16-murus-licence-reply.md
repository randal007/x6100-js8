Reply on https://github.com/OpenX6100/x6100_gui/discussions/302, to Hany's comment of 2026-10-15 13:47Z (posted 2026-10-15 by VE7NHW's account).

---

Thanks Hany, that's great news, and welcome!

I've got your 1.0.2 beta 4 source (the tarball with SSTV RX+TX, WeFax, NavTex/SitorB, DSC and the Channel List). A few licence details, so the PRs are right the first time:

1. **Headers.** In this source your files still say `SPDX-License-Identifier: GPL-3.0-or-later`, and `NOTICE-1KO125.md` adds a GPLv3 section 7(b) term (keep the "Hany El Imam" credit in each app's Info window). As each app goes into OpenX6100, may I change the headers of **your own files** in that PR to `LGPL-2.1-or-later` (your copyright line stays), as you said? A "yes" here is enough as your written OK. LGPL-2.1 has no place for the 7(b) term, so it would be left out; the Info window credits would of course stay in the code.
2. **Files from others** keep their own licence: Project Nayuki's QR code generator in DSC is MIT and cJSON (Channel List) is MIT, both fine with LGPL. The fldigi / Franco Venturi decoder (GPL-3.0) only comes in if NavTex is sent before your own decoder is ready, and then in `third-party/` with its licence, as JS8's engine.
3. A few files have no licence line yet: the Channel List's `channels.c`, `dialog_channels.c` (only a copyright line) and `broadcast_db.c`, and `ui_list.c`, the list helper the apps share. LGPL-2.1-or-later for those too?

**The order I'd suggest,** following your note on NavTex:

1. A small **shared PR** with the pieces the apps use outside their own files: the list and Info window helpers, the recorder's WAV format (the apps' Test files), the tuning markers on the spectrum, and the 4th APP page (Kris, the layout with your apps too).
2. **WeFax.**
3. **SSTV** (receive and transmit).
4. **Channel List / Broadcast Info** (Georgy, your call whether it belongs; it changes the main screen, so it would be its own PR).
5. **NavTex/SitorB**, once Hany's own decoder is in.
6. **DSC**, when Hany says it's ready.

Two smaller things in the source touch every user, so I'd send them as their own little PRs: the **S-meter offset** setting, and a fix to the waterfall's **auto levels at higher zoom** (your note in `dsp.cpp`: the noise floor rose 3 dB per zoom step). App numbers: WeFax 109, NavTex 110, SSTV 111 as reserved; DSC would take the next free one.

73, Randal VE7NHW
