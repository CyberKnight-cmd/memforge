# 7. Objectives, Scope and Limitations

A rigorous evaluation of any systems simulator requires a precise specification of its theoretical objectives, delivery boundaries, and architectural constraints. Because MemForge was designed to balance academic depth with disciplined delivery under a one-week engineering timeline, its feature set was systematically scoped.

This section defines the core functional and non-functional objectives of MemForge (derived from the architectural specification), presents the definitive scope matrix contrasting in-scope, simplified, and cut capabilities, provides the rationale for key scoping decisions, and outlines the system limitations and deliberate non-goals.

---

## 7.1 Primary System Objectives

The objectives of MemForge are partitioned into five functional goals (G1 through G5) and eight non-functional requirements (NFR-1 through NFR-8), establishing formal acceptance criteria for correctness and software quality.

### 7.1.1 Functional Goals

| Goal ID | Functional Objective | Verification Mechanism |
| :--- | :--- | :--- |
| **G1** | **Contiguous Dynamic Allocation:** Faithful simulation of dynamic-partition placement heuristics (First-Fit, Best-Fit, Worst-Fit), lower-bound splitting, immediate free-block coalescing, dynamic fragmentation metrics, and explicit sliding compaction. | Acceptance criteria AC-1 and AC-2; runtime invariants I1 to I8. |
| **G2** | **Virtual Demand Paging:** Accurate simulation of single-level page tables, virtual-to-physical address translation, page-fault handling, and page replacement under FIFO, LRU, and Optimal algorithms, including automated empirical reproduction of Belady's anomaly. | Acceptance criteria AC-3 and AC-4; runtime invariants V1 to V6. |
| **G3** | **Fair Algorithmic Benchmarking:** Standardized, reproducible comparison of competing memory allocation and replacement policies executed across identical workload streams. | Controlled experiments E1 and E4 executed across multiple random seeds. |
| **G4** | **Traceability and Empirical Provenance:** Every quantitative figure, table entry, and metric cited within the project report must directly trace to an automated test assertion or committed benchmark record. | Correctness verification matrix cross-referenced with Git commit SHAs. |
| **G5** | **Accessible Multi-Tier Interface:** Provision of an automated command-line tool (`memsim`) for batch scripting, paired with an interactive visual environment for step-by-step state inspection. | Acceptance criterion AC-8; WebAssembly web laboratory deployment. |

### 7.1.2 Non-Functional Requirements

The architecture enforces eight non-functional requirements to ensure scientific integrity and code maintainability:

1. **NFR-1 (Strict Determinism):** Workload inputs, configuration parameters, and pseudo-random seeds must yield bit-for-bit identical traces across host operating systems and hardware platforms.
2. **NFR-2 (Engine Purity):** Core engines (`src/contiguous/` and `src/paging/`) are decoupled from standard I/O streams, wall-clock timers, and global state. Clients interact with engines exclusively through the `Session` facade (`src/session/`), which returns each step's events to its caller.
3. **NFR-3 (Checked Execution Mode):** In debug and test environments, formal invariants (I1–I8 for contiguous memory and V1–V6 for paging) execute after every operation, immediately aborting via `MS_ASSERT` upon detecting inconsistency.
4. **NFR-4 (Extensibility):** Adding a new placement or replacement heuristic requires only implementing a strategy function and registering a string identifier in the policy registry, with zero modifications to engine mutation logic.
5. **NFR-5 (Execution Throughput):** The native engine processes synthetic workloads of 100,000 memory operations in under 30 seconds on standard consumer hardware.
6. **NFR-6 (Scientific Reproducibility):** Every generated report figure and table records the exact workload SHA-256 hash and Git commit identifier from which it was produced.
7. **NFR-7 (Code Quality and Standards):** The C17 codebase compiles with zero warnings under `-Wall -Wextra -Wpedantic -Werror` on GCC and Clang, and conforms to `clang-tidy` static analysis rules.
8. **NFR-8 (Memory Safety):** Automated CI verifies zero memory leaks, buffer overflows, or undefined behaviors using AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), and Valgrind Memcheck.

---

## 7.2 Project Scope Matrix

To guarantee delivery of a fully verified, zero-defect simulator within a constrained one-week development schedule, features were categorized as **In-Scope**, **Simplified**, or **Cut**.

| Functional Subsystem | Fully In-Scope | Simplified in v1.0 | Explicitly Cut |
| :--- | :--- | :--- | :--- |
| **Contiguous Memory** | First-Fit, Best-Fit, and Worst-Fit placement; split-on-allocation; immediate coalescing on `free` and `terminate`; external and internal fragmentation tracking. | Compaction implemented solely as an explicit, caller-invoked `compact` operation. | Next-Fit placement; byte-alignment constraints; minimum split thresholds (`min_split`); automatic threshold-triggered compaction; $O(\log h)$ indexed search trees. |
| **Demand Paging** | Single-level page tables; virtual address translation; page-fault resolution; FIFO, LRU, and Optimal replacement; automated frame sweeps for Belady's anomaly. | Global frame replacement across processes; fixed single-level page tables. | Clock (Second-Chance) replacement; Least Frequently Used (LFU); local per-process frame quotas; Translation Lookaside Buffer (TLB); multi-level page tables. |
| **Workloads** | Canonical textbook fixtures (AC-1, AC-3, AC-4); synthetic generators (`alloc-uniform`, `refs-locality`). | Line-oriented plain-text workload format (`.wl`) avoiding external C JSON parser dependencies. | Complex JSONL workload input parsing; cJSON library integration. |
| **Traces and Logging** | Structured JSONL trace output containing metadata headers, step-by-step events, full memory snapshots, and execution summaries. | Unified single trace verbosity level capturing complete state transitions. | Multi-tier trace verbosity levels (levels 0, 1, and 2). |
| **Experiments** | Experiment E1 (contiguous placement under memory pressure) and Experiment E4 (page faults versus frame counts). | Shell orchestration scripts driving the CLI over random seeds, outputting CSV datasets. | Automated heuristic parameter search (E5); multi-process thrashing experiments (E6); complex benchmark runner DSL. |
| **User Interfaces** | Command-line interface (`memsim`) supporting `run`, `compare`, `refs`, `sweep`, and `gen`. | Browser-based interactive laboratory built in Svelte and Vite running the C engine compiled to WebAssembly (`memsim.wasm`). | Standalone offline CLI replay viewer; interactive "what-if" branch comparison mode. |
| **Verification & QA** | Eight automated acceptance scenarios (AC-1 to AC-8); invariant checking; ASan, UBSan, and Valgrind pipelines; determinism verification. | Unit and golden test suites covering all reachable execution paths. | Property-based testing engines (e.g., QuickCheck); automated grammar-based fuzzing; formal mutation coverage gating. |

---

## 7.3 Architectural Scope Boundaries

The boundaries separating core engine responsibilities from external infrastructure are illustrated in the scope flow diagram below:

```mermaid
flowchart LR
    subgraph IN["In-Scope Core Engines"]
        direction TB
        C1["Contiguous: FF, BF, WF"]
        C2["Coalescing & Compaction"]
        P1["Paging: FIFO, LRU, OPT"]
        P2["Address Translation & MMU"]
        I1["Invariants: I1-I8, V1-V6"]
    end

    subgraph SIMP["Simplified Mechanisms"]
        direction TB
        S1["Explicit compact op"]
        S2["Global frame replacement"]
        S3["Plain-text .wl workload format"]
        S4["Single-verbosity JSONL trace"]
    end

    subgraph CUT["Explicitly Cut Non-Goals"]
        direction TB
        X1["Next-Fit, Clock, LFU"]
        X2["TLB & Multi-level tables"]
        X3["Local frame quotas"]
        X4["Real arena mmap/malloc hooks"]
        X5["Concurrent multi-threading"]
    end

    IN --> SIMP
    SIMP -.->|Architectural boundaries exclude| CUT
```

---

## 7.4 Architectural Rationale for Simplifications

The architectural trade-offs made during system design were guided by specific engineering considerations:

1. **Plain-Text Workload Format (ADR-012):** Parsing JSON workloads in C requires linking external libraries (such as `cJSON`) and managing heap allocations during parsing. Adopting a line-oriented plain-text format (`alloc PID ID SIZE`, `access PID VA`) eliminated external parsing dependencies, reduced security attack surface, and simplified line-by-line syntax validation.
2. **Explicit Compaction Trigger:** While real kernels may trigger compaction asynchronously upon high-order allocation failure or fragmentation thresholds, MemForge restricts compaction to an explicit `compact` operation. This isolates compaction performance metrics (blocks and units moved) from placement heuristics, enabling deterministic verification under scenario AC-2.
3. **Global Page Replacement:** Local page replacement requires per-process frame quotas, dynamic working-set estimators, and thrashing controls. Standardizing on global replacement models physical memory as a unified frame table, directly matching textbook models and isolating replacement policy evaluation.

---

## 7.5 System Limitations and Non-Goals

To establish the operational scope of MemForge, the following architectural aspects represent deliberate non-goals and limitations:

- **Abstract Space vs Real Memory Allocation:** MemForge does not replace standard runtime allocators (`malloc`, `free`, `sbrk`) via `LD_PRELOAD` or `mmap`. Memory addresses and units are abstract mathematical entities representing simulated units, not raw physical RAM pointers.
- **Single-Threaded Execution:** MemForge models a single simulated CPU executing one memory operation at a time. The core does not simulate multi-core concurrency, bus contention, cache coherency, or synchronization primitives (spinlocks, mutexes).
- **Disk Latency and I/O Modeling:** While page-ins, evictions, and dirty writebacks are tracked and emitted as simulation events, MemForge does not simulate mechanical disk latency, rotational delay, flash wear leveling, or kernel block device request queues.
- **Hardware-Level Virtualization:** The virtual memory engine implements single-level page tables without hardware MMU structures such as TLBs, hierarchical page directories, or inverted page tables.
- **Omission of Next-Fit, Clock, and Alignment:** Next-Fit placement was excluded because its roving-pointer state introduces historical ordering dependencies that complicate static fragmentation analysis. Similarly, Clock replacement (requiring periodic reference-bit clearance) and strict byte alignment constraints were omitted to maintain focus on core allocation dynamics.
