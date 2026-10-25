memsim_module(contiguous DEPS common SOURCES
  # P2
  engine.c
  invariants.c
  model.c
  # P3: one file per policy, plus its registry line
  policies/best_fit.c
  policies/first_fit.c
  policies/worst_fit.c
  # P4
  compaction.c
  fragmentation.c)
