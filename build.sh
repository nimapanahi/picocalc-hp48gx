#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: ./build.sh /full/path/to/gxrom-r"
  exit 2
fi
if [[ -z "${PICO_SDK_PATH:-}" ]]; then
  echo "Set PICO_SDK_PATH to your Raspberry Pi Pico SDK folder first."
  exit 2
fi

project_dir="$(cd "$(dirname "$0")" && pwd)"
rom_path="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
cmake -S "$project_dir" -B "$project_dir/build" -G Ninja \
  -DPICO_SDK_PATH="$PICO_SDK_PATH" -DHP48_ROM="$rom_path"
cmake --build "$project_dir/build"

echo
echo "Built: $project_dir/build/hp48gx_picocalc.uf2"

