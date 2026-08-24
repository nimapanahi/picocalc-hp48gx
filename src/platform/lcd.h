#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HP48_LCD_RENDER_W 300
#define HP48_LCD_RENDER_H 146

void lcd_init(void);
void lcd_fill(uint32_t rgb888);
void lcd_fill_rect(int x, int y, int w, int h, uint32_t rgb888);
void lcd_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg, int scale);
void lcd_draw_hp48_bitmap(int x, int y, const uint8_t bitmap[64][17],
                          uint32_t ink, uint32_t paper);
void lcd_draw_hp48_grayscale(int x, int y,
                             const uint8_t plane_a[64][17],
                             const uint8_t plane_b[64][17],
                             uint32_t ink, uint32_t paper);
void lcd_service(void);
bool lcd_hp48_bitmap_busy(void);
