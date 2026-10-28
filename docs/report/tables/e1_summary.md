**E1: placement policies under memory pressure**

| Utilisation | Policy | Success rate | Fragmentation failures | Holes scanned | Mean ext. fragmentation |
| --- | --- | --- | --- | --- | --- |
| 60% | First-Fit | 99.9% ± 0.0 | 1.2 ± 0.7 | 2.29 ± 0.05 | 0.359 ± 0.007 |
| 60% | Best-Fit | 100.0% ± 0.0 | 0.3 ± 0.3 | 3.77 ± 0.07 | 0.322 ± 0.006 |
| 60% | Worst-Fit | 99.0% ± 0.2 | 14.3 ± 3.1 | 3.88 ± 0.09 | 0.546 ± 0.014 |
| 75% | First-Fit | 97.0% ± 0.3 | 44.6 ± 4.7 | 2.78 ± 0.04 | 0.460 ± 0.013 |
| 75% | Best-Fit | 97.9% ± 0.2 | 32.0 ± 2.8 | 4.66 ± 0.13 | 0.419 ± 0.012 |
| 75% | Worst-Fit | 93.9% ± 0.5 | 92.4 ± 6.8 | 4.61 ± 0.11 | 0.597 ± 0.010 |
| 90% | First-Fit | 90.0% ± 0.6 | 148.6 ± 8.6 | 3.21 ± 0.09 | 0.516 ± 0.014 |
| 90% | Best-Fit | 91.4% ± 0.4 | 127.7 ± 7.0 | 5.24 ± 0.15 | 0.473 ± 0.006 |
| 90% | Worst-Fit | 87.2% ± 0.7 | 192.0 ± 11.1 | 5.19 ± 0.15 | 0.629 ± 0.010 |

Mean ± 95% confidence interval over 10 seeds per cell (git 53039c8; 30 distinct workloads).
