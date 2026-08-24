#include "hp48.h"

#include <string.h>

#include "device.h"
#include "platform_time.h"
#include "platform_ui.h"
#include "x48_x11.h"

display_t display;
disp_t disp = {.w = 131, .h = 64, .mapped = 1};

static uint8_t s_bitmap[64][17];
static int s_framebuffer_dirty;
static unsigned long s_first_dirty_instruction;
static unsigned long s_last_dirty_instruction;
static unsigned long s_observed_dirty_instruction;
static uint64_t s_first_dirty_observed_us;
static uint64_t s_last_write_observed_us;
static int s_dirty_time_observed;
static uint8_t s_dirty_visible_nibbles[64][5];
static unsigned s_dirty_visible_count;
static unsigned s_dirty_visible_target;
static int s_complete_frame_observed;
static uint64_t s_line_counter_reference_us;
static uint8_t s_line_counter_reference_value;
static uint8_t s_line_counter_frozen;
static int s_line_counter_running;
static uint64_t s_line_counter_last_read_us;
static uint64_t s_dirty_scan_frame;
static int s_dirty_scan_frame_valid;

/* The GX LCD controller scans one row per 4096 Hz tick, giving a 64 Hz
 * 64-line frame. Native grayscale interrupt handlers poll the six-bit
 * down-counter at 0x128/0x129 to synchronize their plane updates. The old
 * x48 stub advanced it once per register read, making tight polling loops run
 * at CPU speed and destroying that synchronization. */
#define DISPLAY_LINE_COUNTER_HZ 4096ull

static uint8_t current_line_counter_at(uint64_t now_us) {
  if (!s_line_counter_running) return s_line_counter_frozen;
  uint64_t elapsed_us = now_us - s_line_counter_reference_us;
  uint64_t elapsed_lines =
      elapsed_us * DISPLAY_LINE_COUNTER_HZ / 1000000ull;
  return (uint8_t)((s_line_counter_reference_value - elapsed_lines) & 0x3fu);
}

static uint64_t current_scan_frame_at(uint64_t now_us) {
  if (!s_line_counter_running || now_us < s_line_counter_reference_us)
    return 0;
  uint64_t elapsed_us = now_us - s_line_counter_reference_us;
  return (elapsed_us * DISPLAY_LINE_COUNTER_HZ / 1000000ull) >> 6;
}

static int line_counter_sync_active(uint64_t now_us) {
  /* Some Wario 3 interrupt paths read the counter only once per visible
   * write burst. Keep the scan synchronization alive across its longer
   * presentation delays; ordinary software does not touch this register. */
  return s_line_counter_running && s_line_counter_last_read_us != 0 &&
         now_us >= s_line_counter_last_read_us &&
         now_us - s_line_counter_last_read_us <= 500000ull;
}

void display_line_counter_reset(word_8 initial, int enabled) {
  s_line_counter_reference_us = platform_time_us();
  s_line_counter_reference_value = (uint8_t)(initial & 0x3fu);
  s_line_counter_frozen = s_line_counter_reference_value;
  s_line_counter_running = enabled != 0;
  s_line_counter_last_read_us = 0;
  s_dirty_scan_frame_valid = 0;
}

void display_line_counter_set_enabled(int enabled, word_8 initial) {
  if (enabled) {
    if (s_line_counter_running) return;
    s_line_counter_reference_us = platform_time_us();
    s_line_counter_reference_value = (uint8_t)(initial & 0x3fu);
    s_line_counter_running = 1;
    s_line_counter_last_read_us = 0;
    s_dirty_scan_frame_valid = 0;
    return;
  }
  if (!s_line_counter_running) return;
  s_line_counter_frozen = current_line_counter_at(platform_time_us());
  s_line_counter_running = 0;
  s_dirty_scan_frame_valid = 0;
}

void display_line_counter_configure(word_8 value) {
  if (!s_line_counter_running) s_line_counter_frozen = value & 0x3fu;
}

word_8 display_line_counter_read(void) {
  uint64_t now_us = platform_time_us();
  s_line_counter_last_read_us = now_us;
  return current_line_counter_at(now_us);
}

/* Native Saturn games write directly into the active display RAM. The x48
 * desktop renderer updated individual host pixels from disp_draw_nibble(),
 * but this port originally left both nibble callbacks empty. Coalesce those
 * writes into complete snapshots: a quiet interval catches the end of a
 * normal frame, while the maximum age keeps continuously rendered games
 * moving. Hardware decisions use elapsed time because an instruction-count
 * interval becomes seconds long when Saturn execution is slowed to real GX
 * speed. Instruction limits remain as a deterministic unpaced-host fallback.
 * The physical panel is separately paced by platform_ui_service(). */
#define DISPLAY_WRITE_QUIET_INSTRUCTIONS 8192ul
#define DISPLAY_MAX_PENDING_INSTRUCTIONS 262144ul
#define DISPLAY_WRITE_QUIET_US 2000ull
#define DISPLAY_MAX_PENDING_US 40000ull

extern unsigned long instructions;

static void clear_framebuffer_dirty(void) {
  s_framebuffer_dirty = 0;
  s_first_dirty_instruction = instructions;
  s_last_dirty_instruction = instructions;
  s_observed_dirty_instruction = instructions;
  s_first_dirty_observed_us = 0;
  s_last_write_observed_us = 0;
  s_dirty_time_observed = 0;
  memset(s_dirty_visible_nibbles, 0, sizeof(s_dirty_visible_nibbles));
  s_dirty_visible_count = 0;
  unsigned visible_rows = (unsigned)display.lines + 1u;
  unsigned visible_nibbles = ((unsigned)display.offset + 131u + 3u) / 4u;
  if (visible_rows > 64u) visible_rows = 64u;
  if (visible_nibbles > 36u) visible_nibbles = 36u;
  s_dirty_visible_target = visible_rows * visible_nibbles;
  s_complete_frame_observed = 0;
  s_dirty_scan_frame_valid = 0;
}

static void mark_framebuffer_dirty(void) {
  if (!s_framebuffer_dirty) {
    s_framebuffer_dirty = 1;
    s_first_dirty_instruction = instructions;
    s_dirty_time_observed = 0;
  }
  s_last_dirty_instruction = instructions;
}

static void mark_visible_display_nibble(word_20 addr) {
  mark_framebuffer_dirty();
  if (s_complete_frame_observed || display.nibs_per_line <= 0 ||
      addr < (word_20)display.disp_start)
    return;

  unsigned relative = (unsigned)(addr - (word_20)display.disp_start);
  unsigned row = relative / (unsigned)display.nibs_per_line;
  unsigned column = relative % (unsigned)display.nibs_per_line;
  unsigned visible_nibbles = ((unsigned)display.offset + 131u + 3u) / 4u;
  if (row >= 64u || column >= visible_nibbles || column >= 36u) return;

  uint8_t mask = (uint8_t)(1u << (column & 7u));
  uint8_t *seen = &s_dirty_visible_nibbles[row][column >> 3];
  if ((*seen & mask) == 0) {
    *seen |= mask;
    ++s_dirty_visible_count;
    if (s_dirty_visible_target != 0 &&
        s_dirty_visible_count >= s_dirty_visible_target)
      s_complete_frame_observed = 1;
  }
}

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
  display_line_counter_reset(saturn.line_count, display.on);
  clear_framebuffer_dirty();
  update_display();
}

static uint8_t row_pixel(long row_addr, int x, int bit_offset) {
  int src_bit = x + bit_offset;
  int nib = read_nibble(row_addr + (src_bit >> 2));
  return (nib >> (src_bit & 3)) & 1;
}

void update_display(void) {
  clear_framebuffer_dirty();
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
  uint64_t capture_us = platform_time_us();
  platform_ui_present_lcd(s_bitmap, (uint8_t)display.annunc, display.on != 0,
                          (uint8_t)display.contrast,
                          line_counter_sync_active(capture_us), capture_us);
}

void display_service(int force) {
  if (!s_framebuffer_dirty) return;
  uint64_t now_us = platform_time_us();
  if (!s_dirty_time_observed) {
    s_first_dirty_observed_us = now_us;
    s_last_write_observed_us = now_us;
    s_observed_dirty_instruction = s_last_dirty_instruction;
    s_dirty_time_observed = 1;
  } else if (s_observed_dirty_instruction != s_last_dirty_instruction) {
    s_last_write_observed_us = now_us;
    s_observed_dirty_instruction = s_last_dirty_instruction;
  }
  unsigned long quiet_instructions = instructions - s_last_dirty_instruction;
  unsigned long pending_instructions =
      instructions - s_first_dirty_instruction;
  uint64_t quiet_us = now_us - s_last_write_observed_us;
  uint64_t pending_us = now_us - s_first_dirty_observed_us;
  if (!force && line_counter_sync_active(now_us)) {
    /* Grayscale games poll the real LCD scan counter and modify the visible
     * buffer around vertical blank. Queue only the first coherent snapshot
     * after the next 64-line boundary; write-quiet detection can otherwise
     * expose a half-copied plane as a permanently garbled host frame. */
    uint64_t scan_frame = current_scan_frame_at(now_us);
    if (!s_dirty_scan_frame_valid) {
      s_dirty_scan_frame = scan_frame;
      s_dirty_scan_frame_valid = 1;
      return;
    }
    if (scan_frame == s_dirty_scan_frame) return;
    update_display();
    return;
  }
  if (!force && !s_complete_frame_observed &&
      quiet_instructions < DISPLAY_WRITE_QUIET_INSTRUCTIONS &&
      pending_instructions < DISPLAY_MAX_PENDING_INSTRUCTIONS &&
      quiet_us < DISPLAY_WRITE_QUIET_US &&
      pending_us < DISPLAY_MAX_PENDING_US)
    return;
  update_display();
}

void redraw_display(void) { update_display(); }
void disp_draw_nibble(word_20 addr, word_4 val) {
  (void)val;
  mark_visible_display_nibble(addr);
}
void menu_draw_nibble(word_20 addr, word_4 val) {
  (void)addr;
  (void)val;
  mark_framebuffer_dirty();
}
void draw_annunc(void) {
  display.annunc = saturn.annunc;
  update_display();
}
void redraw_annunc(void) { draw_annunc(); }
void init_annunc(void) {}
void adjust_contrast(int contrast) { display.contrast = contrast; }
void refresh_icon(void) {}
