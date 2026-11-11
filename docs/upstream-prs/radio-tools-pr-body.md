Part (d) of the AI guide (#373), split off as Kris suggested: tools for a
test radio, in `tools/radio/`. Nothing else changes.

Helpers from the JS8 work, cleaned of one PC's paths, for a **Linux** PC on
the radio's DEV USB-C port (they use udev, udisks and `lsblk`; not tried on
Windows, macOS or WSL, and the README says so):

| Tool | What it does |
|---|---|
| `x6100-console` | Logs the console in the background and runs commands. It refuses to type while the radio boots or shows the stock Xiegu login, and after a gap (cable out, daemon restarted) it waits for a fresh prompt before typing |
| `x6100-screenshot` | Saves the radio's screen as a PNG |
| `x6100-cpulog` | Logs each core, each GUI thread and the GUI's memory once a second; the report gives the memory trend in MB per hour (a slow leak shows over hours). Reads /proc and /sys only, never i2c |
| `x6100-flash` | Writes an image to the SD card and keeps DATA: backs it up, writes only the `.img` from a zip, checks DATA afterwards and fails if it differs. Finds the card by its BOOT and DATA labels, in a USB reader or a built-in slot |
| `x6100-syncheck` | ARM compile check of single files |
| udev rule | Lets your user open the radio's serial ports |

Tested: the console against a fake radio on a pseudo-terminal (commands
work; the stock-login guard refuses); the flash script with a real card in
a USB reader up to its "Write?" question (it found the card by its labels
and backed up DATA; nothing was written). The memory trend has only been
checked on made-up data so far.

🤖 Generated with [Claude Code](https://claude.com/claude-code)

https://claude.ai/code/session_011735W4WxdGqVBkzHYAK1C2
