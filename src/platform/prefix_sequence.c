#include "prefix_sequence.h"

enum {
  PREFIX_STAGE_IDLE = 0,
  PREFIX_STAGE_HOLD,
  PREFIX_STAGE_WAIT_LATCH,
};

#define PREFIX_HOLD_POLLS 3
#define PREFIX_LATCH_WAIT_POLLS 20
#define PREFIX_MAX_ATTEMPTS 3

static prefix_transition_t transition(prefix_transition_kind_t kind,
                                      uint16_t code) {
  prefix_transition_t result = {.kind = kind, .code = code};
  return result;
}

void prefix_sequence_reset(prefix_sequence_t *sequence) {
  if (!sequence) return;
  sequence->prefix_code = 0xffff;
  sequence->action_code = 0xffff;
  sequence->stage = PREFIX_STAGE_IDLE;
  sequence->wait_polls = 0;
  sequence->attempts = 0;
}

bool prefix_sequence_active(const prefix_sequence_t *sequence) {
  return sequence && sequence->stage != PREFIX_STAGE_IDLE;
}

prefix_transition_t prefix_sequence_start(prefix_sequence_t *sequence,
                                          uint16_t prefix_code,
                                          uint16_t action_code) {
  if (!sequence || prefix_sequence_active(sequence) ||
      prefix_code == 0xffff || action_code == 0xffff)
    return transition(PREFIX_TRANSITION_NONE, 0xffff);
  sequence->prefix_code = prefix_code;
  sequence->action_code = action_code;
  sequence->stage = PREFIX_STAGE_HOLD;
  sequence->wait_polls = 0;
  sequence->attempts = 1;
  return transition(PREFIX_TRANSITION_PRESS, prefix_code);
}

prefix_transition_t prefix_sequence_advance(prefix_sequence_t *sequence,
                                            bool prefix_latched) {
  if (!sequence) return transition(PREFIX_TRANSITION_NONE, 0xffff);
  if (sequence->stage == PREFIX_STAGE_HOLD) {
    if (sequence->wait_polls < PREFIX_HOLD_POLLS) {
      ++sequence->wait_polls;
      return transition(PREFIX_TRANSITION_NONE, 0xffff);
    }
    sequence->wait_polls = 0;
    sequence->stage = PREFIX_STAGE_WAIT_LATCH;
    return transition(PREFIX_TRANSITION_RELEASE, sequence->prefix_code);
  }
  if (sequence->stage == PREFIX_STAGE_WAIT_LATCH) {
    if (!prefix_latched && sequence->wait_polls < PREFIX_LATCH_WAIT_POLLS) {
      ++sequence->wait_polls;
      return transition(PREFIX_TRANSITION_NONE, 0xffff);
    }
    if (!prefix_latched && sequence->attempts < PREFIX_MAX_ATTEMPTS) {
      ++sequence->attempts;
      sequence->wait_polls = 0;
      sequence->stage = PREFIX_STAGE_HOLD;
      return transition(PREFIX_TRANSITION_PRESS, sequence->prefix_code);
    }
    sequence->stage = PREFIX_STAGE_IDLE;
    return transition(PREFIX_TRANSITION_PRESS, sequence->action_code);
  }
  return transition(PREFIX_TRANSITION_NONE, 0xffff);
}
