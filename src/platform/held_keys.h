#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HELD_KEYS_CAPACITY 8
#define HELD_KEY_NONE 0xffffu

typedef struct {
  uint16_t code;
  uint32_t release_at_ms;
  bool release_requested;
  bool consumes_context;
} held_key_slot_t;

typedef struct {
  held_key_slot_t slots[HELD_KEYS_CAPACITY];
} held_keys_t;

typedef struct {
  uint16_t code;
  bool consumes_context;
} held_key_release_t;

void held_keys_init(held_keys_t *keys);
bool held_keys_empty(const held_keys_t *keys);
bool held_keys_press(held_keys_t *keys, uint16_t code, uint32_t now_ms,
                     uint32_t minimum_hold_ms, bool consumes_context);
bool held_keys_request_release(held_keys_t *keys, uint16_t code);
bool held_keys_pop_due_release(held_keys_t *keys, uint32_t now_ms,
                               held_key_release_t *release);
bool held_keys_release_waiting(const held_keys_t *keys);
bool held_keys_pop_any(held_keys_t *keys, held_key_release_t *release);
