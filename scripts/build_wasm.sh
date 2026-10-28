#!/usr/bin/env bash
# Compile the simulator (every module except the CLI) plus web/wasm/memsim_web.c into
# web/public/memsim.wasm. Needs clang, wasm-ld and wasi-libc (pacman -S wasi-libc).
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"

out="$root/web/public/memsim.wasm"
obj="$(mktemp -d)"
trap 'rm -rf "$obj"' EXIT

includes=()
for d in "$root"/src/*/include; do includes+=(-I "$d"); done
# Find wasi-libc: Arch and wasi-sdk use /usr/share/wasi-sysroot; Debian and Ubuntu install it
# multiarch-style under /usr. Newer wasi-libc names the target wasm32-wasip1, older wasm32-wasi.
sysroot="" target=""
for root_dir in ${WASI_SYSROOT:-} /usr/share/wasi-sysroot /usr; do
    for t in wasm32-wasip1 wasm32-wasi; do
        if [ -f "$root_dir/include/$t/stdio.h" ] || { [ -f "$root_dir/include/stdio.h" ] && [ -d "$root_dir/lib/$t" ]; }; then
            sysroot="$root_dir" target="$t"
            break 2
        fi
    done
done
[ -n "$sysroot" ] || { echo "wasi-libc not found; install it (see README) or set WASI_SYSROOT" >&2; exit 1; }
flags=(--target="$target" --sysroot="$sysroot" -std=c17 -O2 -DNDEBUG -D_GNU_SOURCE -Wall -Wextra)

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
