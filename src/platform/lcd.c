#include "lcd.h"

#include <stdarg.h>
#include <stdio.h>
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
#define LCD_SPI_HZ (25 * 1000 * 1000)
#define LCD_PIO_WAIT_TIMEOUT_US 5000ull

static uint8_t s_line[LCD_W * 3];
static uint s_pio_sm;
static uint s_pio_offset;
static uint8_t s_hp48_levels[64][131];
static uint8_t s_hp48_target_levels[64][131];
static int16_t s_hp48_first_changed[64];
static int16_t s_hp48_last_changed[64];
static uint8_t s_hp48_target_shades[3][3];
static uint32_t s_hp48_ink;
static uint32_t s_hp48_paper;
static uint32_t s_hp48_target_ink;
static uint32_t s_hp48_target_paper;
static bool s_hp48_bitmap_valid;
static bool s_hp48_bitmap_busy;
static int s_hp48_next_row;
static int s_hp48_target_x;
static int s_hp48_target_y;

static void select(bool on) { gpio_put(LCD_CS, !on); }

static void recover_pio_transport(void) {
  pio_sm_set_enabled(LCD_PIO, s_pio_sm, false);
  pio_sm_clear_fifos(LCD_PIO, s_pio_sm);
  pio_sm_restart(LCD_PIO, s_pio_sm);
  pio_sm_exec(LCD_PIO, s_pio_sm,
              pio_encode_jmp(s_pio_offset + lcd_spi_offset_entry_point));
  pio_sm_set_pins_with_mask(LCD_PIO, s_pio_sm, 1u << LCD_SCK,
                            (1u << LCD_SCK) | (1u << LCD_MOSI));
  pio_sm_set_enabled(LCD_PIO, s_pio_sm, true);
}

static bool pio_write8(uint8_t value) {
  bool ok = true;
  uint64_t deadline_us = time_us_64() + LCD_PIO_WAIT_TIMEOUT_US;
  while (pio_sm_is_tx_fifo_full(LCD_PIO, s_pio_sm)) {
    if (time_us_64() >= deadline_us) {
      recover_pio_transport();
      printf("[HP48] LCD PIO TX timeout; transport restarted\n");
      ok = false;
      break;
    }
    tight_loop_contents();
  }
  pio_sm_put(LCD_PIO, s_pio_sm, (uint32_t)value << 24);
  return ok;
}

static bool pio_wait_idle(void) {
  uint64_t deadline_us = time_us_64() + LCD_PIO_WAIT_TIMEOUT_US;
  while (!pio_sm_is_tx_fifo_empty(LCD_PIO, s_pio_sm)) {
    if (time_us_64() >= deadline_us) {
      recover_pio_transport();
      printf("[HP48] LCD PIO drain timeout; transport restarted\n");
      return false;
    }
    tight_loop_contents();
  }
  /* FIFO-empty can precede completion of the byte currently in the output
   * shift register. At 25 MHz that byte takes under 0.4 us; two microseconds
   * is bounded and comfortably covers it without relying on the racy TXSTALL
   * flag. */
  busy_wait_us(2);
  return true;
}

static bool write_bytes(const uint8_t *values, size_t count) {
  bool ok = true;
  for (size_t i = 0; i < count; ++i) ok = pio_write8(values[i]) && ok;
  return pio_wait_idle() && ok;
}

static void command(uint8_t value) {
  gpio_put(LCD_DC, 0);
  select(true);
  (void)write_bytes(&value, 1);
  select(false);
}

static void data(const uint8_t *values, size_t count) {
  gpio_put(LCD_DC, 1);
  select(true);
  (void)write_bytes(values, count);
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
  /* Current PicoCalc panels use mode-3 signaling. ClockworkPi's PicoCalc LCD
   * examples drive this bus at 25 MHz; matching that rate lets a scaled HP
   * frame finish in roughly 42 ms instead of 105 ms. */
  s_pio_offset = pio_add_program(LCD_PIO, &lcd_spi_program);
  pio_sm_config cfg = lcd_spi_program_get_default_config(s_pio_offset);
  sm_config_set_out_pins(&cfg, LCD_MOSI, 1);
  sm_config_set_sideset_pins(&cfg, LCD_SCK);
  sm_config_set_out_shift(&cfg, false, false, 32);
  sm_config_set_fifo_join(&cfg, PIO_FIFO_JOIN_TX);
  uint32_t divider = (clock_get_hz(clk_sys) + (LCD_SPI_HZ * 2) - 1) /
                     (LCD_SPI_HZ * 2);
  if (divider < 1) divider = 1;
  sm_config_set_clkdiv(&cfg, (float)divider);
  s_pio_sm = pio_claim_unused_sm(LCD_PIO, true);
  pio_sm_init(LCD_PIO, s_pio_sm, s_pio_offset, &cfg);
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
  for (int row = 0; row < h; ++row) (void)write_bytes(s_line, w * 3);
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

static void mix_rgb_bytes(uint32_t ink, uint32_t paper, unsigned level,
                          uint8_t out[3]) {
  if (level > 2u) level = 2u;
  for (int component = 0; component < 3; ++component) {
    unsigned shift = (unsigned)(2 - component) * 8u;
    unsigned ink_component = (ink >> shift) & 0xffu;
    unsigned paper_component = (paper >> shift) & 0xffu;
    unsigned mixed =
        (paper_component * (2u - level) + ink_component * level + 1u) / 2u;
    out[component] = (uint8_t)(mixed & 0xfcu);
  }
}

static void lcd_queue_hp48_planes(int x, int y,
                                  const uint8_t plane_a[64][17],
                                  const uint8_t plane_b[64][17],
                                  uint32_t ink, uint32_t paper) {
  if (s_hp48_bitmap_busy) return;
  for (unsigned level = 0; level < 3; ++level)
    mix_rgb_bytes(ink, paper, level, s_hp48_target_shades[level]);

  bool full = !s_hp48_bitmap_valid || ink != s_hp48_ink ||
              paper != s_hp48_paper;
  bool changed = false;

  /* Compute the complete target and the dirty span of each source row before
   * starting panel work. lcd_service() sends at most one row span per main
   * loop, so Saturn execution, scan-counter polling, sound, and input continue
   * between transfers without DMA, a second core, or an inter-core lock. */
  for (int sy = 0; sy < 64; ++sy) {
    int first_sx = 0;
    int last_sx = 130;
    for (int sx = 0; sx < 131; ++sx) {
      unsigned mask = 1u << (sx & 7);
      s_hp48_target_levels[sy][sx] =
          (uint8_t)(((plane_a[sy][sx >> 3] & mask) != 0) +
                    ((plane_b[sy][sx >> 3] & mask) != 0));
    }
    if (!full) {
      first_sx = -1;
      for (int sx = 0; sx < 131; ++sx) {
        if (s_hp48_levels[sy][sx] ==
            s_hp48_target_levels[sy][sx]) continue;
        if (first_sx < 0) first_sx = sx;
        last_sx = sx;
      }
    }
    s_hp48_first_changed[sy] = (int16_t)first_sx;
    s_hp48_last_changed[sy] = (int16_t)last_sx;
    if (first_sx >= 0) changed = true;
  }

  if (!changed) return;
  s_hp48_target_ink = ink;
  s_hp48_target_paper = paper;
  s_hp48_target_x = x;
  s_hp48_target_y = y;
  s_hp48_next_row = 0;
  s_hp48_bitmap_busy = true;
}

static void finish_hp48_frame(void) {
  s_hp48_ink = s_hp48_target_ink;
  s_hp48_paper = s_hp48_target_paper;
  s_hp48_bitmap_valid = true;
  s_hp48_bitmap_busy = false;
}

static void service_hp48_row(void) {
  while (s_hp48_next_row < 64 &&
         s_hp48_first_changed[s_hp48_next_row] < 0)
    ++s_hp48_next_row;
  if (s_hp48_next_row >= 64) {
    finish_hp48_frame();
    return;
  }

  int sy = s_hp48_next_row++;
  int first_sx = s_hp48_first_changed[sy];
  int last_sx = s_hp48_last_changed[sy];

  int first_dx = (first_sx * HP48_LCD_RENDER_W + 130) / 131;
  int dx_after = ((last_sx + 1) * HP48_LCD_RENDER_W + 130) / 131;
  int first_dy = (sy * HP48_LCD_RENDER_H + 63) / 64;
  int dy_after = ((sy + 1) * HP48_LCD_RENDER_H + 63) / 64;
  int width = dx_after - first_dx;
  int height = dy_after - first_dy;

  region(s_hp48_target_x + first_dx, s_hp48_target_y + first_dy,
         width, height);
  gpio_put(LCD_DC, 1);
  select(true);
  for (int dx = first_dx; dx < dx_after; ++dx) {
    int sx = dx * 131 / HP48_LCD_RENDER_W;
    const uint8_t *pixel =
        s_hp48_target_shades[s_hp48_target_levels[sy][sx]];
    memcpy(&s_line[(dx - first_dx) * 3], pixel, 3);
  }
  bool ok = true;
  for (int row = 0; row < height; ++row)
    ok = write_bytes(s_line, (size_t)width * 3) && ok;
  select(false);

  if (!ok) {
    s_hp48_bitmap_valid = false;
    s_hp48_bitmap_busy = false;
    return;
  }
  memcpy(s_hp48_levels[sy], s_hp48_target_levels[sy],
         sizeof(s_hp48_levels[sy]));

  while (s_hp48_next_row < 64 &&
         s_hp48_first_changed[s_hp48_next_row] < 0)
    ++s_hp48_next_row;
  if (s_hp48_next_row >= 64) finish_hp48_frame();
}

void lcd_draw_hp48_bitmap(int x, int y, const uint8_t bitmap[64][17],
                          uint32_t ink, uint32_t paper) {
  lcd_queue_hp48_planes(x, y, bitmap, bitmap, ink, paper);
}

void lcd_draw_hp48_grayscale(int x, int y,
                             const uint8_t plane_a[64][17],
                             const uint8_t plane_b[64][17],
                             uint32_t ink, uint32_t paper) {
  lcd_queue_hp48_planes(x, y, plane_a, plane_b, ink, paper);
}

bool lcd_hp48_bitmap_busy(void) { return s_hp48_bitmap_busy; }

void lcd_service(void) {
  if (s_hp48_bitmap_busy) service_hp48_row();
}
