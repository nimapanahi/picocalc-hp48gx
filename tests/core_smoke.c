#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "core_runtime.h"

int main(void) {
  static uint8_t rom[0x100000];
  assert(!hp48_core_init(rom, sizeof(rom) - 1));
  assert(hp48_core_init(rom, sizeof(rom)));
  assert(hp48_ram_size() == 0x40000);
  hp48_key_set(0x44, true);
  assert(saturn.keybuf.rows[4] & (1 << 4));
  hp48_key_set(0x44, false);
  assert(!(saturn.keybuf.rows[4] & (1 << 4)));
  hp48_core_run(32);
  hp48_core_reset();
  assert(saturn.PC == 0);
  return 0;
}

