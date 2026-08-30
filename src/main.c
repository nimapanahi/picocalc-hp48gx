#include <stdint.h>
#include <stdio.h>

#include "core_runtime.h"
#include "hardware/clocks.h"
#include "keyboard.h"
#include "object_files.h"
#include "pico/stdlib.h"
#include "platform_time.h"
#include "platform_ui.h"
#include "port2_card.h"
#include "rpl_object.h"
#include "realtime_pacer.h"
#include "sound.h"
#include "state.h"
#include "storage.h"
#include "timer.h"
#include "version.h"

extern const uint8_t hp48_rom[];
extern const uint8_t hp48_rom_end[];
extern int got_alarm;
extern int enter_debugger;

#define IMPORT_WAKE_HOLD_MS 75u

/* Direct IPS rates retain exact fractional deadlines above 500k. The final
 * zero entry is MAX: no pacing delay, while the 256-instruction quantum still
 * services input, display snapshots, timers, and sound frequently. */
static const uint32_t s_speed_rates[] = {
    20000, 25000, 33333, 40000, 50000, 66667, 83333, 100000, 125000,
    166667, 200000, 250000, 333333, 500000, 666667, 1000000, 1500000,
    2000000, 0,
};
#define DEFAULT_SPEED_INDEX 13u
#define CORE_RUN_QUANTUM 256u

static object_files_status_t s_files_status;
static bool s_import_wake_down;
static uint32_t s_import_wake_release_ms;
static realtime_pacer_t s_realtime_pacer;
static unsigned s_speed_index = DEFAULT_SPEED_INDEX;

uint64_t platform_time_us(void) { return time_us_64(); }

static bool service_idle_file_import(void) {
  if (!keyboard_take_file_import_request()) return false;

  platform_ui_status("IMPORTING HP FILE...");
  s_files_status = object_files_import_selected();
  char message[48];
  if (s_files_status == OBJECT_FILES_OK) {
    if (hp48_rpl_stack_level1_is_directory())
      snprintf(message, sizeof(message),
               "DIR L1: 'NAME' ENTER STO; VAR -> NAME");
    else if (hp48_rpl_port1_is_merged()) {
      snprintf(message, sizeof(message),
               "IMPORTED %.14s -> L1; CTRL+F10 SAVES",
               object_files_selected_name());
    } else {
      unsigned destination_port =
          hp48_card_slot() == HP48_CARD_PORT1 ? 1u : 2u;
      snprintf(message, sizeof(message),
               "IMPORTED %.14s -> L1; STO :%u: NAME",
               object_files_selected_name(), destination_port);
    }
    platform_ui_status(message);

    /* Emu48's RPL transfer contract requires the ROM to be stopped in
     * SHUTDN while DSKTOP/return-stack pointers are changed. Wake only after
     * the complete object has been committed so the ROM adopts, rather than
     * overwrites, the new stack state. */
    hp48_key_set(0x8000, true);
    s_import_wake_down = true;
    s_import_wake_release_ms =
        to_ms_since_boot(get_absolute_time()) + IMPORT_WAKE_HOLD_MS;
  } else {
    snprintf(message, sizeof(message), "IMPORT: %s",
             object_files_status_label(s_files_status));
    platform_ui_status(message);
  }
  return true;
}

int GetEvent(void) {
  /* x48's SHUTDN loop only wakes when GetEvent reports that an event arrived
   * and the key handler raised a keyboard interrupt. */
  bool event = keyboard_poll();
  if (hp48_core_is_sleeping() && service_idle_file_import()) event = true;
  return event ? 1 : 0;
}

/* x48's shutdown loop uses the POSIX pause() hook.  On PicoCalc, keep the
 * keyboard and HP timers alive while yielding briefly to the hardware. */
void pause(void) {
  got_alarm = 1;
  sleep_ms(1);
  platform_ui_service(time_us_64());
}

static void pace_saturn(unsigned executed) {
  uint64_t now_us = time_us_64();
  realtime_pacer_account(&s_realtime_pacer, now_us, executed);
  uint32_t delay_us = realtime_pacer_delay_us(&s_realtime_pacer, now_us);
  while (delay_us != 0) {
    uint32_t slice_us = delay_us > 1000u ? 1000u : delay_us;
    sleep_us(slice_us);
    hp48_core_service_display();
    platform_ui_service(time_us_64());
    keyboard_poll();
    now_us = time_us_64();
    delay_us = realtime_pacer_delay_us(&s_realtime_pacer, now_us);
  }
}

static void service_speed_adjustment(void) {
  int adjustment = keyboard_take_speed_adjustment();
  if (adjustment == 0) return;
  if (adjustment < 0 && s_speed_index > 0)
    --s_speed_index;
  else if (adjustment > 0 &&
           s_speed_index + 1 <
               sizeof(s_speed_rates) / sizeof(s_speed_rates[0]))
    ++s_speed_index;

  uint32_t rate = s_speed_rates[s_speed_index];
  realtime_pacer_set_rate(&s_realtime_pacer, time_us_64(), rate);
  char message[48];
  if (rate == 0)
    snprintf(message, sizeof(message), "CPU MAX | CTRL+LEFT/RIGHT");
  else
    snprintf(message, sizeof(message), "CPU %luK IPS | CTRL+LEFT/RIGHT",
             (unsigned long)((rate + 500u) / 1000u));
  platform_ui_status(message);
}

int main(void) {
  set_sys_clock_khz(150000, true);
  stdio_init_all();
  printf("\n[HP48] PicoCalc HP 48GX %s starting\n", HP48GX_VERSION);

  /* Bring up the STM32 first because it owns the LCD backlight. */
  bool keyboard_ok = keyboard_init();
  printf("[HP48] Initializing ILI9488 LCD with PIO mode 3 at 25 MHz\n");
  platform_ui_init();
  platform_ui_status(keyboard_ok ? "ARROWS MOVE SPACE=KEY ENTER=ENTER"
                                 : "KEYBOARD CONTROLLER NOT FOUND");
  printf("[HP48] LCD initialized\n");

  platform_sound_init();
  platform_sound_startup_beep();
  printf("[HP48] PicoCalc stereo sound initialized on GP26/GP27\n");

  platform_ui_status("CHECKING SD CARD...");
  storage_init();
  printf("[HP48] Storage: %s\n", storage_status_label());

  size_t rom_size = (size_t)(hp48_rom_end - hp48_rom);
  if (!hp48_core_init(hp48_rom, rom_size)) {
    printf("[HP48] ROM validation failed\n");
    platform_ui_status("ROM INVALID: EXPECTED GX REV R");
    while (true) tight_loop_contents();
  }
  printf("[HP48] ROM accepted; Saturn core initialized\n");

  char boot_status[48];
  bool restored = state_load();
  word_4 saved_card_status = saturn.card_status & 0x0fu;
  if (!restored) {
    set_accesstime();
  }
  port2_card_init();
  hp48_card_slot_t card_slot = hp48_card_slot();
  word_4 active_card_status = card_slot == HP48_CARD_PORT1 ? 0x0au
                              : card_slot == HP48_CARD_PORT2 ? 0x05u : 0u;
  bool card_slot_changed = restored && saved_card_status != active_card_status;
  if (card_slot_changed) {
    /* HP RAM contains the ROM's attached-library and covered-port tables.
     * Never resume a snapshot made with the other slot: keep that flash
     * snapshot recoverable, but start clean with the newly selected card. */
    hp48_core_reset();
    set_accesstime();
    restored = false;
  }
  s_files_status = object_files_init();
  if (card_slot_changed) {
    snprintf(boot_status, sizeof(boot_status), "CARD SLOT CHANGED | COLD | %s",
             port2_card_status_label());
  } else if (restored) {
    snprintf(boot_status, sizeof(boot_status), "RESTORED | SND | %s",
             port2_card_status_label());
  } else {
    snprintf(boot_status, sizeof(boot_status), "COLD BOOT | SND | %s",
             port2_card_status_label());
  }
  platform_ui_status(boot_status);
  got_alarm = 1;
  realtime_pacer_start(&s_realtime_pacer, time_us_64(),
                       s_speed_rates[s_speed_index]);

  while (true) {
    unsigned executed = hp48_core_run(CORE_RUN_QUANTUM);
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    if (s_import_wake_down &&
        (int32_t)(now_ms - s_import_wake_release_ms) >= 0) {
      hp48_key_set(0x8000, false);
      s_import_wake_down = false;
    }
    keyboard_poll();
    platform_ui_service(time_us_64());
    service_speed_adjustment();

    if (keyboard_take_file_cycle_request()) {
      s_files_status = object_files_cycle();
      char message[48];
      if (s_files_status == OBJECT_FILES_OK)
        snprintf(message, sizeof(message), "SELECTED %.20s | CTRL+F8 IMPORT",
                 object_files_selected_name());
      else
        snprintf(message, sizeof(message), "SELECT: %s",
                 object_files_status_label(s_files_status));
      platform_ui_status(message);
    }
    if (keyboard_take_file_export_request()) {
      platform_ui_status("EXPORTING STACK LEVEL 1...");
      s_files_status = object_files_export_stack();
      char message[48];
      if (s_files_status == OBJECT_FILES_OK)
        snprintf(message, sizeof(message), "EXPORTED OUTBOX/%s",
                 object_files_last_output_name());
      else
        snprintf(message, sizeof(message), "EXPORT: %s",
                 object_files_status_label(s_files_status));
      platform_ui_status(message);
    }

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
    pace_saturn(executed);
  }
}
