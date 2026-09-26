# Guidance for AI Agents & Developers
## IT305 Socket Programming Repository Conventions

This document specifies mandatory working rules, coding standards, and operational workflows for AI agents and human developers modifying this repository.

---

## 1. Project Purpose & Scope

This codebase implements a **Fault-Tolerant Topic-Based File Distribution Service** written in POSIX C99. It supports sequential (single-threaded) and concurrent (multi-threaded) file serving, probabilistic fault injection, un-sessioned restart (Part II Case 1), session-based checkpoint resume with explicit ACK commitments (Part II Case 2), and multi-stream non-blocking streaming (Part II Case 2 Enhanced).

---

## 2. Mandatory Working Rules

### Rule 1: Zero Hardcoding Policy
- **NEVER** hardcode IP addresses, TCP ports, directory paths, or topic names in source code.
- All network parameters and filesystem paths MUST be accepted via command-line arguments as specified by the mandatory CLI signatures in `docs/PROJECT_SPEC.md`.

### Rule 2: Protocol Integrity
- The application framing protocol defined in `docs/PROTOCOL.md` is the single source of truth.
- Header layout is strictly 12 bytes: `[Magic(2B: 0x4954) | Type(1B) | Flags(1B) | PayloadLen(4B) | SeqNum(4B)]`.
- `SeqNum` is a 32-bit packet sequence number. All 64-bit file byte offsets (`uint64_t`) are explicitly serialized inside packet payloads.
- Manifest streaming uses a multi-frame sequence (`MSG_MANIFEST_START`, `MSG_MANIFEST_ENTRY`, `MSG_MANIFEST_END`).
- Checkpoint commits require an explicit client `MSG_ACK` frame.
- **DO NOT** modify binary packet headers, message codes, byte order conversions (`htonl`/`ntohl`), or payload layouts without updating `docs/PROTOCOL.md`.

### Rule 3: Robust TCP Byte Stream Handling
- Sockets are byte streams, not message buffers. Never assume a single `recv()` call returns a full application header or frame payload.
- Always use the helper functions `read_n()` and `write_n()` located in `src/common/protocol.c` to guarantee complete header and payload reads/writes.

### Rule 4: System Resource Management & Error Handling
- Check return values of ALL system and library calls: `socket()`, `bind()`, `listen()`, `accept()`, `send()`, `recv()`, `malloc()`, `fopen()`, `pthread_create()`.
- Sockets and file descriptors MUST be closed on every error and exit path to prevent descriptor exhaustion.
- Memory allocated with `malloc()` MUST be freed.

### Rule 5: Submission Directory Integrity
- Each of the four submission folders MUST contain its own `Makefile` and independently compile exactly two executables named `server` and `client`:
  - `Part1/` $\rightarrow$ `server`, `client`
  - `Part2_Case1/` $\rightarrow$ `server`, `client`
  - `Part2_Case2/` $\rightarrow$ `server`, `client`
  - `Part2_Case2_Enhanced/` $\rightarrow$ `server`, `client`
- Do NOT generate extra binary executables or commit compiled artifacts (`.o`, executable files) to Git.

### Rule 6: Verification Before Completion
- Never mark a phase or feature complete without executing compilation (`make`) and running unit/integration test verification.
- Always verify data integrity using `diff -r` or SHA-256 hash comparison between source topic directories and client output directories.

---

## 3. Code & Build Conventions

- **Language:** C99 standard (`-std=c99 -D_POSIX_C_SOURCE=200809L`).
- **Compiler Flags:** `-Wall -Wextra -Werror -pedantic -g -pthread`.
- **Style:** K&R or 4-space indentation. Clear function docstrings. Descriptive variable names (`client_socket_fd`, `bytes_remaining`).
- **Git Hygiene:** Commit logical units of work. Never commit `.o` files, binary executables, core dumps, or local temporary test datasets.

---

## 4. Required CLI Signatures Reminder

### Server Executable
```bash
# Part I Server
./server <port> <topics_root_dir> [--mode single|multi]

# Part II Server (Case 1, Case 2, Case 2 Enhanced)
./server <port> <topics_root_dir> <failure_probability> [--seed <uint32>]
```

### Client Executable
```bash
./client <server_ip> <server_port> <topic> <output_dir> [--max-retries <N>]
```
