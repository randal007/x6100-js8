Six apps are on their way in at once: Kris's WSPR (#271), PSK31 (#272) and Olivia (#274), JS8 (#308), and Hany's WeFax (#311) and SSTV (#312). Each of them adds lines in the same four places: its button on an APP page, its `ACTION_APP_*` number, its `#include` and window case in `main_screen.c`, and its sources in `src/CMakeLists.txt`. So every one of them conflicts with every other, whichever goes first, and the buttons' order depends on the merge order.

This PR fixes both. It changes nothing on the radio except an empty APP page 4 until the apps arrive.

### The layout

| | | | | |
|---|---|---|---|---|
| **APP page 3** | WiFi | Bluetooth | WSPR | JS8 |
| **APP page 4** | PSK31 | Olivia | WeFax | SSTV |

That's the layout we've been testing on the radio with all six apps in one build, and how the app PRs already place their buttons. Each app's place is `NULL` (an empty button: `buttons_load()` already skips a `NULL`) until the app comes in. The page labels go to 1:4 ... 4:4.

### How it avoids the conflicts

In each of the four files every app gets a comment line with its name, and its own lines go below it:

```c
static buttons_page_t page_app_4 = {{
    &btn_app_p4,
    /* PSK31 */
    NULL,
    /* Olivia */
    NULL,
    ...
```

An app's PR then changes only its own lines, with a name line between it and the next app's, and git merges them in any order. I checked it: each of the six app branches (rebased onto this, see below) merges cleanly with each of the other five, all 15 pairs, and all six together on main give exactly the layout above. They compile with the image's GCC 12, and every window case still ends in its `break`.

### The app PRs

Each app branch takes in this PR's commit (a merge) and moves its lines under its name, so **whichever app is merged first brings this layout with it**: nothing has to wait for this PR. This PR can also go first.

- ours: JS8 #308, WeFax #311, SSTV #312, updated today
- Kris's: PRs into his branches, myshak1/x6100_gui#5 (PSK31), #6 (Olivia), #7 (WSPR)

Still left after this, outside these places: the Channel List (#310) and WeFax both change the microphone UP/DOWN keys, and RTTY (#276) touches the same `src/CMakeLists.txt` line as Hany's groundwork. Those are one-time merges, and I'll do them.

### Files

`src/buttons.cpp` (+37 −6), `src/settings_types.h` (+8), `src/main_screen.c` (+20), `src/CMakeLists.txt` (+7). Only comment lines, page 4 and its label.

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_01DuBZGNrpUEoiMp83Y6oN9y
