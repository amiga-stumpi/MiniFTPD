# MiniFTPD

MiniFTPD is a lightweight FTP server for classic Amiga systems. It is being
developed primarily for AmigaOS 1.3 and TheWire13's `bsdsocket.library`.

The project uses a single event loop and avoids threads, POSIX runtime
assumptions and large automatic buffers.

## Current status

Project steps 1 through 13 are complete:

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
- authenticated `PWD` plus filesystem-backed `CWD` and `CDUP`
- canonical virtual FTP paths confined to the configured export root
- DOS-lock ancestry checks preventing traversal and link escapes
- startup validation of the configured root directory
- centralized enforcement of the `readonly` setting
- passive `LIST` for current or explicitly selected contained directories
- streamed Unix-style directory rows with file type and size
- passive `RETR` downloads with byte-preserving block transfers
- passive `STOR` uploads with bounded block transfers
- file size queries, deletion, directory management and safe renaming
- resumable `RETR` and `STOR` transfers through `REST`
- explicit 16 KB public-memory transfer buffer with complete cleanup
- classified passive-connect and transfer timeout errors
- centralized idempotent data-connection cleanup
- recoverable transfer failures that preserve the FTP control session
- `EINTR` retry and `EWOULDBLOCK` waiting without busy loops

Authenticated sessions accept the implemented discovery, authentication,
directory, transfer-mode, passive-mode, file-management, transfer and shutdown
commands.


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

The configured `root` must exist and must be a directory. MiniFTPD refuses to
start otherwise. Client paths are exposed as Unix-style virtual paths below
`/`; Amiga device and volume names supplied by clients are rejected.

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
250 Directory changed.
CDUP
250 Directory changed.
CWD ../..
550 Directory unavailable.
CWD DH0:
550 Directory unavailable.
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

After opening the passive data connection, `LIST`, `LIST -la` and `LIST test`
stream directory rows to that connection. A successful transfer returns `150`
before the listing and `226` after the data socket has closed. Standard FTP
clients can perform this sequence with their normal `ls` command.

Files can be downloaded with the standard FTP `get` command after selecting
binary mode. `RETR` also accepts contained absolute and relative virtual paths.
For byte-for-byte verification, compare the source and downloaded file sizes and
checksums.

Files can be uploaded with the standard FTP `put` command when `readonly=0`.
`STOR` validates the target and its existing parent directory against the
configured FTP root before creating or replacing a file. Upload data is received
and written in 16 KB public-memory blocks. A clean data-socket EOF completes the
transfer. If the connection breaks or a disk write fails, MiniFTPD reports the
error and intentionally keeps the partial file for diagnosis or recovery.

Additional authenticated commands provide `SIZE`, `DELE`, `MKD`, `RMD`,
`RNFR`/`RNTO` and `REST`. All paths remain confined to the configured FTP
root. `readonly=1` rejects every command that changes files or directories.
`REST` stores a one-shot byte offset consumed by the next `RETR` or `STOR`; the
server advertises `SIZE` and `REST STREAM` through `FEAT`.

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
