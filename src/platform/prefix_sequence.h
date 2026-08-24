#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  PREFIX_TRANSITION_NONE = 0,
  PREFIX_TRANSITION_PRESS,
  PREFIX_TRANSITION_RELEASE,
} prefix_transition_kind_t;

typedef struct {
  prefix_transition_kind_t kind;
  uint16_t code;
} prefix_transition_t;

typedef struct {
  uint16_t prefix_code;
  uint16_t action_code;
  uint8_t stage;
  uint8_t wait_polls;
  uint8_t attempts;
} prefix_sequence_t;

void prefix_sequence_reset(prefix_sequence_t *sequence);
bool prefix_sequence_active(const prefix_sequence_t *sequence);

/* Begin an ordered HP prefix tap. The caller applies the returned prefix
 * press immediately, then calls advance only after the Saturn CPU has run. */
prefix_transition_t prefix_sequence_start(prefix_sequence_t *sequence,
                                          uint16_t prefix_code,
                                          uint16_t action_code);

/* Advances first hold the prefix across multiple input polls, then release it.
 * Later advances wait for the HP ROM's annunciator before pressing the action,
 * retrying the prefix with a bounded timeout if necessary. */
prefix_transition_t prefix_sequence_advance(prefix_sequence_t *sequence,
                                            bool prefix_latched);
