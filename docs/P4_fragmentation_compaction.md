# Fragmentation and compaction (P4)

Owner: P4. Code: `src/contiguous/fragmentation.{h,c}`, `src/contiguous/compaction.{h,c}`.
Tests: `tests/unit/contiguous/` (`test_fragmentation`, `test_compaction`, `test_compaction_window`,
`test_p4_random`, `test_p4_demo`). See [Reproduce](#reproduce) to run them.
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

**Preview.** `ms_cont_compaction_preview` (for either strategy) applies a plan to a copy of the layout. Tests, the
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

## Approaches considered

### Fragmentation metric

Four candidate measures of external fragmentation were computed on the same layouts
(`test_p4_demo`, "Candidate metrics"):

| Layout | free | holes | ext_frag | avg hole | unusable(100) |
| --- | --- | --- | --- | --- | --- |
| AC-1 First-Fit | 959 | 5 | 0.687 | 191.8 | 0.000 |
| two 500 holes | 1000 | 2 | 0.500 | 500.0 | 0.000 |
| 900 hole + 4 × 1 | 904 | 5 | 0.004 | 180.8 | 0.004 |
| three 20 holes | 60 | 3 | 0.667 | 20.0 | 1.000 |

- **Hole count** alone misleads: "900 hole + 4 × 1" has as many holes as AC-1 but is almost
  perfectly usable.
- **Average hole size** misleads the same way: 180.8 and 191.8 look alike, yet AC-1 fails a 426
  request and the other does not.
- **unusable_free(s)** is the most practical for a known request size, but it depends on choosing
  s: it is 0 for AC-1 at s = 100, yet AC-1 fails at 426. With mixed request sizes no single s fits.
- **ext_frag = 1 − largest/total** needs no parameter, is 0 exactly when free memory is one
  piece and ranks the layouts sensibly. Its weakness is "two 500 holes", scored 0.5 although
  any request up to 500 fits.

Decision: ext_frag is the headline metric because it is parameter-free and directly explains
FRAGMENTATION failures (a request fails exactly when it is larger than `largest_hole`).
hole_count and unusable_free(s) are reported beside it to cover its blind spots.

### Compaction strategy

| Strategy | Idea | Kept? |
| --- | --- | --- |
| **Sliding** (full) | Slide every allocated block down; one hole at the top | Yes: the explicit `compact` op and AC-2 |
| **Window** ("just enough") | Pack only the cheapest run of consecutive blocks whose free space covers the waiting request | Yes: implemented and compared; recommended for `on_failure` |
| Relocate into holes | Move high blocks into low holes that fit them | No: breaks address order, so a later block can overwrite one not yet moved, and results depend on the order holes are tried |
| Avoid compaction | Better placement, buddy system or paging | Out of P4's scope; P3 compares placement in E1 and paging is P5 and P6 |

Both kept strategies preserve order and only move blocks down, so applying their moves in order
is always safe.

**AC-1 First-Fit, request 426 (`test_p4_demo`):**

```text
Before compaction: request 426 -> FRAGMENTATION
  |...111111133333.....S......S.........S22222222222222......|  0..1740
  total_free 959, largest_hole 300, holes 5, ext_frag 0.687

After sliding: 7 blocks / 781 units moved; request 426 -> fits
  |11111113333S222222222222222...............................|  0..1740
  [781,1740) free 959
  total_free 959, largest_hole 959, holes 1, ext_frag 0.000

After window: 1 blocks / 10 units moved; request 426 -> fits
  |...111111133333.....S................S22222222222222......|  0..1740
  [0,100) free 100  [434,610) free 176  [630,1130) free 500  [1557,1740) free 183
  total_free 959, largest_hole 500, holes 4, ext_frag 0.479
```

Window moves one 10-unit SYS block and joins the 200 and 300 holes. Sliding moves 78 times as
much memory to reach the same outcome for this request, though it leaves memory fully tidy.

**Seeded workload (`test_p4_demo`, "Experiment").** First-Fit on 1024 units, 3000 operations,
10 seeds, request sizes 8 to 128, 45% frees; compaction runs only when a request fails for
FRAGMENTATION, then the request is retried:

| Compaction | Requests | FRAGMENTATION fails | CAPACITY fails | Compactions | Units moved | Mean ext_frag |
| --- | --- | --- | --- | --- | --- | --- |
| never | 16586 | 2807 | 257 | 0 | 0 | 0.484 |
| sliding | 16567 | 0 | 2992 | 1768 | 1,247,938 | 0.343 |
| window | 16567 | 0 | 2992 | 2184 | 715,772 | 0.395 |

- Both strategies remove every FRAGMENTATION failure. The remaining failures are CAPACITY,
  which rise because memory now actually fills up.
- Window moves **43% fewer units** than sliding, but compacts 24% more often, because it leaves
  the rest of memory fragmented (mean ext_frag 0.395 against 0.343).
- On 5,000 random layouts (`test_p4_random`), window never moved more than sliding: less in
  1,617 of 1,711 FRAGMENTATION cases and the same in 94, with 826,827 units moved against
  1,817,000 overall.

**Recommendation.** Keep sliding for the explicit `compact` op, as the plan and AC-2 require: it
gives the simplest, fully predictable result. If the `on_failure` trigger is brought back, use
window: it fixes the same failures for about half the copying. Window costs O(n²) to plan in
the worst case against O(n) for sliding, which is negligible at our sizes.

## Validation beyond the acceptance tests

- **Independent oracle** (`test_p4_random`): 5,000 seeded random layouts (sizes 1 to 4000 units).
  An oracle paints memory unit by unit and measures free space and the longest free run without
  looking at block boundaries. total_free, largest_hole and the failure cause match it in every
  case.
- **Compaction guarantees on every random layout, for both strategies:** same allocated blocks
  (id, pid, size, requested) in the same order, same total free, memory still tiled from 0 to M,
  no two adjacent holes; sliding leaves at most one hole; window always makes the request fit.
- **Determinism:** a fixed-seed xorshift generator instead of `rand()`, so every machine prints
  the same numbers.

## Reproduce

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev --output-on-failure
./build/dev/bin/test_p4_demo     # sample run, metric comparison and experiment
./build/dev/bin/test_p4_random   # oracle and property checks
```

On macOS, if the sanitizer build hangs, use a plain build instead:
`cmake -S . -B build/plain -G Ninja && cmake --build build/plain && ctest --test-dir build/plain`.

## Integration notes

- `src/contiguous/p4_core_types.h` holds verbatim copies of the section 6 types until P1 merges
  `memsim/common/types.h`; then delete it and its two includes.
- The functions take the block list (`const ms_block *`, count) in address order. When P2's
  `ms_cont_view` lands, the vtable entries (`report(view)`, `classify_failure(view, size)`,
  `plan(view, out)`) are thin wrappers that pass the view's block list through.
- P7 computes these metrics in the `MetricsCollector` using the definitions above.
