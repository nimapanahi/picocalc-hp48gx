#pragma once

#include <stdbool.h>
#include <stdint.h>

bool keyboard_init(void);
bool keyboard_poll(void);
bool keyboard_set_lcd_backlight(uint8_t brightness);
bool keyboard_take_save_request(void);
bool keyboard_take_reset_request(void);
bool keyboard_text_mode(void);
