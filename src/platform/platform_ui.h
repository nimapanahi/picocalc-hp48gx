#pragma once


#include <stdbool.h>
#include <stdint.h>

void platform_ui_init(void);
void platform_ui_present_lcd(const uint8_t bitmap[64][17], uint8_t annunciators,
                             bool lcd_on, uint8_t contrast);
void platform_ui_set_text_mode(bool enabled);
void platform_ui_set_prefix_preview(uint16_t shift_code);
void platform_ui_set_battery(uint8_t percent, bool charging, bool valid);
void platform_ui_status(const char *message);
void platform_ui_nav_move(int dx, int dy);
uint16_t platform_ui_nav_selected_code(void);
const char *platform_ui_nav_selected_label(void);
void platform_ui_nav_set_pressed(bool pressed);
