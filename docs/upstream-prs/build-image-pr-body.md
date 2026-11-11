Part (b) of the AI guide (#373), split off as Kris suggested: *Build image*
makes test images from any branch. Only `.github/workflows/main.yml`
changes.

Today *Build image* makes an image only for a tag's release (its upload step
is commented out). With this change:

- run by hand on **any branch**, it keeps the image as a **download on the
  run's page for 30 days**, so a change can be tried on a radio before its
  PR;
- `buildroot_ref`: build with an open AetherX6100Buildroot PR's branch (a
  tag still has to use master);
- it also keeps `x6100_gui-unstripped.<branch>`, the same GUI before
  stripping. It's a Release build, so it gives function names (not source
  lines) for a core dump from the radio;
- `buildroot_ref` reaches the shell through `env`, and a `/` in a branch
  name becomes `-` in the download's name instead of breaking the upload.

Tag builds and the Release step are unchanged. This is the step we've used
in the x6100-js8 fork since October for every test package.

Only OpenX6100 members can run it here; anyone else runs it in their own
fork after turning on Actions there (it needs no secrets).

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_011735W4WxdGqVBkzHYAK1C2
