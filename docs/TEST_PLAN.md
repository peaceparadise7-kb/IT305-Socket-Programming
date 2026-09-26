# IT305 Comprehensive Test Plan
## Fault-Tolerant Topic-Based File Distribution System

**Document Status:** Approved Test Suite Specification  
**Version:** 1.0.0  
**Test Automation:** CUnit / Bash Integration Scripts

---

## 1. Overview & Testing Methodology

The testing strategy encompasses unit-level validation of protocol framing and directory scanning, end-to-end integration testing of topic file transfers, fault-injection verification, and resource leak audits under Valgrind.

---

## 2. Test Cases Matrix

| Test ID | Category | Description | Execution Command / Procedure | Expected Outcome | Acceptance Criteria |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **TC-01** | Protocol | Header Serialization & Byte Order | Unit test packing/unpacking `header_t` with `htonl`/`ntohl`. | Packed bytes match expected Big-Endian format. | Unpacked struct matches original values exactly. |
| **TC-02** | Protocol | TCP Partial Read `read_n()` | Pass fragmented buffers to `read_n()` over socket pairs. | `read_n()` loops until exact byte length is accumulated. | No truncated reads or infinite loops. |
| **TC-03** | Server | Invalid Topic Request | Run `./client 127.0.0.1 <port> non_existent_topic ./out` | Server returns `MSG_ERROR` (code 0x0404). | Client displays error message and exits with status 1. |
| **TC-04** | Server | Empty Topic Directory | Request an existing topic directory containing zero files. | Server responds with manifest `total_files = 0`. | Client completes cleanly without crashes or hangs. |
| **TC-05** | Transfer | Single Large File Transfer | Transfer a topic containing a single 500 MB file. | File transfers completely over TCP stream. | Client SHA-256 hash matches server file SHA-256 hash. |
| **TC-06** | Transfer | Multi-File / Subdir Topic | Transfer Animals10 topic with multiple subfolders and files. | All files and nested directory trees created locally. | `diff -r dataset/Animals10/dog ./out/dog` returns no output. |
| **TC-07** | Server | Concurrency (Part I Mode 1) | Launch 5 concurrent clients against Single-Threaded Server. | Server processes clients sequentially. | Client 1 finishes, then Client 2, etc. All files valid. |
| **TC-08** | Server | Concurrency (Part I Mode 2) | Launch 10 concurrent clients against Multi-Threaded Server. | Server serves all 10 clients simultaneously. | Download completion time scales sub-linearly. Files valid. |
| **TC-09** | Fault | Part II Case 1 Restart | Run Part II Case 1 with failure probability $p = 0.20$. | Connection drops mid-transfer; client reconnects from byte 0. | Transfer completes. Total bytes transferred $> useful\_bytes$. |
| **TC-10** | Fault | Part II Case 2 Checkpoint | Run Part II Case 2 with failure probability $p = 0.20$. | Connection drops; client reconnects sending `Session ID` + offset. | Transfer resumes cleanly. Redundant payload bytes transferred $= 0$. |
| **TC-11** | Fault | Checkpoint Boundary Edge Case | Trigger fault precisely at file boundary (Byte 0 / EOF). | Client/server checkpoint state aligns across file transitions. | Resume begins at file index $k+1$ byte 0. No file truncation. |
| **TC-12** | Enhanced | Case 2 Enhanced Parallel | Run Case 2 Enhanced with 4 parallel streams at $p = 0.10$. | Parallel range requests fill stream; fault drops single stream. | Throughput improves $>30\%$ vs standard Case 2. Files valid. |
| **TC-13** | Reliability | Malformed Input & Fuzzing | Send garbage bytes to server TCP port. | Server detects invalid magic bytes or invalid payload length. | Server logs warning and closes rogue connection cleanly. |
| **TC-14** | Leak Audit | Valgrind Memory / Descriptor | Run server and client under `valgrind --leak-check=full`. | Process client requests and terminate server gracefully. | 0 bytes leaked, 0 unclosed file descriptors. |

---

## 3. Automated Test Verification Harness

An automated shell script `tests/test_runner.sh` will execute the full test matrix:

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "==> Building all submission targets..."
make -C Part1
make -C Part2_Case1
make -C Part2_Case2
make -C Part2_Case2_Enhanced

echo "==> Running Automated Test Suite..."
./tests/bin/test_protocol
./tests/bin/test_topic_mgr

echo "==> Running Integration Tests (Part 1 Single/Multi-threaded)..."
./tests/integration/test_part1.sh

echo "==> Running Integration Tests (Part 2 Case 1 & Case 2 Checkpointing)..."
./tests/integration/test_part2.sh

echo "==> ALL TESTS PASSED SUCCESSFULLY!"
```
