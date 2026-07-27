# MiniFTPD implementation roadmap

This document is the authoritative project plan. Every completed phase must be
updated here in the same commit as its implementation.

## Progress

| Step | Area | Status |
|---:|---|---|
| 1 | Project foundation | Complete |
| 2 | Configuration | Complete |
| 3 | TCP control connection | Planned |
| 4 | Session state | Planned |
| 5 | Authentication | Planned |
| 6 | Passive data connections | Planned |
| 7 | Basic FTP commands | Planned |
| 8 | Filesystem containment | Planned |
| 9 | Directory listings | Planned |
| 10 | Downloads | Planned |
| 11 | Uploads | Planned |
| 12 | Additional file commands | Planned |
| 13 | Error handling and cleanup | Planned |
| 14 | Optional logging | Planned |
| 15 | Regression testing | Planned |
| 16 | Future extensions | Deferred |

## 1. Project foundation

Status: **Complete**

- Build an independent `MiniFTPD` executable.
- Target Motorola 68000 and AmigaOS 1.3.
- Use bebbo GCC with `-mcrt=nix13`.
- Avoid threads and POSIX runtime dependencies.
- Check for `bsdsocket.library` during startup.
- Establish small C89-compatible configuration and session structures.
- Keep large future transfer buffers out of the process stack.
- Ship `miniftpd.conf` beside the executable.

## 2. Configuration

Status: **Complete**

- Load `miniftpd.conf` from the program directory.
- Create a default file when no configuration exists.
- Parse and validate:
  - control port
  - exported root
  - user and password
  - anonymous and read-only modes
  - passive port range
  - inactivity timeout
  - logging switch
- Reject invalid port ranges and unsafe empty roots.
- Never print or log passwords.
- The parser is strict: unknown keys and malformed values stop startup.
- Accepted ranges are control port 1-65535, passive ports 1024-65535,
  and timeout 10-3600 seconds.
- The parser uses a static 2048-byte file buffer to protect the small
  process stack.

## 3. TCP control connection

- Create, bind and listen on the configured control port.
- Initially accept one FTP client at a time.
- Use `WaitSelect()` in a single event loop.
- Receive commands terminated by CRLF.
- Handle partial `send()` and `recv()` results.
- Enforce command length limits.

## 4. Session state

- Track connected, user-seen and authenticated states.
- Track the current virtual FTP directory.
- Support ASCII and binary transfer modes.
- Own control, passive-listener and data sockets independently.
- Apply an inactivity timeout.

## 5. Authentication

- Implement `USER` and `PASS`.
- Optionally allow anonymous access.
- Restrict commands until authentication succeeds.
- Introduce delays or connection limits for repeated failures if necessary.

## 6. Passive data connections

- Implement `PASV`.
- Select an available port from the configured range.
- Return a valid `227 Entering Passive Mode` response.
- Accept one temporary data connection.
- Close passive resources after success, failure or timeout.

## 7. Basic FTP commands

- `SYST`, `FEAT`, `NOOP`, `QUIT`
- `USER`, `PASS`
- `PWD`, `CWD`, `CDUP`
- `TYPE`
- `PASV`
- `LIST`
- `RETR`
- `STOR`

## 8. Filesystem containment

- Treat the configured root as an absolute security boundary.
- Normalize virtual paths.
- Prevent `..` traversal and Amiga device/volume escapes.
- Validate names before joining Amiga paths.
- Enforce read-only mode for every modifying command.

## 9. Directory listings

- Generate broadly compatible `LIST` output.
- Distinguish files and directories.
- Report file sizes.
- Stream entries without building a complete listing on the stack.
- Add `NLST` and `MLSD` later if client compatibility requires them.

## 10. Downloads

- Implement `RETR`.
- Allocate the transfer buffer explicitly.
- Read and send in bounded blocks.
- Handle partial socket writes and remote aborts.
- Always close files and data sockets.

## 11. Uploads

- Implement `STOR`.
- Receive and write bounded blocks.
- Treat data-socket EOF as normal completion.
- Report disk-full and write errors.
- Keep partial files initially and document that behavior.

## 12. Additional file commands

- `SIZE`
- `DELE`
- `MKD`, `RMD`
- `RNFR`, `RNTO`
- `REST` and resumed transfers

## 13. Error handling and cleanup

- Distinguish FTP status, socket errors, timeouts and clean EOF.
- Use centralized idempotent cleanup paths.
- Preserve the control connection after recoverable transfer failures.
- Reset the complete session after control connection loss.
- Handle `EWOULDBLOCK` without busy waiting.

## 14. Optional logging

- Enable logging only with `log_enabled=1`.
- Record connections, logins, commands and transfer results.
- Redact passwords.
- Keep logging disabled by default for performance.

## 15. Regression testing

- Correct and rejected logins.
- Directory navigation and traversal attempts.
- Small and large binary downloads.
- Small and large binary uploads.
- Byte-for-byte transfer verification.
- Interrupted transfers.
- Passive-port exhaustion and timeouts.
- Read-only operation.
- AmigaOS 1.3 and AmigaOS 3.x.
- MiniFTP, command-line FTP, curl and FileZilla clients.

## 16. Future extensions

- Multiple concurrent clients.
- Active FTP using `PORT`.
- Multiple users and exported roots.
- A status/configuration GUI.
- Optional FTPS after TLS and concurrent socket operation are proven stable.
