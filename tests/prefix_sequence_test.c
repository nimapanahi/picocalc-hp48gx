#include <assert.h>

#include "prefix_sequence.h"

static prefix_transition_t advance_to_release(prefix_sequence_t *sequence) {
  for (int poll = 0; poll < 3; ++poll) {
    prefix_transition_t hold = prefix_sequence_advance(sequence, false);
    assert(hold.kind == PREFIX_TRANSITION_NONE);
  }
  return prefix_sequence_advance(sequence, false);
}

int main(void) {
  prefix_sequence_t sequence;
  prefix_sequence_reset(&sequence);
  assert(!prefix_sequence_active(&sequence));

  prefix_transition_t step = prefix_sequence_start(&sequence, 0x15, 0x12);
  assert(step.kind == PREFIX_TRANSITION_PRESS && step.code == 0x15);
  assert(prefix_sequence_active(&sequence));

  step = advance_to_release(&sequence);
  assert(step.kind == PREFIX_TRANSITION_RELEASE && step.code == 0x15);
  assert(prefix_sequence_active(&sequence));

  step = prefix_sequence_advance(&sequence, false);
  assert(step.kind == PREFIX_TRANSITION_NONE);
  assert(prefix_sequence_active(&sequence));

  step = prefix_sequence_advance(&sequence, true);
  assert(step.kind == PREFIX_TRANSITION_PRESS && step.code == 0x12);
  assert(!prefix_sequence_active(&sequence));

  step = prefix_sequence_advance(&sequence, false);
  assert(step.kind == PREFIX_TRANSITION_NONE);

  step = prefix_sequence_start(&sequence, 0x25, 0x23);
  assert(step.kind == PREFIX_TRANSITION_PRESS && step.code == 0x25);
  step = prefix_sequence_start(&sequence, 0x15, 0x12);
  assert(step.kind == PREFIX_TRANSITION_NONE);
  prefix_sequence_reset(&sequence);
  assert(!prefix_sequence_active(&sequence));

  /* A missing annunciator retries the prefix, then cannot wedge forever. */
  step = prefix_sequence_start(&sequence, 0x35, 0x54);
  assert(step.kind == PREFIX_TRANSITION_PRESS);
  step = advance_to_release(&sequence);
  assert(step.kind == PREFIX_TRANSITION_RELEASE);
  for (int poll = 0; poll < 20; ++poll) {
    step = prefix_sequence_advance(&sequence, false);
    assert(step.kind == PREFIX_TRANSITION_NONE);
  }
  step = prefix_sequence_advance(&sequence, false);
  assert(step.kind == PREFIX_TRANSITION_PRESS && step.code == 0x35);
  step = advance_to_release(&sequence);
  assert(step.kind == PREFIX_TRANSITION_RELEASE && step.code == 0x35);
  for (int poll = 0; poll < 20; ++poll)
    assert(prefix_sequence_advance(&sequence, false).kind ==
           PREFIX_TRANSITION_NONE);
  step = prefix_sequence_advance(&sequence, false);
  assert(step.kind == PREFIX_TRANSITION_PRESS && step.code == 0x35);
  step = advance_to_release(&sequence);
  assert(step.kind == PREFIX_TRANSITION_RELEASE && step.code == 0x35);
  for (int poll = 0; poll < 20; ++poll)
    assert(prefix_sequence_advance(&sequence, false).kind ==
           PREFIX_TRANSITION_NONE);
  step = prefix_sequence_advance(&sequence, false);
  assert(step.kind == PREFIX_TRANSITION_PRESS && step.code == 0x54);
  assert(!prefix_sequence_active(&sequence));
  return 0;
}
