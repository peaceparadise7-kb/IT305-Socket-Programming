# IT305 Comprehensive Test Plan
## Fault-Tolerant Topic-Based File Distribution System

**Document Status:** Approved Test Suite Specification (Refined)  
**Version:** 1.1.0  
**Test Automation:** CUnit / Bash Integration Scripts

---

## 1. Overview & Testing Methodology

The testing strategy encompasses unit-level validation of protocol framing and multi-frame manifest streaming, end-to-end integration testing of topic file transfers, comprehensive fault-injection edge-case verification, and memory/descriptor leak audits under Valgrind.

---

## 2. Comprehensive Test Cases Matrix

| Test ID | Category | Description | Execution Command / Procedure | Expected Outcome | Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TC-01** | Protocol | Header Serialization & Seq Num | Test 12-byte header packing/unpacking with 32-bit `seq_num`. | Header packs into Big-Endian network byte order. | Unpacked header fields match exactly. |
| **TC-02** | Protocol | TCP Partial Read `read_n()` | Pass fragmented buffers over socket pairs to `read_n()`. | `read_n()` loops until exact length $L$ is accumulated. | No truncated reads or infinite loops. |
| **TC-03** | Manifest | Multi-Frame Manifest | Request topic directory containing 500 files. | Server streams `MANIFEST_START`, entries, `MANIFEST_END`. | All file metadata received without payload overflow. |
| **TC-04** | Server | Invalid Topic Request | Request non-existent topic directory. | Server returns `MSG_ERROR` (code `0x0404`). | Client displays error message and exits cleanly (status 1). |
| **TC-05** | Server | CLI Server Mode Flag | Run `./server <port> <dir> --mode single` vs default multi. | Server respects `--mode single` (sequential) vs `multi`. | Both modes execute correctly per CLI rules. |
| **TC-06** | Fault | $p = 0.0$ Fault Injection | Run Part II transfer with failure probability $p = 0.0$. | Fault injector injects 0 disconnects. | Transfer completes seamlessly. Total bytes $=useful + overhead$. |
| **TC-07** | Fault | Invalid $p$ Values | Launch server with $p = -0.5$ or $p = 1.5$. | Server validates CLI input argument. | Server prints error message and exits with status 1. |
| **TC-08** | Fault | $p = 1.0$ & Client Retry Guard | Run Part II transfer with failure probability $p = 1.0$. | Connection drops on chunk 1; client retries up to `--max-retries`. | Client aborts cleanly after $N$ retries; no infinite loop. |
| **TC-09** | Fault | Failure Right After Data Chunk | Inject fault on server immediately after chunk `send()`. | Client receives chunk but connection drops before ACK commit. | Client reconnects; server resumes from last committed ACK offset. |
| **TC-10** | Fault | Failure Before Checkpoint ACK | Drop connection while `MSG_ACK` is in flight to server. | Server committed offset remains $O_{last\_ack}$. | Reconnect resumes from $O_{last\_ack}$; client overwrites un-ACKed bytes. |
| **TC-11** | Fault | Duplicate Checkpoint ACK | Send duplicate `MSG_ACK` frame to server worker. | Server handles duplicate ACK idempotently. | Server session state remains valid and consistent. |
| **TC-12** | Fault | Reconnect With Older Checkpoint | Reconnect sending an ACK offset older than server record. | Server rejects invalid regressive offset with `MSG_ERROR`. | Connection resets safely. |
| **TC-13** | Fault | Reconnect Invalid Session ID | Send `MSG_GET_REQ` with unrecognized `Session ID`. | Server returns `MSG_ERROR` (`0x0409 ERR_INVALID_SESSION`). | Client clears checkpoint and starts fresh session. |
| **TC-14** | Fault | File Boundary Resume | Trigger fault precisely at file boundary (`Acked Offset = FileSize`). | Resume starts cleanly at `File Index = k+1`, `Offset = 0`. | No truncated or duplicated files on disk. |
| **TC-15** | Fault | Final Chunk Resume | Trigger fault on final chunk of final file in topic. | Resume retransmits final chunk. | Entire topic completes cleanly. |
| **TC-16** | Fault | Multiple Failures Single Transfer | Run Part II Case 2 at $p = 0.20$ across large topic. | Transfer suffers 10+ intermediate disconnects. | Transfer completes; final files pass byte-for-byte SHA-256 diff. |
| **TC-17** | Enhanced | Case 2 Enhanced Parallel | Run Case 2 Enhanced with 4 parallel streams at $p = 0.10$. | Parallel streams multiplex range requests via `epoll`. | Multi-stream completes correctly; results logged for report. |
| **TC-18** | Leak Audit | Valgrind Memory / Descriptor | Run server/client under `valgrind --leak-check=full`. | Process client requests and terminate server gracefully. | 0 bytes leaked, 0 unclosed file descriptors. |

---

## 3. Automated Integration Test Script (`tests/test_runner.sh`)

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "==> Building all submission targets..."
make -C Part1
make -C Part2_Case1
make -C Part2_Case2
make -C Part2_Case2_Enhanced

echo "==> Executing Comprehensive Test Suite..."
./tests/bin/test_protocol
./tests/bin/test_manifest
./tests/bin/test_fault_edge_cases

echo "==> ALL TESTS PASSED SUCCESSFULLY!"
```
