# IT305 Performance Experimentation Plan
## Measurement Methodology & Empirical Evaluation Framework

**Document Status:** Approved Experimentation Plan  
**Version:** 1.0.0  
**Testbed Hardware:** 3 Physical/Virtual Linux Machines (1 Server, 2 Client Nodes)

---

## 1. Experimental Overview & Objectives

The goal of the experiment suite is to quantitatively measure, evaluate, and compare:
1. Concurrency scalability of Single-Threaded vs. Multi-Threaded server models (Part I).
2. Cost of redundant retransmissions in un-sessioned transfers under connection failures (Part II Case 1).
3. Efficiency of zero-redundancy checkpointing in session-managed transfers (Part II Case 2).
4. Performance gains achieved by multi-stream non-blocking streaming (Part II Case 2 Enhanced).

---

## 2. Experimental Variables & Parameters

### 2.1 Independent Variables
- **Server Concurrency Architecture:** Single-threaded vs. Multi-threaded.
- **Fault Failure Probability ($p$):** $p \in \{0.00, 0.05, 0.10, 0.20, 0.50\}$.
- **Number of Concurrent Clients ($N$):** $N \in \{1, 2, 4, 8, 16, 32\}$.
- **Dataset Topic:** Animals10 dataset subfolders (e.g. `dog`, `cat`, `butterfly`).
- **Transfer Case:** Part I, Part II Case 1, Part II Case 2, Part II Case 2 Enhanced.

### 2.2 Dependent Variables
- **Completion Time ($T_{comp}$):** Total elapsed time (seconds) from request dispatch to final topic receipt.
- **Total Bytes Transferred ($B_{total}$):** Wire bytes recorded on socket interfaces.
- **Useful Bytes Transferred ($B_{useful}$):** Net payload bytes written to disk.
- **Redundant Bytes Transferred ($B_{redundant}$):** $B_{redundant} = B_{total} - B_{useful} - B_{overhead}$.
- **Protocol Overhead Bytes ($B_{overhead}$):** Sum of binary headers, ACKs, and control frames.
- **Aggregate Throughput ($R_{agg}$):** $\frac{\sum_{i=1}^N B_{useful, i}}{\max(T_{comp, i})}$ (Mbps).

---

## 3. Testbed Topology & Setup

```
 +------------------------+
 |     Server Node        |
 |  IP: 192.168.1.100     |
 |  (./server <port> ...) |
 +-----------+------------+
             |
   +---------+---------+ (Gigabit Ethernet / Controlled Wi-Fi)
   |                   |
+--+-------------------+--+   +--+-------------------+--+
|      Client Node A      |   |      Client Node B      |
|  IP: 192.168.1.101      |   |  IP: 192.168.1.102      |
|  (N/2 Client Instances) |   |  (N/2 Client Instances) |
+-------------------------+   +-------------------------+
```

---

## 4. Measurement & Accounting Methodology

### 4.1 Timing Precision
- Use `clock_gettime(CLOCK_MONOTONIC, &spec)` in C before sending `MSG_GET_REQ` and immediately after receiving `MSG_TRANSFER_DONE`.
- Measurement resolution: Microsecond / Nanosecond precision.

### 4.2 Byte Accounting
- Server and client maintain atomic byte counters:
  - `wire_bytes_sent`: Incremented by return value of `send()` calls.
  - `useful_bytes_written`: Incremented only when payload buffer is written to target file.
  - `overhead_bytes`: Incremented by header size (12 bytes) per packet.

---

## 5. CSV Logging Schema

Experiment results MUST be logged in CSV format under `Results/raw_data/experiment_results.csv`:

```csv
part_case,num_clients,failure_prob,trial_id,completion_time_sec,useful_bytes,total_wire_bytes,redundant_bytes,overhead_bytes,throughput_mbps
Part1_Single,1,0.00,1,2.451,52428800,52430000,0,1200,171.12
Part1_Multi,8,0.00,1,3.120,419430400,419445000,0,14600,1075.46
Part2_Case1,4,0.10,1,12.840,209715200,384910000,175180000,14800,130.66
Part2_Case2,4,0.10,1,4.520,209715200,209730000,0,14800,371.17
Part2_Case2_Enhanced,4,0.10,1,3.210,209715200,209735000,0,19800,522.65
```

---

## 6. Plotting & Graph Generation Requirements

A Python script `scripts/plot_results.py` will generate the required benchmark graphs using `matplotlib` and `seaborn`:

1. **Figure 1 (`throughput_vs_clients.png`):** Throughput vs. Number of Clients (Single-Threaded vs. Multi-Threaded).
2. **Figure 2 (`bytes_vs_failure_prob.png`):** Total Bytes Transferred & Redundant Bytes vs. Failure Probability (Case 1 vs. Case 2).
3. **Figure 3 (`completion_time_vs_failure.png`):** Transfer Completion Time vs. Failure Probability (Case 1 vs. Case 2 vs. Enhanced).
4. **Figure 4 (`enhanced_throughput_gain.png`):** Effective Throughput Comparison (Case 2 Standard vs. Case 2 Enhanced).

---

## 7. Statistical Rigor

- **Trial Repetitions:** Each experimental configuration point must be repeated for **$M = 5$ independent trials**.
- **Statistical Measures:** Plots must display mean values with 95% confidence intervals or error bars representing standard deviation across trials.
