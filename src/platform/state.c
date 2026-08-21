#include "state.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core_runtime.h"
#include "hp48_emu.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

#define STATE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - 0x40000u)
#define STATE_HEADER_BYTES 0x1000u
#define STATE_MAGIC 0x48343853u
#define STATE_VERSION 1u

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t saturn_size;
  uint32_t packed_ram_size;
  uint32_t rom_fingerprint;
  uint32_t saturn_crc;
  uint32_t ram_crc;
  saturn_t saved_saturn;
} saved_header_t;

extern uint8_t __flash_binary_end;
extern const uint8_t hp48_rom[];
extern const uint8_t hp48_rom_end[];

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size) {
  crc = ~crc;
  while (size--) {
    crc ^= *data++;
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static uint32_t rom_fingerprint(void) {
  return crc32_update(0, hp48_rom, (size_t)(hp48_rom_end - hp48_rom));
}

static bool flash_layout_ok(void) {
  uintptr_t binary_end = (uintptr_t)&__flash_binary_end - XIP_BASE;
  return binary_end < STATE_FLASH_OFFSET;
}

bool state_save(void) {
  if (!flash_layout_ok()) return false;
  static uint8_t header_sector[STATE_HEADER_BYTES];
  memset(header_sector, 0xff, sizeof(header_sector));
  saved_header_t *h = (saved_header_t *)header_sector;
  h->magic = STATE_MAGIC;
  h->version = STATE_VERSION;
  h->saturn_size = sizeof(saturn_t);
  h->packed_ram_size = RAM_SIZE_GX / 2;
  h->rom_fingerprint = rom_fingerprint();
  h->saved_saturn = saturn;
  h->saved_saturn.rom = NULL;
  h->saved_saturn.ram = NULL;
  h->saved_saturn.port1 = NULL;
  h->saved_saturn.port2 = NULL;
  h->saturn_crc = crc32_update(0, (const uint8_t *)&h->saved_saturn, sizeof(saturn_t));

  uint8_t *ram = hp48_ram_data();
  uint32_t ram_crc = 0;
  for (size_t i = 0; i < RAM_SIZE_GX; i += 2) {
    uint8_t packed = (ram[i] & 0x0f) | ((ram[i + 1] & 0x0f) << 4);
    ram_crc = crc32_update(ram_crc, &packed, 1);
  }
  h->ram_crc = ram_crc;

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(STATE_FLASH_OFFSET, STATE_HEADER_BYTES + RAM_SIZE_GX / 2);
  flash_range_program(STATE_FLASH_OFFSET, header_sector, sizeof(header_sector));
  uint8_t page[FLASH_PAGE_SIZE];
  for (size_t packed_off = 0; packed_off < RAM_SIZE_GX / 2; packed_off += sizeof(page)) {
    for (size_t j = 0; j < sizeof(page); ++j) {
      size_t nib = (packed_off + j) * 2;
      page[j] = (ram[nib] & 0x0f) | ((ram[nib + 1] & 0x0f) << 4);
    }
    flash_range_program(STATE_FLASH_OFFSET + STATE_HEADER_BYTES + packed_off, page, sizeof(page));
  }
  restore_interrupts(ints);
  return true;
}

bool state_load(void) {
  const saved_header_t *h = (const saved_header_t *)(XIP_BASE + STATE_FLASH_OFFSET);
  if (h->magic != STATE_MAGIC || h->version != STATE_VERSION ||
      h->saturn_size != sizeof(saturn_t) || h->packed_ram_size != RAM_SIZE_GX / 2 ||
      h->rom_fingerprint != rom_fingerprint()) return false;
  if (crc32_update(0, (const uint8_t *)&h->saved_saturn, sizeof(saturn_t)) != h->saturn_crc) return false;

  const uint8_t *packed = (const uint8_t *)(XIP_BASE + STATE_FLASH_OFFSET + STATE_HEADER_BYTES);
  if (crc32_update(0, packed, RAM_SIZE_GX / 2) != h->ram_crc) return false;
  uint8_t *ram = hp48_ram_data();
  for (size_t i = 0; i < RAM_SIZE_GX / 2; ++i) {
    ram[i * 2] = packed[i] & 0x0f;
    ram[i * 2 + 1] = packed[i] >> 4;
  }
  const uint8_t *rom = saturn.rom;
  saturn = h->saved_saturn;
  saturn.rom = rom;
  saturn.ram = ram;
  saturn.port1 = NULL;
  saturn.port2 = NULL;
  dev_memory_init();
  init_display();
  return true;
}

void state_clear(void) {
  if (!flash_layout_ok()) return;
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(STATE_FLASH_OFFSET, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);
}
