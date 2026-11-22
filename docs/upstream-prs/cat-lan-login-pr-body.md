For 1.3, as Kris suggested by e-mail: the network CAT (wfview's protocol)
now needs the right login, and only the computer that logged in controls
the radio. Also fixes the CW pitch over CAT changing the TX power.

## What the user sees

- **wfview with the right username and password:** works as before. With
  the radio's default root / root nothing changes.
- **A wrong login:** refused. wfview shows "Invalid Username/Password", as
  with its own server. Before, it was only logged as a warning and the login
  went on.
- **Another computer on the same network** can no longer key the radio,
  send it CI-V commands or audio, or get copies of what it sends. Before,
  the CI-V port ran frames from any address, PTT included, with no login.
- **CW pitch over CAT** (CI-V 14 09, USB and network) no longer changes the
  TX power, and reading it returns the pitch, not the power (a missing
  `break` in `civ_processor.cpp`).

## What changed

`src/cat/lan/cat_lan.cpp`, following wfview's own server:
- each port (control, CI-V, audio) has one gate in the thread's dispatch,
  before any packet is looked at
- control: are-you-there starts a session, but not over a logged-in one
  from another computer (that one ends after the ping timeout, counted from
  the login); login, token, stream, ping, close and retransmit only from
  the client that said are-you-there; token and stream only after a login
- a wrong login gets error 0xFEFFFFFF and no token, stream or CI-V port;
  the fields are compared up to their end, the password encoded or plain,
  as wfview's server does ("rootx" no longer passes as "root")
- CI-V and audio: the handshake only from the logged-in client's IP,
  everything after it only from the address and port that made it
- retransmit copies go only to the logged-in client
- one `end_session()` for login, close, a new are-you-there and the ping
  timeout; a login clears the old session's buffers

`src/cat/civ_processor.cpp`: the missing `break` after 14 09.

## Tests

- **`tests/test_cat_lan.cpp`** (new, in CI): the real server on UDP
  50001-50003, clients on 127.0.0.1 and 127.0.0.2, a fake radio that counts
  PTT and audio. 42 checks pass; on main 17 of them fail.
- **`tests/test_civ.cpp`:** two tests for CW pitch set and read (both fail
  without the fix). Its settings database helper moved to `cat_test_db.h`,
  shared by both tests.
- **On the radio:** not yet. Next: wfview over WiFi, right login (TX, audio,
  frequency) and wrong login (refused).

## Not in this PR

- **A password setting** (root / root is the same on every radio): after
  1.3, as Kris proposed: a Network CAT setting, off by default; turned on,
  the radio picks a random password, shows it on the Info screen, and the
  owner can change it.
- Anyone on the network can still end a session (an are-you-there or close
  after the logged-in client stops pinging), and UDP source addresses can be
  faked: the same as in wfview's own server, a limit of the protocol.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
