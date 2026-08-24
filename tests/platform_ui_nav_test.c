#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lcd.h"
#include "platform_ui.h"

static int s_lcd_bitmap_calls;
static int s_lcd_grayscale_calls;
static int s_lcd_fill_rect_calls;
static uint8_t s_last_lcd_bitmap[64][17];
static uint8_t s_last_lcd_plane_a[64][17];
static uint8_t s_last_lcd_plane_b[64][17];
static bool s_lcd_bitmap_busy;

void lcd_init(void) {}
void lcd_fill(uint32_t rgb) { (void)rgb; }
void lcd_fill_rect(int x, int y, int w, int h, uint32_t rgb) {
  (void)x; (void)y; (void)w; (void)h; (void)rgb;
  ++s_lcd_fill_rect_calls;
}
void lcd_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg,
                   int scale) {
  (void)x; (void)y; (void)text; (void)fg; (void)bg; (void)scale;
}
void lcd_draw_hp48_bitmap(int x, int y, const uint8_t bitmap[64][17],
                          uint32_t ink, uint32_t paper) {
  (void)x; (void)y; (void)ink; (void)paper;
  memcpy(s_last_lcd_bitmap, bitmap, sizeof(s_last_lcd_bitmap));
  ++s_lcd_bitmap_calls;
}
void lcd_draw_hp48_grayscale(int x, int y,
                             const uint8_t plane_a[64][17],
                             const uint8_t plane_b[64][17],
                             uint32_t ink, uint32_t paper) {
  (void)x; (void)y; (void)plane_a; (void)ink; (void)paper;
  memcpy(s_last_lcd_plane_a, plane_a, sizeof(s_last_lcd_plane_a));
  memcpy(s_last_lcd_plane_b, plane_b, sizeof(s_last_lcd_plane_b));
  memcpy(s_last_lcd_bitmap, plane_b, sizeof(s_last_lcd_bitmap));
  ++s_lcd_grayscale_calls;
}
void lcd_service(void) {}
bool lcd_hp48_bitmap_busy(void) { return s_lcd_bitmap_busy; }

static bool last_grayscale_pair_is(const uint8_t first[64][17],
                                   const uint8_t second[64][17]) {
  return (!memcmp(s_last_lcd_plane_a, first, sizeof(s_last_lcd_plane_a)) &&
          !memcmp(s_last_lcd_plane_b, second, sizeof(s_last_lcd_plane_b))) ||
         (!memcmp(s_last_lcd_plane_a, second, sizeof(s_last_lcd_plane_a)) &&
          !memcmp(s_last_lcd_plane_b, first, sizeof(s_last_lcd_plane_b)));
}

int main(void) {
  platform_ui_set_arrow_key_mode(true);
  platform_ui_set_arrow_key_mode(false);
  /* Selection starts on 7. The nearest row above contains the wide ENTER key,
   * and the row below ENTER contains ALPHA. */
  assert(platform_ui_nav_selected_code() == 0x33);
  platform_ui_nav_move(0, -1);
  assert(platform_ui_nav_selected_code() == 0x44);
  assert(strcmp(platform_ui_nav_selected_label(), "ENTER") == 0);

  platform_ui_set_context(false, 0x25);
  assert(strcmp(platform_ui_nav_selected_label(), "EQUATION") == 0);
  platform_ui_set_context(false, 0x15);
  assert(strcmp(platform_ui_nav_selected_label(), "MATRIX") == 0);
  platform_ui_set_context(false, 0xffff);
  assert(strcmp(platform_ui_nav_selected_label(), "ENTER") == 0);

  platform_ui_nav_move(0, 1);
  assert(platform_ui_nav_selected_code() == 0x35);
  platform_ui_nav_move(0, -1);
  assert(platform_ui_nav_selected_code() == 0x44);

  platform_ui_nav_move(1, 0);
  assert(platform_ui_nav_selected_code() == 0x43);
  platform_ui_nav_move(-1, 0);
  assert(platform_ui_nav_selected_code() == 0x44);

  /* The hidden left-shift 2 tools menu is labeled PORTS in the overlay. */
  platform_ui_nav_move(0, 1);
  assert(platform_ui_nav_selected_code() == 0x35);
  platform_ui_nav_move(1, 0);
  assert(platform_ui_nav_selected_code() == 0x33);
  platform_ui_nav_move(0, 1);
  assert(platform_ui_nav_selected_code() == 0x23);
  platform_ui_nav_move(0, 1);
  assert(platform_ui_nav_selected_code() == 0x13);
  platform_ui_nav_move(1, 0);
  assert(platform_ui_nav_selected_code() == 0x12);
  platform_ui_set_context(false, 0x25);
  assert(strcmp(platform_ui_nav_selected_label(), "PORTS") == 0);
  platform_ui_set_context(false, 0x15);
  assert(strcmp(platform_ui_nav_selected_label(), "LIBRARY") == 0);

  /* The ten-second battery refresh is rendered at the next idle service. */
  int fills_before_battery = s_lcd_fill_rect_calls;
  s_lcd_bitmap_busy = true;
  platform_ui_set_battery(73, false, true);
  platform_ui_service(500);
  assert(s_lcd_fill_rect_calls == fills_before_battery);
  s_lcd_bitmap_busy = false;
  platform_ui_service(500);
  assert(s_lcd_fill_rect_calls > fills_before_battery);

  /* Completed game frames may arrive faster than the physical LCD accepts
   * them. The queued successor must be the newest frame, never an OR of old
   * sprite positions that turns animation into a muddy trail. */
  uint8_t plane_a[64][17] = {{0}};
  uint8_t blank[64][17] = {{0}};
  uint8_t plane_b[64][17] = {{0}};
  plane_a[0][0] = 0x01;
  plane_b[0][0] = 0x04;
  s_lcd_bitmap_busy = true;
  platform_ui_present_lcd(plane_a, 0, true, 14, false, 1000);
  platform_ui_service(1000);
  assert(s_lcd_bitmap_calls == 0);
  s_lcd_bitmap_busy = false;
  platform_ui_present_lcd(blank, 0, true, 14, false, 2000);
  platform_ui_present_lcd(plane_b, 0, true, 14, false, 3000);
  platform_ui_service(1000);
  assert(s_lcd_bitmap_calls == 1);
  assert(s_last_lcd_bitmap[0][0] == 0x04);

  /* No redundant cleanup frame is queued. A later genuine blank still clears
   * the calculator region once the 50 ms panel interval has elapsed. */
  platform_ui_service(51000);
  assert(s_lcd_bitmap_calls == 1);

  platform_ui_present_lcd(blank, 0, true, 14, false, 51001);
  platform_ui_service(51001);
  assert(s_lcd_bitmap_calls == 2);
  assert(s_last_lcd_bitmap[0][0] == 0x00);

  /* A large A/B/A alternation inside 140 ms is grayscale, not motion. The
   * renderer receives both planes; small ordinary sprite changes above stay
   * on the one-bit path. */
  uint8_t gray_a[64][17] = {{0}};
  uint8_t gray_b[64][17] = {{0}};
  memset(gray_a, 0x33, sizeof(gray_a));
  memset(gray_b, 0xcc, sizeof(gray_b));
  platform_ui_present_lcd(gray_a, 0, true, 14, false, 100000);
  platform_ui_present_lcd(gray_b, 0, true, 14, false, 125000);
  platform_ui_present_lcd(gray_a, 0, true, 14, false, 150000);
  platform_ui_service(151001);
  assert(s_lcd_grayscale_calls == 1);
  assert(last_grayscale_pair_is(gray_a, gray_b));

  uint8_t gray_a_updated[64][17];
  memcpy(gray_a_updated, gray_a, sizeof(gray_a_updated));
  gray_a_updated[0][0] ^= 0x01;
  platform_ui_present_lcd(gray_a_updated, 0, true, 14, false, 160000);
  platform_ui_service(231001);
  assert(s_lcd_grayscale_calls == 2);
  assert(last_grayscale_pair_is(gray_a_updated, gray_b));

  /* Native code that tightly polls the LCD line counter supplies an explicit
   * temporal-grayscale hint. Even a small consecutive plane difference must
   * use the grayscale path without waiting for a high-difference A/B/A
   * signature. */
  uint8_t scan_synced[64][17];
  memcpy(scan_synced, gray_a_updated, sizeof(scan_synced));
  scan_synced[1][1] ^= 0x04;
  platform_ui_present_lcd(scan_synced, 0, true, 14, true, 240000);
  platform_ui_service(311001);
  assert(s_lcd_grayscale_calls == 3);
  assert(last_grayscale_pair_is(scan_synced, gray_b));

  /* A blocking panel transfer may leave more than one normal A/B/A cycle
   * between captures. Established grayscale remains paired across a bounded
   * delay instead of flashing one raw bitplane. */
  uint8_t delayed_plane[64][17];
  memcpy(delayed_plane, scan_synced, sizeof(delayed_plane));
  memset(delayed_plane[8], 0xff, sizeof(delayed_plane[8]));
  platform_ui_present_lcd(delayed_plane, 0, true, 14, false, 800000);
  platform_ui_service(800000);
  assert(s_lcd_grayscale_calls == 4);
  assert(last_grayscale_pair_is(delayed_plane, gray_b));

  /* A repeated hinted update of the same selected plane must not replace the
   * stored opposite plane. This is the physical Wario failure: consecutive
   * pairing produced A/A for several panel refreshes, visibly dropping the
   * mountains before B returned. */
  uint8_t repeated_plane[64][17];
  memcpy(repeated_plane, delayed_plane, sizeof(repeated_plane));
  repeated_plane[9][0] ^= 0x08;
  int bitmap_calls_before_grayscale = s_lcd_bitmap_calls;
  platform_ui_present_lcd(repeated_plane, 0, true, 14, true, 810000);
  platform_ui_service(851001);
  assert(s_lcd_grayscale_calls == 5);
  assert(s_lcd_bitmap_calls == bitmap_calls_before_grayscale);
  assert(last_grayscale_pair_is(repeated_plane, gray_b));

  uint8_t gray_b_updated[64][17];
  memcpy(gray_b_updated, gray_b, sizeof(gray_b_updated));
  gray_b_updated[12][3] ^= 0x20;
  platform_ui_present_lcd(gray_b_updated, 0, true, 14, false, 825000);
  platform_ui_service(911001);
  assert(s_lcd_grayscale_calls == 6);
  assert(s_lcd_bitmap_calls == bitmap_calls_before_grayscale);
  assert(last_grayscale_pair_is(repeated_plane, gray_b_updated));

  /* Once scan synchronization and A/B alternation have both ceased, a later
   * ordinary screen leaves the latch and returns to one-bit rendering. */
  uint8_t ordinary[64][17] = {{0}};
  ordinary[4][4] = 0x40;
  platform_ui_present_lcd(ordinary, 0, true, 14, false, 1600000);
  platform_ui_service(1600000);
  assert(s_lcd_bitmap_calls == bitmap_calls_before_grayscale + 1);
  assert(s_last_lcd_bitmap[4][4] == 0x40);

  /* LCD OFF clears both the latch and its detection history. On wake, the
   * first hinted frame is not incorrectly paired with the old game or blank
   * screen; a new substantial second plane establishes a fresh pair. */
  platform_ui_present_lcd(blank, 0, false, 14, false, 1610000);
  platform_ui_service(1650000);
  assert(s_lcd_bitmap_calls == bitmap_calls_before_grayscale + 2);
  platform_ui_present_lcd(gray_a, 0, true, 14, true, 1660000);
  platform_ui_service(1700000);
  assert(s_lcd_bitmap_calls == bitmap_calls_before_grayscale + 3);
  platform_ui_present_lcd(gray_b, 0, true, 14, true, 1675000);
  platform_ui_service(1750000);
  assert(s_lcd_grayscale_calls == 7);
  assert(last_grayscale_pair_is(gray_a, gray_b));

  /* A hinted full-screen scene cut must replace both old planes immediately;
   * otherwise the old playfield remains visible underneath a new title. */
  uint8_t presentation[64][17];
  memset(presentation, 0x5a, sizeof(presentation));
  platform_ui_present_lcd(presentation, 0, true, 14, true, 1760000);
  platform_ui_service(1810000);
  assert(s_lcd_grayscale_calls == 8);
  assert(memcmp(s_last_lcd_plane_a, presentation, sizeof(presentation)) == 0);
  assert(memcmp(s_last_lcd_plane_b, presentation, sizeof(presentation)) == 0);

  /* A game can leave temporal grayscale for a stable monochrome game-over
   * screen without turning the HP LCD off. Once alternation stops, expire the
   * old opposite plane even if no additional emulator frame arrives. */
  uint8_t game_over[64][17];
  memset(game_over, 0xa5, sizeof(game_over));
  platform_ui_present_lcd(game_over, 0, true, 14, false, 1820000);
  platform_ui_service(1870000);
  assert(s_lcd_grayscale_calls == 9);
  int bitmap_calls_before_expiry = s_lcd_bitmap_calls;
  platform_ui_service(2520001);
  assert(s_lcd_bitmap_calls == bitmap_calls_before_expiry + 1);
  assert(memcmp(s_last_lcd_bitmap, game_over, sizeof(game_over)) == 0);

  return 0;
}
