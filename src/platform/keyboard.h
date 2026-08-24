#pragma once

#include <stdbool.h>
#include <stdint.h>

bool keyboard_init(void);
bool keyboard_poll(void);
bool keyboard_set_lcd_backlight(uint8_t brightness);
bool keyboard_take_save_request(void);
bool keyboard_take_reset_request(void);
bool keyboard_take_file_cycle_request(void);
bool keyboard_take_file_import_request(void);
bool keyboard_take_file_export_request(void);
int keyboard_take_speed_adjustment(void);
