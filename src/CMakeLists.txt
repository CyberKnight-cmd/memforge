# Order follows the layers: each module may depend only on modules above it.
add_subdirectory(common)      # P1  shared kernel
add_subdirectory(contiguous)  # P2, P3, P4
add_subdirectory(paging)      # P5, P6
add_subdirectory(trace)       # P1
add_subdirectory(workloads)   # P7
add_subdirectory(metrics)     # P7
add_subdirectory(session)     # P1
add_subdirectory(bench)       # P7
add_subdirectory(cli)         # P9
