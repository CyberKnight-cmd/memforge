#!/usr/bin/env python3
"""E4 Faults vs Frames Plotting Script.

Generates:
1. Line plot of mean page faults vs allocated frames (1..16) per replacement
   policy (FIFO, LRU, OPT) with markers.
2. Separate figure of the AC-4 reference string under FIFO (1..6 frames)
   with Bélady's anomaly explicitly annotated.

Output formats: PDF and PNG in docs/report/figures/.
Academic style: white background, no titles inside figures, labelled axes with
units, legend without frame, colour-blind-safe colours (#0072B2, #009E73, #D55E00),
6.5x3.2 inch, 10pt fonts.
"""

import csv
import os
import sys

from stats import summary  # noqa: E402  (analysis/ is on sys.path when run as a script)
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

# Configure clean academic styling
matplotlib.rcParams.update({
    "font.size": 10,
    "axes.labelsize": 10,
    "axes.titlesize": 10,
    "xtick.labelsize": 10,
    "ytick.labelsize": 10,
    "legend.fontsize": 10,
    "figure.titlesize": 10,
    "figure.facecolor": "white",
    "axes.facecolor": "white",
    "savefig.facecolor": "white",
})

POLICY_CONFIG = {
    "fifo": {
        "label": "FIFO",
        "color": "#0072B2",
        "marker": "o",
    },
    "lru": {
        "label": "LRU",
        "color": "#009E73",
        "marker": "s",
    },
    "opt": {
        "label": "OPT",
        "color": "#D55E00",
        "marker": "^",
    },
}

POLICY_ORDER = ["fifo", "lru", "opt"]


def load_e4_data(csv_path):
    # Returns {policy: {frame: [faults_seed1, faults_seed2, ...]}}
    data = {}
    with open(csv_path, mode="r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pol = row["policy"].strip().lower()
            frame = int(row["frames"])
            faults = float(row["faults"])

            if pol not in data:
                data[pol] = {}
            if frame not in data[pol]:
                data[pol][frame] = []
            data[pol][frame].append(faults)
    return data


def load_ac4_data(csv_path):
    # Returns (frames, faults, belady_indices)
    frames = []
    faults = []
    belady_flags = []
    with open(csv_path, mode="r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            frames.append(int(row["frames"]))
            faults.append(float(row["faults"]))
            bp_val = row.get("belady_point", row.get("belady_point(0/1)", "0"))
            belady_flags.append(int(bp_val))
    return frames, faults, belady_flags


def plot_e4(e4_csv, ac4_csv, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    # ---------------------------------------------------------
    # Figure 1: Mean Faults vs Frames per Policy
    # ---------------------------------------------------------
    data = load_e4_data(e4_csv)
    fig1, ax1 = plt.subplots(figsize=(6.5, 3.2))

    for pol in POLICY_ORDER:
        if pol not in data:
            continue
        cfg = POLICY_CONFIG[pol]
        frames_sorted = sorted(data[pol].keys())
        stats = [summary([float(v) for v in data[pol][f]]) for f in frames_sorted]
        mean_faults = [s[1] for s in stats]
        ci = [s[3] for s in stats]
        # 95% confidence band of the mean over seeds (FR-W4).
        ax1.fill_between(frames_sorted, [m - c for m, c in zip(mean_faults, ci)], [m + c for m, c in zip(mean_faults, ci)],
                         color=cfg["color"], alpha=0.15, linewidth=0, zorder=2)

        ax1.plot(
            frames_sorted,
            mean_faults,
            label=cfg["label"],
            color=cfg["color"],
            marker=cfg["marker"],
            linewidth=1.5,
            markersize=5,
            zorder=3,
        )

    ax1.set_xlabel("Allocated Frames (frames)")
    ax1.set_ylabel("Mean Page Faults (faults)")
    ax1.set_xticks(range(1, 17))
    ax1.set_xlim(0.5, 16.5)
    ax1.set_ylim(bottom=0.0)
    ax1.grid(True, linestyle="--", alpha=0.5, color="#CCCCCC", zorder=0)
    ax1.legend(frameon=False, loc="upper right")

    fig1.tight_layout()

    fig1_pdf = os.path.join(output_dir, "e4_faults_vs_frames.pdf")
    fig1_png = os.path.join(output_dir, "e4_faults_vs_frames.png")
    fig1.savefig(fig1_pdf, bbox_inches="tight")
    fig1.savefig(fig1_png, bbox_inches="tight", dpi=300)
    plt.close(fig1)

    # ---------------------------------------------------------
    # Figure 2: AC-4 FIFO Curve with Belady Point Annotated
    # ---------------------------------------------------------
    frames, faults, bp_flags = load_ac4_data(ac4_csv)
    fig2, ax2 = plt.subplots(figsize=(6.5, 3.2))

    cfg_fifo = POLICY_CONFIG["fifo"]
    ax2.plot(
        frames,
        faults,
        label=cfg_fifo["label"],
        color=cfg_fifo["color"],
        marker=cfg_fifo["marker"],
        linewidth=1.5,
        markersize=6,
        zorder=3,
    )

    # Detect Belady anomaly: frame where faults strictly increase compared to previous frame
    anomaly_frame = None
    anomaly_faults = None
    prev_faults = None

    for i in range(1, len(frames)):
        if faults[i] > faults[i - 1]:
            anomaly_frame = frames[i]
            anomaly_faults = faults[i]
            prev_faults = faults[i - 1]
            break

    # If not detected by diff, check explicit belady_flags
    if anomaly_frame is None:
        for i, flag in enumerate(bp_flags):
            if flag == 1:
                anomaly_frame = frames[i]
                anomaly_faults = faults[i]
                prev_faults = faults[i - 1] if i > 0 else faults[i]
                break

    if anomaly_frame is not None:
        # Highlight anomaly point with a vermillion outer ring
        ax2.plot(
            [anomaly_frame],
            [anomaly_faults],
            marker="o",
            markersize=9,
            color="#D55E00",
            fillstyle="none",
            markeredgewidth=2.0,
            zorder=4,
        )
        prev_f_int = int(anomaly_frame - 1)
        anom_f_int = int(anomaly_frame)
        prev_val_int = int(prev_faults)
        anom_val_int = int(anomaly_faults)
        annotation_text = (
            f"Bélady's anomaly\n"
            f"({prev_f_int} \u2192 {anom_f_int} frames: {prev_val_int} \u2192 {anom_val_int} faults)"
        )
        ax2.annotate(
            annotation_text,
            xy=(anomaly_frame, anomaly_faults),
            xytext=(anomaly_frame + 0.35, anomaly_faults + 0.7),
            arrowprops=dict(arrowstyle="->", color="#333333", lw=1.0),
            fontsize=9,
            zorder=5,
        )

    ax2.set_xlabel("Allocated Frames (frames)")
    ax2.set_ylabel("Page Faults (faults)")
    if frames:
        ax2.set_xticks(frames)
        ax2.set_xlim(min(frames) - 0.5, max(frames) + 0.8)
        max_fault = max(faults) if faults else 14
        ax2.set_ylim(0.0, max_fault + 2.5)
    ax2.grid(True, linestyle="--", alpha=0.5, color="#CCCCCC", zorder=0)
    ax2.legend(frameon=False, loc="upper right")

    fig2.tight_layout()

    fig2_pdf = os.path.join(output_dir, "e4_belady_ac4.pdf")
    fig2_png = os.path.join(output_dir, "e4_belady_ac4.png")
    fig2.savefig(fig2_pdf, bbox_inches="tight")
    fig2.savefig(fig2_png, bbox_inches="tight", dpi=300)
    plt.close(fig2)

    print(f"[plot_e4] Generated {fig1_pdf}, {fig1_png}, {fig2_pdf}, {fig2_png}")


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    e4_csv = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo_root, "results", "e4", "e4.csv")
    ac4_csv = sys.argv[2] if len(sys.argv) > 2 else os.path.join(repo_root, "results", "e4", "belady_ac4.csv")
    output_dir = sys.argv[3] if len(sys.argv) > 3 else os.path.join(repo_root, "docs", "report", "figures")
    plot_e4(e4_csv, ac4_csv, output_dir)


if __name__ == "__main__":
    main()
