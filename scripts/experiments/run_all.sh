#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

# Run E1 placement policies experiment
./scripts/experiments/e1_placement.sh

# Run E4 faults vs frames experiment
./scripts/experiments/e4_frames.sh

# Generate report plots
python3 analysis/plot_e1.py
python3 analysis/plot_e4.py
python3 analysis/summarize.py

echo "[run_all] All experiments and report plots completed successfully."
