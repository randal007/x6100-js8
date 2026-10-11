A small fix I found while working on JS8: in the ADIF export, `write_mode()` has no `break` after `MODE_SSB`, so it falls through to `MODE_AM`. Every SSB contact in an exported log goes out as `<MODE:2>AM`.

One line: the missing `break`. Importing is unaffected (`create_mode()` already reads "SSB" back correctly).

73, Randal VE7NHW

🤖 Generated with [Claude Code](https://claude.com/claude-code)
