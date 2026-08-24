#include <assert.h>
#include <stdint.h>

#include "realtime_pacer.h"

int main(void) {
  realtime_pacer_t pacer = {0};
  realtime_pacer_start(&pacer, 1000,
                       HP48_REALTIME_DEFAULT_INSTRUCTIONS_PER_SECOND);
  assert(realtime_pacer_instructions_per_second(&pacer) == 500000);

  /* 256 decoded instructions occupy 512 microseconds at the hardware-tuned
   * native-game default. The millisecond of real work already exceeded it. */
  realtime_pacer_account(&pacer, 2000, 256);
  assert(realtime_pacer_delay_us(&pacer, 2000) == 0);

  /* Deadlines accumulate without rounding drift. */
  realtime_pacer_account(&pacer, 2000, 1000);
  assert(realtime_pacer_delay_us(&pacer, 2000) == 1512);

  /* A live speed change starts a fresh timing epoch, so the old rate cannot
   * leak into the first frame rendered at the new setting. */
  realtime_pacer_set_rate(&pacer, 24000, 100000);
  assert(realtime_pacer_instructions_per_second(&pacer) == 100000);
  realtime_pacer_account(&pacer, 24000, 250);
  assert(realtime_pacer_delay_us(&pacer, 24000) == 2500);

  /* A bounded blocking LCD frame remains timing debt, so a DMA-free display
   * update does not lower game and speaker pitch. */
  realtime_pacer_account(&pacer, 50000, 100);
  assert(realtime_pacer_delay_us(&pacer, 50000) == 0);
  assert(pacer.deadline_us < 50000);

  /* Genuinely long external stalls still reset instead of causing an
   * unbounded catch-up burst. */
  realtime_pacer_account(&pacer, 150000, 100);
  assert(realtime_pacer_delay_us(&pacer, 150000) == 0);
  realtime_pacer_account(&pacer, 150000, 250);
  assert(realtime_pacer_delay_us(&pacer, 150000) == 2500);

  /* Fractional microsecond rates have no 500k ceiling and carry their
   * remainder without long-term truncation drift. */
  realtime_pacer_set_rate(&pacer, 200000, 666667);
  realtime_pacer_account(&pacer, 200000, 3);
  assert(realtime_pacer_delay_us(&pacer, 200000) == 4);
  assert(pacer.fractional_numerator != 0);

  /* Zero is the explicit unpaced MAX setting. */
  realtime_pacer_set_rate(&pacer, 300000, 0);
  assert(realtime_pacer_instructions_per_second(&pacer) == 0);
  realtime_pacer_account(&pacer, 400000, 1000000);
  assert(realtime_pacer_delay_us(&pacer, 400000) == 0);
  return 0;
}
