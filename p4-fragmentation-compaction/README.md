# P4: Fragmentation and compaction

Owner: P4 (@coderalokedipto). Requirements FR-C9 and FR-C10, acceptance test AC-2.
Design and definitions: [docs/P4_fragmentation_compaction.md](../docs/P4_fragmentation_compaction.md).

| File | What it does |
| --- | --- |
| `src/fragmentation.{h,c}` | Metrics (total free, largest hole, hole count, ext_frag, unusable_free, int_frag) and the CAPACITY / FRAGMENTATION failure cause |
| `src/compaction.{h,c}` | Pure sliding compaction plan, its cost (blocks and units moved) and a preview on a copy |
| `src/p4_core_types.h` | Verbatim copy of the ARCHITECTURE.md section 6 types until P1's `types.h` lands |
| `tests/` | 17 unit tests, including the AC-1 First-Fit metrics and every AC-2 expectation |

Build and test (macOS or Linux/WSL, gcc or clang, C17):

```sh
cd p4-fragmentation-compaction
make test
```

All work stays in this folder, as agreed, to avoid merge conflicts. To integrate later, the files
map one-to-one onto the P4 placeholders in `src/contiguous/` and the tests onto
`tests/unit/contiguous/`.
