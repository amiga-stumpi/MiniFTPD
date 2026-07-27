# MiniFTPD

MiniFTPD is a lightweight FTP server for classic Amiga systems. It is being
developed primarily for AmigaOS 1.3 and TheWire13's `bsdsocket.library`.

The project uses a single event loop and avoids threads, POSIX runtime
assumptions and large automatic buffers.

## Current status

Project steps 1 through 3 are complete:

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
- bounded CRLF command parsing and partial socket-write handling

The current protocol skeleton accepts `QUIT`. Other commands receive a
standards-compliant `502 Command not implemented` response until their
respective roadmap phases are implemented.

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

## Step 3 test

Start MiniFTPD on the Amiga and connect from another machine:

```sh
nc AMIGA_IP 21
```

The control connection should behave as follows:

```text
220 MiniFTPD ready.
NOOP
502 Command not implemented.
QUIT
221 MiniFTPD closing connection.
```

Only one client is accepted at a time. A second connection receives
`421 MiniFTPD is busy`.

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
