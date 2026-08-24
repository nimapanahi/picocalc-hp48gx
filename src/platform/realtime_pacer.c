#include "realtime_pacer.h"

#include <limits.h>

void realtime_pacer_start(realtime_pacer_t *pacer, uint64_t now_us,
                          uint32_t instructions_per_second) {
  if (!pacer) return;
  pacer->started = true;
  pacer->deadline_us = now_us;
  pacer->instructions_per_second = instructions_per_second;
  pacer->fractional_numerator = 0;
}

void realtime_pacer_set_rate(realtime_pacer_t *pacer, uint64_t now_us,
                             uint32_t instructions_per_second) {
  realtime_pacer_start(pacer, now_us, instructions_per_second);
}

uint32_t realtime_pacer_instructions_per_second(
    const realtime_pacer_t *pacer) {
  if (!pacer) return 0;
  return pacer->instructions_per_second;
}

void realtime_pacer_account(realtime_pacer_t *pacer, uint64_t now_us,
                            unsigned instructions) {
  if (!pacer) return;
  if (!pacer->started)
    realtime_pacer_start(pacer, now_us,
                         HP48_REALTIME_DEFAULT_INSTRUCTIONS_PER_SECOND);

  /* A zero rate is the explicit MAX setting: service the emulator in the
   * caller's normal small quanta, but never add a pacing delay. */
  if (pacer->instructions_per_second == 0) {
    pacer->deadline_us = now_us;
    pacer->fractional_numerator = 0;
    return;
  }

  uint64_t numerator = (uint64_t)instructions * 1000000u +
                       pacer->fractional_numerator;
  pacer->deadline_us += numerator / pacer->instructions_per_second;
  pacer->fractional_numerator =
      (uint32_t)(numerator % pacer->instructions_per_second);

  /* Bounded dirty-span or full-frame panel work remains timing debt. A flash,
   * storage, UI, or SHUTDN stall beyond the configured window must not create
   * an unbounded visible and audible catch-up burst. */
  if (now_us > pacer->deadline_us + HP48_REALTIME_MAX_LAG_US) {
    pacer->deadline_us = now_us;
    pacer->fractional_numerator = 0;
  }
}

uint32_t realtime_pacer_delay_us(const realtime_pacer_t *pacer,
                                 uint64_t now_us) {
  if (!pacer || !pacer->started || pacer->instructions_per_second == 0 ||
      now_us >= pacer->deadline_us)
    return 0;
  uint64_t delay = pacer->deadline_us - now_us;
  return delay > UINT32_MAX ? UINT32_MAX : (uint32_t)delay;
}
