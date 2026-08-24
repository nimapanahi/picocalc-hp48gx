#pragma once

#include <stdbool.h>
#include <stdint.h>

/* The PicoCalc executes Saturn instructions in short batches. Preserve the
 * HP speaker's instruction-domain edge spacing so the platform can rebuild a
 * continuous waveform instead of exposing those batches to the speaker. */
typedef struct {
  uint32_t last_edge_instruction;
  bool level_known;
  bool level_high;
  bool edge_known;
} sound_timing_t;

void sound_timing_reset(sound_timing_t *timing);
bool sound_timing_observe(sound_timing_t *timing, bool high,
                          uint32_t instruction_count,
                          uint32_t *frequency_hz);
