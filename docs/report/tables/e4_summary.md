**E4: page faults against frames**

| Frames | FIFO | LRU | OPT |
| --- | --- | --- | --- |
| 2 | 1398.2 ± 21.2 | 1393.0 ± 18.8 | 1070.8 ± 13.9 |
| 4 | 840.0 ± 15.9 | 819.6 ± 19.9 | 466.0 ± 9.3 |
| 8 | 253.1 ± 16.5 | 138.9 ± 10.1 | 103.2 ± 5.9 |
| 12 | 151.7 ± 9.7 | 100.7 ± 6.0 | 70.7 ± 4.0 |
| 16 | 106.7 ± 9.0 | 83.5 ± 5.5 | 53.2 ± 3.2 |

Mean page faults ± 95% confidence interval over 10 seeds, 2,000 references each (git 53039c8; 10 distinct workloads).
