#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "core_runtime.h"
#include "platform_ui.h"

static int wake_pending;

int GetEvent(void) {
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
  struct timespec ts;
  timespec_get(&ts, TIME_UTC);
  return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

void platform_ui_init(void) {}
void platform_ui_present_lcd(const uint8_t bitmap[64][17], uint8_t annunciators,
                             _Bool lcd_on, uint8_t contrast) {
  (void)bitmap; (void)annunciators; (void)lcd_on; (void)contrast;
}
void platform_ui_set_text_mode(_Bool enabled) { (void)enabled; }
void platform_ui_status(const char *message) { if (message) puts(message); }
