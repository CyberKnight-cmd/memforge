# 24. Execution Examples and Output

This section demonstrates the operation of MemForge across its CLI and web interfaces, presenting authentic execution logs, trace formats, and visual inspection screens. The examples reproduce benchmark scenarios from operating systems literature: Silberschatz's five-hole contiguous allocation problem, the classic 20-reference paging sequence under FIFO, and Belady's anomaly.

## 24.1 Command-Line Execution Sessions

The CLI sessions presented below are drawn verbatim from [report/tables/cli_examples.md](file:///home/cyberknight/memforge-agy/rep-ops/report/tables/cli_examples.md) and represent actual executions of the `memsim` binary at commit `53039c8`.

### Session 1: Contiguous Policy Comparison on Textbook Five Holes

In this session, `memsim compare` evaluates First-Fit, Best-Fit, and Worst-Fit against [tests/fixtures/ac1_silberschatz.wl](file:///home/cyberknight/memforge-agy/rep-ops/tests/fixtures/ac1_silberschatz.wl). The workload establishes five free holes of sizes 100, 500, 200, 300, and 600 units separated by four 10-unit boundary blocks, followed by four allocation requests: $a_1 = 212$, $a_2 = 417$, $a_3 = 112$, and $a_4 = 426$ units.

```text
$ memsim compare tests/fixtures/ac1_silberschatz.wl --policies first-fit,best-fit,worst-fit
id  size  first-fit           best-fit  worst-fit
a1  212   110                 830       1140
a2  417   1140                110       110
a3  112   322                 620       1352
a4  426   fail FRAGMENTATION  1140      fail FRAGMENTATION

metric                  first-fit  best-fit  worst-fit
alloc_requests          13         13        13
alloc_successes         12         13        12
alloc_failures          1          0         1
failures_fragmentation  1          0         1
failures_capacity       0          0         0
rejections              0          0         0
frees                   5          5         5
compactions             0          0         0
blocks_moved            0          0         0
units_moved             0          0         0
scanned_total           23         29        29
scanned_mean            1.769230   2.230769  2.230769
ext_frag_final          0.687174   0.673545  0.687174
ext_frag_mean           0.245843   0.212145  0.245012
hole_count_final        5          5         5
total_free_final        959        533       959
largest_hole_final      300        174       300
used_final              781        1207      781
live_blocks_final       7          8         7
```

#### Placement Analysis
1. **First-Fit**:
   - $a_1$ (212) scans address order, fits in the 500-unit hole at address 110, leaving 288 units free at address 322.
   - $a_2$ (417) skips remaining holes until the 600-unit hole at 1140, leaving 183 units at 1557.
   - $a_3$ (112) fits in the 288-unit hole at 322, leaving 176 units at 434.
   - $a_4$ (426) fails: although total free memory is 959 units ($100 + 176 + 200 + 300 + 183$), the largest contiguous hole is only 300 units. The engine emits `fail FRAGMENTATION`.
   - Holes examined: 23 total (mean 1.769 per request).
2. **Best-Fit**:
   - $a_1$ (212) selects the 300-unit hole at 830 (slack 88), leaving 88 units.
   - $a_2$ (417) selects the 500-unit hole at 110 (slack 83), leaving 83 units.
   - $a_3$ (112) selects the 200-unit hole at 620 (slack 88), leaving 88 units.
   - $a_4$ (426) selects the 600-unit hole at 1140 (slack 174), leaving 174 units.
   - All four requests succeed, achieving 13/13 allocations with zero fragmentation failures.
   - Holes examined: 29 total (mean 2.231 per request).
3. **Worst-Fit**:
   - $a_1$ (212) selects the 600-unit hole at 1140, leaving 388 units at 1352.
   - $a_2$ (417) selects the 500-unit hole at 110, leaving 83 units at 527.
   - $a_3$ (112) selects the remaining 388-unit hole at 1352, leaving 276 units at 1464.
   - $a_4$ (426) fails: the largest available hole is 300 units, resulting in `fail FRAGMENTATION`.

This output directly validates acceptance criteria **AC-1** and **AC-8**.

### Session 2: Demand Paging Reference String and Frame Grid

The `memsim refs` command evaluates the classic 20-reference paging string under FIFO replacement with 3 physical frames:

```text
$ memsim refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --frames 3 --policy fifo --table
7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1
7 7 7 2 2 2 2 4 4 4 0 0 0 0 0 0 0 7 7 7
  0 0 0 0 3 3 3 2 2 2 2 2 1 1 1 1 1 0 0
    1 1 1 1 0 0 0 3 3 3 3 3 2 2 2 2 2 1
F F F F   F F F F F F     F F     F F F
```

#### Frame Grid Interpretation
The top row lists the 20 reference page numbers. The next three rows show physical frame contents (frames 0, 1, and 2) after each memory reference. The bottom row marks each reference resulting in a page fault with `F`.
- The first three references (`7`, `0`, `1`) cause compulsory faults, populating empty frames.
- Reference `2` faults; frame 0 holds the oldest page (`7`), which is evicted and replaced by `2`.
- Reference `0` hits in frame 1.
- Reference `3` faults; frame 1 holds the oldest page (`0`), which is evicted and replaced by `3`.
- Across all 20 references, 15 page faults occur and 5 references hit. This matches the textbook theoretical expectation, confirming acceptance criterion **AC-3**.

### Session 3: Belady's Anomaly Frame Sweep

The `memsim sweep` command evaluates the Belady reference string from [tests/fixtures/ac4_belady.wl](file:///home/cyberknight/memforge-agy/rep-ops/tests/fixtures/ac4_belady.wl) ($[1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]$) across frame allocations ranging from 1 to 6 frames:

```text
$ memsim sweep tests/fixtures/ac4_belady.wl --policy fifo --frames 1..6
1 12
2 12
3 9
4 10 *
5 5
6 5
```

#### Anomaly Analysis
Under normal virtual memory operation, increasing physical frame capacity monotonically decreases or maintains page fault frequency. However, under FIFO replacement:
- With 3 physical frames, the 12-reference string incurs 9 page faults.
- With 4 physical frames, the exact same string incurs 10 page faults.

The CLI detects this non-monotonic increase and automatically flags the row with `*`. This confirms acceptance criterion **AC-4**.

## 24.2 Canonical JSONL Trace Format

Every simulation run with `--trace` produces an append-only JSON Lines (JSONL) file. The trace format uses a schema-governed, canonical representation where all numeric metrics are derived strictly from integer state and formatted deterministically (ratios printed via `%.6f`). Timestamps and wall-clock times are omitted to ensure byte-level reproducibility (NFR-1).

A complete trace contains four line types:
1. `TraceHeader`: Configuration parameters, workload identity, and git commit hash.
2. Event Records: Discrete events emitted by the engine during step execution.
3. `Snapshot`: Complete memory layout or frame table state recorded after each step.
4. `RunSummary`: Aggregated performance metrics closing the trace.

### Exemplary Trace Excerpt

The following excerpt illustrates the execution of step 17 from the Silberschatz benchmark, capturing the exact failure event for request $a_4$:

```json
{"type":"TraceHeader","schema":1,"engine":"0.1.0","git":"53039c8","workload":"ac1_silberschatz","workload_sha256":"6ebf...","policy":"first-fit","config":{"memory_size":1740,"alignment":1,"min_split":1,"compaction":"never"},"trace_level":1}
{"seq":52,"step":17,"type":"PlacementDecided","data":{"policy":"first-fit","requested":426,"granted":426,"hole":null,"scanned":5}}
{"seq":53,"step":17,"type":"AllocationFailed","data":{"pid":"P4","id":"a4","requested":426,"granted":426,"cause":"FRAGMENTATION","total_free":959,"largest_hole":300}}
{"type":"Snapshot","step":17,"contiguous":{"memory_size":1740,"blocks":[{"start":0,"size":100,"state":"free"},{"start":100,"size":10,"state":"allocated","pid":"SYS","id":"s1","requested":10},{"start":110,"size":212,"state":"allocated","pid":"P1","id":"a1","requested":212},{"start":322,"size":112,"state":"allocated","pid":"P3","id":"a3","requested":112},{"start":434,"size":176,"state":"free"},{"start":610,"size":10,"state":"allocated","pid":"SYS","id":"s2","requested":10},{"start":620,"size":200,"state":"free"},{"start":820,"size":10,"state":"allocated","pid":"SYS","id":"s3","requested":10},{"start":830,"size":300,"state":"free"},{"start":1130,"size":10,"state":"allocated","pid":"SYS","id":"s4","requested":10},{"start":1140,"size":417,"state":"allocated","pid":"P2","id":"a2","requested":417},{"start":1557,"size":183,"state":"free"}],"frag":{"total_free":959,"largest_hole":300,"hole_count":5,"ext_frag":0.687174}}}
{"type":"RunSummary","metrics":{"alloc_requests":13,"alloc_successes":12,"alloc_failures":1,"failures_fragmentation":1,"failures_capacity":0,"rejections":0,"frees":5,"compactions":0,"blocks_moved":0,"units_moved":0,"scanned_total":23,"scanned_mean":1.769230,"ext_frag_final":0.687174,"ext_frag_mean":0.245843,"hole_count_final":5,"total_free_final":959,"largest_hole_final":300,"used_final":781,"live_blocks_final":7}}
```

```mermaid
flowchart TD
    TH["TraceHeader (schema, git, config)"] --> EV["Event Stream (seq, step, type, data)"]
    EV --> SN["Snapshot (blocks / frames layout)"]
    SN --> RS["RunSummary (canonical metrics)"]

    subgraph Event Stream Elements
        PD["PlacementDecided"]
        AF["AllocationFailed (FRAGMENTATION)"]
        BA["BlockAllocated"]
        BF["BlockFreed"]
        PF["PageFault"]
        PE["PageEvicted"]
    end
    EV -.-> Event Stream Elements
```

### Trace Deserialization Algorithm

The procedure for reconstructing simulation states from a canonical JSONL trace during playback is defined below.

```text
Algorithm 2: Trace Event Stream Reconstruction
Input: Array of JSON strings lines representing a canonical JSONL trace
Output: Structured trace object T containing header, steps array, and summary

1:  if Length(lines) = 0 then
2:      return Error("Empty trace file")
3:  end if
4:  header ← JSONParse(lines[0])
5:  if header.type ≠ "TraceHeader" or header.schema ≠ 1 then
6:      return Error("Invalid trace header or unsupported schema")
7:  end if
8:  T.header ← header
9:  T.steps ← []
10: current_step ← null
11: for i ← 1 to Length(lines) - 1 do
12:     record ← JSONParse(lines[i])
13:     if record.type = "RunSummary" then
14:         T.summary ← record.metrics
15:     else if record.type = "Snapshot" then
16:         if current_step ≠ null and current_step.step = record.step then
17:             current_step.snapshot ← record
18:         end if
19:     else
20:         // Record is a discrete event
21:         step_num ← record.step
22:         if current_step = null or current_step.step ≠ step_num then
23:             if current_step ≠ null then
24:                 Append(T.steps, current_step)
25:             end if
26:             current_step ← NewStep(step: step_num, events: [], snapshot: null)
27:         end if
28:         Append(current_step.events, record)
29:     end if
30: end for
31: if current_step ≠ null then
32:     Append(T.steps, current_step)
33: end if
34: return Success(T)
```

**Complexity Analysis**:
- **Time Complexity**: $\mathcal{O}(N)$, where $N$ is the number of lines in the trace. Each line is parsed once, and events are grouped into contiguous steps in linear time.
- **Space Complexity**: $\mathcal{O}(N)$ to store the parsed object graph in memory.

## 24.3 Web Application Visual Inspection Screens

The web application exposes graphical interfaces for each major feature of the simulator. The screens below illustrate the key functional views.

### 1. Comparative Policy View
The comparative view renders First-Fit, Best-Fit, and Worst-Fit side-by-side against the same allocation requests, highlighting where policies diverge in placement choices and fragmentation accumulation.

![Screenshot: Web app comparative policy view showing First-Fit, Best-Fit, and Worst-Fit side-by-side](../figures/screenshots/web_compare_view.png)

### 2. Fragmentation Failure View
Visual depiction of allocation request $a_4$ failing under First-Fit. The visualization displays the memory strip with 959 units of total free space, divided across five disjoint holes, none of which can accommodate 426 units.

![Screenshot: Web app contiguous memory failure under external fragmentation](../figures/screenshots/web_fragmentation_failure.png)

### 3. Sliding Compaction View
Demonstrates the compaction phase following fragmentation. The visual strip depicts all seven allocated blocks shifted toward low memory addresses ($[0, 1207)$), consolidating all free units into a single 533-unit hole at address 1207.

![Screenshot: Web app sliding compaction relocating blocks to form contiguous free space](../figures/screenshots/web_compaction.png)

### 4. Demand Paging Frame Table
The textbook frame grid visualizes page hit and fault progressions, showing frame contents per reference and compulsory versus capacity fault flags.

![Screenshot: Web app demand paging frame-by-reference table](../figures/screenshots/web_paging_table.png)

### 5. Real-Time Belady Anomaly Chart
The live sweep curve plots fault counts as a function of frame capacity ($1 \le F \le 7$). The chart displays the upward inflection at 4 frames where FIFO incurs 10 faults compared to 9 faults at 3 frames.

![Screenshot: Web app real-time Belady anomaly frame sweep chart](../figures/screenshots/web_belady_chart.png)

### 6. Virtual Address Translation Inspector
Interactive breakdown of a 32-bit virtual address into Virtual Page Number (VPN) and page offset, showing the corresponding physical frame lookup, Present/Referenced/Dirty flags, and resolved physical address.

![Screenshot: Web app virtual address translation and page table inspection](../figures/screenshots/web_address_translation.png)
