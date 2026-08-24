#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "held_keys.h"

int main(void) {
  held_keys_t keys;
  held_key_release_t release;
  held_keys_init(&keys);
  assert(held_keys_empty(&keys));

  /* Android digs only while ENTER and an HP arrow are simultaneously down. */
  assert(held_keys_press(&keys, 0x44, 1000, 75, false));
  assert(held_keys_press(&keys, 0x62, 1015, 75, false));
  assert(!held_keys_empty(&keys));
  assert(held_keys_request_release(&keys, 0x44));
  assert(held_keys_release_waiting(&keys));
  assert(!held_keys_pop_due_release(&keys, 1074, &release));
  assert(held_keys_pop_due_release(&keys, 1075, &release));
  assert(release.code == 0x44);
  assert(!release.consumes_context);
  assert(!held_keys_empty(&keys));

  assert(held_keys_request_release(&keys, 0x62));
  assert(held_keys_pop_due_release(&keys, 1090, &release));
  assert(release.code == 0x62);
  assert(held_keys_empty(&keys));

  /* A drawn shifted action owns the one-shot context until that action ends. */
  assert(held_keys_press(&keys, 0x12, 2000, 75, true));
  assert(held_keys_request_release(&keys, 0x12));
  assert(held_keys_pop_due_release(&keys, 2075, &release));
  assert(release.code == 0x12);
  assert(release.consumes_context);

  /* Reset/power paths can drain every held matrix key without leaving one. */
  for (uint16_t code = 0; code < HELD_KEYS_CAPACITY; ++code)
    assert(held_keys_press(&keys, code, 3000, 75, false));
  assert(!held_keys_press(&keys, 0x44, 3000, 75, false));
  for (int count = 0; count < HELD_KEYS_CAPACITY; ++count)
    assert(held_keys_pop_any(&keys, &release));
  assert(!held_keys_pop_any(&keys, &release));
  assert(held_keys_empty(&keys));
  return 0;
}
