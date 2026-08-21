#include "lcd.h"

#include <stdarg.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "lcd_spi.pio.h"
#include "pico/stdlib.h"

#define LCD_PIO pio0
#define LCD_SCK 10
#define LCD_MOSI 11
#define LCD_CS 13
#define LCD_DC 14
#define LCD_RST 15
#define LCD_W 320
#define LCD_H 320
#define LCD_SPI_HZ (10 * 1000 * 1000)

static uint8_t s_line[LCD_W * 3];
static uint s_pio_sm;

static void select(bool on) { gpio_put(LCD_CS, !on); }

static inline void pio_write8(uint8_t value) {
  pio_sm_put_blocking(LCD_PIO, s_pio_sm, (uint32_t)value << 24);
}

static void pio_wait_idle(void) {
  while (!pio_sm_is_tx_fifo_empty(LCD_PIO, s_pio_sm)) tight_loop_contents();
  uint32_t stall = 1u << (PIO_FDEBUG_TXSTALL_LSB + s_pio_sm);
  LCD_PIO->fdebug = stall;
  while (!(LCD_PIO->fdebug & stall)) tight_loop_contents();
  busy_wait_us(1);
}

static void write_bytes(const uint8_t *values, size_t count) {
  for (size_t i = 0; i < count; ++i) pio_write8(values[i]);
  pio_wait_idle();
}

static void command(uint8_t value) {
  gpio_put(LCD_DC, 0);
  select(true);
  write_bytes(&value, 1);
  select(false);
}

static void data(const uint8_t *values, size_t count) {
  gpio_put(LCD_DC, 1);
  select(true);
  write_bytes(values, count);
  select(false);
}

static void command_data(uint8_t cmd, const uint8_t *values, size_t count) {
  command(cmd);
  if (count) data(values, count);
}

static void region(int x, int y, int w, int h) {
  uint8_t c[4] = {(uint8_t)(x >> 8), (uint8_t)x,
                  (uint8_t)((x + w - 1) >> 8), (uint8_t)(x + w - 1)};
  uint8_t r[4] = {(uint8_t)(y >> 8), (uint8_t)y,
                  (uint8_t)((y + h - 1) >> 8), (uint8_t)(y + h - 1)};
  command_data(0x2a, c, sizeof(c));
  command_data(0x2b, r, sizeof(r));
  command(0x2c);
}

static void rgb_bytes(uint32_t rgb, uint8_t out[3]) {
  /* ILI9488 serial mode consumes three bytes per pixel.  Masking to six
   * significant bits per component matches COLMOD=0x66 without changing the
   * convenient RGB888 color constants used by the UI. */
  out[0] = (uint8_t)((rgb >> 16) & 0xfcu);
  out[1] = (uint8_t)((rgb >> 8) & 0xfcu);
  out[2] = (uint8_t)(rgb & 0xfcu);
}

void lcd_init(void) {
  /* Current PicoCalc panels use mode-3 signaling.  Drive it with the same PIO
   * transport as the proven ST7365P path, conservatively clocked at 10 MHz. */
  uint offset = pio_add_program(LCD_PIO, &lcd_spi_program);
  pio_sm_config cfg = lcd_spi_program_get_default_config(offset);
  sm_config_set_out_pins(&cfg, LCD_MOSI, 1);
  sm_config_set_sideset_pins(&cfg, LCD_SCK);
  sm_config_set_out_shift(&cfg, false, false, 32);
  sm_config_set_fifo_join(&cfg, PIO_FIFO_JOIN_TX);
  uint32_t divider = (clock_get_hz(clk_sys) + (LCD_SPI_HZ * 2) - 1) /
                     (LCD_SPI_HZ * 2);
  if (divider < 1) divider = 1;
  sm_config_set_clkdiv(&cfg, (float)divider);
  s_pio_sm = pio_claim_unused_sm(LCD_PIO, true);
  pio_sm_init(LCD_PIO, s_pio_sm, offset, &cfg);
  pio_sm_set_pins_with_mask(LCD_PIO, s_pio_sm, 1u << LCD_SCK,
                            (1u << LCD_SCK) | (1u << LCD_MOSI));
  pio_sm_set_pindirs_with_mask(LCD_PIO, s_pio_sm,
                               (1u << LCD_SCK) | (1u << LCD_MOSI),
                               (1u << LCD_SCK) | (1u << LCD_MOSI));
  pio_gpio_init(LCD_PIO, LCD_MOSI);
  pio_gpio_init(LCD_PIO, LCD_SCK);
  pio_sm_set_enabled(LCD_PIO, s_pio_sm, true);

  for (int pin = LCD_CS; pin <= LCD_RST; ++pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 1);
  }
  gpio_put(LCD_RST, 0);
  sleep_ms(10);
  gpio_put(LCD_RST, 1);
  sleep_ms(120);

  command(0x01); /* software reset */
  sleep_ms(10);

  static const uint8_t gamma_p[] = {0x00,0x03,0x09,0x08,0x16,0x0a,0x3f,0x78,
                                    0x4c,0x09,0x0a,0x08,0x16,0x1a,0x0f};
  static const uint8_t gamma_n[] = {0x00,0x16,0x19,0x03,0x0f,0x05,0x32,0x45,
                                    0x46,0x04,0x0e,0x0d,0x35,0x37,0x0f};
  command_data(0xe0, gamma_p, sizeof(gamma_p));
  command_data(0xe1, gamma_n, sizeof(gamma_n));
  command_data(0xc0, (const uint8_t[]){0x17,0x15}, 2);
  command_data(0xc1, (const uint8_t[]){0x41}, 1);
  command_data(0xc5, (const uint8_t[]){0x00,0x12,0x80}, 3);
  command_data(0x36, (const uint8_t[]){0x48}, 1);
  command_data(0x3a, (const uint8_t[]){0x66}, 1); /* RGB666, 3 bytes/pixel */
  command_data(0xb0, (const uint8_t[]){0x00}, 1);
  command_data(0xb1, (const uint8_t[]){0xa0}, 1);
  command(0x21);
  command_data(0xb4, (const uint8_t[]){0x02}, 1);
  command_data(0xb6, (const uint8_t[]){0x02,0x02,0x3b}, 3);
  command_data(0xb7, (const uint8_t[]){0xc6}, 1);
  command_data(0xe9, (const uint8_t[]){0x00}, 1);
  command_data(0xf7, (const uint8_t[]){0xa9,0x51,0x2c,0x82}, 4);
  command(0x11);
  sleep_ms(120);
  command(0x29);
  sleep_ms(120);
}

void lcd_fill_rect(int x, int y, int w, int h, uint32_t rgb) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > LCD_W) w = LCD_W - x;
  if (y + h > LCD_H) h = LCD_H - y;
  if (w <= 0 || h <= 0) return;
  uint8_t pixel[3];
  rgb_bytes(rgb, pixel);
  for (int i = 0; i < w; ++i) memcpy(&s_line[i * 3], pixel, 3);
  region(x, y, w, h);
  gpio_put(LCD_DC, 1);
  select(true);
  for (int row = 0; row < h; ++row) write_bytes(s_line, w * 3);
  select(false);
}

void lcd_fill(uint32_t rgb) { lcd_fill_rect(0, 0, LCD_W, LCD_H, rgb); }

typedef struct { char ch; uint8_t col[5]; } glyph_t;
static const glyph_t s_font[] = {
  {' ',{0,0,0,0,0}}, {'!',{0,0,0x5f,0,0}}, {'\'',{0,0x03,0x07,0,0}},
  {'(',{0,0x1c,0x22,0x41,0}},
  {')',{0,0x41,0x22,0x1c,0}}, {'*',{0x14,0x08,0x3e,0x08,0x14}},
  {'+',{0x08,0x08,0x3e,0x08,0x08}}, {',',{0,0x50,0x30,0,0}},
  {'-',{0x08,0x08,0x08,0x08,0x08}}, {'.',{0,0x60,0x60,0,0}},
  {'/',{0x20,0x10,0x08,0x04,0x02}}, {':',{0,0x36,0x36,0,0}},
  {'=',{0x14,0x14,0x14,0x14,0x14}}, {'<',{0x08,0x14,0x22,0x41,0}},
  {'>',{0x41,0x22,0x14,0x08,0}}, {'[',{0,0x7f,0x41,0x41,0}},
  {']',{0,0x41,0x41,0x7f,0}}, {'_',{0x40,0x40,0x40,0x40,0x40}},
  {'^',{0x04,0x02,0x01,0x02,0x04}}, {'|',{0,0,0x7f,0,0}},
  {'0',{0x3e,0x51,0x49,0x45,0x3e}}, {'1',{0,0x42,0x7f,0x40,0}},
  {'2',{0x42,0x61,0x51,0x49,0x46}}, {'3',{0x21,0x41,0x45,0x4b,0x31}},
  {'4',{0x18,0x14,0x12,0x7f,0x10}}, {'5',{0x27,0x45,0x45,0x45,0x39}},
  {'6',{0x3c,0x4a,0x49,0x49,0x30}}, {'7',{0x01,0x71,0x09,0x05,0x03}},
  {'8',{0x36,0x49,0x49,0x49,0x36}}, {'9',{0x06,0x49,0x49,0x29,0x1e}},
  {'A',{0x7e,0x11,0x11,0x11,0x7e}}, {'B',{0x7f,0x49,0x49,0x49,0x36}},
  {'C',{0x3e,0x41,0x41,0x41,0x22}}, {'D',{0x7f,0x41,0x41,0x22,0x1c}},
  {'E',{0x7f,0x49,0x49,0x49,0x41}}, {'F',{0x7f,0x09,0x09,0x09,0x01}},
  {'G',{0x3e,0x41,0x49,0x49,0x7a}}, {'H',{0x7f,0x08,0x08,0x08,0x7f}},
  {'I',{0,0x41,0x7f,0x41,0}}, {'J',{0x20,0x40,0x41,0x3f,0x01}},
  {'K',{0x7f,0x08,0x14,0x22,0x41}}, {'L',{0x7f,0x40,0x40,0x40,0x40}},
  {'M',{0x7f,0x02,0x04,0x02,0x7f}}, {'N',{0x7f,0x04,0x08,0x10,0x7f}},
  {'O',{0x3e,0x41,0x41,0x41,0x3e}}, {'P',{0x7f,0x09,0x09,0x09,0x06}},
  {'Q',{0x3e,0x41,0x51,0x21,0x5e}}, {'R',{0x7f,0x09,0x19,0x29,0x46}},
  {'S',{0x46,0x49,0x49,0x49,0x31}}, {'T',{0x01,0x01,0x7f,0x01,0x01}},
  {'U',{0x3f,0x40,0x40,0x40,0x3f}}, {'V',{0x1f,0x20,0x40,0x20,0x1f}},
  {'W',{0x3f,0x40,0x38,0x40,0x3f}}, {'X',{0x63,0x14,0x08,0x14,0x63}},
  {'Y',{0x07,0x08,0x70,0x08,0x07}}, {'Z',{0x61,0x51,0x49,0x45,0x43}},
};

static const uint8_t *glyph(char ch) {
  if (ch >= 'a' && ch <= 'z') ch -= 32;
  for (size_t i = 0; i < sizeof(s_font) / sizeof(s_font[0]); ++i)
    if (s_font[i].ch == ch) return s_font[i].col;
  return s_font[0].col;
}

void lcd_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg, int scale) {
  if (!text || scale < 1) return;
  while (*text) {
    const uint8_t *g = glyph(*text++);
    for (int cy = 0; cy < 7; ++cy) {
      for (int cx = 0; cx < 6; ++cx) {
        bool on = cx < 5 && ((g[cx] >> cy) & 1);
        lcd_fill_rect(x + cx * scale, y + cy * scale, scale, scale, on ? fg : bg);
      }
    }
    x += 6 * scale;
  }
}

void lcd_draw_hp48_bitmap(int x, int y, const uint8_t bitmap[64][17],
                          uint32_t ink, uint32_t paper) {
  uint8_t ink_b[3], paper_b[3];
  rgb_bytes(ink, ink_b);
  rgb_bytes(paper, paper_b);
  region(x, y, HP48_LCD_RENDER_W, HP48_LCD_RENDER_H);
  gpio_put(LCD_DC, 1);
  select(true);
  for (int dy = 0; dy < HP48_LCD_RENDER_H; ++dy) {
    int sy = dy * 64 / HP48_LCD_RENDER_H;
    for (int dx = 0; dx < HP48_LCD_RENDER_W; ++dx) {
      int sx = dx * 131 / HP48_LCD_RENDER_W;
      const uint8_t *p = (bitmap[sy][sx >> 3] & (1u << (sx & 7))) ? ink_b : paper_b;
      memcpy(&s_line[dx * 3], p, 3);
    }
    write_bytes(s_line, HP48_LCD_RENDER_W * 3);
  }
  select(false);
}
