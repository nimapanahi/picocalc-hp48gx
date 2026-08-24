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
  "$project_dir/src/core/rpl_object.c"
  "$project_dir/src/core/timer.c"
)

"${CC:-cc}" -std=c11 -O2 -DBUILD_UNIX_TIME=1700000000 \
  -I"$project_dir/src/core" -I"$project_dir/src/platform" \
  "$project_dir/tests/core_smoke.c" "${common_sources[@]}" \
  -o "$test_dir/core_smoke"

"$test_dir/core_smoke"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/src/platform" \
  "$project_dir/tests/power_sequence_test.c" \
  "$project_dir/src/platform/power_sequence.c" \
  -o "$test_dir/power_sequence_test"

"$test_dir/power_sequence_test"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/src/platform" \
  "$project_dir/tests/prefix_sequence_test.c" \
  "$project_dir/src/platform/prefix_sequence.c" \
  -o "$test_dir/prefix_sequence_test"

"$test_dir/prefix_sequence_test"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/src/platform" \
  "$project_dir/tests/held_keys_test.c" \
  "$project_dir/src/platform/held_keys.c" \
  -o "$test_dir/held_keys_test"

"$test_dir/held_keys_test"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/src/platform" \
  "$project_dir/tests/realtime_pacer_test.c" \
  "$project_dir/src/platform/realtime_pacer.c" \
  -o "$test_dir/realtime_pacer_test"

"$test_dir/realtime_pacer_test"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/src/platform" \
  "$project_dir/tests/sound_timing_test.c" \
  "$project_dir/src/platform/sound_timing.c" \
  -o "$test_dir/sound_timing_test"

"$test_dir/sound_timing_test"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/tests/fake_pico" \
  -I"$project_dir/src/platform" \
  "$project_dir/tests/platform_ui_nav_test.c" \
  "$project_dir/src/platform/platform_ui.c" \
  -o "$test_dir/platform_ui_nav_test"

"$test_dir/platform_ui_nav_test"

fake_sd="$test_dir/fake-sd"
mkdir -p "$fake_sd/HP48GX/PROGRAMS/INBOX" \
  "$fake_sd/HP48GX/PROGRAMS/OUTBOX"
cp "$project_dir/samples/PICOCALC.48G" \
  "$fake_sd/HP48GX/PROGRAMS/INBOX/PICOCALC.48G"
cp "$project_dir/samples/PICOCALC.48G" \
  "$fake_sd/HP48GX/PROGRAMS/INBOX/ZZZ.48G"
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/tests/fake_fatfs" \
  -I"$project_dir/src/core" -I"$project_dir/src/platform" \
  "$project_dir/tests/object_files_test.c" \
  "$project_dir/tests/fake_fatfs/fake_fatfs.c" \
  "$project_dir/src/platform/object_files.c" \
  -o "$test_dir/object_files_test"
"$test_dir/object_files_test" "$fake_sd"

"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$project_dir/tests/fake_fatfs" \
  -I"$project_dir/src/core" -I"$project_dir/src/platform" \
  "$project_dir/tests/port2_card_test.c" \
  "$project_dir/tests/fake_fatfs/fake_fatfs.c" \
  "$project_dir/src/platform/port2_card.c" \
  -o "$test_dir/port2_card_test"
"$test_dir/port2_card_test" "$fake_sd"

python3 -m py_compile "$project_dir/tools/prepare_rom.py" \
  "$project_dir/tools/make_uf2.py" \
  "$project_dir/tools/make_sample_objects.py" \
  "$project_dir/tools/make_validation_video.py" \
  "$project_dir/tools/make_calculator_validation_video.py"
python3 "$project_dir/tools/make_sample_objects.py" \
  "$test_dir/PICOCALC.48G"
cmp "$project_dir/samples/PICOCALC.48G" "$test_dir/PICOCALC.48G"

echo "Host core, ordered HP prefixes, context-aware keyboard navigation, RPL object transfer, FatFs inbox/outbox, Port 1/2 persistence and recovery, sample HP file, packed virtual cards, realtime pacing, queued display, sound hook, power-sequence, battery protocol, and Python tool checks passed."
