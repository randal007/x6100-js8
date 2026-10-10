# Release checklist

For a new beta (or any image that goes out to testers).

1. **The version string.** Set `JS8_APP_VERSION` at the top of
   `src/dialog_js8.c` to the release's name (e.g. `"X6100 JS8 beta 5"`).
   It's what `<MYVERSION>` sends, and the default STATUS
   (`IDLE <MYIDLE> VERSION <MYVERSION>`) answers every STATUS? with it.
   Beta 4.5 went out still saying "beta 5" because this was missed.
2. **The README is the manual:** "New in ...", "Known issues in ...",
   "Coming next" (from [BETA5_PLAN.md](../BETA5_PLAN.md) or its successor),
   and the summary at the top.
3. **Release notes:** `docs/releases/js8-<name>.md` (new features first,
   then bug fixes).
4. **Tag and build:** an annotated tag `js8-<name>` on `main`, pushed;
   pushing a tag does **not** start the build, so run *Build image* on the
   tag by hand:
   `gh workflow run 366471886 -R randal007/x6100-js8 --ref js8-<name>`
   (about an hour; it publishes the release with the image attached).
5. **Right after it publishes:** `gh release edit js8-<name> --title "X6100
   JS8 <name>" --notes-file docs/releases/js8-<name>.md --prerelease`
   (otherwise the page sits bare, untitled and marked Latest).
6. **Older pages:** if this replaces a release with a bug, put a "Please
   use <name>" banner at the top of the older release page and its notes.
7. **Check:** the asset's sha256 matches GitHub's digest; download it once.
