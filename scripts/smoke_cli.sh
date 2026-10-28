#!/usr/bin/env bash
set -euo pipefail

MEMSIM_BINARY="${1:?usage: smoke_cli.sh MEMSIM_BINARY}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIXTURES_DIR="$SCRIPT_DIR/../tests/fixtures"

# 1. runs validate on every fixture
for f in "$FIXTURES_DIR"/*.wl; do
    "$MEMSIM_BINARY" validate "$f"
done

# 2. compare on ac1 and greps for "fail FRAGMENTATION"
"$MEMSIM_BINARY" compare "$FIXTURES_DIR/ac1_silberschatz.wl" --policies first-fit,best-fit,worst-fit | grep "fail FRAGMENTATION"

# 3. refs AC-3 string with --frames 3 for fifo/lru/opt and checks 15/12/9
AC3_STR="7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1"
"$MEMSIM_BINARY" refs "$AC3_STR" --frames 3 --policy fifo | grep "15"
"$MEMSIM_BINARY" refs "$AC3_STR" --frames 3 --policy lru | grep "12"
"$MEMSIM_BINARY" refs "$AC3_STR" --frames 3 --policy opt | grep "9"

# 4. sweep ac4 fifo 1..6 and checks the "4 10 *" line
"$MEMSIM_BINARY" sweep "$FIXTURES_DIR/ac4_belady.wl" --policy fifo --frames 1..6 | grep "4 10 \*"

# 5. gen both generators into a mktemp dir and validates them
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

"$MEMSIM_BINARY" gen alloc-uniform --seed 42 -o "$TMPDIR/alloc.wl"
"$MEMSIM_BINARY" validate "$TMPDIR/alloc.wl"

"$MEMSIM_BINARY" gen refs-locality --seed 42 -o "$TMPDIR/refs.wl"
"$MEMSIM_BINARY" validate "$TMPDIR/refs.wl"

# 6. run --trace on ac2 and checks the file's last line contains RunSummary
"$MEMSIM_BINARY" run "$FIXTURES_DIR/ac2_compact.wl" --policy first-fit --trace "$TMPDIR/ac2.trace"
tail -n 1 "$TMPDIR/ac2.trace" | grep "RunSummary"
