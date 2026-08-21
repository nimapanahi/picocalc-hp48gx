#include "hp48.h"

#include <string.h>

#include "device.h"
#include "platform_ui.h"
#include "x48_x11.h"

display_t display;
disp_t disp = {.w = 131, .h = 64, .mapped = 1};

static uint8_t s_bitmap[64][17];

void init_display(void) {
  display.on = (saturn.disp_io & 0x8) >> 3;
  display.disp_start = saturn.disp_addr & 0xffffe;
  display.offset = saturn.disp_io & 0x7;
  display.lines = saturn.line_count & 0x3f;
  if (display.lines == 0) display.lines = 63;
  display.nibs_per_line = (NIBBLES_PER_ROW + saturn.line_offset
                         + (display.offset > 3 ? 2 : 0)) & 0xfff;
  display.disp_end = display.disp_start + display.nibs_per_line * (display.lines + 1);
  display.menu_start = saturn.menu_addr;
  display.menu_end = display.menu_start + 0x110;
  display.contrast = saturn.contrast_ctrl | ((saturn.disp_test & 1) << 4);
  display.annunc = saturn.annunc;
  update_display();
}

static uint8_t row_pixel(long row_addr, int x, int bit_offset) {
  int src_bit = x + bit_offset;
  int nib = read_nibble(row_addr + (src_bit >> 2));
  return (nib >> (src_bit & 3)) & 1;
}

void update_display(void) {
  memset(s_bitmap, 0, sizeof(s_bitmap));
  if (display.on) {
    int main_rows = display.lines + 1;
    if (main_rows > 64) main_rows = 64;
    for (int y = 0; y < main_rows; ++y) {
      long addr = display.disp_start + (long)y * display.nibs_per_line;
      for (int x = 0; x < 131; ++x) {
        if (row_pixel(addr, x, display.offset)) s_bitmap[y][x >> 3] |= 1u << (x & 7);
      }
    }
    for (int y = main_rows; y < 64; ++y) {
      long addr = display.menu_start + (long)(y - main_rows) * NIBBLES_PER_ROW;
      for (int x = 0; x < 131; ++x) {
        if (row_pixel(addr, x, 0)) s_bitmap[y][x >> 3] |= 1u << (x & 7);
      }
    }
  }
  platform_ui_present_lcd(s_bitmap, (uint8_t)display.annunc, display.on != 0,
                          (uint8_t)display.contrast);
}

void redraw_display(void) { update_display(); }
void disp_draw_nibble(word_20 addr, word_4 val) { (void)addr; (void)val; }
void menu_draw_nibble(word_20 addr, word_4 val) { (void)addr; (void)val; }
void draw_annunc(void) {
  display.annunc = saturn.annunc;
  update_display();
}
void redraw_annunc(void) { draw_annunc(); }
void init_annunc(void) {}
void adjust_contrast(int contrast) { display.contrast = contrast; }
void refresh_icon(void) {}

