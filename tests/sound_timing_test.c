#include <assert.h>
#include <stdint.h>

#include "sound_timing.h"

int main(void) {
  sound_timing_t timing;
  uint32_t frequency = 0;
  sound_timing_reset(&timing);

  assert(!sound_timing_observe(&timing, false, 1000u, &frequency));
  assert(!sound_timing_observe(&timing, false, 1050u, &frequency));
  assert(sound_timing_observe(&timing, true, 1097u, &frequency));
  assert(frequency == 2577u);
  assert(sound_timing_observe(&timing, false, 1194u, &frequency));
  assert(frequency == 2577u);

  /* A silent gap begins a fresh waveform instead of becoming a low chirp. */
  assert(!sound_timing_observe(&timing, true, 10000u, &frequency));
  assert(sound_timing_observe(&timing, false, 10097u, &frequency));
  assert(frequency == 2577u);

  /* Very short valid intervals are bounded below the timer's Nyquist limit. */
  assert(sound_timing_observe(&timing, true, 10098u, &frequency));
  assert(frequency == 5000u);

  sound_timing_reset(&timing);
  assert(!sound_timing_observe(&timing, true, 0xfffffff0u, &frequency));
  assert(sound_timing_observe(&timing, false, 0x00000051u, &frequency));
  assert(frequency == 2577u);
  return 0;
}
