Reply on OpenX6100/x6100_gui discussion #23 (to gdyuldin, 2026-10-12)

---

Thanks @gdyuldin. I think I explained it badly before: nobody's code has to change licence, and the project doesn't become GPL v3.

- **Every file in x6100_gui stays LGPL-2.1-or-later**, exactly as it is now. No headers change, nobody is asked to relicense anything.
- **Only the JS8 engine** (one folder, `third-party/js8core`) is GPL v3, with its own LICENSE file next to it, the same way `third-party/rapidxml` carries its own licence today.
- **What changes is only the built SD card image when JS8 is in it:** that combined image is distributed under GPL v3 terms. The LGPL 2.1 itself allows this without asking the authors (section 3: a copy of LGPL code may be used under the GPL version 2 "or a newer version"), and the "or later" in our headers says the same. In practice it means the image's source has to stay public, which it already is.

If you'd rather keep the standard images purely LGPL, JS8 can be a build option (`-DWITH_JS8=ON`), off by default, so only images built with it switched on carry GPL v3.

Either way, the next pull requests stay as planned: 2/4 the engine (with its licence note), 3/4 the library and tests, 4/4 the app. Which would you prefer, JS8 always built in or as an option?

73, Randal VE7NHW
