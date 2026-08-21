#pragma once

#include <stdbool.h>
#include <stdint.h>

#define POWER_SEQUENCE_MAX_ATTEMPTS 3u

typedef enum {
  POWER_ACTION_NONE,
  POWER_ACTION_PRESS_TEAL,
  POWER_ACTION_RELEASE_TEAL,
  POWER_ACTION_PRESS_ON,
  POWER_ACTION_RELEASE_ON,
  POWER_ACTION_HP_OFF_DETECTED,
  POWER_ACTION_HP_OFF_REAWAKENED,
  POWER_ACTION_HP_OFF_CONFIRMED,
  POWER_ACTION_REQUEST_SYSTEM_OFF,
  POWER_ACTION_REQUEST_SYSTEM_OFF_UNCONFIRMED,
  POWER_ACTION_FAILED
} power_action_t;

typedef struct {
  uint8_t stage;
  uint8_t attempts;
  uint32_t next_ms;
  uint32_t deadline_ms;
} power_sequence_t;

void power_sequence_reset(power_sequence_t *sequence);
void power_sequence_start(power_sequence_t *sequence, uint32_t now_ms);
bool power_sequence_active(const power_sequence_t *sequence);
uint8_t power_sequence_attempts(const power_sequence_t *sequence);
power_action_t power_sequence_poll(power_sequence_t *sequence,
                                   uint32_t now_ms, bool hp_lcd_on);
