# 17. Page Replacement Worked Examples

To establish formal verification and empirical validation against classical operating systems literature (Silberschatz et al., 2018), this section provides step-by-step simulations of the replacement policies implemented in MemForge. Two reference workloads are analyzed in detail:
1. The textbook 20-reference string under 3 physical frames, evaluating First-In, First-Out (FIFO), Least Recently Used (LRU), and Optimal (OPT) replacement.
2. The canonical 12-reference Bélady string under 3 and 4 physical frames, demonstrating the manifestation of Bélady's anomaly in FIFO and contrasting it with the monotonic behavior of LRU and OPT.

All simulation steps, resident frame layouts, and fault counts match the exact outputs produced by the `memsim refs` CLI command documented in `docs/report/tables/cli_examples.md`.

## 17.1 The Silberschatz 20-Reference Benchmark

The benchmark reference string consists of 20 virtual page accesses issued by a single process:
$$\sigma = \langle 7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1 \rangle$$
The system allocates $F = 3$ physical frames (`Frame 0`, `Frame 1`, `Frame 2`), initially empty.

### First-In, First-Out (FIFO) Simulation

FIFO replaces the resident page with the smallest loading timestamp $t_{\text{load}}$. When candidate timestamps are equal or during cold-start allocation, frames are assigned in ascending index order.

Table 17.1 illustrates the cycle-by-cycle residency matrix.

| Reference ($t$) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Page Ref** | **7** | **0** | **1** | **2** | **0** | **3** | **0** | **4** | **2** | **3** | **0** | **3** | **2** | **1** | **2** | **0** | **1** | **7** | **0** | **1** |
| Frame 0 | 7 | 7 | 7 | 2 | 2 | 2 | 2 | 4 | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 7 | 7 | 7 |
| Frame 1 | | 0 | 0 | 0 | 0 | 3 | 3 | 3 | 2 | 2 | 2 | 2 | 2 | 1 | 1 | 1 | 1 | 1 | 0 | 0 |
| Frame 2 | | | 1 | 1 | 1 | 1 | 0 | 0 | 0 | 3 | 3 | 3 | 3 | 3 | 2 | 2 | 2 | 2 | 2 | 1 |
| **Outcome** | **F** | **F** | **F** | **F** | **H** | **F** | **F** | **F** | **F** | **F** | **F** | **H** | **H** | **F** | **F** | **H** | **H** | **F** | **F** | **F** |

Table 17.1: FIFO page replacement simulation with 3 frames (15 page faults, 5 hits).

#### Step-by-Step Trajectory
- **$t = 0, 1, 2$ (Pages 7, 0, 1)**: Three compulsory page faults. Pages are placed in lowest available free frames 0, 1, and 2 with load stamps $t = 0, 1, 2$. Resident: `[7, 0, 1]`.
- **$t = 3$ (Page 2)**: Fault. All frames occupied. FIFO timestamps are `F0:0`, `F1:1`, `F2:2`. Frame 0 holds the minimum timestamp ($t = 0$). Page 7 is evicted, page 2 loaded into Frame 0 with stamp $t = 3$. Resident: `[2, 0, 1]`.
- **$t = 4$ (Page 0)**: Hit. Page 0 is resident in Frame 1. Under FIFO, load timestamps are unchanged (`F0:3`, `F1:1`, `F2:2`).
- **$t = 5$ (Page 3)**: Fault. Oldest frame is Frame 1 ($t = 1$). Page 0 is evicted, page 3 loaded into Frame 1 with stamp $t = 5$. Resident: `[2, 3, 1]`.
- **$t = 6$ (Page 0)**: Fault. Oldest frame is Frame 2 ($t = 2$). Page 1 is evicted, page 0 loaded into Frame 2 with stamp $t = 6$. Resident: `[2, 3, 0]`.
- **$t = 7$ (Page 4)**: Fault. Oldest frame is Frame 0 ($t = 3$). Page 2 is evicted, page 4 loaded into Frame 0 with stamp $t = 7$. Resident: `[4, 3, 0]`.
- **$t = 8$ (Page 2)**: Fault. Oldest frame is Frame 1 ($t = 5$). Page 3 is evicted, page 2 loaded into Frame 1 with stamp $t = 8$. Resident: `[4, 2, 0]`.
- **$t = 9$ (Page 3)**: Fault. Oldest frame is Frame 2 ($t = 6$). Page 0 is evicted, page 3 loaded into Frame 2 with stamp $t = 9$. Resident: `[4, 2, 3]`.
- **$t = 10$ (Page 0)**: Fault. Oldest frame is Frame 0 ($t = 7$). Page 4 is evicted, page 0 loaded into Frame 0 with stamp $t = 10$. Resident: `[0, 2, 3]`.
- **$t = 11, 12$ (Pages 3, 2)**: Hits. Page 3 resides in Frame 2, page 2 in Frame 1. Stamps remain `F0:10`, `F1:8`, `F2:9`.
- **$t = 13$ (Page 1)**: Fault. Oldest frame is Frame 1 ($t = 8$). Page 2 is evicted, page 1 loaded into Frame 1 with stamp $t = 13$. Resident: `[0, 1, 3]`.
- **$t = 14$ (Page 2)**: Fault. Oldest frame is Frame 2 ($t = 9$). Page 3 is evicted, page 2 loaded into Frame 2 with stamp $t = 14$. Resident: `[0, 1, 2]`.
- **$t = 15, 16$ (Pages 0, 1)**: Hits. Page 0 is in Frame 0, page 1 is in Frame 1. Stamps: `F0:10`, `F1:13`, `F2:14`.
- **$t = 17$ (Page 7)**: Fault. Oldest frame is Frame 0 ($t = 10$). Page 0 is evicted, page 7 loaded into Frame 0 with stamp $t = 17$. Resident: `[7, 1, 2]`.
- **$t = 18$ (Page 0)**: Fault. Oldest frame is Frame 1 ($t = 13$). Page 1 is evicted, page 0 loaded into Frame 1 with stamp $t = 18$. Resident: `[7, 0, 2]`.
- **$t = 19$ (Page 1)**: Fault. Oldest frame is Frame 2 ($t = 14$). Page 2 is evicted, page 1 loaded into Frame 2 with stamp $t = 19$. Resident: `[7, 0, 1]`.

**FIFO Summary**: 15 page faults (3 compulsory, 12 capacity), 5 page hits. Hit ratio: $25.0\%$.

### Least Recently Used (LRU) Simulation

LRU replaces the resident page whose most recent access timestamp $t_{\text{access}}$ is smallest. Access timestamps are updated on every reference.

Table 17.2 illustrates the residency matrix.

| Reference ($t$) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Page Ref** | **7** | **0** | **1** | **2** | **0** | **3** | **0** | **4** | **2** | **3** | **0** | **3** | **2** | **1** | **2** | **0** | **1** | **7** | **0** | **1** |
| Frame 0 | 7 | 7 | 7 | 2 | 2 | 2 | 2 | 4 | 4 | 4 | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| Frame 1 | | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 | 3 | 3 | 3 | 3 | 3 | 0 | 0 | 0 | 0 | 0 |
| Frame 2 | | | 1 | 1 | 1 | 3 | 3 | 3 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 7 | 7 | 7 |
| **Outcome** | **F** | **F** | **F** | **F** | **H** | **F** | **H** | **F** | **F** | **F** | **F** | **H** | **H** | **F** | **H** | **F** | **H** | **F** | **H** | **H** |

Table 17.2: LRU page replacement simulation with 3 frames (12 page faults, 8 hits).

#### Step-by-Step Trajectory
- **$t = 0, 1, 2$ (Pages 7, 0, 1)**: Three compulsory page faults loaded into Frames 0, 1, 2. Access stamps: `F0:0`, `F1:1`, `F2:2`. Resident: `[7, 0, 1]`.
- **$t = 3$ (Page 2)**: Fault. Minimum access stamp is Frame 0 ($t = 0$, page 7). Page 7 is evicted, page 2 loaded into Frame 0. Access stamps: `F0:3`, `F1:1`, `F2:2`. Resident: `[2, 0, 1]`.
- **$t = 4$ (Page 0)**: Hit. Page 0 in Frame 1 is updated: `F1:4`. Access stamps: `F0:3`, `F1:4`, `F2:2`.
- **$t = 5$ (Page 3)**: Fault. Minimum access stamp is Frame 2 ($t = 2$, page 1). Page 1 is evicted, page 3 loaded into Frame 2. Access stamps: `F0:3`, `F1:4`, `F2:5`. Resident: `[2, 0, 3]`.
- **$t = 6$ (Page 0)**: Hit. Page 0 in Frame 1 updated: `F1:6`. Stamps: `F0:3`, `F1:6`, `F2:5`.
- **$t = 7$ (Page 4)**: Fault. Minimum access stamp is Frame 0 ($t = 3$, page 2). Page 2 is evicted, page 4 loaded into Frame 0. Stamps: `F0:7`, `F1:6`, `F2:5`. Resident: `[4, 0, 3]`.
- **$t = 8$ (Page 2)**: Fault. Minimum access stamp is Frame 2 ($t = 5$, page 3). Page 3 is evicted, page 2 loaded into Frame 2. Stamps: `F0:7`, `F1:6`, `F2:8`. Resident: `[4, 0, 2]`.
- **$t = 9$ (Page 3)**: Fault. Minimum access stamp is Frame 1 ($t = 6$, page 0). Page 0 is evicted, page 3 loaded into Frame 1. Stamps: `F0:7`, `F1:9`, `F2:8`. Resident: `[4, 3, 2]`.
- **$t = 10$ (Page 0)**: Fault. Minimum access stamp is Frame 0 ($t = 7$, page 4). Page 4 is evicted, page 0 loaded into Frame 0. Stamps: `F0:10`, `F1:9`, `F2:8`. Resident: `[0, 3, 2]`.
- **$t = 11, 12$ (Pages 3, 2)**: Hits. Page 3 in Frame 1 updated to $t = 11$; page 2 in Frame 2 updated to $t = 12$. Stamps: `F0:10`, `F1:11`, `F2:12`.
- **$t = 13$ (Page 1)**: Fault. Minimum stamp is Frame 0 ($t = 10$, page 0). Page 0 is evicted, page 1 loaded into Frame 0. Stamps: `F0:13`, `F1:11`, `F2:12`. Resident: `[1, 3, 2]`.
- **$t = 14$ (Page 2)**: Hit. Page 2 in Frame 2 updated to $t = 14$. Stamps: `F0:13`, `F1:11`, `F2:14`.
- **$t = 15$ (Page 0)**: Fault. Minimum stamp is Frame 1 ($t = 11$, page 3). Page 3 is evicted, page 0 loaded into Frame 1. Stamps: `F0:13`, `F1:15`, `F2:14`. Resident: `[1, 0, 2]`.
- **$t = 16$ (Page 1)**: Hit. Page 1 in Frame 0 updated to $t = 16$. Stamps: `F0:16`, `F1:15`, `F2:14`.
- **$t = 17$ (Page 7)**: Fault. Minimum stamp is Frame 2 ($t = 14$, page 2). Page 2 is evicted, page 7 loaded into Frame 2. Stamps: `F0:16`, `F1:15`, `F2:17`. Resident: `[1, 0, 7]`.
- **$t = 18, 19$ (Pages 0, 1)**: Hits. Page 0 in Frame 1 updated to $t = 18$; page 1 in Frame 0 updated to $t = 19$. Resident: `[1, 0, 7]`.

**LRU Summary**: 12 page faults (3 compulsory, 9 capacity), 8 page hits. Hit ratio: $40.0\%$.

### Optimal Replacement (OPT) Simulation

OPT evicts the resident page whose next access occurs farthest in future references.

Table 17.3 illustrates the residency matrix.

| Reference ($t$) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Page Ref** | **7** | **0** | **1** | **2** | **0** | **3** | **0** | **4** | **2** | **3** | **0** | **3** | **2** | **1** | **2** | **0** | **1** | **7** | **0** | **1** |
| Frame 0 | 7 | 7 | 7 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 2 | 7 | 7 | 7 |
| Frame 1 | | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 4 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| Frame 2 | | | 1 | 1 | 1 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 3 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| **Outcome** | **F** | **F** | **F** | **F** | **H** | **F** | **H** | **F** | **H** | **H** | **F** | **H** | **H** | **F** | **H** | **H** | **H** | **F** | **H** | **H** |

Table 17.3: OPT page replacement simulation with 3 frames (9 page faults, 11 hits).

#### Step-by-Step Trajectory
- **$t = 0, 1, 2$ (Pages 7, 0, 1)**: Three compulsory page faults. Frames 0, 1, 2 loaded. Next uses: `F0(7):17`, `F1(0):4`, `F2(1):13`. Resident: `[7, 0, 1]`.
- **$t = 3$ (Page 2)**: Fault. Future accesses for resident pages: page 7 at $t = 17$, page 0 at $t = 4$, page 1 at $t = 13$. Page 7 has the farthest next use ($t = 17$). Frame 0 is selected; page 7 is evicted, page 2 loaded into Frame 0 with next use $t = 8$. Resident: `[2, 0, 1]`.
- **$t = 4$ (Page 0)**: Hit in Frame 1. Next use of page 0 updated to $t = 6$.
- **$t = 5$ (Page 3)**: Fault. Future accesses: page 2 at $t = 8$, page 0 at $t = 6$, page 1 at $t = 13$. Page 1 has the farthest next use ($t = 13$). Frame 2 is selected; page 1 is evicted, page 3 loaded into Frame 2 with next use $t = 9$. Resident: `[2, 0, 3]`.
- **$t = 6$ (Page 0)**: Hit in Frame 1. Next use of page 0 updated to $t = 10$.
- **$t = 7$ (Page 4)**: Fault. Future accesses: page 2 at $t = 8$, page 0 at $t = 10$, page 3 at $t = 9$. Page 0 has the farthest next use ($t = 10$). Frame 1 is selected; page 0 is evicted, page 4 loaded into Frame 1 with next use $\infty$ (`OPT_NEVER`, page 4 is never referenced again). Resident: `[2, 4, 3]`.
- **$t = 8, 9$ (Pages 2, 3)**: Hits. Page 2 hits in Frame 0 (next use $t = 12$); page 3 hits in Frame 2 (next use $t = 11$). Next uses: `F0(2):12`, `F1(4):∞`, `F2(3):11`.
- **$t = 10$ (Page 0)**: Fault. Candidates: page 2 ($t = 12$), page 4 ($t = \infty$), page 3 ($t = 11$). Page 4 has stamp $\infty$. Frame 1 is selected; page 4 evicted, page 0 loaded into Frame 1 with next use $t = 15$. Resident: `[2, 0, 3]`.
- **$t = 11, 12$ (Pages 3, 2)**: Hits. Page 3 hits in Frame 2 (next use $\infty$); page 2 hits in Frame 0 (next use $t = 14$). Next uses: `F0(2):14`, `F1(0):15`, `F2(3):∞`.
- **$t = 13$ (Page 1)**: Fault. Candidates: page 2 ($t = 14$), page 0 ($t = 15$), page 3 ($t = \infty$). Page 3 has stamp $\infty$. Frame 2 is selected; page 3 evicted, page 1 loaded into Frame 2 with next use $t = 16$. Resident: `[2, 0, 1]`.
- **$t = 14, 15, 16$ (Pages 2, 0, 1)**: Hits across three consecutive steps. Next uses updated: page 2 has no further uses ($\infty$), page 0 at $t = 18$, page 1 at $t = 19$. Next uses: `F0(2):∞`, `F1(0):18`, `F2(1):19`.
- **$t = 17$ (Page 7)**: Fault. Candidates: page 2 ($t = \infty$), page 0 ($t = 18$), page 1 ($t = 19$). Page 2 has stamp $\infty$. Frame 0 is selected; page 2 evicted, page 7 loaded into Frame 0 with next use $\infty$. Resident: `[7, 0, 1]`.
- **$t = 18, 19$ (Pages 0, 1)**: Hits in Frame 1 and Frame 2.

**OPT Summary**: 9 page faults (3 compulsory, 6 capacity), 11 page hits. Hit ratio: $55.0\%$.

## 17.2 The Bélady Anomaly Benchmark

To demonstrate Bélady's anomaly, MemForge evaluates the reference string:
$$\sigma_{\text{Belady}} = \langle 1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5 \rangle$$
consisting of 12 memory accesses referencing 5 distinct virtual pages.

### FIFO with 3 Frames (9 Faults)

Under $F = 3$, FIFO incurs 9 page faults and 3 hits.

| Reference ($t$) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Page Ref** | **1** | **2** | **3** | **4** | **1** | **2** | **5** | **1** | **2** | **3** | **4** | **5** |
| Frame 0 | 1 | 1 | 1 | 4 | 4 | 4 | 5 | 5 | 5 | 5 | 5 | 5 |
| Frame 1 | | 2 | 2 | 2 | 1 | 1 | 1 | 1 | 1 | 3 | 3 | 3 |
| Frame 2 | | | 3 | 3 | 3 | 2 | 2 | 2 | 2 | 2 | 4 | 4 |
| **Outcome** | **F** | **F** | **F** | **F** | **F** | **F** | **F** | **H** | **H** | **F** | **F** | **H** |

Table 17.4: FIFO page replacement with 3 frames on Bélady string (9 page faults, 3 hits).

Notice that at $t = 6$ (page 5), page 5 replaces page 4 in Frame 0. Consequently, when pages 1 and 2 are requested at $t = 7$ and $t = 8$, both remain resident in Frame 1 and Frame 2, yielding two consecutive hits. At $t = 11$, page 5 is already resident in Frame 0, yielding a third hit.

### FIFO with 4 Frames (10 Faults)

When physical capacity is increased to $F = 4$, intuition suggests page faults should decrease or remain constant. Instead, page faults increase to 10.

| Reference ($t$) | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Page Ref** | **1** | **2** | **3** | **4** | **1** | **2** | **5** | **1** | **2** | **3** | **4** | **5** |
| Frame 0 | 1 | 1 | 1 | 1 | 1 | 1 | 5 | 5 | 5 | 5 | 4 | 4 |
| Frame 1 | | 2 | 2 | 2 | 2 | 2 | 2 | 1 | 1 | 1 | 1 | 5 |
| Frame 2 | | | 3 | 3 | 3 | 3 | 3 | 3 | 2 | 2 | 2 | 2 |
| Frame 3 | | | | 4 | 4 | 4 | 4 | 4 | 4 | 3 | 3 | 3 |
| **Outcome** | **F** | **F** | **F** | **F** | **H** | **H** | **F** | **F** | **F** | **F** | **F** | **F** |

Table 17.5: FIFO page replacement with 4 frames on Bélady string (10 page faults, 2 hits).

#### Anatomy of the Anomaly
With 4 frames, pages 1, 2, 3, 4 occupy all frames at $t = 3$. References to 1 and 2 at $t = 4, 5$ hit in memory. However, because FIFO does not adjust stamps on hits, Frame 0 (page 1) remains marked with load stamp $t = 0$.

When page 5 arrives at $t = 6$, Frame 0 is evicted, removing page 1 right before it is referenced again at $t = 7$. Loading page 1 at $t = 7$ evicts Frame 1 (page 2), right before page 2 is requested at $t = 8$. This starts a cascade of cyclic thrashing across references 6 through 11, where every single memory access faults. Adding a 4th frame reduces hits from 3 to 2, increasing total faults from 9 to 10.

### Monotonic Immunity: LRU and OPT on the Bélady String

In contrast to FIFO, stack algorithms guarantee that fault counts never increase as frames expand.
- **LRU**:
  - At $F = 3$: 10 page faults (faults at $t = 0, 1, 2, 3, 4, 5, 6, 9, 10, 11$; hits at $t = 7, 8$).
  - At $F = 4$: 8 page faults (faults at $t = 0, 1, 2, 3, 6, 9, 10, 11$; hits at $t = 4, 5, 7, 8$).
  - Fault count decreases by 2.
- **OPT**:
  - At $F = 3$: 7 page faults (faults at $t = 0, 1, 2, 3, 6, 9, 10$; hits at $t = 4, 5, 7, 8, 11$).
  - At $F = 4$: 6 page faults (faults at $t = 0, 1, 2, 3, 6, 10$; hits at $t = 4, 5, 7, 8, 9, 11$).
  - Fault count decreases by 1.

Table 17.6 summarizes the comparative fault counts across policies and frame allocations for the two reference strings.

| Workload | Frames ($F$) | FIFO Faults | LRU Faults | OPT Faults | Anomaly Present? |
| --- | --- | --- | --- | --- | --- |
| Silberschatz ($\sigma$) | 3 | 15 | 12 | 9 | N/A |
| Bélady ($\sigma_{\text{Belady}}$) | 3 | 9 | 10 | 7 | No |
| Bélady ($\sigma_{\text{Belady}}$) | 4 | **10** | **8** | **6** | **Yes (FIFO only)** |

Table 17.6: Summary comparison of replacement performance on worked reference strings.
