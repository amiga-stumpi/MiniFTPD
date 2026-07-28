# MiniFTPD

MiniFTPD is a lightweight FTP server for classic Amiga systems. It is being
developed primarily for AmigaOS 1.3 and TheWire13's `bsdsocket.library`.

The project uses a single event loop and avoids threads, POSIX runtime
assumptions and large automatic buffers.

## Current status

Project steps 1 through 7 are complete:

- standalone AmigaOS executable scaffold
- bebbo GCC build using the Kickstart 1.3 `nix13` CRT
- startup check for `bsdsocket.library`
- initial configuration file named `miniftpd.conf`
- configuration and session data structures
- complete implementation roadmap
- strict `miniftpd.conf` loading and validation
- automatic default configuration creation
- effective configuration summary without password disclosure
- one-client TCP control listener on the configured port
- `WaitSelect()`-driven control connection handling
- reliable Ctrl-C shutdown with direct Exec-signal fallback
- bounded CRLF command parsing and partial socket-write handling
- explicit connection, login and transfer-mode session state
- configurable control-connection inactivity timeout
- independent cleanup for control, passive-listener and data sockets
- `TYPE A` and `TYPE I` transfer-mode selection
- configured `USER`/`PASS` authentication
- optional anonymous login as `anonymous` or `ftp`
- pre-authentication command restrictions and failed-login limiting
- authenticated `PASV` with rotating configured passive ports
- valid `227` replies using the control connection local IPv4 address
- asynchronous acceptance of one temporary passive data connection
- FTP client discovery commands `SYST`, `FEAT` and `NOOP`
- authenticated `PWD` plus safe root-only `CWD` and `CDUP`

Authenticated sessions accept the implemented discovery, authentication,
directory, transfer-mode, passive-mode and shutdown commands. File listing,
download and upload commands remain assigned to their respective roadmap
phases.


## Build

Requirements:

- `/opt/amiga/bin/m68k-amigaos-gcc`
- `/opt/amiga-netinclude/include`

Build:

```sh
make clean
make
```

Output:

```text
build/MiniFTPD
build/miniftpd.conf
```

## Configuration

The configuration filename is always:

```text
miniftpd.conf
```

Configuration loading and validation are implemented. If the file is missing,
MiniFTPD creates it beside the executable using safe defaults. Unknown keys,
malformed values, reversed passive-port ranges and oversized files stop startup
with an error. The configured password is never printed.

## Control and authentication test

Start MiniFTPD on the Amiga and connect from another machine:

```sh
nc AMIGA_IP 21
```

The control connection should behave as follows:

```text
220 MiniFTPD ready.
TYPE I
530 Please login with USER and PASS.
USER amiga
331 Password required.
PASS amiga
230 Login successful.
SYST
215 AMIGA Type: L8.
FEAT
211 No additional features.
PWD
257 "/" is the current directory.
CWD /
250 Directory changed to /.
CDUP
250 Directory changed to /.
TYPE A
200 Type set to A.
TYPE I
200 Type set to I.
QUIT
221 MiniFTPD closing connection.
```

Only one client is accepted at a time. A second connection receives
`421 MiniFTPD is busy`.

After login, test the passive listener:

```text
PASV
227 Entering Passive Mode (192,168,7,25,195,80).
```

The final two values encode the selected port (`195 * 256 + 80 = 50000`).
Connect to that address and port from a second shell. MiniFTPD then reports
`FTP passive data client connected.` on its console. The exact address and port
depend on the Amiga configuration and available passive ports.

## Documentation

The complete implementation plan and progress are maintained in
[`docs/ROADMAP.md`](docs/ROADMAP.md).

## Compatibility target

- AmigaOS 1.3
- Motorola 68000
- `bsdsocket.library`
- TheWire13

Other compatible Amiga TCP/IP stacks may work later, but are not claimed as
tested at this stage.
