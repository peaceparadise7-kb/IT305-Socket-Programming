# IT305 Socket Programming Course Project Specification
## Fault-Tolerant Topic-Based File Distribution Service

**Document Status:** Approved Engineering Specification (Final Refinement)  
**Version:** 1.2.0  
**Repository:** IT305-Socket-Programming  
**Target Platform:** POSIX-compliant UNIX/Linux Systems (C99 / GCC)

---

## 1. Executive Summary

This document defines the complete engineering specification for the **Fault-Tolerant Topic-Based File Distribution Service**. The system is a high-performance, resilient client-server application built in C using POSIX socket programming. It distributes topic-organized datasets (specifically the **Animals10** dataset) over TCP networks under both ideal and lossy network conditions.

The specification covers both **Part I** (Single-threaded and Multi-threaded topic file transfer) and **Part II** (Fault-tolerant transfer under probabilistic connection interruptions, covering un-sessioned restart, checkpointed session resume with explicit acknowledgement commitments, and enhanced parallel non-blocking streaming).

---

## 2. Functional Requirements

### 2.1 Topic-Based File Distribution (Part I & Part II)
- **FR-1.1 Topic Mapping:** The server shall dynamically map a user-requested topic name (e.g., `dog`, `cat`, `elephant`) to a physical filesystem directory located under `<topics_root_dir>/<topic>/`.
- **FR-1.2 Topic Directory Inspection & Bounded Multi-Frame Manifest:** Upon receiving a valid topic request (`GET <topic>`), the server shall scan the directory, build a file manifest, and stream the manifest using a multi-frame sequence (`MSG_MANIFEST_START`, repeating `MSG_MANIFEST_ENTRY`, `MSG_MANIFEST_END`).
- **FR-1.3 Manifest Relative Path Length Limit:** The server enforces a maximum relative path length of $MAX\_PATH\_LEN = 4096$ bytes per file entry. If any file path exceeds 4096 bytes or causes a frame to exceed $MAX\_PAYLOAD\_LEN = 65536$ bytes, the server shall abort manifest creation and return an explicit error frame (`MSG_ERROR`, code `0x0400 ERR_PATH_TOO_LONG`).
- **FR-1.4 Invalid Topic Handling:** If a requested topic directory does not exist or is unreadable, the server shall respond with `MSG_ERROR` (code `0x0404 ERR_TOPIC_NOT_FOUND`) and close the connection.
- **FR-1.5 Data Integrity:** Files received by the client must match the server's source files byte-for-byte upon completion. Relative directory structures within topic folders must be preserved.

### 2.2 Concurrency Architecture (Part I)
- **FR-2.1 Concurrency Modes:**
  - **Single-Threaded Server Mode:** Handles client connections sequentially on a single main thread. While serving client $N$, incoming connections from client $N+1$ wait in the TCP listen backlog queue.
  - **Multi-Threaded Server Mode:** Handles multiple concurrent client connections simultaneously. The listener thread accepts connections (`accept()`) and immediately delegates each socket to a detached worker thread (`pthread_create`).
- **FR-2.2 Mode Selection CLI:**
  - The default invocation `./server <port> <topics_root_dir>` operates in **Multi-Threaded Mode**.
  - An optional trailing flag `--mode single` or `--mode multi` allows explicit selection without breaking mandatory assignment interface rules.

### 2.3 Fault Injection & Interruption (Part II)
- **FR-3.1 Failure Probability Injection:**
  - The server accepts a floating-point failure probability $p \in [0.0, 1.0]$ via CLI.
  - The Bernoulli failure decision occurs on the server per data chunk **immediately before** invoking socket write (`write_n()`).
  - At $p = 0.0$, zero artificial failures are injected. At $p = 1.0$, connection drops on the first chunk attempt (client max retry limit enforces abort to prevent infinite non-progressing loops).
  - Interruption is executed by abruptly closing (`close()`) or shutting down (`shutdown(fd, SHUT_RDWR)`) the active client socket mid-stream.
- **FR-3.2 Case 1: No Session Management (Full Retransmission Restart):**
  - Upon connection drop, the client detects disconnection (`recv()` returns 0 or error).
  - Client automatically reconnects to the server with a clean session request.
  - The transfer restarts completely from the beginning (File 0, Byte Offset 0).
  - All previously received data for that topic must be retransmitted from the beginning, resulting in significant **redundant retransmissions**.
- **FR-3.3 Case 2: Session Management & Checkpoint-Based Minimized Redundancy:**
  - **Commitment Semantics:** A byte becomes committed ONLY after:
    1. Client receives the byte payload.
    2. Client writes the payload to disk.
    3. Client updates its local checkpoint file (`.session_<topic>.chk`).
    4. Client sends a `MSG_ACK` frame.
    5. Server receives and processes that `MSG_ACK`.
  - **Pre-ACK Failure Behavior:** If a failure occurs before the server commits the `MSG_ACK`, data sent after the last committed checkpoint may need to be retransmitted.
  - **Redundant Retransmission Definition:** Those retransmitted file payload bytes are counted as $B_{redundant}$.
  - **Checkpoint Guarantee:** Case 2 guarantees that bytes BEFORE the last committed checkpoint are NEVER retransmitted. Case 1 restarts from the beginning and can therefore retransmit a much larger amount of data.

### 2.4 Case 2 Enhanced: Multi-Stream Non-Blocking Range Streaming
- **FR-4.1 Enhancement Architecture:** Multi-stream parallel TCP connections with non-blocking socket I/O (`epoll`/`select`).
- **FR-4.2 Configurable Stream Count:** Number of parallel streams $K$ is configurable via CLI (default $K=4$).
- **FR-4.3 Disjoint Range Assignment:** Parallel streams pull non-overlapping chunk ranges $[start\_offset, end\_offset)$ from a synchronized work-queue.
- **FR-4.4 Failure Recovery & Checkpointing:** If stream $i$ fails, its un-ACKed range chunk is returned to the work-queue. Checkpoints commit the contiguous completed byte offsets across streams.
- **FR-4.5 Objective Measurement:** Performance characteristics (completion time, throughput, overhead) are evaluated experimentally against standard Case 2; no fixed percentage improvement is assumed.

---

## 3. Mandatory Command-Line Interfaces (CLI)

### 3.1 Server CLI

#### Part I Server
```bash
./server <port> <topics_root_dir> [--mode single|multi]
```

#### Part II Server (Case 1, Case 2, Case 2 Enhanced)
```bash
./server <port> <topics_root_dir> <failure_probability> [--seed <uint32>]
```

### 3.2 Client CLI (All Parts & Cases)
```bash
./client <server_ip> <server_port> <topic> <output_dir> [--max-retries <N>] [--parallel-streams <K>]
```

---

## 4. Non-Functional Requirements

- **NFR-1 Portability:** Clean C99 standard (`-std=c99 -D_POSIX_C_SOURCE=200809L`). Compiles cleanly under GCC with `-Wall -Wextra -Werror -pedantic`.
- **NFR-2 Zero Hardcoding:** Zero hardcoded IPs, ports, filesystem paths, or topic names.
- **NFR-3 Protocol Robustness:** Protocol explicitly handles partial TCP reads (`recv()`) and writes (`send()`). Framing headers use network byte order (`htonl`/`ntohl`).
- **NFR-4 Resource Protection:** Memory allocated via `malloc()` is freed. Sockets (`close()`), file descriptors (`close()`), directory handles (`closedir()`), and mutexes are cleanly cleaned up on all exit and error paths.
- **NFR-5 Path Length Safety:** Relative file paths must not exceed $MAX\_PATH\_LEN = 4096$ bytes.

---

## 5. Experimental Measurement & Metrics Requirements

### 5.1 Mutually Consistent Metrics Definitions
- **Useful Bytes ($B_{useful}$):** Net topic file payload bytes successfully received and written to disk forming the requested dataset.
- **Redundant Bytes ($B_{redundant}$):** Retransmitted **FILE PAYLOAD bytes ONLY** that are sent after the last committed checkpoint due to un-ACKed connection failures.
- **Protocol Overhead Bytes ($B_{overhead}$):** Every transmitted application-protocol byte that is NOT file payload data, including:
  - All 12-byte application headers,
  - Manifest frame payloads (`MSG_MANIFEST_START`, `MSG_MANIFEST_ENTRY`, `MSG_MANIFEST_END`),
  - ACK frames (`MSG_ACK`),
  - Error/control frames (`MSG_ERROR`, `MSG_GET_REQ`, `MSG_RANGE_REQ`, `MSG_TRANSFER_DONE`),
  - Application headers belonging to retransmitted data chunks.
- **Wire Bytes ($B_{wire}$):** Total bytes transmitted across the TCP socket interface.
- **Conservation Equation:**
  $$B_{wire} = B_{useful} + B_{redundant} + B_{overhead}$$
- **Completion Time ($T_{comp}$):** Total wall-clock seconds from initial GET request to final topic completion signal.
- **Effective Throughput ($R_{eff}$):**
  $$R_{eff} = \frac{B_{useful}}{T_{comp}} \quad \text{(Mbps)}$$

---

## 6. Submission Structure

Matches mandatory directory layout: `Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`, `Results/`, `FinalReport_Group_<XXX>_<YYY>.pdf`, `README.md`.
