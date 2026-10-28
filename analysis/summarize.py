"""Aggregate the experiment CSVs over seeds into mean, std and 95% CI (FR-W4).

Writes results/e1/e1_summary.csv, results/e4/e4_summary.csv and Markdown tables for the report in
docs/report/tables/. Run after scripts/experiments/e1_placement.sh and e4_frames.sh.
"""

import csv
import os
from collections import defaultdict

from stats import summary

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TARGET_LABEL = {"low": "60%", "medium": "75%", "high": "90%"}
POLICY_LABEL = {"first-fit": "First-Fit", "best-fit": "Best-Fit", "worst-fit": "Worst-Fit",
                "fifo": "FIFO", "lru": "LRU", "opt": "OPT"}


def read(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def provenance(rows):
    shas = sorted({r.get("git_sha", "") for r in rows} - {""})
    return f"git {', '.join(shas) or 'unknown'}; {len({r.get('workload_sha256') for r in rows})} distinct workloads"


def e1():
    rows = read(os.path.join(ROOT, "results/e1/e1.csv"))
    groups = defaultdict(list)
    for r in rows:
        groups[(r["target"], r["policy"])].append(r)
    metrics = ["success_rate", "failures_fragmentation", "scanned_mean", "ext_frag_mean"]
    out = []
    for (target, pol), rs in sorted(groups.items(), key=lambda kv: (list(TARGET_LABEL).index(kv[0][0]), list(POLICY_LABEL).index(kv[0][1]))):
        row = {"target": target, "policy": pol}
        for m in metrics:
            n, mean, std, ci = summary([float(r[m]) for r in rs])
            row.update({"n": n, f"{m}_mean": mean, f"{m}_std": std, f"{m}_ci95": ci})
        out.append(row)
    write_csv(os.path.join(ROOT, "results/e1/e1_summary.csv"), out)
    md = ["| Utilisation | Policy | Success rate | Fragmentation failures | Holes scanned | Mean ext. fragmentation |",
          "| --- | --- | --- | --- | --- | --- |"]
    for r in out:
        md.append(f"| {TARGET_LABEL[r['target']]} | {POLICY_LABEL[r['policy']]} | "
                  f"{r['success_rate_mean'] * 100:.1f}% ± {r['success_rate_ci95'] * 100:.1f} | "
                  f"{r['failures_fragmentation_mean']:.1f} ± {r['failures_fragmentation_ci95']:.1f} | "
                  f"{r['scanned_mean_mean']:.2f} ± {r['scanned_mean_ci95']:.2f} | "
                  f"{r['ext_frag_mean_mean']:.3f} ± {r['ext_frag_mean_ci95']:.3f} |")
    note = f"\nMean ± 95% confidence interval over {out[0]['n']} seeds per cell ({provenance(rows)}).\n"
    write_md("e1_summary.md", "E1: placement policies under memory pressure", md, note)


def e4():
    rows = read(os.path.join(ROOT, "results/e4/e4.csv"))
    groups = defaultdict(list)
    for r in rows:
        groups[(r["policy"], int(r["frames"]))].append(float(r["faults"]))
    out = []
    for (pol, frames), vals in sorted(groups.items(), key=lambda kv: (list(POLICY_LABEL).index(kv[0][0]), kv[0][1])):
        n, mean, std, ci = summary(vals)
        out.append({"policy": pol, "frames": frames, "n": n, "faults_mean": mean, "faults_std": std, "faults_ci95": ci})
    write_csv(os.path.join(ROOT, "results/e4/e4_summary.csv"), out)
    shown = [2, 4, 8, 12, 16]
    by = {(r["policy"], r["frames"]): r for r in out}
    md = ["| Frames | " + " | ".join(POLICY_LABEL[p] for p in ("fifo", "lru", "opt")) + " |", "| --- | --- | --- | --- |"]
    for f in shown:
        md.append(f"| {f} | " + " | ".join(f"{by[(p, f)]['faults_mean']:.1f} ± {by[(p, f)]['faults_ci95']:.1f}" for p in ("fifo", "lru", "opt")) + " |")
    note = f"\nMean page faults ± 95% confidence interval over {out[0]['n']} seeds, 2,000 references each ({provenance(rows)}).\n"
    write_md("e4_summary.md", "E4: page faults against frames", md, note)


def write_csv(path, rows):
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]))
        w.writeheader()
        for r in rows:
            w.writerow({k: (f"{v:.6f}" if isinstance(v, float) else v) for k, v in r.items()})


def write_md(name, title, lines, note):
    os.makedirs(os.path.join(ROOT, "docs/report/tables"), exist_ok=True)
    with open(os.path.join(ROOT, "docs/report/tables", name), "w") as f:
        f.write(f"**{title}**\n\n" + "\n".join(lines) + "\n" + note)


if __name__ == "__main__":
    e1()
    e4()
    print("[summarize] wrote results/*/*_summary.csv and docs/report/tables/e1_summary.md, e4_summary.md")
