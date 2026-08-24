#include "platform_ui.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "pico/stdlib.h"
#include "version.h"

#define C_BG           0x252a31u
#define C_PANEL        0x3c424bu
#define C_TEXT         0xe8ecf1u
#define C_MUTED        0xaeb7c2u
#define C_PURPLE       0xc184c5u
#define C_PURPLE_DARK  0x523853u
#define C_TEAL         0x4bc3c7u
#define C_TEAL_DARK    0x234b50u
#define C_ALPHA        0xe5e9f2u
#define C_ALPHA_DARK   0x36445bu
#define C_LCD          0xb8c9a0u
#define C_LCD_INK      0x1a2b25u
#define C_BORDER       0x101317u
#define C_CURSOR       0xf0d264u
#define C_PRESSED      0xd7bc58u

/* Retain only the newest completed emulator snapshot between panel updates;
 * combining animation frames smears moving sprites and projectiles into a
 * single muddy image. lcd.c sends only changed source-row spans. */
#define PANEL_FRAME_INTERVAL_US 50000ull
#define GRAYSCALE_PANEL_FRAME_INTERVAL_US 50000ull
#define GRAYSCALE_PAIR_MAX_US 70000ull
#define GRAYSCALE_CYCLE_MAX_US 140000ull
#define GRAYSCALE_MODE_HOLD_US 750000ull
#define GRAYSCALE_MIN_PLANE_DIFFERENCE 512
#define GRAYSCALE_SCENE_CUT_DIFFERENCE 1024
#define GRAYSCALE_MATCH_MARGIN 192

typedef struct {
  const char *base;
  const char *left;
  const char *right;
  uint16_t code;
  int16_t x, y, w, h;
  uint8_t tone;
} nav_key_t;

enum { TONE_NORMAL, TONE_PURPLE, TONE_TEAL };
enum { PREFIX_BASE, PREFIX_PURPLE, PREFIX_TEAL, PREFIX_ALPHA };

/* The LCD is 300 pixels wide, so each of its six soft-menu zones is exactly
 * 50 pixels. The 48-pixel A-F keys are centered under those zones. All 49
 * keys fit in nine compact 13-pixel rows. */
static const nav_key_t s_nav_keys[] = {
  {"A",     "",       "",         0x14,  11,182, 48,13,TONE_NORMAL},
  {"B",     "",       "",         0x84,  61,182, 48,13,TONE_NORMAL},
  {"C",     "",       "",         0x83, 111,182, 48,13,TONE_NORMAL},
  {"D",     "",       "",         0x82, 161,182, 48,13,TONE_NORMAL},
  {"E",     "",       "",         0x81, 211,182, 48,13,TONE_NORMAL},
  {"F",     "",       "",         0x80, 261,182, 48,13,TONE_NORMAL},

  {"MTH",   "RAD",    "POLAR",    0x24,  11,197, 48,13,TONE_NORMAL},
  {"PRG",   "",       "CHARS",    0x74,  61,197, 48,13,TONE_NORMAL},
  {"CST",   "",       "MODES",    0x73, 111,197, 48,13,TONE_NORMAL},
  {"VAR",   "",       "MEMORY",   0x72, 161,197, 48,13,TONE_NORMAL},
  {"^",     "",       "STACK",    0x71, 211,197, 48,13,TONE_NORMAL},
  {"NXT",   "PREV",   "MENU",     0x70, 261,197, 48,13,TONE_NORMAL},

  {"'",     "UP",     "HOME",     0x04,  11,212, 48,13,TONE_NORMAL},
  {"STO",   "DEF",    "RCL",      0x64,  61,212, 48,13,TONE_NORMAL},
  {"EVAL",  "->NUM",  "UNDO",     0x63, 111,212, 48,13,TONE_NORMAL},
  {"<",     "PICT",   "",         0x62, 161,212, 48,13,TONE_NORMAL},
  {"V",     "VIEW",   "",         0x61, 211,212, 48,13,TONE_NORMAL},
  {">",     "SWAP",   "",         0x60, 261,212, 48,13,TONE_NORMAL},

  {"SIN",   "ASIN",   "DIFF",     0x34,  11,227, 48,13,TONE_NORMAL},
  {"COS",   "ACOS",   "INTEG",    0x54,  61,227, 48,13,TONE_NORMAL},
  {"TAN",   "ATAN",   "SIGMA",    0x53, 111,227, 48,13,TONE_NORMAL},
  {"SQRT",  "X^2",    "ROOT",     0x52, 161,227, 48,13,TONE_NORMAL},
  {"Y^X",   "10^X",   "LOG",      0x51, 211,227, 48,13,TONE_NORMAL},
  {"1/X",   "E^X",    "LN",       0x50, 261,227, 48,13,TONE_NORMAL},

  {"ENTER", "EQUATION","MATRIX",   0x44,  11,242, 98,13,TONE_NORMAL},
  {"+/-",   "EDIT",   "CMD",      0x43, 111,242, 48,13,TONE_NORMAL},
  {"EEX",   "PURG",   "ARG",      0x42, 161,242, 48,13,TONE_NORMAL},
  {"DEL",   "CLEAR",  "",         0x41, 211,242, 48,13,TONE_NORMAL},
  {"BS",    "DROP",   "",         0x40, 261,242, 48,13,TONE_NORMAL},

  {"ALPHA", "USER",   "ENTRY",    0x35,  11,257, 48,13,TONE_NORMAL},
  {"7",     "",       "SOLVE",    0x33,  68,257, 54,13,TONE_NORMAL},
  {"8",     "",       "PLOT",     0x32, 129,257, 54,13,TONE_NORMAL},
  {"9",     "",       "SYMBOLIC", 0x31, 190,257, 54,13,TONE_NORMAL},
  {"/",     "( )",    "#",        0x30, 251,257, 54,13,TONE_NORMAL},

  {"LSH",   "LSH",    "LSH",      0x25,  11,272, 48,13,TONE_PURPLE},
  {"4",     "",       "TIME",     0x23,  68,272, 54,13,TONE_NORMAL},
  {"5",     "",       "STAT",     0x22, 129,272, 54,13,TONE_NORMAL},
  {"6",     "",       "UNITS",    0x21, 190,272, 54,13,TONE_NORMAL},
  {"*",     "[ ]",    "_",        0x20, 251,272, 54,13,TONE_NORMAL},

  {"RSH",   "RSH",    "RSH",      0x15,  11,287, 48,13,TONE_TEAL},
  {"1",     "",       "I/O",      0x13,  68,287, 54,13,TONE_NORMAL},
  {"2",     "PORTS",  "LIBRARY",  0x12, 129,287, 54,13,TONE_NORMAL},
  {"3",     "",       "EQ LIB",   0x11, 190,287, 54,13,TONE_NORMAL},
  {"-",     "<< >>",  "\" \"",    0x10, 251,287, 54,13,TONE_NORMAL},

  {"ON",    "CONT",   "OFF",      0x8000,11,302, 48,13,TONE_NORMAL},
  {"0",     "=",      "->",       0x03,  68,302, 54,13,TONE_NORMAL},
  {".",     ",",      "NL",       0x02, 129,302, 54,13,TONE_NORMAL},
  {"SPC",   "PI",     "ANGLE",    0x01, 190,302, 54,13,TONE_NORMAL},
  {"+",     "{ }",    "::",       0x00, 251,302, 54,13,TONE_NORMAL},
};

#define NAV_KEY_COUNT ((int)(sizeof(s_nav_keys) / sizeof(s_nav_keys[0])))

static bool s_text_mode;
static bool s_arrow_key_mode;
static bool s_nav_pressed;
static bool s_battery_valid;
static bool s_battery_charging;
static bool s_battery_header_dirty;
static uint8_t s_battery_percent;
static int s_nav_selected = 30; /* Start on 7, near the numeric-entry area. */
static uint8_t s_prefix = PREFIX_BASE;
static char s_status[48] = "ARROWS MOVE SPACE=KEY ENTER=ENTER";
static uint8_t s_pending_lcd[64][17];
static uint8_t s_pending_lcd_alternate[64][17];
static uint8_t s_panel_lcd[64][17];
static uint8_t s_panel_lcd_alternate[64][17];
static uint8_t s_previous_distinct_lcd[64][17];
static uint8_t s_older_distinct_lcd[64][17];
static uint8_t s_grayscale_planes[2][64][17];
static uint8_t s_pending_annunciators;
static uint8_t s_pending_contrast;
static bool s_pending_lcd_on;
static bool s_lcd_pending;
static bool s_pending_grayscale;
static bool s_previous_distinct_valid;
static bool s_older_distinct_valid;
static bool s_grayscale_active;
static bool s_grayscale_reseed_pending;
static uint8_t s_grayscale_latest_plane;
static uint64_t s_previous_distinct_us;
static uint64_t s_older_distinct_us;
static uint64_t s_last_grayscale_evidence_us;
static uint64_t s_next_panel_frame_us;

static bool grayscale_hold_expired(uint64_t now_us) {
  return s_grayscale_active && s_last_grayscale_evidence_us != 0 &&
      now_us >= s_last_grayscale_evidence_us &&
      now_us - s_last_grayscale_evidence_us > GRAYSCALE_MODE_HOLD_US;
}

static void grayscale_expire_to_latest(void) {
  s_grayscale_active = false;
  s_grayscale_reseed_pending = false;
  s_last_grayscale_evidence_us = 0;
  s_older_distinct_valid = false;
  s_pending_grayscale = false;
  if (s_previous_distinct_valid) {
    memcpy(s_pending_lcd, s_previous_distinct_lcd, sizeof(s_pending_lcd));
    s_lcd_pending = true;
  }
}

static int bitmap_differing_bits(const uint8_t a[64][17],
                                 const uint8_t b[64][17]) {
  int differing = 0;
  for (int row = 0; row < 64; ++row) {
    for (int column = 0; column < 17; ++column) {
      uint8_t bits = a[row][column] ^ b[row][column];
      while (bits) {
        differing += bits & 1u;
        bits >>= 1;
      }
    }
  }
  return differing;
}

static void grayscale_start(const uint8_t previous[64][17],
                            const uint8_t current[64][17],
                            uint64_t capture_us) {
  memcpy(s_grayscale_planes[0], previous, sizeof(s_grayscale_planes[0]));
  memcpy(s_grayscale_planes[1], current, sizeof(s_grayscale_planes[1]));
  s_grayscale_latest_plane = 1;
  s_grayscale_active = true;
  s_grayscale_reseed_pending = false;
  s_last_grayscale_evidence_us = capture_us;
}

static void grayscale_update(const uint8_t bitmap[64][17],
                             bool grayscale_hint, uint64_t capture_us) {
  int differences[2] = {
    bitmap_differing_bits(bitmap, s_grayscale_planes[0]),
    bitmap_differing_bits(bitmap, s_grayscale_planes[1]),
  };
  uint8_t plane = differences[1] < differences[0] ? 1 : 0;
  uint8_t other = plane ^ 1u;
  bool confident_match =
      differences[plane] * 3 <
          differences[other] + GRAYSCALE_MATCH_MARGIN;
  bool far_from_both =
      differences[0] >= GRAYSCALE_SCENE_CUT_DIFFERENCE &&
      differences[1] >= GRAYSCALE_SCENE_CUT_DIFFERENCE;

  /* A large hinted frame that resembles neither tracked plane is a scene
   * change, not a third grayscale plane. Collapse both stored planes to the
   * new scene immediately so Android cannot retain gameplay underneath its
   * presentation. The next substantial hinted difference re-establishes a
   * pair without exposing either raw plane on the physical panel. */
  if (grayscale_hint && s_grayscale_reseed_pending) {
    if (differences[0] >= GRAYSCALE_MIN_PLANE_DIFFERENCE) {
      memcpy(s_grayscale_planes[1], bitmap,
             sizeof(s_grayscale_planes[1]));
      s_grayscale_latest_plane = 1;
      s_grayscale_reseed_pending = false;
    } else {
      memcpy(s_grayscale_planes[0], bitmap,
             sizeof(s_grayscale_planes[0]));
      memcpy(s_grayscale_planes[1], bitmap,
             sizeof(s_grayscale_planes[1]));
    }
    s_last_grayscale_evidence_us = capture_us;
    return;
  }
  if (grayscale_hint && far_from_both) {
    memcpy(s_grayscale_planes[0], bitmap,
           sizeof(s_grayscale_planes[0]));
    memcpy(s_grayscale_planes[1], bitmap,
           sizeof(s_grayscale_planes[1]));
    s_grayscale_latest_plane = 0;
    s_grayscale_reseed_pending = true;
    s_last_grayscale_evidence_us = capture_us;
    return;
  }

  /* Keep the two temporal planes independent. A native game often rewrites
   * one selected plane several times before switching to the other. Pairing
   * consecutive captures would then combine A with A and expose that raw
   * plane on the slow physical panel. Nearest-plane tracking updates only A
   * or B while every presentation continues to contain both. */
  if (grayscale_hint || confident_match) {
    memcpy(s_grayscale_planes[plane], bitmap,
           sizeof(s_grayscale_planes[plane]));
    if (grayscale_hint || plane != s_grayscale_latest_plane)
      s_last_grayscale_evidence_us = capture_us;
    s_grayscale_latest_plane = plane;
  }
}

static const char *alpha_label(int index) {
  static const char *const letters[] = {
    "A","B","C","D","E","F", "G","H","I","J","K","L",
    "M","N","O","P","Q","R", "S","T","U","V","W","X"
  };
  if (index >= 0 && index < 24) return letters[index];
  if (index == 25) return "Y";
  if (index == 26) return "Z";
  return "";
}

static const char *display_label(int index) {
  const nav_key_t *key = &s_nav_keys[index];
  const char *context = s_prefix == PREFIX_PURPLE ? key->left
                        : s_prefix == PREFIX_TEAL ? key->right
                        : s_prefix == PREFIX_ALPHA ? alpha_label(index) : "";
  return context[0] ? context : key->base;
}

static uint32_t key_tone_color(int index) {
  const nav_key_t *key = &s_nav_keys[index];
  if (key->tone == TONE_PURPLE) return C_PURPLE;
  if (key->tone == TONE_TEAL) return C_TEAL;
  if (s_prefix == PREFIX_PURPLE) return key->left[0] ? C_PURPLE : C_MUTED;
  if (s_prefix == PREFIX_TEAL) return key->right[0] ? C_TEAL : C_MUTED;
  if (s_prefix == PREFIX_ALPHA) return alpha_label(index)[0] ? C_ALPHA : C_MUTED;
  return C_TEXT;
}

static void draw_nav_key(int index) {
  const nav_key_t *key = &s_nav_keys[index];
  const char *label = display_label(index);
  bool selected = index == s_nav_selected;
  uint32_t normal_fill = s_prefix == PREFIX_PURPLE ? C_PURPLE_DARK
                         : s_prefix == PREFIX_TEAL ? C_TEAL_DARK
                         : s_prefix == PREFIX_ALPHA ? C_ALPHA_DARK : C_PANEL;
  uint32_t fill = selected ? (s_nav_pressed ? C_PRESSED : C_CURSOR) : normal_fill;
  uint32_t text_color = selected ? C_BORDER : key_tone_color(index);
  lcd_fill_rect(key->x, key->y, key->w, key->h, C_BORDER);
  lcd_fill_rect(key->x + 1, key->y + 1, key->w - 2, key->h - 2, fill);
  int text_w = (int)strlen(label) * 6 - 1;
  int text_x = key->x + (key->w - text_w) / 2;
  if (text_x < key->x + 2) text_x = key->x + 2;
  lcd_draw_text(text_x, key->y + 3, label, text_color, fill, 1);
}

static void draw_nav_keyboard(void) {
  for (int i = 0; i < NAV_KEY_COUNT; ++i) draw_nav_key(i);
}

static void draw_prefix_header(void) {
  const char *name = s_prefix == PREFIX_PURPLE ? "PURPLE"
                     : s_prefix == PREFIX_TEAL ? "TEAL"
                     : s_prefix == PREFIX_ALPHA ? "ALPHA" : "BASE";
  uint32_t color = s_prefix == PREFIX_PURPLE ? C_PURPLE
                   : s_prefix == PREFIX_TEAL ? C_TEAL
                   : s_prefix == PREFIX_ALPHA ? C_ALPHA : C_MUTED;
  lcd_fill_rect(244, 5, 72, 10, C_PANEL);
  int text_w = (int)strlen(name) * 6 - 1;
  lcd_draw_text(315 - text_w, 7, name, color, C_PANEL, 1);
}

static void draw_battery_header(void) {
  char label[12];
  if (s_battery_valid) {
    snprintf(label, sizeof(label), "BAT %u%%%c", (unsigned)s_battery_percent,
             s_battery_charging ? '+' : ' ');
  } else {
    snprintf(label, sizeof(label), "BAT --");
  }
  lcd_fill_rect(178, 5, 62, 10, C_PANEL);
  int text_w = (int)strlen(label) * 6 - 1;
  lcd_draw_text(239 - text_w, 7, label,
                s_battery_charging ? C_TEAL
                : s_battery_valid && s_battery_percent <= 15 ? C_PURPLE
                : C_MUTED,
                C_PANEL, 1);
}

static void draw_static_ui(void) {
  lcd_fill(C_BG);
  lcd_fill_rect(0, 0, 320, 20, C_PANEL);
  lcd_draw_text(5, 7, "HP48GX R", C_TEXT, C_PANEL, 1);
  draw_battery_header();
  draw_prefix_header();

  lcd_fill_rect(7, 20, 306, 150, C_BORDER);
  lcd_fill_rect(10, 22, HP48_LCD_RENDER_W, HP48_LCD_RENDER_H, C_LCD);
  lcd_fill_rect(0, 170, 320, 11, C_PANEL);
  draw_nav_keyboard();
}

static void draw_status(void) {
  lcd_fill_rect(4, 170, 312, 11, C_PANEL);
  const char *mode = s_arrow_key_mode ? "ARR" : s_text_mode ? "TXT" : "NAV";
  uint32_t mode_color = s_arrow_key_mode ? C_TEAL
                        : s_text_mode ? C_ALPHA : C_CURSOR;
  lcd_draw_text(7, 172, mode, mode_color, C_PANEL, 1);
  lcd_draw_text(31, 172, s_status, C_TEXT, C_PANEL, 1);
}

static void update_context_status(void) {
  if (s_prefix == PREFIX_ALPHA) {
    snprintf(s_status, sizeof(s_status), "%s - %s",
             s_text_mode ? "ALPHA LOCK" : "ALPHA ONE-SHOT",
             display_label(s_nav_selected));
  } else {
    snprintf(s_status, sizeof(s_status), "%s CONTEXT - %s",
             s_prefix == PREFIX_PURPLE ? "PURPLE"
             : s_prefix == PREFIX_TEAL ? "TEAL" : "BASE",
             display_label(s_nav_selected));
  }
  draw_status();
}

void platform_ui_set_context(bool text_mode, uint16_t shift_code) {
  uint8_t prefix = shift_code == 0x25 ? PREFIX_PURPLE
                   : shift_code == 0x15 ? PREFIX_TEAL
                   : shift_code == 0x35 ? PREFIX_ALPHA : PREFIX_BASE;
  bool prefix_changed = prefix != s_prefix;
  s_text_mode = text_mode;
  s_prefix = prefix;
  if (prefix_changed) {
    draw_prefix_header();
    draw_nav_keyboard();
  }
  update_context_status();
}

void platform_ui_set_arrow_key_mode(bool enabled) {
  if (s_arrow_key_mode == enabled) return;
  s_arrow_key_mode = enabled;
  draw_status();
}

void platform_ui_init(void) {
  s_lcd_pending = false;
  s_pending_grayscale = false;
  s_previous_distinct_valid = false;
  s_older_distinct_valid = false;
  s_grayscale_active = false;
  s_grayscale_reseed_pending = false;
  s_grayscale_latest_plane = 0;
  s_last_grayscale_evidence_us = 0;
  s_next_panel_frame_us = 0;
  lcd_init();
  lcd_fill(C_BORDER);
  lcd_draw_text(62, 148, "HP48GX " HP48GX_VERSION_DISPLAY, C_TEAL, C_BORDER, 1);
  sleep_ms(700);
  draw_static_ui();
  draw_status();
}

void platform_ui_present_lcd(const uint8_t bitmap[64][17], uint8_t annunciators,
                             bool lcd_on, uint8_t contrast,
                             bool grayscale_hint, uint64_t capture_us) {
  /* display.c now captures after a real-time write-quiet interval, so this is
   * already a completed emulated frame. If the physical LCD is still busy,
   * replace its queued successor instead of accumulating old animation. */
  int adjacent_difference =
      s_previous_distinct_valid
          ? bitmap_differing_bits(bitmap, s_previous_distinct_lcd)
          : 0;
  if (!lcd_on) {
    s_grayscale_active = false;
    s_grayscale_reseed_pending = false;
    s_last_grayscale_evidence_us = 0;
    s_previous_distinct_valid = false;
    s_older_distinct_valid = false;
  } else if (!s_previous_distinct_valid || adjacent_difference != 0) {
    bool new_plane =
        adjacent_difference >= GRAYSCALE_MIN_PLANE_DIFFERENCE;
    bool hold_active = s_last_grayscale_evidence_us != 0 &&
        capture_us >= s_last_grayscale_evidence_us &&
        capture_us - s_last_grayscale_evidence_us <=
            GRAYSCALE_MODE_HOLD_US;
    if (s_grayscale_active && !grayscale_hint && !hold_active)
      s_grayscale_active = false;

    if (s_grayscale_active) {
      grayscale_update(bitmap, grayscale_hint, capture_us);
    } else if (grayscale_hint && s_previous_distinct_valid && new_plane) {
      /* Tight polling of the GX LCD line counter is a stronger native
       * grayscale signal than bitmap similarity: Wario's interrupt handler
       * uses it to synchronize temporal planes in the same visible buffer.
       * A substantial difference is still required before starting, since
       * two small rewrites of one plane are not an A/B pair. */
      grayscale_start(s_previous_distinct_lcd, bitmap, capture_us);
    } else if (new_plane && s_previous_distinct_valid &&
        s_older_distinct_valid &&
        capture_us >= s_previous_distinct_us &&
        capture_us - s_previous_distinct_us <= GRAYSCALE_PAIR_MAX_US &&
        capture_us >= s_older_distinct_us &&
        capture_us - s_older_distinct_us <= GRAYSCALE_CYCLE_MAX_US) {
      int same_plane_difference =
          bitmap_differing_bits(bitmap, s_older_distinct_lcd);
      /* True HP grayscale alternates two substantially different bitplanes.
       * Two captures apart, the same plane is much more similar. Requiring
       * that A/B/A signature avoids blending ordinary moving sprites and the
       * muddy trails produced by indiscriminate frame averaging. */
      if (same_plane_difference * 3 <
          adjacent_difference + GRAYSCALE_MATCH_MARGIN)
        grayscale_start(s_previous_distinct_lcd, bitmap, capture_us);
    }

    if (s_previous_distinct_valid) {
      memcpy(s_older_distinct_lcd, s_previous_distinct_lcd,
             sizeof(s_older_distinct_lcd));
      s_older_distinct_us = s_previous_distinct_us;
      s_older_distinct_valid = true;
    }
    memcpy(s_previous_distinct_lcd, bitmap,
           sizeof(s_previous_distinct_lcd));
    s_previous_distinct_us = capture_us;
    s_previous_distinct_valid = true;
  }
  s_pending_grayscale = s_grayscale_active;
  if (s_grayscale_active) {
    memcpy(s_pending_lcd_alternate, s_grayscale_planes[0],
           sizeof(s_pending_lcd_alternate));
    memcpy(s_pending_lcd, s_grayscale_planes[1], sizeof(s_pending_lcd));
  } else {
    memcpy(s_pending_lcd, bitmap, sizeof(s_pending_lcd));
  }
  s_pending_annunciators = annunciators;
  s_pending_contrast = contrast;
  s_pending_lcd_on = lcd_on;
  s_lcd_pending = true;
}

void platform_ui_service(uint64_t now_us) {
  lcd_service();
  /* If a temporal-grayscale program stops alternating, the newest raw LCD
   * image becomes the real steady screen. Do not leave its last opposite
   * plane paired forever: that retained Android's gameplay underneath the
   * monochrome GAME OVER/presentation screen until another program ran. */
  if (grayscale_hold_expired(now_us)) grayscale_expire_to_latest();
  /* Battery polling runs every ten seconds. Keep the redraw in the normal UI
   * service path rather than the keyboard-controller transaction itself. */
  if (!lcd_hp48_bitmap_busy() && s_battery_header_dirty) {
    draw_battery_header();
    s_battery_header_dirty = false;
  }
  if (lcd_hp48_bitmap_busy() || !s_lcd_pending ||
      (s_next_panel_frame_us != 0 && now_us < s_next_panel_frame_us))
    return;

  memcpy(s_panel_lcd, s_pending_lcd, sizeof(s_panel_lcd));
  bool grayscale = s_pending_grayscale;
  if (grayscale)
    memcpy(s_panel_lcd_alternate, s_pending_lcd_alternate,
           sizeof(s_panel_lcd_alternate));
  uint8_t annunciators = s_pending_annunciators;
  bool lcd_on = s_pending_lcd_on;
  (void)s_pending_contrast;
  s_lcd_pending = false;
  s_next_panel_frame_us =
      now_us + (grayscale ? GRAYSCALE_PANEL_FRAME_INTERVAL_US
                          : PANEL_FRAME_INTERVAL_US);

  char ann[24];
  snprintf(ann, sizeof(ann), "%c %c %c %c %c %c",
           (annunciators & 0x81) == 0x81 ? '<' : ' ',
           (annunciators & 0x82) == 0x82 ? '>' : ' ',
           (annunciators & 0x84) == 0x84 ? 'A' : ' ',
           (annunciators & 0x88) == 0x88 ? 'B' : ' ',
           (annunciators & 0x90) == 0x90 ? '*' : ' ',
           (annunciators & 0xa0) == 0xa0 ? 'I' : ' ');
  lcd_fill_rect(72, 5, 105, 10, C_PANEL);
  lcd_draw_text(72, 7, ann, C_MUTED, C_PANEL, 1);
  if (grayscale) {
    lcd_draw_hp48_grayscale(10, 22, s_panel_lcd_alternate, s_panel_lcd,
                            C_LCD_INK, lcd_on ? C_LCD : C_PANEL);
  } else {
    lcd_draw_hp48_bitmap(10, 22, s_panel_lcd, C_LCD_INK,
                         lcd_on ? C_LCD : C_PANEL);
  }
}

void platform_ui_set_battery(uint8_t percent, bool charging, bool valid) {
  if (valid && percent > 100) valid = false;
  if (s_battery_valid == valid && s_battery_charging == charging &&
      (!valid || s_battery_percent == percent)) return;
  s_battery_valid = valid;
  s_battery_charging = charging;
  s_battery_percent = percent;
  s_battery_header_dirty = true;
}

void platform_ui_status(const char *message) {
  if (!message) return;
  strncpy(s_status, message, sizeof(s_status) - 1);
  s_status[sizeof(s_status) - 1] = 0;
  draw_status();
}

void platform_ui_nav_move(int dx, int dy) {
  if ((dx == 0) == (dy == 0)) return;
  const nav_key_t *current = &s_nav_keys[s_nav_selected];
  int current_x = current->x + current->w / 2;
  int current_y = current->y + current->h / 2;
  int best = -1;
  int best_score = INT_MAX;

  for (int i = 0; i < NAV_KEY_COUNT; ++i) {
    if (i == s_nav_selected) continue;
    const nav_key_t *candidate = &s_nav_keys[i];
    int candidate_x = candidate->x + candidate->w / 2;
    int candidate_y = candidate->y + candidate->h / 2;
    int delta_x = candidate_x - current_x;
    int delta_y = candidate_y - current_y;
    if ((dx < 0 && delta_x >= 0) || (dx > 0 && delta_x <= 0) ||
        (dy < 0 && delta_y >= 0) || (dy > 0 && delta_y <= 0)) continue;
    int primary = dx ? (delta_x < 0 ? -delta_x : delta_x)
                     : (delta_y < 0 ? -delta_y : delta_y);
    int perpendicular = dx ? (delta_y < 0 ? -delta_y : delta_y)
                           : (delta_x < 0 ? -delta_x : delta_x);
    /* Horizontal movement stays on the current row when possible. Vertical
     * movement must choose the nearest row first; the old symmetric score
     * skipped the wide ENTER key in favor of keys two rows away. */
    int score = dx ? primary + perpendicular * 4
                   : primary * 4 + perpendicular;
    if (score < best_score) {
      best_score = score;
      best = i;
    }
  }

  if (best >= 0) {
    int old = s_nav_selected;
    s_nav_selected = best;
    s_nav_pressed = false;
    draw_nav_key(old);
    draw_nav_key(s_nav_selected);
  }
}

uint16_t platform_ui_nav_selected_code(void) {
  return s_nav_keys[s_nav_selected].code;
}

const char *platform_ui_nav_selected_label(void) {
  return display_label(s_nav_selected);
}

void platform_ui_nav_set_pressed(bool pressed) {
  if (s_nav_pressed == pressed) return;
  s_nav_pressed = pressed;
  draw_nav_key(s_nav_selected);
}
