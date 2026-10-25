# MemForge

A deterministic simulator of an operating system's memory manager, written in C. MemForge's
simulator (working name MemSim, binary `memsim`) covers both halves of the classic OS course
material:

- **Contiguous allocation** with First-Fit, Best-Fit and Worst-Fit placement, block splitting and
  coalescing, fragmentation metrics and sliding compaction.
- **Demand paging** with per-process page tables, address translation, and FIFO, LRU and OPT page
  replacement, including a frame sweep that reproduces Belady's anomaly.

Every run is reproducible: the same workload, configuration and seed always give a byte-identical
trace, so policies can be compared fairly and every number in the report traces back to a test or a
benchmark row.

> **Status:** pre-0.1, milestone M0 (contracts). The repository skeleton builds and its test passes;
> the engines are not implemented yet. See [the architecture](docs/ARCHITECTURE.md) for the plan.

## Quick start

Requirements: a C17 compiler (gcc or clang), CMake 3.25 or later, and Ninja.

```sh
cmake --preset dev            # Debug build with AddressSanitizer and UBSan
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/memsim
```

Use the `release` preset for benchmarks. Before opening a pull request, also run the checks CI runs:

```sh
scripts/check_layers.sh
find src tests -name '*.[ch]' -print0 | xargs -0 clang-format --dry-run --Werror
```

## What it will do

Once the engines land, the CLI covers the textbook exercises directly:

```sh
memsim compare workload.jsonl --policies first-fit,best-fit,worst-fit
memsim refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --frames 3 --policy lru --table
memsim sweep refs.jsonl --policy fifo --frames 1..8
```

Workloads, traces and benchmark results are versioned JSONL files, described in sections 11 to 13
of the architecture.

## Repository layout

```text
src/<module>/          one static library per module: exported headers in include/, private ones beside the code
  common/              shared kernel: types, config, errors, events, event bus, registry
  contiguous/          memory model, placement policies, fragmentation, compaction
  paging/              MMU, page and frame tables, replacement policies, Belady sweep
  trace/, session/     trace writer, snapshots, and the Session facade every client uses
  workloads/, metrics/, bench/   loader, generators, metrics collector, benchmark runner
  cli/                 the memsim command
tests/                 unit tests per module, plus golden, property, oracle and integration tests
benchmarks/specs/      committed benchmark specs
viewer/                trace replay viewer
third_party/           vendored libraries
docs/                  architecture, conventions, research
```

Modules can only use what they link, so the layering in the architecture is checked by the
compiler, and `scripts/check_layers.sh` covers what the compiler can't.

## Documentation

- [System architecture](docs/ARCHITECTURE.md): requirements, acceptance criteria, module design,
  formats, test strategy, decision log and milestones. Start with sections 1 to 6.
- [C conventions](docs/CONVENTIONS.md): module layout, naming, interfaces, ownership and
  determinism rules.
- [Research notes](docs/RESEARCH.md)

## Team

Ten owners, one per area; [CODEOWNERS](.github/CODEOWNERS) routes reviews accordingly.

| Role | Area |
| --- | --- |
| P1 | Lead: shared kernel, `Session`, trace format, architecture |
| P2 | Memory model and contiguous engine |
| P3 | Placement policies |
| P4 | Fragmentation and compaction |
| P5 | Paging and address translation |
| P6 | Replacement policies and Belady analysis |
| P7 | Workloads, metrics and benchmarks |
| P8 | QA and correctness |
| P9 | CLI and viewer |
| P10 | Repository, CI and docs |

## Contributing

- Branch from `main` as `feat/<module>-<topic>` and keep branches short-lived; `main` stays green.
- Use [Conventional Commits](https://www.conventionalcommits.org/) (`feat:`, `fix:`, `docs:` and so
  on).
- Each pull request needs its module owner's approval. Changes to `src/common/`, a shared header, an
  event or a file format also need P1, and an RFC issue once kickoff has ratified the contracts.
- Follow [the conventions](docs/CONVENTIONS.md); CI rejects formatting, layering, warning and test
  failures.
