#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT

common_sources=(
  "$project_dir/tests/host_stubs.c"
  "$project_dir/src/core/actions.c"
  "$project_dir/src/core/core_runtime.c"
  "$project_dir/src/core/device.c"
  "$project_dir/src/core/display.c"
  "$project_dir/src/core/emulate.c"
  "$project_dir/src/core/memory.c"
  "$project_dir/src/core/register.c"
  "$project_dir/src/core/timer.c"
)

"${CC:-cc}" -std=c11 -O2 -DBUILD_UNIX_TIME=1700000000 \
  -I"$project_dir/src/core" -I"$project_dir/src/platform" \
  "$project_dir/tests/core_smoke.c" "${common_sources[@]}" \
  -o "$test_dir/core_smoke"

"$test_dir/core_smoke"
python3 -m py_compile "$project_dir/tools/prepare_rom.py" \
  "$project_dir/tools/make_uf2.py"

echo "Host core and Python tool checks passed."
