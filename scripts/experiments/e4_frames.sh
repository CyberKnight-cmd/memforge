#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

if [[ -n "${MEMSIM:-}" ]]; then
    if [[ "$MEMSIM" != /* ]]; then
        MEMSIM="$PWD/$MEMSIM"
    fi
else
    MEMSIM="$REPO_ROOT/build/release/bin/memsim"
fi

RESULTS_DIR="results/e4"
WORKLOADS_DIR="$RESULTS_DIR/workloads"
CSV_FILE="$RESULTS_DIR/e4.csv"
AC4_CSV_FILE="$RESULTS_DIR/belady_ac4.csv"

mkdir -p "$WORKLOADS_DIR"

echo "seed,policy,frames,faults,belady_point" > "$CSV_FILE"

POLICIES=("fifo" "lru" "opt")

# Generate reference locality workloads for seeds 1..10
for seed in $(seq 1 10); do
    wl_file="$WORKLOADS_DIR/refs_seed_${seed}.txt"
    "$MEMSIM" gen refs-locality \
        --seed "$seed" \
        --n 2000 \
        --pages 32 \
        --working-set 6 \
        --shift-every 200 \
        --p-out-ppm 50000 \
        --frames 16 \
        --page-size 4096 \
        -o "$wl_file"
done

# Run sweeps for each policy across all seeds
for policy in "${POLICIES[@]}"; do
    echo "[E4] Policy: $policy"
    for seed in $(seq 1 10); do
        wl_file="$WORKLOADS_DIR/refs_seed_${seed}.txt"
        json_output="$("$MEMSIM" sweep "$wl_file" --policy "$policy" --frames 1..16 --json)"

        python3 -c '
import json, sys

seed = sys.argv[1]
raw = sys.stdin.read().strip()
if not raw:
    sys.exit(0)

data = json.loads(raw)
pol = data.get("policy", "")
frames = data.get("frames", [])
faults = data.get("faults", [])
bp_set = set(data.get("belady_points", []))

for i, frame in enumerate(frames):
    fault = faults[i] if i < len(faults) else 0
    is_bp = 1 if (frame in bp_set) else 0
    print(f"{seed},{pol},{frame},{fault},{is_bp}")
' "$seed" <<< "$json_output" >> "$CSV_FILE"
    done
done

# Sweep AC-4 reference string under FIFO (frames 1..6)
echo "[E4] Policy: fifo (AC-4)"
echo "policy,frames,faults,belady_point" > "$AC4_CSV_FILE"

ac4_file="$WORKLOADS_DIR/ac4.txt"
cat << 'EOF' > "$ac4_file"
schema 1
kind paging
page_size 4096
frames 6
spawn P0 16
access P0 1
access P0 2
access P0 3
access P0 4
access P0 1
access P0 2
access P0 5
access P0 1
access P0 2
access P0 3
access P0 4
access P0 5
terminate P0
EOF

json_ac4="$("$MEMSIM" sweep "$ac4_file" --policy fifo --frames 1..6 --json)"

python3 -c '
import json, sys

raw = sys.stdin.read().strip()
if not raw:
    sys.exit(0)

data = json.loads(raw)
pol = data.get("policy", "fifo")
frames = data.get("frames", [])
faults = data.get("faults", [])
bp_set = set(data.get("belady_points", []))

for i, frame in enumerate(frames):
    fault = faults[i] if i < len(faults) else 0
    if bp_set:
        is_bp = 1 if (frame in bp_set) else 0
    else:
        is_bp = 1 if (i > 0 and faults[i] > faults[i - 1]) else 0
    print(f"{pol},{frame},{fault},{is_bp}")
' <<< "$json_ac4" >> "$AC4_CSV_FILE"
