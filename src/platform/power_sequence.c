#include "power_sequence.h"

#include <stddef.h>

enum {
  POWER_STAGE_IDLE,
  POWER_STAGE_PRESS_TEAL,
  POWER_STAGE_RELEASE_TEAL,
  POWER_STAGE_PRESS_ON,
  POWER_STAGE_RELEASE_ON,
  POWER_STAGE_WAIT_FOR_HP_OFF,
  POWER_STAGE_VERIFY_HP_OFF,
  POWER_STAGE_CONFIRMED_VISIBLE,
  POWER_STAGE_FINISHED,
  POWER_STAGE_FAILED
};

enum {
  START_SETTLE_MS = 250,
  TEAL_HOLD_MS = 180,
  PREFIX_SETTLE_MS = 300,
  ON_HOLD_MS = 220,
  HP_OFF_TIMEOUT_MS = 6000,
  HP_OFF_STABLE_MS = 0,
  CONFIRM_VISIBLE_MS = 0
};

static bool time_reached(uint32_t now, uint32_t target) {
  return (int32_t)(now - target) >= 0;
}

void power_sequence_reset(power_sequence_t *sequence) {
  if (!sequence) return;
  sequence->stage = POWER_STAGE_IDLE;
  sequence->attempts = 0;
  sequence->next_ms = 0;
  sequence->deadline_ms = 0;
}

void power_sequence_start(power_sequence_t *sequence, uint32_t now_ms) {
  if (!sequence) return;
  sequence->stage = POWER_STAGE_PRESS_TEAL;
  sequence->attempts = 1;
  sequence->next_ms = now_ms + START_SETTLE_MS;
  sequence->deadline_ms = 0;
}

bool power_sequence_active(const power_sequence_t *sequence) {
  return sequence && sequence->stage != POWER_STAGE_IDLE &&
         sequence->stage != POWER_STAGE_FINISHED &&
         sequence->stage != POWER_STAGE_FAILED;
}

uint8_t power_sequence_attempts(const power_sequence_t *sequence) {
  return sequence ? sequence->attempts : 0;
}

power_action_t power_sequence_poll(power_sequence_t *sequence,
                                   uint32_t now_ms, bool hp_lcd_on) {
  if (!sequence) return POWER_ACTION_NONE;

  if (sequence->stage == POWER_STAGE_WAIT_FOR_HP_OFF) {
    if (!hp_lcd_on) {
      sequence->stage = POWER_STAGE_VERIFY_HP_OFF;
      sequence->next_ms = now_ms + HP_OFF_STABLE_MS;
      return POWER_ACTION_HP_OFF_DETECTED;
    }
    if (!time_reached(now_ms, sequence->deadline_ms)) {
      return POWER_ACTION_NONE;
    }
    if (sequence->attempts >= POWER_SEQUENCE_MAX_ATTEMPTS) {
      sequence->stage = POWER_STAGE_FINISHED;
      return POWER_ACTION_REQUEST_SYSTEM_OFF_UNCONFIRMED;
    }
    ++sequence->attempts;
    sequence->stage = POWER_STAGE_RELEASE_TEAL;
    sequence->next_ms = now_ms + TEAL_HOLD_MS;
    return POWER_ACTION_PRESS_TEAL;
  }

  if (sequence->stage == POWER_STAGE_VERIFY_HP_OFF ||
      sequence->stage == POWER_STAGE_CONFIRMED_VISIBLE) {
    if (hp_lcd_on) {
      if (sequence->attempts >= POWER_SEQUENCE_MAX_ATTEMPTS) {
        sequence->stage = POWER_STAGE_FINISHED;
        return POWER_ACTION_REQUEST_SYSTEM_OFF_UNCONFIRMED;
      }
      ++sequence->attempts;
      sequence->stage = POWER_STAGE_PRESS_TEAL;
      sequence->next_ms = now_ms + START_SETTLE_MS;
      return POWER_ACTION_HP_OFF_REAWAKENED;
    }
    if (!time_reached(now_ms, sequence->next_ms)) {
      return POWER_ACTION_NONE;
    }
    if (sequence->stage == POWER_STAGE_VERIFY_HP_OFF) {
      sequence->stage = POWER_STAGE_CONFIRMED_VISIBLE;
      sequence->next_ms = now_ms + CONFIRM_VISIBLE_MS;
      return POWER_ACTION_HP_OFF_CONFIRMED;
    }
    sequence->stage = POWER_STAGE_FINISHED;
    return POWER_ACTION_REQUEST_SYSTEM_OFF;
  }

  if (sequence->stage == POWER_STAGE_IDLE ||
      sequence->stage == POWER_STAGE_FINISHED ||
      sequence->stage == POWER_STAGE_FAILED ||
      !time_reached(now_ms, sequence->next_ms)) {
    return POWER_ACTION_NONE;
  }

  switch (sequence->stage) {
    case POWER_STAGE_PRESS_TEAL:
      sequence->stage = POWER_STAGE_RELEASE_TEAL;
      sequence->next_ms = now_ms + TEAL_HOLD_MS;
      return POWER_ACTION_PRESS_TEAL;
    case POWER_STAGE_RELEASE_TEAL:
      sequence->stage = POWER_STAGE_PRESS_ON;
      sequence->next_ms = now_ms + PREFIX_SETTLE_MS;
      return POWER_ACTION_RELEASE_TEAL;
    case POWER_STAGE_PRESS_ON:
      sequence->stage = POWER_STAGE_RELEASE_ON;
      sequence->next_ms = now_ms + ON_HOLD_MS;
      return POWER_ACTION_PRESS_ON;
    case POWER_STAGE_RELEASE_ON:
      sequence->stage = POWER_STAGE_WAIT_FOR_HP_OFF;
      sequence->deadline_ms = now_ms + HP_OFF_TIMEOUT_MS;
      return POWER_ACTION_RELEASE_ON;
    default:
      sequence->stage = POWER_STAGE_FAILED;
      return POWER_ACTION_FAILED;
  }
}
