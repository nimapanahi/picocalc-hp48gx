#include "sound_timing.h"

#include <stddef.h>

/* Revision-R diagnostic sound toggles OUT every 97 decoded instructions.
 * 500k decoded instructions/s is the hardware-validated GX operating point,
 * producing the calculator's high ~2.58 kHz beep. This clock stays fixed when
 * the UI speed selector changes: HP timers and audio represent calculator
 * time, not the host's rendering throughput. */
#define HP48_AUDIO_INSTRUCTIONS_PER_SECOND 500000u
#define HP48_AUDIO_MIN_HZ 100u
#define HP48_AUDIO_MAX_HZ 5000u
#define HP48_AUDIO_MAX_EDGE_INSTRUCTIONS \
  (HP48_AUDIO_INSTRUCTIONS_PER_SECOND / (2u * HP48_AUDIO_MIN_HZ))

void sound_timing_reset(sound_timing_t *timing) {
  if (!timing) return;
  *timing = (sound_timing_t){0};
}

bool sound_timing_observe(sound_timing_t *timing, bool high,
                          uint32_t instruction_count,
                          uint32_t *frequency_hz) {
  if (!timing || !frequency_hz) return false;
  if (timing->level_known && timing->level_high == high) return false;

  timing->level_known = true;
  timing->level_high = high;
  if (!timing->edge_known) {
    timing->last_edge_instruction = instruction_count;
    timing->edge_known = true;
    return false;
  }

  uint32_t delta = instruction_count - timing->last_edge_instruction;
  timing->last_edge_instruction = instruction_count;
  if (delta == 0 || delta > HP48_AUDIO_MAX_EDGE_INSTRUCTIONS) return false;

  uint32_t frequency =
      (HP48_AUDIO_INSTRUCTIONS_PER_SECOND + delta) / (2u * delta);
  if (frequency < HP48_AUDIO_MIN_HZ) frequency = HP48_AUDIO_MIN_HZ;
  if (frequency > HP48_AUDIO_MAX_HZ) frequency = HP48_AUDIO_MAX_HZ;
  *frequency_hz = frequency;
  return true;
}
