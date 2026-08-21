#include <stdint.h>
#include <stdio.h>

#include "core_runtime.h"
#include "hardware/clocks.h"
#include "keyboard.h"
#include "pico/stdlib.h"
#include "platform_time.h"
#include "platform_ui.h"
#include "state.h"
#include "timer.h"
#include "version.h"

extern const uint8_t hp48_rom[];
extern const uint8_t hp48_rom_end[];
extern int got_alarm;
extern int enter_debugger;

uint64_t platform_time_us(void) { return time_us_64(); }

int GetEvent(void) {
  /* x48's SHUTDN loop only wakes when GetEvent reports that an event arrived
   * and the key handler raised a keyboard interrupt. */
  return keyboard_poll() ? 1 : 0;
}

/* x48's shutdown loop uses the POSIX pause() hook.  On PicoCalc, keep the
 * keyboard and HP timers alive while yielding briefly to the hardware. */
void pause(void) {
  got_alarm = 1;
  sleep_ms(1);
}

int main(void) {
  set_sys_clock_khz(150000, true);
  stdio_init_all();
  printf("\n[HP48] PicoCalc HP 48GX %s starting\n", HP48GX_VERSION);

  /* Bring up the STM32 first because it owns the LCD backlight. */
  bool keyboard_ok = keyboard_init();
  printf("[HP48] Initializing ILI9488 LCD with PIO mode 3 at 10 MHz\n");
  platform_ui_init();
  platform_ui_status(keyboard_ok ? "ARROWS MOVE SPACE=KEY ENTER=ENTER"
                                 : "KEYBOARD CONTROLLER NOT FOUND");
  printf("[HP48] LCD initialized\n");

  size_t rom_size = (size_t)(hp48_rom_end - hp48_rom);
  if (!hp48_core_init(hp48_rom, rom_size)) {
    printf("[HP48] ROM validation failed\n");
    platform_ui_status("ROM INVALID: EXPECTED GX REV R");
    while (true) tight_loop_contents();
  }
  printf("[HP48] ROM accepted; Saturn core initialized\n");

  if (state_load()) {
    platform_ui_status("STATE RESTORED");
  } else {
    set_accesstime();
    platform_ui_status("COLD BOOT");
  }
  got_alarm = 1;

  while (true) {
    hp48_core_run(4096);
    keyboard_poll();

    if (keyboard_take_save_request()) {
      platform_ui_status("SAVING...");
      platform_ui_status(state_save() ? "STATE SAVED" : "SAVE FAILED");
    }
    if (keyboard_take_reset_request()) {
      platform_ui_status("RESETTING...");
      state_clear();
      hp48_core_reset();
      set_accesstime();
      platform_ui_status("COLD BOOT");
    }
    if (enter_debugger) {
      platform_ui_status("CORE HALTED: ILLEGAL INSTRUCTION");
      sleep_ms(100);
    }
  }
}
