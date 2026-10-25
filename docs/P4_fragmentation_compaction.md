# Fragmentation and compaction (P4)

Owner: P4. Code: `p4-fragmentation-compaction/src/` (fragmentation, compaction).
Tests: `p4-fragmentation-compaction/tests/` (run `make test` in that folder).
Requirements: FR-C9 (metrics), FR-C10 (compaction), AC-2. Design: ARCHITECTURE.md section 8, ADR-011.

## Fragmentation metrics

All metrics are computed from the block list alone, in one O(n) pass, and never change it. Holes
are FREE blocks; because the model coalesces neighbours (invariant I3), each FREE block is one
maximal hole.

| Metric | Definition | Meaning |
| --- | --- | --- |
| `total_free` | Sum of FREE block sizes | How much memory is unused |
| `largest_hole` | Size of the largest FREE block, 0 if none | The biggest request that can succeed right now |
| `hole_count` | Number of FREE blocks | How many pieces free memory is split into |
| `ext_frag` | 1 − largest_hole / total_free; 0 when total_free = 0 | 0: all free memory is one hole. Near 1: it is scattered in small pieces |
| `unusable_free(s)` | Share of free memory in holes smaller than a reference size s | How much free memory is useless for requests of size s |
| `int_frag` | Sum of (size − requested) over ALLOCATED blocks | Memory wasted inside allocated blocks |

**Why this external fragmentation formula.** It answers the practical question "what fraction of
free memory can a single request *not* use?". It is 0 exactly when free memory is contiguous and
grows as free memory is split up. It needs only two numbers the allocator already tracks, and it
is the definition used in the OS literature. Its limit is that it ignores the shape of the
smaller holes, which is why `hole_count` and `unusable_free(s)` are reported alongside it.

**No floating point in engine state.** Fractions are stored as integer parts per million
(`ext_frag_ppm`), rounded down, so runs are bit-for-bit reproducible across machines.
`ms_cont_ppm_to_double` converts for reports and the trace only.

## Failure cause

When an allocation of `granted` units fails, `ms_cont_frag_classify_failure` labels why:

| Cause | Condition | Meaning |
| --- | --- | --- |
| CAPACITY | total_free < granted | Memory is genuinely full; no strategy could help |
| FRAGMENTATION | total_free ≥ granted, largest_hole < granted | Enough memory is free, but no single hole is large enough |

It returns NONE if the request would fit, so a caller bug cannot be mistaken for a failure.

## Compaction design

**Strategy (sliding).** Visit allocated blocks in address order and slide each one down to the
end of the previous block. The first block goes to address 0.

- Order is preserved, and all free memory ends up as **one hole at the top**, so ext_frag becomes 0.
- Moves are produced in ascending address order and every block only moves down, so applying them
  in order never overwrites a block that has not moved yet.
- Blocks already in place produce no move.

**Never silent (ADR-011).** `ms_cont_compaction_plan_sliding` is pure: it reads the layout and
returns a list of moves `(alloc_id, src, dst)`. Only P2's `apply_relocations` changes the model,
and the engine emits `CompactionStarted`, one `BlockRelocated` per move and `CompactionCompleted`.
Compaction runs only on an explicit `compact` op this week; automatic triggers are cut.

**Cost.** `ms_cont_compaction_cost_of` reports blocks moved and units moved (the sum of the moved
blocks' sizes), which is deterministic, unlike wall-clock time.

**Guarantees, each covered by a test.** Every allocated block keeps its id, pid, size and
requested size; total free is unchanged; at most one hole remains; relative order is kept; the
result still tiles memory from 0 to M.

**Why it is safe to move blocks.** A real OS needs dynamic relocation (a base register) to move a
running process. Our stable allocation ids play that role: a later `free` finds the block by id,
wherever it now is.

**Preview.** `ms_cont_compaction_preview` applies a plan to a copy of the layout. Tests, the
viewer and the report use it to show the effect of compaction without touching the model.

## Test workloads

**AC-1 First-Fit (Silberschatz five holes).** Memory of 1740 units holds holes of 100, 500, 200, 300
and 600 between four 10-unit SYS blocks. First-Fit places 212 at 110, 417 at 1140 and 112 at 322.
The holes are then 100, 176, 200, 300 and 183:

| Before compaction | Value |
| --- | --- |
| total_free | 959 |
| largest_hole | 300 |
| hole_count | 5 |
| ext_frag | 1 − 300/959 = 0.687 |
| Request 426 | Fails: FRAGMENTATION (959 ≥ 426, but 300 < 426) |

**AC-2: compact, then retry.** The sliding plan moves seven blocks:

| Block | Size | From | To |
| --- | --- | --- | --- |
| SYS s1 | 10 | 100 | 0 |
| P1 a1 | 212 | 110 | 10 |
| P3 a3 | 112 | 322 | 222 |
| SYS s2 | 10 | 610 | 334 |
| SYS s3 | 10 | 820 | 344 |
| SYS s4 | 10 | 1130 | 354 |
| P2 a2 | 417 | 1140 | 364 |

Cost: 7 blocks, 10 + 212 + 112 + 10 + 10 + 10 + 417 = **781 units moved**. Free memory becomes one
hole [781, 1740) of 959, so ext_frag = **0**. The 426 retry now succeeds at 781, leaving one hole
[1207, 1740) of 533.

**Edge cases tested.** Empty memory (ext_frag 0, nothing to move), full memory (no holes, CAPACITY
on any request, ext_frag defined as 0 rather than dividing by zero), the exact boundaries of the
cause (a request equal to the largest hole fits; one equal to total_free is FRAGMENTATION),
already-compact memory (no moves), and a layout where only some blocks need to move.

## Integration notes

- `p4_core_types.h` holds verbatim copies of the section 6 types until P1 merges
  `memsim/common/types.h`; then delete it and its two includes.
- The functions take the block list (`const ms_block *`, count) in address order. When P2's
  `ms_cont_view` lands, the vtable entries (`report(view)`, `classify_failure(view, size)`,
  `plan(view, out)`) are thin wrappers that pass the view's block list through.
- P7 computes these metrics in the `MetricsCollector` using the definitions above.
