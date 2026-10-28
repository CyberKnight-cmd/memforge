#!/usr/bin/env bash
# scripts/report/verify_report.sh: Runs test suite and checks all acceptance criteria, writing the verification report.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MEMSIM="${MEMSIM:-build/release/bin/memsim}"
if [[ "$MEMSIM" != /* ]]; then
  MEMSIM="$REPO_ROOT/$MEMSIM"
fi

TMPDIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMPDIR"
}
trap cleanup EXIT

# 1. Run ctest --preset dev (configure/build dev first if build/dev is missing)
if [[ ! -d "$REPO_ROOT/build/dev" ]]; then
  cmake -S "$REPO_ROOT" --preset dev
  cmake --build --preset dev
fi

(cd "$REPO_ROOT" && ctest --preset dev 2>&1) > "$TMPDIR/ctest.log" || true

# 2. Check memsim binary
if [[ ! -x "$MEMSIM" ]]; then
  echo "Error: memsim binary not found or not executable: $MEMSIM" >&2
  exit 1
fi

# 3. AC-1: Placement on five holes (compare ac1 first-fit, best-fit, worst-fit)
"$MEMSIM" compare "$REPO_ROOT/tests/fixtures/ac1_silberschatz.wl" --policies first-fit,best-fit,worst-fit --json > "$TMPDIR/ac1_compare.json" 2>&1 || true

# 4. AC-2: Compaction and retry (run ac2 first-fit)
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac2_compact.wl" --policy first-fit --trace "$TMPDIR/ac2_trace.jsonl" --json > "$TMPDIR/ac2_run.json" 2>&1 || true

# 5. AC-3: Reference string page fault counts
"$MEMSIM" refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --frames 3 --policy fifo --json > "$TMPDIR/ac3_fifo.json" 2>&1 || true
"$MEMSIM" refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --frames 3 --policy lru --json > "$TMPDIR/ac3_lru.json" 2>&1 || true
"$MEMSIM" refs "7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1" --frames 3 --policy opt --json > "$TMPDIR/ac3_opt.json" 2>&1 || true

# 6. AC-4: Belady's anomaly frame sweep
"$MEMSIM" sweep "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy fifo --frames 1..6 --json > "$TMPDIR/ac4_fifo.json" 2>&1 || true
"$MEMSIM" sweep "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy lru --frames 1..6 --json > "$TMPDIR/ac4_lru.json" 2>&1 || true
"$MEMSIM" sweep "$REPO_ROOT/tests/fixtures/ac4_belady.wl" --policy opt --frames 1..6 --json > "$TMPDIR/ac4_opt.json" 2>&1 || true

# 7. AC-5: Negative tests (rejections and access violations)
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac5_contiguous.wl" --policy first-fit --trace "$TMPDIR/ac5_c_trace.jsonl" --json > "$TMPDIR/ac5_contiguous.json" 2>&1 || true
"$MEMSIM" run "$REPO_ROOT/tests/fixtures/ac5_paging.wl" --policy fifo --trace "$TMPDIR/ac5_p_trace.jsonl" --json > "$TMPDIR/ac5_paging.json" 2>&1 || true

# 8. AC-6: Determinism across two runs for each fixture
ac6_matches=0
fixtures=(
  "tests/fixtures/ac1_silberschatz.wl:first-fit"
  "tests/fixtures/ac2_compact.wl:first-fit"
  "tests/fixtures/ac3_refs.wl:fifo"
  "tests/fixtures/ac4_belady.wl:fifo"
  "tests/fixtures/ac5_contiguous.wl:first-fit"
  "tests/fixtures/ac5_paging.wl:fifo"
)
for item in "${fixtures[@]}"; do
  fix="${item%%:*}"
  pol="${item##*:}"
  base="$(basename "$fix" .wl)"
  "$MEMSIM" run "$REPO_ROOT/$fix" --policy "$pol" --trace "$TMPDIR/ac6_${base}_1.jsonl" >/dev/null 2>&1 || true
  "$MEMSIM" run "$REPO_ROOT/$fix" --policy "$pol" --trace "$TMPDIR/ac6_${base}_2.jsonl" >/dev/null 2>&1 || true
  if cmp -s "$TMPDIR/ac6_${base}_1.jsonl" "$TMPDIR/ac6_${base}_2.jsonl"; then
    ac6_matches=$((ac6_matches + 1))
  fi
done
echo "$ac6_matches" > "$TMPDIR/ac6_cmp.txt"

# 9. AC-8: compare ac1 human-readable table contains "fail FRAGMENTATION"
"$MEMSIM" compare "$REPO_ROOT/tests/fixtures/ac1_silberschatz.wl" --policies first-fit,best-fit,worst-fit > "$TMPDIR/ac8_compare.txt" 2>&1 || true

# 10. Process results and write report tables
GIT_COMMIT="$(git -C "$REPO_ROOT" rev-parse --short HEAD)"

python3 - "$TMPDIR" "$REPO_ROOT" "$GIT_COMMIT" <<'EOF'
import csv
import json
import os
import re
import sys

tmpdir = sys.argv[1]
repo_root = sys.argv[2]
git_commit = sys.argv[3]
ctest_log_path = os.path.join(tmpdir, "ctest.log")

# Parse ctest results
ctest_passed = 0
ctest_total = 0
if os.path.exists(ctest_log_path):
    with open(ctest_log_path, "r", encoding="utf-8") as f:
        ctest_text = f.read()
    ctest_passed = len(re.findall(r"\bTest\s+#\d+:.*?\bPassed\b", ctest_text))
    m = re.search(r"tests passed out of (\d+)", ctest_text)
    if m:
        ctest_total = int(m.group(1))
    else:
        ctest_total = len(re.findall(r"\bTest\s+#\d+:", ctest_text))
    if ctest_total == 0 and ctest_passed > 0:
        ctest_total = ctest_passed

# AC-1
ac1_exp = "FF 110,1140,322,fail FRAGMENTATION; BF 830,110,620,1140; WF 1140,110,1352,fail FRAGMENTATION"
try:
    with open(os.path.join(tmpdir, "ac1_compare.json"), "r", encoding="utf-8") as f:
        ac1_data = json.load(f)
    pols = [p.lower() for p in ac1_data.get("policies", [])]
    ff_idx = pols.index("first-fit") if "first-fit" in pols else 0
    bf_idx = pols.index("best-fit") if "best-fit" in pols else 1
    wf_idx = pols.index("worst-fit") if "worst-fit" in pols else 2

    placements = ac1_data.get("placements", [])
    id_map = {p.get("id"): p.get("results", []) for p in placements}
    req_order = ["a1", "a2", "a3", "a4"]
    if all(k in id_map for k in req_order):
        ff_res = [str(id_map[k][ff_idx]) for k in req_order]
        bf_res = [str(id_map[k][bf_idx]) for k in req_order]
        wf_res = [str(id_map[k][wf_idx]) for k in req_order]
    else:
        ff_res = [str(p.get("results", [])[ff_idx]) for p in placements]
        bf_res = [str(p.get("results", [])[bf_idx]) for p in placements]
        wf_res = [str(p.get("results", [])[wf_idx]) for p in placements]

    ac1_obs = f"FF {','.join(ff_res)}; BF {','.join(bf_res)}; WF {','.join(wf_res)}"
    ac1_pass = (
        ff_res == ["110", "1140", "322", "fail FRAGMENTATION"]
        and bf_res == ["830", "110", "620", "1140"]
        and wf_res == ["1140", "110", "1352", "fail FRAGMENTATION"]
    )
except Exception as e:
    ac1_obs = f"Error: {e}"
    ac1_pass = False

# AC-2
ac2_exp = "blocks_moved 7, units_moved 781; free block start 1207 size 533"
try:
    with open(os.path.join(tmpdir, "ac2_run.json"), "r", encoding="utf-8") as f:
        ac2_meta = json.load(f)
    metrics = ac2_meta.get("metrics", {})
    bm = metrics.get("blocks_moved")
    um = metrics.get("units_moved")

    last_snap = None
    trace_file = os.path.join(tmpdir, "ac2_trace.jsonl")
    if os.path.exists(trace_file):
        with open(trace_file, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    obj = json.loads(line)
                    if obj.get("type") == "Snapshot":
                        last_snap = obj
                except Exception:
                    pass

    free_start = None
    free_size = None
    if last_snap and "contiguous" in last_snap:
        blocks = last_snap["contiguous"].get("blocks", [])
        for b in blocks:
            if b.get("state") == "free":
                free_start = b.get("start")
                free_size = b.get("size")

    ac2_obs = f"blocks_moved {bm}, units_moved {um}; free block start {free_start} size {free_size}"
    ac2_pass = (bm == 7 and um == 781 and free_start == 1207 and free_size == 533)
except Exception as e:
    ac2_obs = f"Error: {e}"
    ac2_pass = False

# AC-3
ac3_exp = "fifo 15, lru 12, opt 9"
try:
    with open(os.path.join(tmpdir, "ac3_fifo.json"), "r", encoding="utf-8") as f:
        fifo_f = json.load(f).get("faults")
    with open(os.path.join(tmpdir, "ac3_lru.json"), "r", encoding="utf-8") as f:
        lru_f = json.load(f).get("faults")
    with open(os.path.join(tmpdir, "ac3_opt.json"), "r", encoding="utf-8") as f:
        opt_f = json.load(f).get("faults")

    ac3_obs = f"fifo {fifo_f}, lru {lru_f}, opt {opt_f}"
    ac3_pass = (fifo_f == 15 and lru_f == 12 and opt_f == 9)
except Exception as e:
    ac3_obs = f"Error: {e}"
    ac3_pass = False

# AC-4
ac4_exp = "fifo faults [12,12,9,10,5,5] with belady point 4; lru 10,8; opt 7,6"
try:
    with open(os.path.join(tmpdir, "ac4_fifo.json"), "r", encoding="utf-8") as f:
        fifo_sweep = json.load(f)
    with open(os.path.join(tmpdir, "ac4_lru.json"), "r", encoding="utf-8") as f:
        lru_sweep = json.load(f)
    with open(os.path.join(tmpdir, "ac4_opt.json"), "r", encoding="utf-8") as f:
        opt_sweep = json.load(f)

    fifo_faults = fifo_sweep.get("faults", [])
    fifo_belady = fifo_sweep.get("belady_points", [])

    lru_frames = lru_sweep.get("frames", [])
    lru_faults = lru_sweep.get("faults", [])
    lru_3 = lru_faults[lru_frames.index(3)] if 3 in lru_frames else (lru_faults[2] if len(lru_faults) > 2 else None)
    lru_4 = lru_faults[lru_frames.index(4)] if 4 in lru_frames else (lru_faults[3] if len(lru_faults) > 3 else None)

    opt_frames = opt_sweep.get("frames", [])
    opt_faults = opt_sweep.get("faults", [])
    opt_3 = opt_faults[opt_frames.index(3)] if 3 in opt_frames else (opt_faults[2] if len(opt_faults) > 2 else None)
    opt_4 = opt_faults[opt_frames.index(4)] if 4 in opt_frames else (opt_faults[3] if len(opt_faults) > 3 else None)

    belady_str = ",".join(str(x) for x in fifo_belady) if fifo_belady else "none"
    ac4_obs = f"fifo faults [{','.join(str(x) for x in fifo_faults)}] with belady point {belady_str}; lru {lru_3},{lru_4}; opt {opt_3},{opt_4}"
    ac4_pass = (
        fifo_faults == [12, 12, 9, 10, 5, 5]
        and (4 in fifo_belady or fifo_belady == [4])
        and lru_3 == 10
        and lru_4 == 8
        and opt_3 == 7
        and opt_4 == 6
    )
except Exception as e:
    ac4_obs = f"Error: {e}"
    ac4_pass = False

# AC-5
ac5_exp = "rejections 3; violations 2"
try:
    with open(os.path.join(tmpdir, "ac5_contiguous.json"), "r", encoding="utf-8") as f:
        cont_rej = json.load(f).get("metrics", {}).get("rejections")
    with open(os.path.join(tmpdir, "ac5_paging.json"), "r", encoding="utf-8") as f:
        page_viol = json.load(f).get("metrics", {}).get("violations")

    ac5_obs = f"rejections {cont_rej}; violations {page_viol}"
    ac5_pass = (cont_rej == 3 and page_viol == 2)
except Exception as e:
    ac5_obs = f"Error: {e}"
    ac5_pass = False

# AC-6
ac6_exp = "All 6 fixtures byte-identical"
try:
    with open(os.path.join(tmpdir, "ac6_cmp.txt"), "r", encoding="utf-8") as f:
        ac6_matches = int(f.read().strip())
    ac6_obs = f"{ac6_matches}/6 fixtures byte-identical"
    ac6_pass = (ac6_matches == 6)
except Exception as e:
    ac6_obs = f"Error: {e}"
    ac6_pass = False

# AC-7
ac7_exp = f"{ctest_total}/{ctest_total} passed"
ac7_obs = f"{ctest_passed}/{ctest_total} passed"
ac7_pass = (ctest_passed == ctest_total and ctest_total > 0)

# AC-8
ac8_exp = 'Table contains "fail FRAGMENTATION"'
try:
    with open(os.path.join(tmpdir, "ac8_compare.txt"), "r", encoding="utf-8") as f:
        ac8_text = f.read()
    if "fail FRAGMENTATION" in ac8_text:
        ac8_obs = 'Table contains "fail FRAGMENTATION"'
        ac8_pass = True
    else:
        ac8_obs = 'Output missing "fail FRAGMENTATION"'
        ac8_pass = False
except Exception as e:
    ac8_obs = f"Error: {e}"
    ac8_pass = False

rows = [
    (
        "AC-1",
        "Placement on five holes (Silberschatz)",
        "Placements of a1, a2, a3, a4 across FF, BF, WF",
        ac1_exp,
        ac1_obs,
        "PASS" if ac1_pass else "FAIL",
    ),
    (
        "AC-2",
        "Contiguous compaction and allocation retry",
        "Compaction metrics blocks_moved/units_moved and final free block",
        ac2_exp,
        ac2_obs,
        "PASS" if ac2_pass else "FAIL",
    ),
    (
        "AC-3",
        "Reference string page replacement",
        "Page fault counts for FIFO, LRU, OPT with 3 frames",
        ac3_exp,
        ac3_obs,
        "PASS" if ac3_pass else "FAIL",
    ),
    (
        "AC-4",
        "Belady's anomaly frame sweep",
        "FIFO sweep faults 1..6 and Belady point; LRU/OPT faults at 3,4 frames",
        ac4_exp,
        ac4_obs,
        "PASS" if ac4_pass else "FAIL",
    ),
    (
        "AC-5",
        "Negative tests: rejections and access violations",
        "Contiguous rejections count and paging violations count",
        ac5_exp,
        ac5_obs,
        "PASS" if ac5_pass else "FAIL",
    ),
    (
        "AC-6",
        "Workload trace determinism",
        "Byte-identical traces across two runs for all 6 fixtures",
        ac6_exp,
        ac6_obs,
        "PASS" if ac6_pass else "FAIL",
    ),
    (
        "AC-7",
        "Simulation invariants checked under test suite",
        "Invariants hold on every step in test suite (ctest)",
        ac7_exp,
        ac7_obs,
        "PASS" if ac7_pass else "FAIL",
    ),
    (
        "AC-8",
        "Comparative policy table output",
        'Human-readable compare output displays "fail FRAGMENTATION"',
        ac8_exp,
        ac8_obs,
        "PASS" if ac8_pass else "FAIL",
    ),
]

md_lines = [
    "Acceptance criteria verification results for MemSim.",
    "",
    "| ID | Scenario | What is checked | Expected | Observed | Result |",
    "| --- | --- | --- | --- | --- | --- |",
]
for r in rows:
    escaped = [cell.replace("|", "\\|") for cell in r]
    md_lines.append(f"| {' | '.join(escaped)} |")

md_lines.append("")
md_lines.append(f"Generated by scripts/report/verify_report.sh at commit {git_commit}; ctest: {ctest_passed}/{ctest_total} passed.")
md_lines.append("")

out_dir = os.path.join(repo_root, "report", "tables")
os.makedirs(out_dir, exist_ok=True)
md_path = os.path.join(out_dir, "verification.md")
with open(md_path, "w", encoding="utf-8") as f:
    f.write("\n".join(md_lines))

csv_path = os.path.join(out_dir, "verification.csv")
with open(csv_path, "w", newline="", encoding="utf-8") as f:
    writer = csv.writer(f)
    writer.writerow(["ID", "Scenario", "What is checked", "Expected", "Observed", "Result"])
    for r in rows:
        writer.writerow(r)

all_pass = all(r[5] == "PASS" for r in rows)
if not all_pass:
    sys.exit(1)
EOF
