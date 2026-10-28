#!/usr/bin/env bash
# Compile the simulator (every module except the CLI) plus web/wasm/memsim_web.c into
# web/public/memsim.wasm. Needs clang, wasm-ld and wasi-libc (pacman -S wasi-libc).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
sysroot="${WASI_SYSROOT:-/usr/share/wasi-sysroot}"
out="$root/web/public/memsim.wasm"
obj="$(mktemp -d)"
trap 'rm -rf "$obj"' EXIT

includes=()
for d in "$root"/src/*/include; do includes+=(-I "$d"); done
flags=(--target=wasm32-wasip1 --sysroot="$sysroot" -std=c17 -O2 -DNDEBUG -D_GNU_SOURCE -Wall -Wextra)

sources=("$root/web/wasm/memsim_web.c")
while IFS= read -r f; do sources+=("$f"); done < <(find "$root/src" -name '*.c' -not -path '*/cli/*' | sort)
objects=()
for src in "${sources[@]}"; do
    o="$obj/$(echo "${src#"$root"/}" | tr '/' '_').o"
    mod="$(echo "${src#"$root"/src/}" | cut -d/ -f1)"
    clang "${flags[@]}" "${includes[@]}" -I "$(dirname "$src")" -I "$root/src/$mod" -c "$src" -o "$o"
    objects+=("$o")
done
mkdir -p "$(dirname "$out")"
clang "${flags[@]}" -mexec-model=reactor "${objects[@]}" -o "$out"
echo "wrote $out ($(stat -c %s "$out") bytes)"
