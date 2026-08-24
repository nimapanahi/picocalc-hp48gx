#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hp48.h"

bool hp48_core_init(const uint8_t *unpacked_rom, size_t rom_size);
void hp48_core_reset(void);
void hp48_key_set(uint16_t matrix_code, bool pressed);
/* Report whether the stock ROM has actually latched an HP prefix. */
bool hp48_prefix_latched(uint16_t matrix_code);
unsigned hp48_core_run(unsigned instruction_budget);
/* Complete a queued native-framebuffer snapshot when its real-time quiet
 * interval expires, including while the CPU is being paced between batches. */
void hp48_core_service_display(void);
bool hp48_core_lcd_on(void);
/* True only while the stock ROM is stopped inside its SHUTDN idle loop. */
bool hp48_core_is_sleeping(void);
void hp48_core_platform_wake(void);
uint8_t *hp48_ram_data(void);
size_t hp48_ram_size(void);

#define HP48_PORT2_PACKED_SIZE 0x20000u
#define HP48_PORT2_NIBBLE_SIZE (HP48_PORT2_PACKED_SIZE * 2u)

typedef enum {
  HP48_CARD_NONE = 0,
  HP48_CARD_PORT1 = 1,
  HP48_CARD_PORT2 = 2,
} hp48_card_slot_t;

/* One 128 KiB GX card is kept in x48's portable packed format: the low nibble
 * is stored first in each byte. It can be mounted in Port 1 or Port 2; sharing
 * the buffer keeps the original one-byte-per-nibble core from consuming an
 * additional 256 KiB of SRAM for either slot. */
void hp48_port2_attach_blank(void);
void hp48_port1_attach_blank(void);
void hp48_port2_detach(void);
bool hp48_port2_attached(void);
bool hp48_port1_attached(void);
bool hp48_port2_dirty(void);
void hp48_port2_mark_clean(void);
uint8_t *hp48_port2_packed_data(void);
hp48_card_slot_t hp48_card_slot(void);
uint8_t hp48_port1_read_nibble(size_t offset);
void hp48_port1_write_nibble(size_t offset, uint8_t value);
uint8_t hp48_port2_read_nibble(size_t offset);
void hp48_port2_write_nibble(size_t offset, uint8_t value);
