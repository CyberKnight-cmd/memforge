memsim_module(paging DEPS common SOURCES
  # P5
  engine.c
  frames.c
  invariants.c
  mmu.c
  tables.c
  # P6: one file per policy, plus its registry line
  policies/fifo.c
  policies/lru.c
  policies/opt.c
  policies/registry.c
  belady.c)
