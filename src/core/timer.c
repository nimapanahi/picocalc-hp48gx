#include "timer.h"

#include <stdbool.h>
#include <string.h>

#include "platform_time.h"
#include "resources.h"

typedef struct {
  bool running;
  uint64_t started_us;
  uint64_t elapsed_us;
} timer_state_t;

static timer_state_t s_timers[NR_TIMERS];

#define HP_UNIX_EPOCH_TICKS 0x1cf2e8f800000ull
#define HP_TEN_MINUTES_TICKS 0x00b40000ull
#define ACCESSTIME_GX 0x58u
#define ACCESSCRC_GX 0x65u
#define TIMEOUT_GX 0x69u
#define TIMEOUTCLK_GX 0x76u

static uint64_t hp_absolute_ticks(void) {
  return HP_UNIX_EPOCH_TICKS + ((uint64_t)BUILD_UNIX_TIME * 8192ull)
       + (platform_time_us() * 8192ull / 1000000ull);
}

static uint64_t timer_elapsed_us(int timer) {
  timer_state_t *t = &s_timers[timer];
  return t->elapsed_us + (t->running ? platform_time_us() - t->started_us : 0);
}

void reset_timer(int timer) {
  if (timer >= 0 && timer < NR_TIMERS) memset(&s_timers[timer], 0, sizeof(s_timers[timer]));
}

void start_timer(int timer) {
  if (timer < 0 || timer >= NR_TIMERS || s_timers[timer].running) return;
  s_timers[timer].running = true;
  s_timers[timer].started_us = platform_time_us();
}

void restart_timer(int timer) {
  reset_timer(timer);
  start_timer(timer);
}

void stop_timer(int timer) {
  if (timer < 0 || timer >= NR_TIMERS || !s_timers[timer].running) return;
  s_timers[timer].elapsed_us = timer_elapsed_us(timer);
  s_timers[timer].running = false;
}

word_64 get_timer(int timer) {
  if (timer < 0 || timer >= NR_TIMERS) return 0;
  uint64_t us = timer_elapsed_us(timer);
  return (timer == T1_TIMER) ? (word_64)(us * 512ull / 1000000ull)
                             : (word_64)(us * 8192ull / 1000000ull);
}

long diff_timer(word_64 *t1, word_64 *t2) { return (long)(*t2 - *t1); }

t1_t2_ticks get_t1_t2(void) {
  t1_t2_ticks out;
  out.t1_ticks = (unsigned long)get_timer(T1_TIMER);
  uint64_t access_time = 0;
  for (int i = 12; i >= 0; --i) {
    access_time = (access_time << 4) | (saturn.ram[ACCESSTIME_GX + i] & 0x0f);
  }
  int64_t remaining = (int64_t)access_time - (int64_t)hp_absolute_ticks();
  uint32_t realtime_value = (uint32_t)remaining;
  uint32_t current_value = (uint32_t)saturn.timer2;

  /* TIMER2 is a wrapping down-counter. The instruction scheduler estimates
   * its progress between wall-clock samples, but a fast emulation rate can
   * put that estimate ahead of real time. Never move the counter backwards
   * (up) when reconciling it. This also preserves a value just programmed by
   * native games instead of replacing it with the older ACCESSTIME-derived
   * value at the next scheduler adjustment. TetrisGX polls the low TIMER2
   * nibbles continuously and otherwise becomes trapped in its frame wait. */
  uint32_t forward_ticks = current_value - realtime_value;
  if (forward_ticks < 0x80000000u) {
    out.t2_ticks = realtime_value;
  } else {
    out.t2_ticks = current_value;
    if (saturn.t2_tick < INT16_MAX) ++saturn.t2_tick;
  }
  return out;
}

void set_accesstime(void) {
  uint64_t ticks = hp_absolute_ticks() + (int32_t)saturn.timer2;
  uint16_t crc = 0;
  for (int i = 0; i < 13; ++i) {
    uint8_t nib = ticks & 0x0f;
    saturn.ram[ACCESSTIME_GX + i] = nib;
    crc = (crc >> 4) ^ (((crc ^ nib) & 0xf) * 0x1081);
    ticks >>= 4;
  }
  for (int i = 0; i < 4; ++i) {
    saturn.ram[ACCESSCRC_GX + i] = crc & 0x0f;
    crc >>= 4;
  }

  uint64_t timeout = hp_absolute_ticks() + (int32_t)saturn.timer2
                   + HP_TEN_MINUTES_TICKS;
  for (int i = 0; i < 13; ++i) {
    saturn.ram[TIMEOUT_GX + i] = timeout & 0x0f;
    timeout >>= 4;
  }
  saturn.ram[TIMEOUTCLK_GX] = 0x0f;
}
