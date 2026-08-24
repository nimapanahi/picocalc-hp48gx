#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "core_runtime.h"
#include "platform_ui.h"
#include "sound.h"

static int wake_pending;
uint64_t test_platform_time_advance_us;
_Bool test_platform_time_instruction_driven;
unsigned test_platform_time_us_per_instruction = 2u;
void (*test_getevent_hook)(void);
int test_sound_level;
int test_sound_calls;
int test_sound_transitions;
int test_sound_short_runs;
int test_sound_short_samples;
int test_sound_run_samples;
int test_display_present_calls;
int test_display_bitmap_changes;
int test_display_blank_changes;
int test_display_nonblank_changes;
int test_display_last_ink_bits;
int test_display_grayscale_hint_calls;
static uint32_t test_display_checksum;

uint64_t platform_time_us(void);

static const char *capture_dir(void) {
  const char *path = getenv("HP48_CAPTURE_DIR");
  return path && path[0] ? path : NULL;
}

static void dump_frame(const uint8_t bitmap[64][17], int number,
                       uint64_t capture_us, _Bool grayscale_hint) {
  const char *directory = capture_dir();
  if (!directory) return;

  char path[512];
  snprintf(path, sizeof(path), "%s/frame-%05d.pgm", directory, number);
  FILE *file = fopen(path, "wb");
  if (!file) return;
  fprintf(file, "P5\n131 64\n255\n");
  for (int row = 0; row < 64; ++row) {
    for (int column = 0; column < 131; ++column) {
      int ink = (bitmap[row][column >> 3] >> (column & 7)) & 1;
      fputc(ink ? 0 : 255, file);
    }
  }
  fclose(file);

  snprintf(path, sizeof(path), "%s/frames.csv", directory);
  file = fopen(path, "a");
  if (!file) return;
  fprintf(file, "%d,%llu,%d\n", number,
          (unsigned long long)capture_us, grayscale_hint ? 1 : 0);
  fclose(file);
}

static void dump_audio_edge(_Bool high, unsigned long instruction_count) {
  const char *directory = capture_dir();
  if (!directory) return;

  char path[512];
  snprintf(path, sizeof(path), "%s/audio-edges.csv", directory);
  FILE *file = fopen(path, "a");
  if (!file) return;
  fprintf(file, "%llu,%d,%lu\n",
          (unsigned long long)platform_time_us(), high ? 1 : 0,
          instruction_count);
  fclose(file);
}

void test_capture_marker(const char *label) {
  const char *directory = capture_dir();
  if (!directory || !label) return;

  char path[512];
  snprintf(path, sizeof(path), "%s/markers.csv", directory);
  FILE *file = fopen(path, "a");
  if (!file) return;
  fprintf(file, "%s,%d,%llu\n", label, test_display_bitmap_changes,
          (unsigned long long)platform_time_us());
  fclose(file);
}

int GetEvent(void) {
  if (test_getevent_hook) {
    test_getevent_hook();
    /* The hook supplies its own wake event. Do not inject the harness's
     * normal synthetic ON as well: a second ON while interrupts are disabled
     * becomes a pending abort and does not model the platform transaction. */
    return 1;
  }
  if (!wake_pending) return 0;
  wake_pending = 0;
  hp48_key_set(0x8000u, true);
  hp48_key_set(0x8000u, false);
  return 1;
}

extern int got_alarm;
void pause(void) {
  got_alarm = 1;
  wake_pending = 1;
}

uint64_t platform_time_us(void) {
  if (test_platform_time_instruction_driven) {
    static unsigned long last_instructions;
    static uint64_t instruction_time_us = 1000000ull;
    extern unsigned long instructions;
    unsigned long elapsed = instructions - last_instructions;
    instruction_time_us +=
        (uint64_t)elapsed * test_platform_time_us_per_instruction;
    last_instructions = instructions;
    return instruction_time_us + test_platform_time_advance_us;
  }
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (uint64_t)ts.tv_sec * 1000000ull +
         (uint64_t)ts.tv_nsec / 1000ull + test_platform_time_advance_us;
}

void test_set_instruction_time_rate(unsigned us_per_instruction) {
  (void)platform_time_us();
  test_platform_time_us_per_instruction = us_per_instruction;
}

void platform_ui_init(void) {}
void platform_ui_present_lcd(const uint8_t bitmap[64][17], uint8_t annunciators,
                             _Bool lcd_on, uint8_t contrast,
                             _Bool grayscale_hint, uint64_t capture_us) {
  (void)capture_us;
  if (grayscale_hint) ++test_display_grayscale_hint_calls;
  uint32_t checksum = 2166136261u;
  int ink_bits = 0;
  for (int row = 0; row < 64; ++row) {
    for (int column = 0; column < 17; ++column) {
      checksum ^= bitmap[row][column];
      checksum *= 16777619u;
      uint8_t pixels = bitmap[row][column];
      while (pixels) {
        ink_bits += pixels & 1u;
        pixels >>= 1;
      }
    }
  }
  checksum ^= annunciators;
  checksum *= 16777619u;
  checksum ^= lcd_on ? 1u : 0u;
  checksum *= 16777619u;
  checksum ^= contrast;
  checksum *= 16777619u;
  if (test_display_present_calls != 0 && checksum != test_display_checksum) {
    ++test_display_bitmap_changes;
    if (ink_bits == 0) ++test_display_blank_changes;
    else ++test_display_nonblank_changes;
    dump_frame(bitmap, test_display_bitmap_changes, capture_us,
               grayscale_hint);
  }
  test_display_last_ink_bits = ink_bits;
  test_display_checksum = checksum;
  ++test_display_present_calls;
}
void platform_ui_set_context(_Bool text_mode, uint16_t shift_code) {
  (void)text_mode; (void)shift_code;
}
void platform_ui_set_arrow_key_mode(_Bool enabled) { (void)enabled; }
void platform_ui_status(const char *message) { if (message) puts(message); }

void platform_sound_init(void) {}
void platform_sound_set_level(_Bool high, unsigned long instruction_count) {
  if (test_sound_calls != 0 && test_sound_level != (high ? 1 : 0)) {
    ++test_sound_transitions;
    if (test_sound_run_samples <= 256) {
      ++test_sound_short_runs;
      test_sound_short_samples += test_sound_run_samples;
    }
    test_sound_run_samples = 0;
    dump_audio_edge(high, instruction_count);
  }
  test_sound_level = high ? 1 : 0;
  ++test_sound_calls;
  ++test_sound_run_samples;
}
void platform_sound_silence(void) { test_sound_level = 0; }
void platform_sound_startup_beep(void) {}
