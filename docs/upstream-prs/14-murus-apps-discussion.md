Discussion for OpenX6100/x6100_gui (General), posted 2026-10-14 as https://github.com/OpenX6100/x6100_gui/discussions/302 (VE7NHW sends the link to Hany on Facebook).

Title: WeFax, NavTex and SSTV from the Murus team: bringing them into OpenX6100

---

Hi @gdyuldin, @myshak1, and Hany (Murus team),

Hany El Imam's **WeFax, NavTex and SSTV** apps are in the Murus releases ([TheMurusTeam/custom-r1cbu](https://github.com/TheMurusTeam/custom-r1cbu/releases), now on 1.0.2). Hany would like them in OpenX6100, so everyone gets them in the main firmware. He doesn't really use GitHub, so the plan is: when he's happy with a version, he sends me the source and I prepare the pull requests, with him as the author.

Before any code, a few things I'd like us to agree on:

1. **Licence.** Hany's own files (`dialog_wefax.c`, `dialog_navtex.c`, `dialog_sstv.c`, `src/sstv/`) don't have a licence notice yet. Hany, could you confirm here which licence they're under? LGPL-2.1-or-later, like the rest of x6100_gui, would be simplest. NavTex also uses code from fldigi and from Franco Venturi, both GPL-3.0; that would go in `third-party/` with its own licence, as agreed for JS8 in #23.
2. **Credit.** The commits authored by Hany, with me as co-author, so GitHub shows it's his work.
3. **App numbers.** 109 WeFax, 110 NavTex, 111 SSTV, as reserved in #283.
4. **The APP menu.** Page 3 holds four apps. With WSPR, PSK31, Olivia, JS8 and these three, we'll need a 4th page, maybe a 5th. Kris, it might make sense to agree one layout for all of them at once.
5. **One PR per app,** each small enough to review, probably WeFax first (it's been in the Murus releases longest).
6. **After they're in:** bug reports for these apps go to Hany (through me if needed).

Hany is still finishing the apps. The idea is to agree on a "version 1" of each, send it once it works, and bring later improvements as follow-up PRs.

Does this sound right to you? Anything you'd like done differently?

73, Randal VE7NHW
