#!/usr/bin/env python3
"""E1 Placement Policies Plotting Script.

Generates:
1. Grouped bar chart of allocation success rate and fragmentation failure rate
   per target and policy with mean and 95% confidence-interval error bars over seeds.
2. Grouped bar chart of scanned_mean (search cost) per target and policy
   with mean and 95% confidence-interval error bars over seeds.

Output formats: PDF and PNG in report/figures/.
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

COLORS = {
    "first-fit": "#0072B2",
    "best-fit": "#009E73",
    "worst-fit": "#D55E00",
}

POLICY_LABELS = {
    "first-fit": "First-Fit",
    "best-fit": "Best-Fit",
    "worst-fit": "Worst-Fit",
}

TARGET_LABELS = {
    "low": "60% utilisation",
    "medium": "75% utilisation",
    "high": "90% utilisation",
}

TARGET_ORDER = ["low", "medium", "high"]
POLICY_ORDER = ["first-fit", "best-fit", "worst-fit"]


def load_data(csv_path):
    data = {}
    with open(csv_path, mode="r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            target = row["target"].strip().lower()
            pol = row["policy"].strip().lower()
            req = float(row["alloc_requests"])
            failures = float(row["alloc_failures"])
            frag_failures = float(row["failures_fragmentation"])
            scanned = float(row["scanned_mean"])

            success_rate = float(row["success_rate"]) if "success_rate" in row else (
                (1.0 - failures / req) if req > 0 else 1.0
            )
            frag_rate = (frag_failures / req) if req > 0 else 0.0

            key = (target, pol)
            if key not in data:
                data[key] = {
                    "success_rate": [],
                    "frag_rate": [],
                    "scanned_mean": [],
                }
            data[key]["success_rate"].append(success_rate)
            data[key]["frag_rate"].append(frag_rate)
            data[key]["scanned_mean"].append(scanned)
    return data


def compute_stats(values):
    if not values:
        return 0.0, 0.0, 0.0
    # Error bars are the 95% confidence interval of the mean over seeds (FR-W4).
    _, mean_val, _, ci = summary([float(v) for v in values])
    return mean_val, ci, ci


def plot_e1(csv_path, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    data = load_data(csv_path)

    x = np.arange(len(TARGET_ORDER))
    width = 0.25

    # ---------------------------------------------------------
    # Figure 1: Grouped bars for Success Rate and Frag-Failure Rate
    # ---------------------------------------------------------
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(6.5, 3.2))

    for p_idx, pol in enumerate(POLICY_ORDER):
        offset = (p_idx - 1) * width
        succ_means, succ_err_low, succ_err_high = [], [], []
        frag_means, frag_err_low, frag_err_high = [], [], []

        for target in TARGET_ORDER:
            key = (target, pol)
            s_list = data.get(key, {}).get("success_rate", [])
            f_list = data.get(key, {}).get("frag_rate", [])

            s_mean, s_low, s_high = compute_stats(s_list)
            succ_means.append(s_mean)
            succ_err_low.append(s_low)
            succ_err_high.append(s_high)

            f_mean, f_low, f_high = compute_stats(f_list)
            frag_means.append(f_mean)
            frag_err_low.append(f_low)
            frag_err_high.append(f_high)

        succ_yerr = np.array([succ_err_low, succ_err_high])
        frag_yerr = np.array([frag_err_low, frag_err_high])

        ax1.bar(
            x + offset,
            succ_means,
            width,
            yerr=succ_yerr,
            label=POLICY_LABELS[pol],
            color=COLORS[pol],
            capsize=3,
            error_kw={"elinewidth": 0.8, "capthick": 0.8},
            zorder=3,
        )

        ax2.bar(
            x + offset,
            frag_means,
            width,
            yerr=frag_yerr,
            label=POLICY_LABELS[pol],
            color=COLORS[pol],
            capsize=3,
            error_kw={"elinewidth": 0.8, "capthick": 0.8},
            zorder=3,
        )

    # Style ax1 (Success Rate)
    ax1.set_xlabel("Utilisation Target")
    ax1.set_ylabel("Allocation Success Rate")
    ax1.set_xticks(x)
    ax1.set_xticklabels([TARGET_LABELS[t] for t in TARGET_ORDER])
    ax1.set_ylim(0.0, 1.05)
    ax1.grid(True, axis="y", linestyle="--", alpha=0.5, color="#CCCCCC", zorder=0)
    ax1.legend(frameon=False, loc="lower left")

    # Style ax2 (Fragmentation Failure Rate)
    ax2.set_xlabel("Utilisation Target")
    ax2.set_ylabel("Fragmentation Failure Rate")
    ax2.set_xticks(x)
    ax2.set_xticklabels([TARGET_LABELS[t] for t in TARGET_ORDER])
    ax2.set_ylim(bottom=0.0)
    ax2.grid(True, axis="y", linestyle="--", alpha=0.5, color="#CCCCCC", zorder=0)

    fig.tight_layout()

    fig1_pdf = os.path.join(output_dir, "e1_placement.pdf")
    fig1_png = os.path.join(output_dir, "e1_placement.png")
    fig.savefig(fig1_pdf, bbox_inches="tight")
    fig.savefig(fig1_png, bbox_inches="tight", dpi=300)
    plt.close(fig)

    # ---------------------------------------------------------
    # Figure 2: Grouped bars for scanned_mean (search cost)
    # ---------------------------------------------------------
    fig2, ax = plt.subplots(figsize=(6.5, 3.2))

    for p_idx, pol in enumerate(POLICY_ORDER):
        offset = (p_idx - 1) * width
        scan_means, scan_err_low, scan_err_high = [], [], []

        for target in TARGET_ORDER:
            key = (target, pol)
            sc_list = data.get(key, {}).get("scanned_mean", [])
            sc_mean, sc_low, sc_high = compute_stats(sc_list)
            scan_means.append(sc_mean)
            scan_err_low.append(sc_low)
            scan_err_high.append(sc_high)

        scan_yerr = np.array([scan_err_low, scan_err_high])

        ax.bar(
            x + offset,
            scan_means,
            width,
            yerr=scan_yerr,
            label=POLICY_LABELS[pol],
            color=COLORS[pol],
            capsize=3,
            error_kw={"elinewidth": 0.8, "capthick": 0.8},
            zorder=3,
        )

    ax.set_xlabel("Utilisation Target")
    ax.set_ylabel("Mean Scanned Holes (holes / alloc)")
    ax.set_xticks(x)
    ax.set_xticklabels([TARGET_LABELS[t] for t in TARGET_ORDER])
    ax.set_ylim(bottom=0.0)
    ax.grid(True, axis="y", linestyle="--", alpha=0.5, color="#CCCCCC", zorder=0)
    ax.legend(frameon=False, loc="upper left")

    fig2.tight_layout()

    fig2_pdf = os.path.join(output_dir, "e1_scanned_mean.pdf")
    fig2_png = os.path.join(output_dir, "e1_scanned_mean.png")
    fig2.savefig(fig2_pdf, bbox_inches="tight")
    fig2.savefig(fig2_png, bbox_inches="tight", dpi=300)
    plt.close(fig2)

    print(f"[plot_e1] Generated {fig1_pdf}, {fig1_png}, {fig2_pdf}, {fig2_png}")


def main():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo_root, "results", "e1", "e1.csv")
    output_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(repo_root, "report", "figures")
    plot_e1(csv_path, output_dir)


if __name__ == "__main__":
    main()
