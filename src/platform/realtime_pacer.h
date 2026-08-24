#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Decoded instructions are not Saturn clock cycles. Once HP timers were moved
 * to wall time, physical Phoenix testing found 500k IPS preserves game speed,
 * makes animation smoother, and puts the ROM's 97-instruction speaker
 * half-wave at about 2.58 kHz instead of a low 644 Hz chirp. */
#define HP48_REALTIME_DEFAULT_INSTRUCTIONS_PER_SECOND 500000u
/* A complete DMA-free 25 MHz panel refresh totals about 42 ms, although the
 * cooperative row queue now splits it into sub-millisecond spans. Preserve
 * that aggregate timing debt so Saturn catches up to the selected average
 * rate; genuinely long UI, storage, flash, or SHUTDN stalls still rebase. */
#define HP48_REALTIME_MAX_LAG_US 75000u

typedef struct {
  bool started;
  uint64_t deadline_us;
  uint32_t instructions_per_second;
  uint32_t fractional_numerator;
} realtime_pacer_t;

void realtime_pacer_start(realtime_pacer_t *pacer, uint64_t now_us,
                          uint32_t instructions_per_second);
void realtime_pacer_set_rate(realtime_pacer_t *pacer, uint64_t now_us,
                             uint32_t instructions_per_second);
uint32_t realtime_pacer_instructions_per_second(const realtime_pacer_t *pacer);
void realtime_pacer_account(realtime_pacer_t *pacer, uint64_t now_us,
                            unsigned instructions);
uint32_t realtime_pacer_delay_us(const realtime_pacer_t *pacer,
                                 uint64_t now_us);
