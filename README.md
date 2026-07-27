# MiniFTPD

MiniFTPD is a lightweight FTP server for classic Amiga systems. It is being
developed primarily for AmigaOS 1.3 and TheWire13's `bsdsocket.library`.

The project uses a single event loop and avoids threads, POSIX runtime
assumptions and large automatic buffers.

## Current status

Project steps 1 and 2 are complete:

- standalone AmigaOS executable scaffold
- bebbo GCC build using the Kickstart 1.3 `nix13` CRT
- startup check for `bsdsocket.library`
- initial configuration file named `miniftpd.conf`
- configuration and session data structures
- complete implementation roadmap
- strict `miniftpd.conf` loading and validation
- automatic default configuration creation
- effective configuration summary without password disclosure

No FTP port is opened in this development build yet.

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
