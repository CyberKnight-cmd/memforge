# AC-2 Compaction benchmark
schema 1
kind contiguous
memory_size 1740

# Setup: 5 holes separated by four 10-unit SYS blocks
alloc SYS h1 100
alloc SYS s1 10
alloc SYS h2 500
alloc SYS s2 10
alloc SYS h3 200
alloc SYS s3 10
alloc SYS h4 300
alloc SYS s4 10
alloc SYS h5 600

# Free the five hole blocks in address order
free SYS h1
free SYS h2
free SYS h3
free SYS h4
free SYS h5

# Workload requests
alloc P1 a1 212
alloc P2 a2 417
alloc P3 a3 112
alloc P4 a4 426

# Compact and retry
compact
alloc P4 a4-retry 426
