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

RESULTS_DIR="results/e1"
WORKLOADS_DIR="$RESULTS_DIR/workloads"
CSV_FILE="$RESULTS_DIR/e1.csv"

mkdir -p "$WORKLOADS_DIR"

echo "target,seed,policy,alloc_requests,alloc_failures,failures_fragmentation,failures_capacity,success_rate,scanned_mean,ext_frag_mean" > "$CSV_FILE"

TARGETS=("low" "medium" "high")

for target in "${TARGETS[@]}"; do
    echo "[E1] Target: $target"
    # Section 13: hold emitted utilisation at 0.6, 0.75 or 0.9 of memory; sizes 16..128 throughout.
    case "$target" in
        low) util_ppm=600000 ;;
        medium) util_ppm=750000 ;;
        high) util_ppm=900000 ;;
        *)
            echo "Unknown target: $target" >&2
            exit 1
            ;;
    esac

    for seed in $(seq 1 10); do
        wl_file="$WORKLOADS_DIR/alloc_${target}_seed${seed}.txt"

        "$MEMSIM" gen alloc-uniform \
            --seed "$seed" \
            --ops 3000 \
            --memory-size 1024 \
            --size-min 16 \
            --size-max 128 \
            --p-alloc-ppm 600000 \
            --target-util-ppm "$util_ppm" \
            -o "$wl_file"

        json_output="$("$MEMSIM" compare "$wl_file" --policies first-fit,best-fit,worst-fit --json)"

        python3 -c '
import json, sys

target = sys.argv[1]
seed = sys.argv[2]
raw = sys.stdin.read().strip()
if not raw:
    sys.exit(0)

data = json.loads(raw)
policies = data.get("policies", [])
metrics = data.get("metrics", {})

def get_metric(name, idx, pol):
    val_list = metrics.get(name, [])
    if isinstance(val_list, list):
        if idx < len(val_list):
            return val_list[idx]
        return 0
    if isinstance(val_list, dict):
        return val_list.get(pol, 0)
    return 0

for i, pol in enumerate(policies):
    req = int(round(float(get_metric("alloc_requests", i, pol))))
    fail = int(round(float(get_metric("alloc_failures", i, pol))))
    frag_fail = int(round(float(get_metric("failures_fragmentation", i, pol))))
    cap_fail = int(round(float(get_metric("failures_capacity", i, pol))))
    succ = (1.0 - (fail / req)) if req > 0 else 1.0
    scanned = float(get_metric("scanned_mean", i, pol))
    ext_frag = float(get_metric("ext_frag_mean", i, pol))
    print(f"{target},{seed},{pol},{req},{fail},{frag_fail},{cap_fail},{succ:.6f},{scanned:.6f},{ext_frag:.6f}")
' "$target" "$seed" <<< "$json_output" >> "$CSV_FILE"
    done
done
