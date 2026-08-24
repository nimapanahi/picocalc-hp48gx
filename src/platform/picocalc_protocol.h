#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Official PicoCalc keyboard/PMU registers. The battery byte uses bit 7 for
 * charging and bits 0-6 for percent. Writing REG_POWER_OFF asks the keyboard
 * MCU's AXP2101 PMU to remove system power after the requested delay; current
 * keyboard firmware clamps that delay to a minimum of six seconds. */
#define PICOCALC_REG_BATTERY 0x0bu
#define PICOCALC_REG_POWER_OFF 0x0eu
#define PICOCALC_KEY_POWER 0x91u
#define PICOCALC_POWER_OFF_DELAY_SECONDS 6u
#define PICOCALC_BIOS_POWER_OFF_MIN 0x16u

static inline bool picocalc_decode_battery(uint8_t raw, uint8_t *percent,
                                           bool *charging) {
  uint8_t decoded_percent = raw & 0x7fu;
  if (decoded_percent > 100u) return false;
  if (percent) *percent = decoded_percent;
  if (charging) *charging = (raw & 0x80u) != 0;
  return true;
}

static inline bool picocalc_bios_supports_power_off(uint8_t bios_version) {
  /* ClockworkPi BIOS versions use packed display digits (0x16 is v1.6).
   * REG_POWER_OFF first appeared in v1.6. The version read is the reliable
   * capability boundary; probing 0x0E during boot can return a stale/default
   * response while the keyboard controller is still settling. */
  return bios_version >= PICOCALC_BIOS_POWER_OFF_MIN;
}
