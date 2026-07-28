# MiniFTPD implementation roadmap

This document is the authoritative project plan. Every completed phase must be
updated here in the same commit as its implementation.

## Progress

| Step | Area | Status |
|---:|---|---|
| 1 | Project foundation | Complete |
| 2 | Configuration | Complete |
| 3 | TCP control connection | Complete |
| 4 | Session state | Complete |
| 5 | Authentication | Complete |
| 6 | Passive data connections | Complete |
| 7 | Basic FTP commands | Complete |
| 8 | Filesystem containment | Complete |
| 9 | Directory listings | Complete |
| 10 | Downloads | Complete |
| 11 | Uploads | Complete |
| 12 | Additional file commands | Complete |
| 13 | Error handling and cleanup | Complete |
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

Status: **Complete**

- Create, bind and listen on the configured control port.
- Initially accept one FTP client at a time.
- Use `WaitSelect()` in a single event loop.
- Receive commands terminated by CRLF.
- Handle partial `send()` and `recv()` results.
- Enforce command length limits.
- Set accepted sockets to non-blocking mode.
- Reject additional clients with FTP status `421`.
- Support `QUIT` as the first control command and return `502` for commands
  assigned to later phases.
- Stop cleanly on Ctrl-C.
- Poll the Exec break signal directly as a compatibility fallback for stacks
  that do not return signals through `WaitSelect()`.

## 4. Session state

Status: **Complete**

- Track connected, user-seen and authenticated states.
- Track the current virtual FTP directory.
- Support ASCII and binary transfer modes.
- Own control, passive-listener and data sockets independently.
- Apply an inactivity timeout.
- Reset inactivity accounting whenever control data is received.
- Close data, passive-listener and control sockets independently and safely.
- Initialize each connection in binary mode with virtual directory `/`.
- Accept `TYPE A` and `TYPE I` to update the transfer mode.

## 5. Authentication

Status: **Complete**

- Implement `USER` and `PASS`.
- Optionally allow anonymous access.
- Restrict commands until authentication succeeds.
- Limit each control connection to three failed password attempts.
- Keep password arguments out of console logging.
- Return the same password challenge for known and unknown users.
- Support `anonymous` and `ftp` when anonymous access is enabled.


## 6. Passive data connections

Status: **Complete**

- Implement `PASV` after successful authentication.
- Rotate through the configured passive port range and skip occupied ports.
- Derive the advertised IPv4 address from the control connection with
  `getsockname()`.
- Return a valid `227 Entering Passive Mode` response.
- Accept one temporary non-blocking data connection in the main event loop.
- Replace stale passive or data sockets when a new `PASV` command arrives.
- Close passive resources after control loss, failure, timeout or shutdown.

## 7. Basic FTP commands

Status: **Complete**

- Implement `SYST`, `FEAT`, `NOOP` and `QUIT`.
- Keep `SYST`, `FEAT`, `NOOP`, `USER`, `PASS` and `QUIT` available before
  authentication for broad FTP-client compatibility.
- Implement authenticated `PWD`.
- Establish initial root-only `CWD` and `CDUP` behavior, extended to real
  contained directories in step 8.
- Retain authenticated `TYPE` and `PASV`.
- Return standards-based status codes for unsupported directories and
  commands.
- Implement `LIST`, `RETR` and `STOR` only in steps 9 through 11 after the
  filesystem security boundary from step 8 is complete.

## 8. Filesystem containment

Status: **Complete**

- Treat the configured root as an absolute security boundary.
- Verify at startup that the configured root exists and is a directory.
- Normalize absolute and relative virtual FTP paths.
- Reject traversal above the virtual root.
- Reject control characters, quotes and Amiga device or volume separators.
- Build DOS paths only from validated virtual path components.
- Verify target ancestry with OS1.x-compatible DOS locks and `ParentDir()`.
- Make `CWD` and `CDUP` operate on real directories inside the root.
- Centralize read-only policy and reject `STOR` before transfer handling when
  `readonly=1`.
- Keep path and `FileInfoBlock` work buffers outside the process stack.

## 9. Directory listings

Status: **Complete**

- Implement authenticated `LIST` over a passive data connection.
- Support the current directory, a validated path argument and common ignored
  option forms such as `LIST -la`.
- Generate broadly compatible Unix-style listing rows.
- Distinguish files and directories and report file sizes.
- Stream each entry directly instead of building a complete listing in memory.
- Keep listing path, filename, line and `FileInfoBlock` buffers off the stack.
- Sanitize control characters in existing filesystem names.
- Wait up to ten seconds for a pending passive data connection.
- Close data and passive sockets after success, failure or timeout.
- Return `150`, `226`, `425`, `426`, `451` and `550` as appropriate.
- Add `NLST` and `MLSD` later if client compatibility requires them.

## 10. Downloads

Status: **Complete**

- Implement authenticated `RETR` over passive data connections.
- Resolve absolute and relative file paths through the containment layer.
- Reject directories, missing files, traversal and Amiga device escapes.
- Preflight the file and report its size in the `150` response.
- Allocate a 16 KB transfer buffer with `AllocMem(MEMF_PUBLIC)`.
- Read and send files in bounded blocks without loading the complete file.
- Preserve file bytes exactly for reliable binary downloads.
- Handle partial socket writes, stalled sockets and remote aborts.
- Always close the file and data socket and free the transfer buffer.
- Return `150`, `226`, `425`, `426`, `451` and `550` as appropriate.

## 11. Uploads

Status: **Complete**

- [x] Implement `STOR`.
- [x] Receive and write bounded 16 KB public-memory blocks.
- [x] Treat data-socket EOF as normal completion.
- [x] Report data, disk-full and write errors.
- [x] Keep partial files after interrupted or failed transfers and document
  that behavior.

## 12. Additional file commands

Status: **Complete**

- [x] `SIZE` with exact byte counts.
- [x] `DELE` for contained files.
- [x] `MKD` and empty-directory `RMD`.
- [x] Stateful `RNFR` and `RNTO` for contained files and directories.
- [x] One-shot `REST` offsets and resumed `RETR` and `STOR` transfers.
- [x] Central read-only enforcement for all mutating commands.
- [x] Advertise `SIZE` and `REST STREAM` through `FEAT`.

## 13. Error handling and cleanup

Status: **Complete**

- [x] Distinguish missing passive setup, socket errors, timeouts, interruption and clean upload EOF.
- [x] Return specific `425` passive-connect and `426` transfer failure replies.
- [x] Use centralized idempotent cleanup for passive-listener and data sockets.
- [x] Clean data sockets when preliminary `150` replies cannot be sent.
- [x] Preserve the control connection after recoverable transfer failures.
- [x] Reset the complete session after control connection loss.
- [x] Retry interrupted socket and `WaitSelect()` operations.
- [x] Handle `EWOULDBLOCK` through `WaitSelect()` without busy waiting.

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
