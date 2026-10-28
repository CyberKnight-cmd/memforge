# 30. Appendix

This appendix provides formal specifications, reference catalogues, empirical verification tables, and command-line usage transcripts for MemForge.

## 30.1 Appendix A: Workload File Format Specification and Grammar

MemForge workloads are plain-text, line-oriented files (conventionally with extension `.wl`). This design enables human readability, rapid generation via scripts, and clean line-by-line diffing in source control without requiring a heavyweight JSON or YAML parser in the C runtime.

### Extended Backus-Naur Form (EBNF) Grammar

The formal syntax of a valid MemForge workload file is defined below.

```ebnf
Workload         ::= Header OperationList

Header           ::= SchemaLine KindLine ConfigLines
SchemaLine       ::= "schema" Spaces "1" Newline
KindLine         ::= "kind" Spaces ("contiguous" | "paging") Newline
ConfigLines      ::= (ContiguousConfig | PagingConfig)
ContiguousConfig ::= "memory_size" Spaces Integer Newline [OptionalConfig]
PagingConfig     ::= "page_size" Spaces Integer Newline
                     "frames" Spaces Integer Newline [OptionalConfig]
OptionalConfig   ::= "negative_test" Spaces ("0" | "1") Newline

OperationList    ::= { Line }
Line             ::= [ Spaces ] [ Comment | Operation ] Newline
Comment          ::= "#" { AnyCharExceptNewline }

Operation        ::= ContiguousOp | PagingOp | CommonOp

ContiguousOp     ::= AllocOp | FreeOp | CompactOp
AllocOp          ::= "alloc" Spaces Ident Spaces Ident Spaces PositiveInt
FreeOp           ::= "free" Spaces Ident Spaces Ident
CompactOp        ::= "compact"

PagingOp         ::= SpawnOp | AccessOp
SpawnOp          ::= "spawn" Spaces Ident Spaces PositiveInt
AccessOp         ::= "access" Spaces Ident Spaces PageRef [ Spaces "w" ]
PageRef          ::= VpnRef | VaRef
VpnRef           ::= NonNegativeInt
VaRef            ::= "@" NonNegativeInt

CommonOp         ::= TerminateOp
TerminateOp      ::= "terminate" Spaces Ident

Ident            ::= Letter { Letter | Digit | "_" | "-" }
PositiveInt      ::= DigitNonZero { Digit }
NonNegativeInt   ::= "0" | PositiveInt
Integer          ::= [ "-" ] NonNegativeInt
Spaces           ::= { " " | "\t" }+
Newline          ::= "\r\n" | "\n"
```

### State Transitions of Memory Entities

Memory blocks in contiguous memory and physical frames in demand paging transition through well-defined lifecycle states, illustrated below.

```mermaid
stateDiagram-v2
    [*] --> Free: Memory Initialized
    Free --> Allocated: alloc (split hole)
    Allocated --> Free: free (coalesce adjacent)
    Allocated --> Relocated: compact (sliding shift)
    Relocated --> Allocated: move completed
    Allocated --> Free: terminate process
    Free --> [*]: Memory Teardown

    note right of Free: Invariant I2: No adjacent holes
    note right of Allocated: Invariant I1: No overlapping blocks
```

### Parsing Algorithm Specification

The workload lexing and validation algorithm implemented in [src/workloads/loader.c](file:///home/cyberknight/memforge-agy/rep-ops/src/workloads/loader.c) is formalized below.

```text
Algorithm 7: Deterministic Workload Parser and Lexer
Input: File stream or memory buffer B containing workload text, StrictMode flag
Output: Structured workload instance W or error descriptor (line, message)

1:  line_number ← 0
2:  W.operations ← []
3:  header_parsed ← False
4:  allocated_ids ← Set()
5:  for each line L in ReadLines(B) do
6:      line_number ← line_number + 1
7:      stripped ← StripCommentsAndWhitespace(L)
8:      if stripped is empty then
9:          continue
10:     end if
11:     tokens ← Tokenize(stripped, delimiter: ' ')
12:     if not header_parsed then
13:         ParseHeaderLine(tokens, W, StrictMode, line_number)
14:         if W.header_complete then
15:             header_parsed ← True
16:         end if
17:         continue
18:     end if
19:     op ← ParseOperation(tokens, W.kind, line_number)
20:     if StrictMode then
21:         if op.type = "alloc" then
22:             if op.id ∈ allocated_ids then
23:                 return Error(line: line_number, message: "Duplicate allocation ID: " + op.id)
24:             end if
25:             Insert(allocated_ids, op.id)
26:         else if op.type = "free" then
27:             if op.id ∉ allocated_ids then
28:                 return Error(line: line_number, message: "Free of unknown ID: " + op.id)
29:             end if
30:             Remove(allocated_ids, op.id)
31:         end if
32:     end if
33:     Append(W.operations, op)
34: end for
35: if not header_parsed then
36:     return Error(line: line_number, message: "Incomplete workload header")
37: end if
38: return Success(W)
```

**Complexity Analysis**:
- **Time Complexity**: $\mathcal{O}(L)$, where $L$ is the total length of the workload text in bytes. Tokenization and op parsing require a single linear scan over lines. Identifier tracking under strict mode uses hash lookups with $\mathcal{O}(1)$ average-case complexity.
- **Space Complexity**: $\mathcal{O}(N_{\text{ops}})$, proportional to the number of operations parsed and loaded into the `ms_op` array.

## 30.2 Appendix B: Canonical Event Catalogue

Table 30.1 provides the complete catalogue of all 21 discrete events defined in Section 11 of the system architecture. These events constitute the complete observational contract of MemForge; metrics calculators, trace emitters, and graphical interfaces derive all runtime state from this stream.

*Table 30.1: Complete Canonical Simulation Event Catalogue*

| Event Name | Key Data Fields | Emitting Module | Semantic Meaning & Trigger Condition |
| --- | --- | --- | --- |
| `PlacementDecided` | `policy`, `requested`, `granted`, `hole`, `scanned` | Contiguous ($P_2$) | Emitted when a placement policy evaluates the free hole list and selects a target hole (or determines that no fitting hole exists). |
| `BlockSplit` | `hole`, `allocated_part`, `remainder` | Contiguous ($P_2$) | Emitted when a selected hole is larger than the requested size and is split into an allocated prefix and a remaining free hole. |
| `BlockAllocated` | `pid`, `id`, `start`, `size`, `requested` | Contiguous ($P_2$) | Emitted when a contiguous block is successfully recorded in the active allocation table. |
| `AllocationFailed` | `pid`, `id`, `requested`, `granted`, `cause`, `total_free`, `largest_hole` | Contiguous ($P_2$) | Emitted when an allocation fails. `cause` indicates whether failure is due to total memory exhaustion (`CAPACITY`) or fragmentation (`FRAGMENTATION`). |
| `AllocRejected` | `pid`, `id`, `reason` | Contiguous ($P_2$) | Emitted when an allocation request is syntactically invalid, such as non-positive size (`INVALID_SIZE`) or identifier reuse (`DUPLICATE_ID`). |
| `BlockFreed` | `pid`, `id`, `start`, `size` | Contiguous ($P_2$) | Emitted when an allocated block is returned to the unallocated pool. |
| `BlocksCoalesced` | `merged_ranges`, `resulting_range` | Contiguous ($P_2$) | Emitted when a freed block is merged with adjacent free holes at lower or higher memory addresses to maintain Invariant I2. |
| `FreeRejected` | `pid`, `id`, `reason` | Contiguous ($P_2$) | Emitted on invalid deallocation: `UNKNOWN_ID`, `DOUBLE_FREE`, or deallocation by a non-owning process (`WRONG_OWNER`). |
| `FreeSkipped` | `pid`, `id` | Contiguous ($P_2$) | Emitted when a workload requests deallocation of a block whose prior allocation failed due to fragmentation. |
| `ProcessTerminated` | `pid`, `freed_ids`, `released_frames` | Contiguous ($P_2$), Paging ($P_5$) | Emitted when a process terminates, releasing all allocated contiguous blocks or virtual page frames. |
| `CompactionStarted` | `trigger`, `holes_before`, `largest_before` | Contiguous ($P_2, P_4$) | Emitted immediately before executing sliding compaction. |
| `BlockRelocated` | `id`, `pid`, `src`, `dst`, `size` | Contiguous ($P_2, P_4$) | Emitted for each allocated block moved toward lower memory addresses during compaction. |
| `CompactionCompleted` | `blocks_moved`, `units_moved`, `holes_after`, `largest_after` | Contiguous ($P_2, P_4$) | Emitted upon compaction completion, summarizing total blocks shifted and cumulative memory units relocated. |
| `ProcessSpawned` | `pid`, `pages` | Paging ($P_5$) | Emitted when a process is initialized with a virtual address space of size `pages`. |
| `PageHit` | `pid`, `vpn`, `frame`, `write` | Paging ($P_5$) | Emitted when a referenced virtual page is currently resident in physical memory (`present = true`). |
| `PageFault` | `pid`, `vpn`, `kind`, `write` | Paging ($P_5$) | Emitted when a referenced virtual page is not resident. `kind` is classified as `COMPULSORY` (first access) or `CAPACITY` (pool exhausted). |
| `FrameAllocated` | `frame`, `pid`, `vpn` | Paging ($P_5$) | Emitted when an available physical frame is assigned to a virtual page. |
| `VictimSelected` | `policy`, `frame`, `victim_pid`, `victim_vpn` | Paging ($P_5, P_6$) | Emitted when a replacement policy selects a resident frame to be evicted. |
| `PageEvicted` | `frame`, `pid`, `vpn`, `dirty` | Paging ($P_5$) | Emitted when a victim page is unmapped from its owner's page table. |
| `PageWrittenBack` | `frame`, `pid`, `vpn` | Paging ($P_5$) | Emitted prior to eviction when a victim page has its dirty bit set (`dirty = true`), modeling disk writeback latency. |
| `PageLoaded` | `frame`, `pid`, `vpn` | Paging ($P_5$) | Emitted when virtual page contents are loaded into the target physical frame. |
| `ReferenceBitCleared` | `frame` | Paging ($P_5$) | Emitted when an aging or Clock replacement policy clears the page reference bit. |
| `AccessViolation` | `pid`, `vpn` or `va`, `reason` | Paging ($P_5$) | Emitted on invalid memory reference: `NO_SUCH_PROCESS` or `OUT_OF_RANGE`. |
| `AddressTranslated` | `pid`, `va`, `vpn`, `offset`, `frame`, `pa` | Paging ($P_5$) | Emitted in detailed tracing mode when a 32-bit virtual address is successfully translated into a physical address. |

## 30.3 Appendix C: Acceptance Verification Matrix

Table 30.2 reproduces the acceptance verification results generated by `scripts/report/verify_report.sh` at commit `53039c8`. All eight formal acceptance criteria pass with 100% compliance across 17 test suites under CTest.

*Table 30.2: Acceptance Criteria Verification Results for MemForge*

| ID | Scenario | What is Checked | Expected Value | Observed Value | Result |
| --- | --- | --- | --- | --- | --- |
| **AC-1** | Placement on five holes (Silberschatz) | Placements of $a_1, a_2, a_3, a_4$ across FF, BF, WF | FF 110,1140,322,fail FRAGMENTATION;<br>BF 830,110,620,1140;<br>WF 1140,110,1352,fail FRAGMENTATION | FF 110,1140,322,fail FRAGMENTATION;<br>BF 830,110,620,1140;<br>WF 1140,110,1352,fail FRAGMENTATION | **PASS** |
| **AC-2** | Contiguous compaction and allocation retry | Compaction metrics `blocks_moved`, `units_moved`, and final free block | `blocks_moved` 7, `units_moved` 781;<br>free block start 1207 size 533 | `blocks_moved` 7, `units_moved` 781;<br>free block start 1207 size 533 | **PASS** |
| **AC-3** | Reference string page replacement | Page fault counts for FIFO, LRU, OPT with 3 frames | fifo 15, lru 12, opt 9 | fifo 15, lru 12, opt 9 | **PASS** |
| **AC-4** | Belady's anomaly frame sweep | FIFO sweep faults 1..6 and Belady point; LRU/OPT faults at 3,4 frames | fifo faults [12,12,9,10,5,5] with Belady point 4;<br>lru 10,8; opt 7,6 | fifo faults [12,12,9,10,5,5] with Belady point 4;<br>lru 10,8; opt 7,6 | **PASS** |
| **AC-5** | Negative tests: rejections and access violations | Contiguous rejections count and paging violations count | rejections 3; violations 2 | rejections 3; violations 2 | **PASS** |
| **AC-6** | Workload trace determinism | Byte-identical traces across two consecutive runs for all 6 fixtures | All 6 fixtures byte-identical | 6/6 fixtures byte-identical | **PASS** |
| **AC-7** | Simulation invariants checked under test suite | Invariants hold on every step in test suite (`ctest`) | 17/17 passed | 17/17 passed | **PASS** |
| **AC-8** | Comparative policy table output | Human-readable compare output displays `fail FRAGMENTATION` | Table contains `fail FRAGMENTATION` | Table contains `fail FRAGMENTATION` | **PASS** |

## 30.4 Appendix D: Command-Line Interface Usage Text

The verbatim help output emitted by `memsim --help` is provided below:

```text
Usage: memsim <command> [options]

Commands:
  validate <workload>
  run <workload> --policy <P> [--trace <file>] [--no-check] [--json]
  compare <workload> --policies <a,b,c> [--json]
  refs <"7 0 1 ..."> --frames <F> --policy <P> [--table] [--json]
  sweep <file> --policy <P> --frames <A..B> [--json]
  gen alloc-uniform|refs-locality --seed <S> [options] -o <file>
  help, --help
```

### Detailed Flag and Option Descriptions

*Table 30.3: Command-Line Argument and Option Reference*

| Command | Option / Flag | Type & Format | Description |
| --- | --- | --- | --- |
| `validate` | `<workload>` | File path (`.wl`) | Path to the workload file to validate. Exits with code 0 on success or 2 on syntax/semantic error. |
| `run` | `--policy <P>` | String (`first-fit`, `best-fit`, `worst-fit`, `fifo`, `lru`, `opt`) | Allocation or replacement policy to execute. |
| `run` | `--trace <file>` | Output file path | File to receive the canonical JSONL trace stream. |
| `run` | `--no-check` | Boolean flag | Disables post-step invariant assertions (Invariant checking is enabled by default). |
| `run` | `--json` | Boolean flag | Emits run metrics in structured JSON format rather than plain text. |
| `compare` | `--policies <list>` | Comma-separated strings | List of policies to execute side-by-side against the same workload. |
| `refs` | `"<r1 r2 ...>"` | Space-separated integers | Paging reference string provided directly on the command line. |
| `refs` | `--frames <F>` | Positive integer | Number of physical memory frames allocated to the paging engine. |
| `refs` | `--table` | Boolean flag | Prints the ASCII frame-by-reference table showing frame states and faults (`F`). |
| `sweep` | `--frames <A..B>` | Integer range (e.g. `1..6`) | Evaluates fault counts across frame allocations from $A$ to $B$. Flags Belady points with `*`. |
| `gen` | `alloc-uniform` | Subcommand | Generates contiguous workloads. Flags: `--seed`, `--ops`, `--memory-size`, `--size-min`, `--size-max`, `--p-alloc-ppm`, `-o`. |
| `gen` | `refs-locality` | Subcommand | Generates localized paging workloads. Flags: `--seed`, `--n`, `--pages`, `--working-set`, `--shift-every`, `--p-out-ppm`, `--p-write-ppm`, `--frames`, `--page-size`, `-o`. |
