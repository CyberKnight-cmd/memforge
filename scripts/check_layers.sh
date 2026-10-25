#!/usr/bin/env bash
# Layering checks the compiler cannot make on its own (README section 5, NFR-2).
#
# Private headers and unlinked modules are already invisible to the compiler. This script catches
# the rest: a module adding a forbidden DEPS entry, a policy reaching into its engine, and I/O or
# wall-clock use inside the simulation core.
set -euo pipefail
cd "$(dirname "$0")/.."

# Which modules each module may include. Must agree with DEPS in src/<module>/CMakeLists.txt.
declare -A allowed=(
  [common]="common"
  [contiguous]="common contiguous"
  [paging]="common paging"
  [trace]="common trace"
  [workloads]="common workloads"
  [metrics]="common metrics"
  [session]="common contiguous paging trace session"
  [bench]="common contiguous paging trace workloads metrics session bench"
  [cli]="*"
)
core=(src/common src/contiguous src/paging)

fail=0
violation() { echo "layer violation: $1"; fail=1; }

# memsim/<module>.h or memsim/<module>/... -> <module>
module_of() { local m=${1#memsim/}; m=${m%%/*}; echo "${m%.h}"; }

for dir in src/*/; do
  mod=$(basename "$dir")
  [[ -v allowed[$mod] ]] || { violation "$dir is not in the allowed table"; continue; }
  [[ ${allowed[$mod]} == "*" ]] && continue
  while IFS=: read -r file line inc; do
    target=$(module_of "$inc")
    [[ " ${allowed[$mod]} " == *" $target "* ]] || violation "$file:$line: $mod must not include $inc"
  done < <(grep -rnoE '#include "memsim/[^"]+"' "$dir" | sed -E 's/#include "(.*)"/\1/' || true)
done

# Policies decide, engines mutate: a policy never sees its engine (ADR-010).
while IFS=: read -r file line _; do
  violation "$file:$line: a policy must not include its engine"
done < <(grep -rnE '#include "(memsim/(contiguous|paging)/)?(\.\./)?engine\.h"' \
           src/contiguous/policies src/paging/policies || true)

# Engine purity (NFR-2): no I/O or wall clock in the core.
while IFS=: read -r file line text; do
  violation "$file:$line: the core must not use $text"
done < <(grep -rnoE '#include <(stdio|time|unistd|fcntl|sys/[a-z_]+)\.h>' "${core[@]}" || true)

((fail)) && exit 1
echo "layers OK"
