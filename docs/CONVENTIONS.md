# C conventions

How MemSim's C code is laid out and named. Owner: P10, with P1 for anything touching module
boundaries. Changes go through a normal PR.

## Modules

Every module is one folder under `src/` and one static library, `memsim_<module>`:

```text
src/<module>/
  CMakeLists.txt                  memsim_module(<module> DEPS ... SOURCES ...)
  include/memsim/<module>/*.h     exported: the module's interface, seen by whoever links it
  *.h, *.c                        private: visible only inside the module and its unit tests
  policies/*.c                    one file per policy (contiguous and paging only)
```

- Export as little as possible. If only your own module uses a header, keep it private.
- `DEPS` is the layering contract from README section 5, and `scripts/check_layers.sh` checks it
  in CI. Adding a dependency means updating the allowed table in that script, which needs P1's
  review.
- A module includes its own private headers by bare name (`#include "model.h"`) and every exported
  header by its full path (`#include "memsim/common/events.h"`).
- When a file grows past roughly 500 lines, split it by responsibility inside the same folder
  (`engine_alloc.c`, `engine_free.c`) and add the new file to `SOURCES`.

## Names

C has one global namespace, so every non-static symbol carries its module's prefix.

| Module | Prefix | Example |
| --- | --- | --- |
| common | `ms_` | `ms_event`, `ms_bus_emit` |
| contiguous | `ms_cont_` | `ms_cont_engine_alloc` |
| paging | `ms_page_` | `ms_page_engine_access` |
| trace | `ms_trace_` | `ms_trace_writer_open` |
| session | `ms_session_` | `ms_session_apply` |
| workloads | `ms_wl_` | `ms_wl_load` |
| metrics | `ms_metrics_` | `ms_metrics_on_events` |
| bench | `ms_bench_` | `ms_bench_run` |

- Types are `snake_case` structs with typedefs (`typedef struct ms_hole ms_hole;`), and macros and
  enum constants are `MS_UPPER_CASE`.
- Anything not exported is `static`, and `-Wmissing-prototypes` reports a non-static function
  that has no declaration.
- Header guards are `MEMSIM_<PATH>_H_`.

## Interfaces and ownership

- Policies are vtables: a `const` struct of function pointers plus a `void *` state, registered by
  name in the module's registry. A new policy is one `.c` file, one `SOURCES` line and one
  registry line (NFR-4).
- Whoever allocates an object frees it, through a matching `_create` / `_destroy` pair. Policies
  never allocate engine state.
- Domain outcomes such as a failed allocation are return codes plus events. Contract violations go
  through `MS_ASSERT`, which aborts even in release builds (ADR-006).

## Determinism

- The core (`common`, `contiguous`, `paging`) never includes `<stdio.h>`, `<time.h>`, `<unistd.h>`
  or `<sys/*>`. CI enforces this.
- Use integer state only: fixed-width types (`int64_t`, `uint32_t`) for sizes, addresses and
  counters, and no floats in engine state.
- Never depend on hash iteration order or on pointer values, and break every tie as section 6 says.
