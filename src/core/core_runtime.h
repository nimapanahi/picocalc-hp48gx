#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hp48.h"

bool hp48_core_init(const uint8_t *unpacked_rom, size_t rom_size);
void hp48_core_reset(void);
void hp48_key_set(uint16_t matrix_code, bool pressed);
void hp48_core_run(unsigned instruction_budget);
bool hp48_core_lcd_on(void);
uint8_t *hp48_ram_data(void);
size_t hp48_ram_size(void);
