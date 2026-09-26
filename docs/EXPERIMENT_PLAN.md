# IT305 Performance Experimentation Plan
## Measurement Methodology & Empirical Evaluation Framework

**Document Status:** Approved Experimentation Plan (Final Refinement)  
**Version:** 1.2.0  
**Testbed Infrastructure:** 3 Dedicated Physical/Virtual Linux Machines (1 Server Node, 2 Client Nodes)

---

## 1. Experimental Overview & Objectives

The experimentation framework quantitatively measures, evaluates, and compares:
1. Concurrency scalability of Single-Threaded vs. Multi-Threaded server models (Part I).
2. Cost of full retransmissions in un-sessioned transfers under connection failures (Part II Case 1).
3. Efficiency of checkpoint-based minimized redundancy in session-managed transfers (Part II Case 2).
4. Performance characteristics of multi-stream non-blocking streaming (Part II Case 2 Enhanced).

---

## 2. Experimental Variables & Parameters

### 2.1 Independent Variables
- **Server Concurrency Mode:** Single-Threaded vs. Multi-Threaded (`--mode single` vs `--mode multi`).
- **Fault Failure Probability ($p$):** $p \in \{0.00, 0.05, 0.10, 0.20, 0.50\}$.
- **Number of Concurrent Clients ($N$):** $N \in \{1, 2, 4, 8, 16, 32\}$.
- **Dataset Topic:** Animals10 dataset subfolders (e.g. `dog`, `cat`, `butterfly`).
- **Transfer Case:** Part I, Part II Case 1, Part II Case 2, Part II Case 2 Enhanced.

### 2.2 Dependent Variables & Precise Byte Accounting
- **Useful Bytes ($B_{useful}$):** Net file payload bytes successfully received and written to disk forming the requested dataset.
- **Redundant Bytes ($B_{redundant}$):** Retransmitted **FILE PAYLOAD bytes ONLY** that are sent after the last committed checkpoint due to un-ACKed connection failures.
- **Protocol Overhead Bytes ($B_{overhead}$):** Every transmitted application-protocol byte that is NOT file payload data, including:
  - All 12-byte application headers,
  - Manifest frame payloads (`MSG_MANIFEST_START`, `MSG_MANIFEST_ENTRY`, `MSG_MANIFEST_END`),
  - ACK frames (`MSG_ACK`),
  - Error and control frames (`MSG_ERROR`, `MSG_GET_REQ`, `MSG_RANGE_REQ`, `MSG_TRANSFER_DONE`),
  - Application headers belonging to retransmitted data chunks.
- **Wire Bytes ($B_{wire}$):** Total bytes transmitted across the TCP socket interface.
- **Conservation Equation:**
  $$B_{wire} = B_{useful} + B_{redundant} + B_{overhead}$$
- **Completion Time ($T_{comp}$):** Total wall-clock seconds (`clock_gettime(CLOCK_MONOTONIC)`) from initial request to final topic completion signal.
- **Effective Throughput ($R_{eff}$):**
  $$R_{eff} = \frac{B_{useful}}{T_{comp}} \quad \text{(Mbps)}$$

---

## 3. Testbed Topology (Placeholder IPs)

```
 +-------------------------------------+
 |            Server Node              |
 |      <SERVER_IP_PLACEHOLDER>        |
 | (./server <port> <topics> [p] ...)  |
 +------------------+------------------+
                    |
   +----------------+----------------+ (Gigabit LAN / Controlled Switch)
   |                                 |
+--+--------------------------+   +--+--------------------------+
|       Client Node A         |   |       Client Node B         |
|  <CLIENT_NODE_A_PLACEHOLDER>|   |  <CLIENT_NODE_B_PLACEHOLDER>|
|   (N/2 Client Instances)    |   |   (N/2 Client Instances)    |
+-----------------------------+   +-----------------------------+
```

---

## 4. CSV Logging Schema

Results MUST be logged in CSV format under `Results/raw_data/experiment_results.csv`:

```csv
part_case,num_clients,failure_prob,trial_id,completion_time_sec,useful_bytes,total_wire_bytes,redundant_bytes,overhead_bytes,throughput_mbps
Part1_Single,1,0.00,1,2.451,52428800,52430000,0,1200,171.12
Part1_Multi,8,0.00,1,3.120,419430400,419445000,0,14600,1075.46
Part2_Case1,4,0.10,1,12.840,209715200,384910000,175180000,14800,130.66
Part2_Case2,4,0.10,1,4.520,209715200,209730000,128000,16800,371.17
Part2_Case2_Enhanced,4,0.10,1,3.210,209715200,209735000,64000,21800,522.65
```

---

## 5. Required Plotting & Analysis

Script `scripts/plot_results.py` generates the required benchmark figures:
1. **Figure 1 (`throughput_vs_clients.png`):** Throughput vs. Number of Clients (Single-Threaded vs. Multi-Threaded).
2. **Figure 2 (`bytes_vs_failure_prob.png`):** Total Wire Bytes & Redundant Bytes vs. Failure Probability (Case 1 vs. Case 2).
3. **Figure 3 (`completion_time_vs_failure.png`):** Completion Time vs. Failure Probability (Case 1 vs. Case 2 vs. Enhanced).
4. **Figure 4 (`enhanced_throughput_gain.png`):** Throughput Comparison (Case 2 Standard vs. Case 2 Enhanced).

---

## 6. Statistical Rigor

- **Trial Repetitions:** Each experimental configuration point is executed for **$M = 5$ independent trials**.
- **Statistical Measures:** Graphs plot mean values with error bars displaying 95% confidence intervals or standard deviation across trials.
