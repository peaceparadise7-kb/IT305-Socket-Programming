# IT305 Six-Workstream Integration Plan
## Parallel Development Strategy & Collaborative Engineering Framework

**Document Status:** Approved Integration Strategy  
**Version:** 1.0.0  
**Team Structure:** 6 Logical Workstreams

---

## 1. Six-Workstream Organization & Ownership

To maximize parallel development speed without causing code collision, the project is structured into six clean workstreams.

```mermaid
graph TD
    WS1[Workstream 1: Architecture & Protocol Core] --> WS2[Workstream 2: Single-Threaded Server]
    WS1 --> WS3[Workstream 3: Client & Framing Protocol]
    WS2 & WS3 --> WS4[Workstream 4: Part II Case 1 Fault Injection]
    WS4 --> WS5[Workstream 5: Part II Case 2 Session & Checkpoint]
    WS5 --> WS6[Workstream 6: Case 2 Enhanced & Experimentation]
```

### Workstream Responsibilities Matrix

| Workstream | Primary Role & Responsibility | Core Deliverables / Files Owned |
| :--- | :--- | :--- |
| **Workstream 1** | System Architect & Integration Lead | `src/common/protocol.h/.c`, `docs/*`, `AGENTS.md`, Root Makefile, CI scripts |
| **Workstream 2** | Part I Single-Threaded Server Engineer | `src/server/topic_mgr.h/.c`, `Part1/server.c` (sequential mode) |
| **Workstream 3** | Part I Client & Protocol Framing Engineer | `src/client/client_core.h/.c`, `Part1/client.c` |
| **Workstream 4** | Part II Case 1 Fault Injection Engineer | `src/server/fault_inject.h/.c`, `Part2_Case1/server.c`, `Part2_Case1/client.c` |
| **Workstream 5** | Part II Case 2 Session & Checkpoint Specialist | `src/server/session_mgr.h/.c`, `src/client/checkpoint.h/.c`, `Part2_Case2/*` |
| **Workstream 6** | Enhanced Streaming & Benchmarking Specialist | `src/client/range_stream.h/.c`, `Part2_Case2_Enhanced/*`, `scripts/*`, `Results/*` |

---

## 2. Shared Core Architecture & Header Contracts

All six workstreams rely on common interfaces defined by Workstream 1. Unilateral changes to shared headers are prohibited.

- `src/common/protocol.h`: Defines packet types, 12-byte binary header, bit flags, error codes, `read_n()`, `write_n()`.
- `src/common/utils.h`: Dynamic path formatting, monotonic timing helpers, synchronized logging.
- `src/server/topic_mgr.h`: Topic validation, directory scanner, file manifest building functions.

---

## 3. Branching & Git Integration Strategy

- **Protected Main Branch:** `main` branch remains stable and always buildable.
- **Feature Branches:**
  - `feature/ws1-protocol-core`
  - `feature/ws2-single-threaded-server`
  - `feature/ws3-client-protocol`
  - `feature/ws4-fault-injection-case1`
  - `feature/ws5-session-checkpoint-case2`
  - `feature/ws6-enhanced-streaming-benchmarks`

### Merge Sequence
1. **Merge 1 (Foundation):** `feature/ws1-protocol-core` merged into `main`.
2. **Merge 2 (Part I Base):** `feature/ws2` and `feature/ws3` merged into `main`. Clean build of `Part1/` verified.
3. **Merge 3 (Fault Injection):** `feature/ws4` merged into `main`. Verification of `Part2_Case1/`.
4. **Merge 4 (Checkpointing):** `feature/ws5` merged into `main`. Verification of `Part2_Case2/`.
5. **Merge 5 (Enhancement & Benchmark):** `feature/ws6` merged into `main`. Verification of `Part2_Case2_Enhanced/` and `Results/`.

---

## 4. Submission Directory Modular Reusability Design

Each submission directory (`Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`) compiles binaries linked against shared core modules from `src/`.

```makefile
# Example Part1/Makefile using modular src/ linkage
CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c99 -I../src/common -I../src/server -I../src/client -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

COMMON_SRCS = ../src/common/protocol.c ../src/common/utils.c
SERVER_SRCS = server.c ../src/server/topic_mgr.c ../src/server/server_core.c $(COMMON_SRCS)
CLIENT_SRCS = client.c ../src/client/client_core.c $(COMMON_SRCS)

all: server client

server: $(SERVER_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: $(CLIENT_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f server client *.o
```

This guarantees complete independence for each submission part while preventing duplicate code proliferation.
