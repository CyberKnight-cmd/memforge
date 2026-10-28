#!/usr/bin/env bash
# scripts/demo/build_scenarios.sh: Regenerates the viewer demo scenario data from real simulation runs.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MEMSIM="${MEMSIM:-build/release/bin/memsim}"
if [[ "$MEMSIM" != /* ]]; then
  MEMSIM="$REPO_ROOT/$MEMSIM"
fi

if [[ ! -x "$MEMSIM" ]]; then
  echo "Error: memsim executable not found: $MEMSIM" >&2
  exit 1
fi

TMPDIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMPDIR"
}
trap cleanup EXIT

# 1. AC-1 traces with first-fit, best-fit, worst-fit
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac1_silberschatz.wl" --policy first-fit --trace "$TMPDIR/ac1_first-fit.jsonl"
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac1_silberschatz.wl" --policy best-fit --trace "$TMPDIR/ac1_best-fit.jsonl"
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac1_silberschatz.wl" --policy worst-fit --trace "$TMPDIR/ac1_worst-fit.jsonl"

# 2. AC-2 trace with first-fit
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac2_compact.wl" --policy first-fit --trace "$TMPDIR/ac2_first-fit.jsonl"

# 3. AC-3 traces with fifo, lru, opt
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac3_refs.wl" --policy fifo --trace "$TMPDIR/ac3_fifo.jsonl"
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac3_refs.wl" --policy lru --trace "$TMPDIR/ac3_lru.jsonl"
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac3_refs.wl" --policy opt --trace "$TMPDIR/ac3_opt.jsonl"

# 4. AC-4 traces with fifo at frames 3 and frames 4
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy fifo --trace "$TMPDIR/ac4_fifo_f3.jsonl"
sed 's/^frames 3/frames 4/' "$REPO_ROOT/tests/fixtures/ac4_belady.wl" > "$TMPDIR/ac4_f4.wl"
"$MEMSIM" run "$TMPDIR/ac4_f4.wl" --policy fifo --trace "$TMPDIR/ac4_fifo_f4.jsonl"

# 5. AC-4 sweeps for fifo, lru, opt over frames 1..6
"$MEMSIM" sweep "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy fifo --frames 1..6 --json > "$TMPDIR/sweep_fifo.json"
"$MEMSIM" sweep "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy lru --frames 1..6 --json > "$TMPDIR/sweep_lru.json"
"$MEMSIM" sweep "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy opt --frames 1..6 --json > "$TMPDIR/sweep_opt.json"

# 6. Generate viewer/scenarios-data.js
OUT_FILE="$REPO_ROOT/viewer/scenarios-data.js"
python3 - "$TMPDIR" "$OUT_FILE" <<'EOF'
import json
import os
import sys

tmpdir = sys.argv[1]
output_file = sys.argv[2]

trace_keys = [
    ("ac1_first-fit", "ac1_first-fit.jsonl"),
    ("ac1_best-fit", "ac1_best-fit.jsonl"),
    ("ac1_worst-fit", "ac1_worst-fit.jsonl"),
    ("ac2_first-fit", "ac2_first-fit.jsonl"),
    ("ac3_fifo", "ac3_fifo.jsonl"),
    ("ac3_lru", "ac3_lru.jsonl"),
    ("ac3_opt", "ac3_opt.jsonl"),
    ("ac4_fifo_f3", "ac4_fifo_f3.jsonl"),
    ("ac4_fifo_f4", "ac4_fifo_f4.jsonl"),
]

traces = {}
for key, filename in trace_keys:
    filepath = os.path.join(tmpdir, filename)
    with open(filepath, "r", encoding="utf-8") as f:
        traces[key] = f.read()

sweeps = {}
for policy in ["fifo", "lru", "opt"]:
    filepath = os.path.join(tmpdir, f"sweep_{policy}.json")
    with open(filepath, "r", encoding="utf-8") as f:
        sweeps[policy] = json.load(f)

payload = {
    "traces": traces,
    "sweeps": sweeps,
}

os.makedirs(os.path.dirname(os.path.abspath(output_file)), exist_ok=True)
with open(output_file, "w", encoding="utf-8") as f:
    f.write("window.MEMFORGE_SCENARIOS = " + json.dumps(payload) + ";\n")
EOF

# 7. Run node viewer/build.js if node exists
if command -v node >/dev/null 2>&1 && [[ -f "$REPO_ROOT/viewer/build.js" ]]; then
  (cd "$REPO_ROOT" && node viewer/build.js)
fi
