# Experimental Analysis and Reproducibility

This directory contains the plotting tools and documentation for reproducing the experimental results of **E1** (contiguous memory placement policies) and **E4** (paging frame sweeps and Bélady's anomaly).

---

## 1. Reproduction (One Command)

To run the complete experimental pipeline—executing all E1 and E4 workloads across seeds 1–10 and generating all publication-ready figures—run from the repository root:

```bash
./scripts/experiments/run_all.sh
```

By default, the scripts locate the simulator binary at `build/release/bin/memsim`. To use a binary located elsewhere, specify `$MEMSIM`:

```bash
MEMSIM=/path/to/memsim ./scripts/experiments/run_all.sh
```

Individual experiments or plots can also be executed independently:
- **E1 Placement experiment**: `./scripts/experiments/e1_placement.sh`
- **E4 Frames experiment**: `./scripts/experiments/e4_frames.sh`
- **E1 Plots**: `python3 analysis/plot_e1.py`
- **E4 Plots**: `python3 analysis/plot_e4.py`

---

## 2. CSV Column Definitions

All generated datasets are written deterministically and idempotently to the `results/` directory.

### `results/e1/e1.csv`
Evaluates contiguous placement policies (`first-fit`, `best-fit`, `worst-fit`) under uniform allocation workloads across three utilisation targets:

| Column | Type | Description |
|---|---|---|
| `target` | string | Utilisation the generator holds memory at: `low` 60%, `medium` 75%, `high` 90% (sizes 16..128 throughout). |
| `seed` | integer | Workload generator pseudo-random seed (1..10). |
| `policy` | string | Placement policy evaluated (`first-fit`, `best-fit`, `worst-fit`). |
| `alloc_requests` | integer | Total number of allocation requests attempted during the simulation. |
| `alloc_failures` | integer | Total count of failed allocation requests. |
| `failures_fragmentation` | integer | Allocation failures attributable to external fragmentation (sufficient total free capacity existed across memory, but no single contiguous hole was large enough). |
| `failures_capacity` | integer | Allocation failures attributable to total capacity exhaustion. |
| `success_rate` | float | Proportion of successful allocations: $1 - \frac{\text{alloc\_failures}}{\text{alloc\_requests}}$. |
| `scanned_mean` | float | Mean number of free holes inspected per allocation request (search cost). |
| `ext_frag_mean` | float | Time-step-weighted mean external fragmentation ratio ($1 - \frac{\text{largest\_hole\_size}}{\text{total\_free\_bytes}}$). |

### `results/e4/e4.csv`
Evaluates virtual memory paging replacement policies (`fifo`, `lru`, `opt`) over 1 to 16 allocated frames on locality-driven reference strings:

| Column | Type | Description |
|---|---|---|
| `seed` | integer | Reference locality generator seed (1..10). |
| `policy` | string | Page replacement policy (`fifo`, `lru`, `opt`). |
| `frames` | integer | Number of physical page frames allocated to the engine ($1 \le f \le 16$). |
| `faults` | integer | Total count of page faults incurred over the 2,000 reference accesses. |
| `belady_point` | integer (0/1) | Binary indicator: `1` if this frame count is identified as an anomaly point where faults increased relative to the previous frame count, `0` otherwise. |

### `results/e4/belady_ac4.csv`
Contains the frame sweep (1 to 6 frames) of the canonical Bélady AC-4 reference string (`1 2 3 4 1 2 5 1 2 3 4 5`) under FIFO:

| Column | Type | Description |
|---|---|---|
| `policy` | string | Paging replacement policy (`fifo`). |
| `frames` | integer | Frame count evaluated ($1 \le f \le 6$). |
| `faults` | integer | Number of page faults observed. |
| `belady_point` | integer (0/1) | Binary indicator: `1` at the anomaly point where increasing frames increases faults, `0` otherwise. |

---

## 3. Report Figures and Research Questions

Generated figures are saved in vector (`.pdf`) and high-resolution raster (`.png`, 300 dpi) formats in `report/figures/`. All figures follow academic formatting guidelines (white background, no embedded titles, labelled axes with units, unframed legends, colour-blind-safe Wong palette, 6.5×3.2 in, 10pt font).

### `e1_placement.{pdf,png}`
- **Research Question**: *Which contiguous placement policy (First-Fit, Best-Fit, or Worst-Fit) achieves the highest allocation success rate and lowest fragmentation-induced failure rate under varying memory pressure?*
- **Contents**: Two-panel grouped bar chart showing:
  1. Mean allocation success rate with 95% confidence-interval error bars across 10 random seeds for Low, Medium, and High utilisation targets.
  2. Mean fragmentation failure rate with 95% confidence-interval error bars across the same conditions.

### `e1_scanned_mean.{pdf,png}`
- **Research Question**: *What is the search overhead (computational cost of hole examination) imposed by each placement policy across different memory pressure loads?*
- **Contents**: Grouped bar chart depicting the mean number of holes scanned per allocation request (`scanned_mean`) with 95% confidence-interval error bars across seeds.

### `e4_faults_vs_frames.{pdf,png}`
- **Research Question**: *How do FIFO, LRU, and OPT compare in miss ratios as available physical memory (frames 1–16) expands under locality-of-reference workloads?*
- **Contents**: Line chart with distinct markers plotting mean page faults versus frame count for FIFO, LRU, and OPT over 10 random seeds, illustrating the theoretical optimality lower bound of OPT and the close approximation provided by LRU.

### `e4_belady_ac4.{pdf,png}`
- **Research Question**: *Does the First-In, First-Out (FIFO) replacement policy exhibit Bélady's anomaly on the canonical reference sequence, and where does the anomaly manifest?*
- **Contents**: Line plot of page faults against frame count (1 to 6) for the 12-reference sequence `1 2 3 4 1 2 5 1 2 3 4 5`, featuring an annotated callout at the Bélady anomaly point (3 → 4 frames: 9 → 10 faults).

## Summary tables and provenance

`analysis/summarize.py` (run last by `run_all.sh`) aggregates each condition over its seeds into mean, sample standard deviation and the 95% confidence interval of the mean (Student t), writing `results/e1/e1_summary.csv`, `results/e4/e4_summary.csv` and the report tables `report/tables/e1_summary.md` and `report/tables/e4_summary.md`. Every row of `e1.csv` and `e4.csv` records the SHA-256 of its workload file and the git commit that produced it, so each figure and table traces back to exact inputs and code (NFR-6).
