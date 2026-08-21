#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: ./make_firmware.sh /path/to/gxrom-r-or-zip"
  exit 2
fi

project_dir="$(cd "$(dirname "$0")" && pwd)"
python3 "$project_dir/tools/make_uf2.py" \
  "$project_dir/hp48gx_picocalc_template.uf2" "$1" \
  "$project_dir/hp48gx_picocalc.uf2"
echo "Ready to flash: $project_dir/hp48gx_picocalc.uf2"
