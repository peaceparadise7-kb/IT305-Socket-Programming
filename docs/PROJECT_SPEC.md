# IT305 Socket Programming Course Project Specification
## Fault-Tolerant Topic-Based File Distribution Service

**Document Status:** Approved Engineering Specification (Refined)  
**Version:** 1.1.0  
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
- **FR-1.2 Topic Directory Inspection & Multi-Frame Manifest:** Upon receiving a valid topic request (`GET <topic>`), the server shall scan the directory, build a file manifest, and stream the manifest to the client using a multi-frame sequence (`MANIFEST_START`, repeating `MANIFEST_ENTRY`, `MANIFEST_END`). This prevents payload size overflow for directories with large file counts.
- **FR-1.3 Invalid Topic Handling:** If a client requests a topic directory that does not exist or is unreadable, the server shall respond with an explicit error frame (`MSG_ERROR`, code `0x0404`) and gracefully terminate the request.
- **FR-1.4 Data Integrity:** Files received by the client must match the server's source files byte-for-byte upon completion. Relative directory structures within topic folders must be preserved.

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
- **FR-3.2 Case 1: No Session Management:**
  - Upon connection drop, the client detects disconnection (`recv()` returns 0 or error).
  - Client automatically reconnects to the server with a clean session request.
  - The transfer restarts completely from the beginning (File 0, Byte Offset 0).
  - All previously received bytes for that topic are overwritten, resulting in measured **redundant retransmissions**.
- **FR-3.3 Case 2: Session Management & Explicit Checkpoint Commitment:**
  - **Committed Transfer Definition:** A byte offset is considered *successfully transferred and committed* ONLY when:
    1. Client receives the data chunk.
    2. Client writes the payload to disk.
    3. Client updates its local checkpoint file (`.session_<topic>.chk`).
    4. Client sends a `MSG_ACK` packet containing `(Session_ID, File_Index, Committed_Byte_Offset)`.
    5. Server receives `MSG_ACK` and updates its session record.
  - **Pre-ACK Failure Behavior:** If a failure occurs before `MSG_ACK` is committed by the server, the server retains the last committed offset $O_{last\_ack}$. Upon reconnection, the server resumes streaming from $O_{last\_ack}$. The client receives data starting at $O_{last\_ack}$ and overwrites un-ACKed trailing local bytes.
  - **Redundant Retransmission Definition:** Redundant bytes are precisely defined as **payload data retransmitted after the last committed checkpoint** due to un-ACKed network interruptions.

### 2.4 Case 2 Enhanced: Performance Optimization
- **FR-4.1 Selected Enhancement: Multi-Stream Parallel TCP Range Streaming with Non-Blocking I/O (`epoll`/`select`)**:
  - Rather than a single sequential TCP pipe, the enhanced client establishes $K$ concurrent TCP connections for a requested topic, issuing ranged chunk requests across topic files using non-blocking socket I/O.
  - *Evaluation Criteria:* System performance is evaluated by measuring total completion time and effective throughput across varying failure probabilities $p$, comparing Case 2 Enhanced directly against standard Case 2.

---

## 3. Mandatory Command-Line Interfaces (CLI)

### 3.1 Server CLI

#### Part I Server
```bash
./server <port> <topics_root_dir> [--mode single|multi]
```
- `<port>`: Integer TCP port to bind (1024–65535).
- `<topics_root_dir>`: Path to root topic directory.
- `[--mode single|multi]`: Optional trailing argument. Defaults to `multi` if omitted, strictly matching mandatory `./server <port> <topics_root_dir>` signature.

#### Part II Server (Case 1, Case 2, Case 2 Enhanced)
```bash
./server <port> <topics_root_dir> <failure_probability> [--seed <uint32>]
```
- `<port>`: Integer TCP port to bind.
- `<topics_root_dir>`: Path to root topic folder.
- `<failure_probability>`: Floating-point value $p \in [0.0, 1.0]$.
- `[--seed <uint32>]`: Optional trailing seed for deterministic pseudo-random fault generation during test automation.

### 3.2 Client CLI (All Parts & Cases)
```bash
./client <server_ip> <server_port> <topic> <output_dir> [--max-retries <N>]
```
- `<server_ip>`: IPv4 address of target server.
- `<server_port>`: TCP port of target server.
- `<topic>`: Name of requested topic subfolder.
- `<output_dir>`: Local destination directory.
- `[--max-retries <N>]`: Optional trailing argument (default: 10) to prevent infinite retry loops when $p=1.0$ or server is unreachable.

---

## 4. Non-Functional Requirements

- **NFR-1 Portability:** Clean C99 standard (`-std=c99 -D_POSIX_C_SOURCE=200809L`). Compiles cleanly under GCC with `-Wall -Wextra -Werror -pedantic`.
- **NFR-2 Zero Hardcoding:** Zero hardcoded IPs, ports, filesystem paths, or topic names.
- **NFR-3 Protocol Robustness:** Protocol explicitly handles partial TCP reads (`recv()`) and writes (`send()`). Framing headers use network byte order (`htonl`/`ntohl`).
- **NFR-4 Resource Protection:** Memory allocated via `malloc()` is freed. Sockets (`close()`), file descriptors (`close()`), directory handles (`closedir()`), and mutexes are cleanly cleaned up on all exit and error paths.
- **NFR-5 Dynamic Session Storage:** Server session table uses a thread-safe, dynamically allocated structure (or documented max capacity with overflow error response `0x0503 ERR_SERVER_FULL`).

---

## 5. Experimental Measurement & Metrics Requirements

### 5.1 Mutually Consistent Metrics Definitions
- **Wire Bytes ($B_{wire}$):** Total bytes transmitted across the TCP socket (Headers + Control Frames + Payload Data + Retransmissions).
- **Useful Bytes ($B_{useful}$):** Net topic payload bytes successfully written to disk.
- **Overhead Bytes ($B_{overhead}$):** Sum of 12-byte header fields and control frame payloads (`MANIFEST_*`, `ACK`, `MSG_ERROR`).
- **Redundant Bytes ($B_{redundant}$):** File payload bytes retransmitted *after* the last committed checkpoint due to un-ACKed socket failures.
- **Consistency Conservation Equation:**
  $$B_{wire} = B_{useful} + B_{redundant} + B_{overhead}$$
- **Completion Time ($T_{comp}$):** Wall-clock seconds from initial request to final completion acknowledgement.
- **Effective Throughput ($R_{eff}$):**
  $$R_{eff} = \frac{B_{useful}}{T_{comp}} \quad \text{(bytes/sec or Mbps)}$$

---

## 6. Submission Structure

Matches mandatory directory layout: `Part1/`, `Part2_Case1/`, `Part2_Case2/`, `Part2_Case2_Enhanced/`, `Results/`, `FinalReport_Group_<XXX>_<YYY>.pdf`, `README.md`.
