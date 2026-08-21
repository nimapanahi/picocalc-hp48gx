#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "picocalc_protocol.h"
#include "power_sequence.h"

static uint32_t run_after_teal_press(power_sequence_t *sequence,
                                     uint32_t start) {
  assert(power_sequence_poll(sequence, start + 179, true) == POWER_ACTION_NONE);
  assert(power_sequence_poll(sequence, start + 180, true) ==
         POWER_ACTION_RELEASE_TEAL);
  assert(power_sequence_poll(sequence, start + 479, true) == POWER_ACTION_NONE);
  assert(power_sequence_poll(sequence, start + 480, true) ==
         POWER_ACTION_PRESS_ON);
  assert(power_sequence_poll(sequence, start + 699, true) == POWER_ACTION_NONE);
  assert(power_sequence_poll(sequence, start + 700, true) ==
         POWER_ACTION_RELEASE_ON);
  return start + 700;
}

static uint32_t run_one_attempt(power_sequence_t *sequence, uint32_t start) {
  assert(power_sequence_poll(sequence, start + 249, true) == POWER_ACTION_NONE);
  assert(power_sequence_poll(sequence, start + 250, true) ==
         POWER_ACTION_PRESS_TEAL);
  return run_after_teal_press(sequence, start + 250);
}

static void test_confirmed_shutdown(void) {
  power_sequence_t sequence;
  power_sequence_reset(&sequence);
  assert(!power_sequence_active(&sequence));
  power_sequence_start(&sequence, 1000);
  assert(power_sequence_active(&sequence));
  assert(power_sequence_attempts(&sequence) == 1);

  uint32_t released = run_one_attempt(&sequence, 1000);
  assert(power_sequence_poll(&sequence, released + 1, true) ==
         POWER_ACTION_NONE);
  assert(power_sequence_poll(&sequence, released + 50, false) ==
         POWER_ACTION_HP_OFF_DETECTED);
  assert(power_sequence_poll(&sequence, released + 649, false) ==
         POWER_ACTION_NONE);
  assert(power_sequence_poll(&sequence, released + 650, false) ==
         POWER_ACTION_HP_OFF_CONFIRMED);
  assert(power_sequence_poll(&sequence, released + 999, false) ==
         POWER_ACTION_NONE);
  assert(power_sequence_poll(&sequence, released + 1000, false) ==
         POWER_ACTION_REQUEST_SYSTEM_OFF);
  assert(!power_sequence_active(&sequence));
}

static void test_retry_and_failure(void) {
  power_sequence_t sequence;
  power_sequence_start(&sequence, 0);
  uint32_t released = run_one_attempt(&sequence, 0);

  uint32_t retry_start = released + 6000;
  assert(power_sequence_poll(&sequence, retry_start - 1, true) ==
         POWER_ACTION_NONE);
  assert(power_sequence_poll(&sequence, retry_start, true) ==
         POWER_ACTION_PRESS_TEAL);
  assert(power_sequence_attempts(&sequence) == 2);
  released = run_after_teal_press(&sequence, retry_start);

  retry_start = released + 6000;
  assert(power_sequence_poll(&sequence, retry_start, true) ==
         POWER_ACTION_PRESS_TEAL);
  assert(power_sequence_attempts(&sequence) == 3);
  released = run_after_teal_press(&sequence, retry_start);

  assert(power_sequence_poll(&sequence, released + 6000, true) ==
         POWER_ACTION_REQUEST_SYSTEM_OFF_UNCONFIRMED);
  assert(!power_sequence_active(&sequence));
}

static void test_lcd_reawakening_is_detected(void) {
  power_sequence_t sequence;
  power_sequence_start(&sequence, 0);
  uint32_t released = run_one_attempt(&sequence, 0);
  assert(power_sequence_poll(&sequence, released + 10, false) ==
         POWER_ACTION_HP_OFF_DETECTED);
  assert(power_sequence_poll(&sequence, released + 200, true) ==
         POWER_ACTION_HP_OFF_REAWAKENED);
  assert(power_sequence_attempts(&sequence) == 2);
  assert(power_sequence_poll(&sequence, released + 449, true) ==
         POWER_ACTION_NONE);
  assert(power_sequence_poll(&sequence, released + 450, true) ==
         POWER_ACTION_PRESS_TEAL);
}

static void test_battery_protocol(void) {
  uint8_t percent = 0;
  bool charging = false;
  assert(picocalc_decode_battery(73, &percent, &charging));
  assert(percent == 73 && !charging);
  assert(picocalc_decode_battery((uint8_t)(0x80 | 42), &percent, &charging));
  assert(percent == 42 && charging);
  assert(!picocalc_decode_battery(127, &percent, &charging));
  assert(PICOCALC_REG_BATTERY == 0x0b);
  assert(PICOCALC_REG_POWER_OFF == 0x0e);
  assert(PICOCALC_POWER_OFF_DELAY_SECONDS == 6);
  assert(PICOCALC_BIOS_POWER_OFF_MIN == 0x16);
  assert(!picocalc_bios_supports_power_off(0x15));
  assert(picocalc_bios_supports_power_off(0x16));
  assert(picocalc_bios_supports_power_off(0x17));
}

int main(void) {
  test_confirmed_shutdown();
  test_retry_and_failure();
  test_lcd_reawakening_is_detected();
  test_battery_protocol();
  return 0;
}
