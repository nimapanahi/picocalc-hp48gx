#include "held_keys.h"

static bool deadline_reached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

void held_keys_init(held_keys_t *keys) {
  if (!keys) return;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i) {
    keys->slots[i].code = HELD_KEY_NONE;
    keys->slots[i].release_at_ms = 0;
    keys->slots[i].release_requested = false;
    keys->slots[i].consumes_context = false;
  }
}

bool held_keys_empty(const held_keys_t *keys) {
  if (!keys) return true;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i)
    if (keys->slots[i].code != HELD_KEY_NONE) return false;
  return true;
}

bool held_keys_press(held_keys_t *keys, uint16_t code, uint32_t now_ms,
                     uint32_t minimum_hold_ms, bool consumes_context) {
  if (!keys || code == HELD_KEY_NONE) return false;
  held_key_slot_t *free_slot = NULL;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i) {
    held_key_slot_t *slot = &keys->slots[i];
    if (slot->code == code) {
      slot->consumes_context =
          slot->consumes_context || consumes_context;
      return true;
    }
    if (!free_slot && slot->code == HELD_KEY_NONE) free_slot = slot;
  }
  if (!free_slot) return false;
  free_slot->code = code;
  free_slot->release_at_ms = now_ms + minimum_hold_ms;
  free_slot->release_requested = false;
  free_slot->consumes_context = consumes_context;
  return true;
}

bool held_keys_request_release(held_keys_t *keys, uint16_t code) {
  if (!keys) return false;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i) {
    if (keys->slots[i].code == code) {
      keys->slots[i].release_requested = true;
      return true;
    }
  }
  return false;
}

static bool pop_slot(held_key_slot_t *slot, held_key_release_t *release) {
  if (!slot || slot->code == HELD_KEY_NONE || !release) return false;
  release->code = slot->code;
  release->consumes_context = slot->consumes_context;
  slot->code = HELD_KEY_NONE;
  slot->release_at_ms = 0;
  slot->release_requested = false;
  slot->consumes_context = false;
  return true;
}

bool held_keys_pop_due_release(held_keys_t *keys, uint32_t now_ms,
                               held_key_release_t *release) {
  if (!keys || !release) return false;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i) {
    held_key_slot_t *slot = &keys->slots[i];
    if (slot->code != HELD_KEY_NONE && slot->release_requested &&
        deadline_reached(now_ms, slot->release_at_ms))
      return pop_slot(slot, release);
  }
  return false;
}

bool held_keys_release_waiting(const held_keys_t *keys) {
  if (!keys) return false;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i)
    if (keys->slots[i].code != HELD_KEY_NONE &&
        keys->slots[i].release_requested)
      return true;
  return false;
}

bool held_keys_pop_any(held_keys_t *keys, held_key_release_t *release) {
  if (!keys || !release) return false;
  for (size_t i = 0; i < HELD_KEYS_CAPACITY; ++i)
    if (keys->slots[i].code != HELD_KEY_NONE)
      return pop_slot(&keys->slots[i], release);
  return false;
}
