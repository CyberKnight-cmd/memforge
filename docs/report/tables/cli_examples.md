```text
$ memsim compare tests/fixtures/ac1_silberschatz.wl --policies first-fit,best-fit,worst-fit
id  size  first-fit           best-fit  worst-fit
a1  212   110                 830       1140
a2  417   1140                110       110
a3  112   322                 620       1352
a4  426   fail FRAGMENTATION  1140      fail FRAGMENTATION

metric                  first-fit  best-fit  worst-fit
alloc_requests          13         13        13
alloc_successes         12         13        12
alloc_failures          1          0         1
failures_fragmentation  1          0         1
failures_capacity       0          0         0
rejections              0          0         0
frees                   5          5         5
compactions             0          0         0
blocks_moved            0          0         0
units_moved             0          0         0
scanned_total           23         29        29
scanned_mean            1.769230   2.230769  2.230769
ext_frag_final          0.687174   0.673545  0.687174
ext_frag_mean           0.245843   0.212145  0.245012
hole_count_final        5          5         5
total_free_final        959        533       959
largest_hole_final      300        174       300
used_final              781        1207      781
live_blocks_final       7          8         7

$ memsim refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --frames 3 --policy fifo --table
7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1
7 7 7 2 2 2 2 4 4 4 0 0 0 0 0 0 0 7 7 7
  0 0 0 0 3 3 3 2 2 2 2 2 1 1 1 1 1 0 0
    1 1 1 1 0 0 0 3 3 3 3 3 2 2 2 2 2 1
F F F F   F F F F F F     F F     F F F

$ memsim sweep tests/fixtures/ac4_belady.wl --policy fifo --frames 1..6
1 12
2 12
3 9
4 10 *
5 5
6 5
```
