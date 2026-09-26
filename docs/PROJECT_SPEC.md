# IT305 Socket Programming Course Project Specification
## Fault-Tolerant Topic-Based File Distribution Service

**Document Status:** Approved Engineering Specification  
**Version:** 1.0.0  
**Repository:** IT305-Socket-Programming  
**Target Platform:** POSIX-compliant UNIX/Linux Systems (C99 / GCC)

---

## 1. Executive Summary

This document defines the complete engineering specification for the **Fault-Tolerant Topic-Based File Distribution Service**. The system is a high-performance, resilient client-server application built in C using POSIX socket programming. It distributes topic-organized datasets (specifically the **Animals10** dataset) over TCP networks under both ideal and lossy network conditions.

The specification covers both **Part I** (Single-threaded and Multi-threaded topic file transfer) and **Part II** (Fault-tolerant transfer under probabilistic connection interruptions, covering un-sessioned restart, checkpointed session resume, and enhanced parallel non-blocking streaming).

---

## 2. Functional Requirements

### 2.1 Topic-Based File Distribution (Part I & Part II)
- **FR-1.1 Topic Mapping:** The server shall dynamically map a user-requested topic name (e.g., `dog`, `cat`, `elephant`) to a physical filesystem directory located under `<topics_root_dir>/<topic>/`.
- **FR-1.2 Topic Directory Inspection:** Upon receiving a valid topic request (`GET <topic>`), the server shall scan the directory, build a file manifest (relative paths and exact byte sizes), and transmit all files belonging to that topic directory to the client.
- **FR-1.3 Invalid Topic Handling:** If a client requests a topic directory that does not exist or is unreadable, the server shall respond with an explicit error frame (`ERR_TOPIC_NOT_FOUND`) and gracefully close or reset the client request.
- **FR-1.4 Data Integrity:** Files received by the client must match the server's source files byte-for-byte. The client must preserve relative sub-paths if subdirectories exist within a topic folder.

### 2.2 Concurrency Architecture (Part I)
- **FR-2.1 Single-Threaded Server (Part I Mode 1):**
  - Shall handle client requests sequentially on a single main thread.
  - While serving client $N$, incoming connections from client $N+1$ must wait in the TCP listen backlog queue.
- **FR-2.2 Multi-Threaded Server (Part I Mode 2 & Part II):**
  - Shall handle multiple concurrent client connections simultaneously.
  - Upon accepting a connection (`accept()`), the main thread shall delegate socket processing to a worker thread (e.g. `pthread_create` or pre-forked thread pool model).
  - Main listener loop shall remain immediately responsive to new incoming client connection attempts.

### 2.3 Fault Injection & Interruption (Part II)
- **FR-3.1 Failure Probability Injection:**
  - The server shall accept a floating-point failure probability $p \in [0.0, 1.0]$ via CLI.
  - During file transmission, the server shall probabilistically interrupt active client socket connections based on $p$.
  - Interruption shall be simulated by forcefully closing (`close()`) or shutting down (`shutdown(fd, SHUT_RDWR)`) the active client socket mid-stream without sending a graceful completion framing.
- **FR-3.2 Case 1: No Session Management:**
  - Upon connection interruption, the client detects socket disconnection (`recv()` returns 0 or `ECONNRESET`).
  - Client automatically attempts reconnection to the server.
  - Because no session state is maintained, the transfer restarts completely from the beginning (File 0, Byte Offset 0).
  - All previously received bytes for that topic session are discarded or overwritten, resulting in measured **redundant retransmissions**.
- **FR-3.3 Case 2: Session Management & Checkpointing:**
  - The system shall maintain session state on both client and server.
  - **Server Session State:** Stores active/suspended sessions indexed by a unique `Session-ID`. State includes: `Session-ID`, `Topic`, `Current File Index`, `Current File Byte Offset`, `Total Useful Bytes Transferred`, `Last Activity Timestamp`.
  - **Client Checkpoint State:** Maintains a local persistent checkpoint file (`.session_<topic>.chk`) containing the server-assigned `Session-ID`, `File Index`, `Byte Offset`, and SHA-256 / length progress per file.
  - **Resume Mechanism:** Following a connection interruption and reconnection, the client transmits a resume request containing `Session-ID`, `Resume File Index`, and `Resume Byte Offset`.
  - **Zero-Redundancy Resume:** The server validates the session, seeks to `Resume File Index` at `Resume Byte Offset` using `lseek()`, and resumes streaming strictly from that offset. Data successfully transferred before failure is **never retransmitted**.

### 2.4 Case 2 Enhanced: Performance Optimization
- **FR-4.1 Selected Enhancement: Multi-Stream Parallel TCP Range Streaming with Non-Blocking I/O (`epoll`/`select`)**:
  - *Technical Choice:* Rather than a single sequential TCP pipe, the enhanced client establishes $K$ concurrent TCP socket connections for a requested topic, issuing ranged chunk requests across topic files using non-blocking socket I/O.
  - *Session Integration:* Each parallel stream registers under the parent `Session-ID` with range offsets $[start\_offset, end\_offset)$.
  - *Fault Resilience:* If stream $i$ suffers probabilistic failure, only stream $i$'s chunk range is re-queued/resumed, avoiding single-thread bottlenecking and maximizing bandwidth utilization over lossy links.

---

## 3. Mandatory Command-Line Interfaces (CLI)

Strict compliance with the assigned CLI signatures is required across all executables.

### 3.1 Server CLI

#### Part I Server (Single-threaded & Multi-threaded variants)
```bash
./server <port> <topics_root_dir>
```
- `<port>`: Integer TCP port to bind (1024–65535).
- `<topics_root_dir>`: Absolute or relative path to the directory containing topic subfolders (e.g., `./dataset/Animals10`).

#### Part II Server (Case 1, Case 2, Case 2 Enhanced)
```bash
./server <port> <topics_root_dir> <failure_probability>
```
- `<port>`: Integer TCP port to bind.
- `<topics_root_dir>`: Path to root topic folder.
- `<failure_probability>`: Floating-point value $p \in [0.0, 1.0]$ representing probability of connection fault per chunk/block.

### 3.2 Client CLI (All Parts & Cases)
```bash
./client <server_ip> <server_port> <topic> <output_dir>
```
- `<server_ip>`: IPv4 address of target server (e.g. `192.168.1.50` or `127.0.0.1`).
- `<server_port>`: TCP port of target server.
- `<topic>`: Name of requested topic subfolder (e.g. `dog`, `cat`).
- `<output_dir>`: Local destination directory where downloaded topic files are stored.

*Note on Optional Trailing Arguments:* Optional flags (e.g., `--parallel-streams K` or `--session-file path`) may be accepted as optional trailing arguments, provided default fallback behavior strictly adheres to the mandatory 4-argument client CLI.

---

## 4. Non-Functional Requirements

- **NFR-1 Portability:** Clean C99 code standard. Compiles without warnings under standard GCC flags (`-Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L`).
- **NFR-2 No Hardcoded Configuration:** Zero hardcoded IPs, ports, filesystem paths, or topic directories. All runtime parameters must originate from CLI or dynamic inspection.
- **NFR-3 Protocol Robustness:** Protocol must explicitly handle partial TCP reads (`recv()`) and partial TCP writes (`send()`). Framing headers must use network byte order (`htonl`/`ntohl`).
- **NFR-4 Resource Leak Protection:** Memory allocated via `malloc()` must be freed. Sockets (`close()`), file descriptors (`close()`), directory handles (`closedir()`), and mutexes (`pthread_mutex_destroy()`) must be cleanly cleaned up on normal exit and error paths.
- **NFR-5 Thread Safety:** Server session tables, logger instances, and shared statistics counters must be synchronized via POSIX mutexes (`pthread_mutex_t`) or atomic operations.

---

## 5. Experimental Measurement & Metrics Requirements

Experiments require empirical measurements collected using 3 physical/virtual computers (1 Server machine, 2 Client machines hosting multiple client instances).

### 5.1 Primary Metrics
1. **Transfer Time ($T_{comp}$):** Total elapsed wall-clock seconds from initial GET request dispatch to final file receipt acknowledgement (measured via `clock_gettime(CLOCK_MONOTONIC)`).
2. **Total Bytes Transferred ($B_{total}$):** Combined count of all bytes transmitted across the wire (including framing headers, retransmitted data, and metadata).
3. **Useful Bytes Transferred ($B_{useful}$):** Actual net payload file bytes stored on disk forming the requested topic.
4. **Redundant Bytes Transferred ($B_{redundant}$):** $B_{redundant} = B_{total} - B_{useful} - B_{overhead}$.
5. **Protocol Overhead ($B_{overhead}$):** Sum of protocol header bytes, control messages, and ACK packets.
6. **Aggregate Throughput ($R_{agg}$):** $\frac{\text{Total Useful Bytes from all concurrent clients}}{\text{Max Completion Time across clients}}$ (in Mbps or MB/s).

### 5.2 Required Experimental Graphs
- **Graph 1:** Throughput vs. Number of Concurrent Clients ($N \in \{1, 2, 4, 8, 16, 32\}$) — Single-Threaded vs. Multi-Threaded Server (Part I).
- **Graph 2:** Total Bytes Transferred & Redundant Bytes vs. Failure Probability ($p \in \{0.0, 0.05, 0.1, 0.2, 0.5\}$) — Case 1 (No Session) vs. Case 2 (Session Management).
- **Graph 3:** Transfer Completion Time vs. Failure Probability — Case 1 vs. Case 2 vs. Case 2 Enhanced.
- **Graph 4:** Effective Throughput vs. Failure Probability — Case 2 Standard vs. Case 2 Enhanced (Parallel non-blocking streaming).

---

## 6. Mandatory Repository & Submission Structure

The final deliverable repository must build clean executables inside each part directory, satisfying:

```
FinalSubmission_Group_<XXX>_<YYY>/
├── FinalReport_Group_<XXX>_<YYY>.pdf
├── README.md
├── Part1/
│   ├── Makefile
│   ├── server
│   └── client
├── Part2_Case1/
│   ├── Makefile
│   ├── server
│   └── client
├── Part2_Case2/
│   ├── Makefile
│   ├── server
│   └── client
├── Part2_Case2_Enhanced/
│   ├── Makefile
│   ├── server
│   └── client
└── Results/
    ├── raw_data/          # CSV experiment logs
    └── plots/             # Generated performance graphs
```

---

## 7. Design Decisions & Unspecified Requirements Resolution

| Item | Unspecified in Assignment | Architected Solution & Justification |
| :--- | :--- | :--- |
| **1. TCP Framing** | TCP stream message boundaries are un-framed in raw TCP. | Fixed 12-byte protocol header `[Magic(2B) | Type(1B) | Flags(1B) | PayloadLen(4B) | Seq/Offset_Low(4B)]` to eliminate framing ambiguity and partial `recv()` corruption. |
| **2. Session Identifier** | Format of session ID not defined. | 128-bit random token formatted as 32-character hexadecimal string, generated by server on initial `GET` request. |
| **3. Directory Recursion** | Subdirectories inside topic folders. | Dynamic recursive traversal (`nftw` or POSIX `opendir` recursive stack), preserving relative sub-paths in manifest. |
| **4. Fault Injection Mechanism** | How server triggers probabilistic failure. | Per-chunk pseudo-random evaluation (`drand48() < failure_prob`) during socket write loop, executing abrupt `close(fd)`. |
| **5. Case 2 Enhanced Selection** | Assignment suggests non-blocking / parallel TCP. | Multi-stream Parallel Range Transfer with non-blocking `epoll`/`select` I/O. Justified: maximises TCP link fill rate and isolates packet loss per range chunk. |
