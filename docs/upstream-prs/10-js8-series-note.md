**Where:** OpenX6100/x6100_gui discussion 23 (JS8call), a new comment once 1/4 is open.

---

The JS8 series has started: **#xxx JS8 (1/4): groundwork in shared code.**

JS8 comes in four pull requests, one at a time:

1. Groundwork in shared code (#xxx, open now)
2. The JS8 engine from JS8Call-improved (GPL-3.0)
3. The JS8 library and its unit tests
4. The app itself, with its map and the APP button

The JS8 app has been running on 1.0.2 in test builds since early October (https://github.com/randal007/x6100-js8), and testers use it on the air.

@gdyuldin, about the licence question above: the engine in part 2 is GPL-3.0. It can be combined with the GUI's LGPL-2.1-or-later (the firmware image as a whole would then be GPL-3.0), or JS8 can be a build option, off by default. Your call. It would help to know before part 2.

73, Randal VE7NHW
