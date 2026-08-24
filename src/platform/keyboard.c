#include "keyboard.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "core_runtime.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "held_keys.h"
#include "pico/stdlib.h"
#include "picocalc_protocol.h"
#include "platform_ui.h"
#include "power_sequence.h"
#include "prefix_sequence.h"
#include "sound.h"
#include "state.h"
#include "storage.h"

#define KBD_I2C i2c1
#define KBD_SDA 6
#define KBD_SCL 7
#define KBD_ADDR 0x1f
#define KBD_FIFO 0x09
#define KBD_VERSION 0x01
#define KBD_CONFIG 0x02
#define KBD_KEY_STATUS 0x04
#define KBD_LCD_BACKLIGHT 0x05
#define KBD_WRITE_MASK 0x80
#define KBD_DEFAULT_CONFIG 0xd2
#define KBD_KEY_CAPS_LOCK 0x20

/* A real HP key is normally down far longer than one 15 ms controller poll.
 * Normalize quick PicoCalc taps so the Saturn keyboard scanner cannot miss
 * them, while remaining well below the ROM's key-repeat delay. */
#define HP_KEY_MIN_HOLD_MS 75
#define HP_GAME_COMMAND_MIN_HOLD_MS 300
#define WARM_START_TIMEOUT_MS 3000

#define K_IDLE 0
#define K_PRESS 1
#define K_HOLD 2
#define K_RELEASE 3

#define KEY_F1 0x81
#define KEY_F2 0x82
#define KEY_F3 0x83
#define KEY_F4 0x84
#define KEY_F5 0x85
#define KEY_F6 0x86
#define KEY_F7 0x87
#define KEY_F8 0x88
#define KEY_F9 0x89
#define KEY_F10 0x90
#define KEY_ALT 0xa1
#define KEY_SHL 0xa2
#define KEY_SHR 0xa3
#define KEY_CTRL 0xa5
#define KEY_ESC 0xb1
#define KEY_LEFT 0xb4
#define KEY_UP 0xb5
#define KEY_DOWN 0xb6
#define KEY_RIGHT 0xb7
#define KEY_CAPS 0xc1
#define KEY_DEL 0xd4

static bool s_ctrl;
static bool s_alt;
static bool s_text_mode;
static bool s_lshift;
static bool s_rshift;
static bool s_lshift_used;
static bool s_rshift_used;
static bool s_save_request;
static bool s_file_cycle_request;
static bool s_file_import_request;
static bool s_file_export_request;
static int8_t s_speed_adjustment;
static bool s_arrow_key_mode;
static bool s_nav_poweroff_active;
static bool s_reset_request;
static bool s_warm_start_chord;
static bool s_warm_start_pending;
static uint32_t s_warm_start_deadline_ms;
static uint8_t s_library_tools_page;
static uint8_t s_controller_version;
static uint32_t s_next_poll_ms;
static uint32_t s_next_battery_ms;
static uint32_t s_next_caps_sync_ms;
static bool s_follow_controller_caps = true;
static uint16_t s_pending_context_code = 0xffff;
static uint16_t s_nav_active_code = 0xffff;
static held_keys_t s_held_keys;
static power_sequence_t s_poweroff;
static prefix_sequence_t s_prefix_sequence;

static void setup_i2c(void) {
  i2c_init(KBD_I2C, 10 * 1000);
  gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
  gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(KBD_SDA);
  gpio_pull_up(KBD_SCL);
}

/* A short or interrupted controller read can leave SDA asserted.  Clock the
 * pending byte out and issue a STOP before reinitializing the RP2350 I2C
 * block.  This also makes recovery automatic after hot-plug/noisy starts. */
static void recover_i2c(void) {
  i2c_deinit(KBD_I2C);

  gpio_init(KBD_SDA);
  gpio_set_dir(KBD_SDA, GPIO_IN);
  gpio_pull_up(KBD_SDA);
  gpio_init(KBD_SCL);
  gpio_set_dir(KBD_SCL, GPIO_OUT);
  gpio_put(KBD_SCL, 1);
  sleep_us(50);

  for (int pulse = 0; pulse < 9 && !gpio_get(KBD_SDA); ++pulse) {
    gpio_put(KBD_SCL, 0);
    sleep_us(50);
    gpio_put(KBD_SCL, 1);
    sleep_us(50);
  }

  /* Explicit STOP: SDA low-to-high while SCL is high. */
  gpio_set_dir(KBD_SDA, GPIO_OUT);
  gpio_put(KBD_SDA, 0);
  sleep_us(50);
  gpio_put(KBD_SCL, 1);
  sleep_us(50);
  gpio_put(KBD_SDA, 1);
  sleep_us(50);
  gpio_set_dir(KBD_SDA, GPIO_IN);
  gpio_pull_up(KBD_SDA);

  sleep_ms(10);
  setup_i2c();
}

static bool read_reg(uint8_t reg, uint8_t *data, size_t length) {
  if (i2c_write_timeout_us(KBD_I2C, KBD_ADDR, &reg, 1, false, 5000) != 1) {
    recover_i2c();
    return false;
  }
  sleep_ms(1);
  if (i2c_read_timeout_us(KBD_I2C, KBD_ADDR, data, length, false, 5000) !=
      (int)length) {
    recover_i2c();
    return false;
  }
  return true;
}

static bool write_reg(uint8_t reg, uint8_t value) {
  uint8_t request[2] = {(uint8_t)(reg | KBD_WRITE_MASK), value};
  if (i2c_write_timeout_us(KBD_I2C, KBD_ADDR, request, sizeof(request),
                           false, 5000) != (int)sizeof(request)) {
    recover_i2c();
    return false;
  }
  return true;
}

static bool read_fifo(uint8_t event[2]) {
  return read_reg(KBD_FIFO, event, 2);
}

bool keyboard_set_lcd_backlight(uint8_t brightness) {
  return write_reg(KBD_LCD_BACKLIGHT, brightness);
}

static void update_battery_status(uint32_t now) {
  if ((int32_t)(now - s_next_battery_ms) < 0) return;
  s_next_battery_ms = now + 10000;

  uint8_t reply[2] = {0, 0};
  uint8_t percent = 0;
  bool charging = false;
  bool valid = read_reg(PICOCALC_REG_BATTERY, reply, sizeof(reply)) &&
               reply[0] == PICOCALC_REG_BATTERY &&
               picocalc_decode_battery(reply[1], &percent, &charging);
  platform_ui_set_battery(percent, charging, valid);
}

static uint16_t letter_code(char c) {
  static const uint16_t codes[26] = {
    0x14,0x84,0x83,0x82,0x81,0x80, 0x24,0x74,0x73,0x72,0x71,0x70,
    0x04,0x64,0x63,0x62,0x61,0x60, 0x34,0x54,0x53,0x52,0x51,0x50,
    0x43,0x42
  };
  c = (char)toupper((unsigned char)c);
  return c >= 'A' && c <= 'Z' ? codes[c - 'A'] : 0xffff;
}

static uint16_t direct_code(uint8_t key) {
  static const uint16_t digits[] = {0x03,0x13,0x12,0x11,0x23,0x22,0x21,0x33,0x32,0x31};
  if (key >= '0' && key <= '9') return digits[key - '0'];
  if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) return letter_code((char)key);
  switch (key) {
    case KEY_F1: return 0x14;
    case KEY_F2: return 0x84;
    case KEY_F3: return 0x83;
    case KEY_F4: return 0x82;
    case KEY_F5: return 0x81;
    case KEY_F6: return 0x80;
    case KEY_F7: return 0x24;
    case KEY_F8: return 0x74;
    case KEY_F9: return 0x73;
    case KEY_F10: return 0x72;
    case KEY_LEFT: return 0x62;
    case KEY_DOWN: return 0x61;
    case KEY_RIGHT: return 0x60;
    case KEY_UP: return 0x71;
    case 0x0a: return 0x44;
    case 0x08: return 0x40;
    case KEY_DEL: return 0x41;
    case 0x09: return 0x70;
    case KEY_ESC: return 0x8000;
    case ' ': return 0x01;
    case '.': return 0x02;
    case '+': case '=': return 0x00;
    case '-': return 0x10;
    case '*': return 0x20;
    case '/': return 0x30;
    default: return 0xffff;
  }
}

static const char *key_name(uint8_t key, char text[8]) {
  switch (key) {
    case KEY_F1: return "F1";
    case KEY_F2: return "F2";
    case KEY_F3: return "F3";
    case KEY_F4: return "F4";
    case KEY_F5: return "F5";
    case KEY_F6: return "F6";
    case KEY_F7: return "F7";
    case KEY_F8: return "F8";
    case KEY_F9: return "F9";
    case KEY_F10: return "F10";
    case KEY_ALT: return "ALT";
    case KEY_SHL: return "LSHIFT";
    case KEY_SHR: return "RSHIFT";
    case KEY_CTRL: return "CTRL";
    case KEY_ESC: return "ESC";
    case KEY_LEFT: return "LEFT";
    case KEY_UP: return "UP";
    case KEY_DOWN: return "DOWN";
    case KEY_RIGHT: return "RIGHT";
    case KEY_CAPS: return "CAPS";
    case KEY_DEL: return "DEL";
    case 0x0a: return "ENTER";
    case 0x08: return "BKSP";
    case 0x09: return "TAB";
    case ' ': return "SPACE";
    default:
      if (key >= 0x21 && key <= 0x7e) {
        text[0] = (char)key;
        text[1] = 0;
      } else {
        snprintf(text, 8, "%02X", key);
      }
      return text;
  }
}

static void show_key_press(uint8_t key, uint16_t matrix_code) {
  char name_buffer[8];
  char message[48];
  const char *name = key_name(key, name_buffer);
  if (matrix_code == 0xffff) {
    snprintf(message, sizeof(message), "KEY %s  RAW=%02X  HP=--", name, key);
  } else if (matrix_code == 0x8000) {
    snprintf(message, sizeof(message), "KEY %s  RAW=%02X  HP=ON", name, key);
  } else {
    snprintf(message, sizeof(message), "KEY %s  RAW=%02X  HP=%02X", name, key,
             matrix_code);
  }
  platform_ui_status(message);
}

static void sync_context_ui(void) {
  platform_ui_set_context(s_text_mode, s_pending_context_code);
}

static void clear_pending_context(void) {
  s_pending_context_code = 0xffff;
  sync_context_ui();
}

static void set_pending_context(uint16_t code) {
  s_pending_context_code = code;
  sync_context_ui();
}

static bool read_controller_caps(bool *enabled) {
  uint8_t reply[2] = {0, 0};
  if (!enabled || !read_reg(KBD_KEY_STATUS, reply, sizeof(reply))) return false;
  *enabled = (reply[0] & KBD_KEY_CAPS_LOCK) != 0;
  return true;
}

static void apply_controller_caps(bool enabled, bool announce) {
  if (s_text_mode == enabled &&
      (enabled ? s_pending_context_code == 0x35
               : s_pending_context_code != 0x35))
    return;
  s_text_mode = enabled;
  if (enabled) {
    set_pending_context(0x35);
  } else if (s_pending_context_code == 0x35) {
    clear_pending_context();
  } else {
    sync_context_ui();
  }
  if (announce)
    platform_ui_status(enabled ? "CAPS -> ALPHA LOCK ON"
                               : "CAPS -> ALPHA LOCK OFF");
}

static void update_controller_caps(uint32_t now) {
  if (!s_follow_controller_caps ||
      (int32_t)(now - s_next_caps_sync_ms) < 0)
    return;
  s_next_caps_sync_ms = now + 250;
  bool enabled;
  if (read_controller_caps(&enabled)) apply_controller_caps(enabled, true);
}

static void toggle_pending_context(uint16_t code, const char *name) {
  if (code == 0x35) {
    if (s_pending_context_code != 0x35) {
      s_text_mode = false;
      set_pending_context(0x35);
      platform_ui_status("ALPHA ONE-SHOT - SELECT A-Z");
    } else if (!s_text_mode) {
      s_text_mode = true;
      sync_context_ui();
      platform_ui_status("ALPHA LOCK ON - SELECT A-Z");
    } else {
      s_text_mode = false;
      clear_pending_context();
      platform_ui_status("ALPHA LOCK CANCELED");
    }
    return;
  }

  if (s_text_mode) {
    s_text_mode = false;
  }
  if (s_pending_context_code == code) {
    clear_pending_context();
    platform_ui_status("HP SHIFT PREFIX CANCELED");
    return;
  }
  set_pending_context(code);
  char message[48];
  snprintf(message, sizeof(message), "PENDING %s -> HP %s", name,
           code == 0x25 ? "PURPLE" : "TEAL");
  platform_ui_status(message);
}

/* A pending context is never a continuously held matrix key. Drawn contexts
 * are already latched by the ROM; controller-only contexts need an ordered
 * prefix tap before the chosen action. Alpha Lock remains active while all
 * one-shot contexts return to the base keyboard. */
static void apply_prefix_transition(prefix_transition_t transition) {
  if (transition.kind == PREFIX_TRANSITION_PRESS)
    hp48_key_set(transition.code, true);
  else if (transition.kind == PREFIX_TRANSITION_RELEASE)
    hp48_key_set(transition.code, false);
}

static void finish_action_context(void) {
  if (s_pending_context_code != 0xffff) {
    hp48_key_set(s_pending_context_code, false);
    if (s_text_mode) {
      /* Alpha Lock stays active in the context UI. The next action checks the
       * ROM annunciator and re-arms Alpha automatically if necessary. */
      sync_context_ui();
    } else {
      clear_pending_context();
    }
  }
}

static void release_hp_action(held_key_release_t release) {
  hp48_key_set(release.code, false);
  if (release.consumes_context) finish_action_context();
}

static void finish_hp_action(void) {
  held_key_release_t release;
  while (held_keys_pop_any(&s_held_keys, &release))
    release_hp_action(release);
}

static void begin_hp_action_for(uint16_t code, bool consumes_context,
                                uint32_t minimum_hold_ms) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (held_keys_press(&s_held_keys, code, now, minimum_hold_ms,
                      consumes_context)) {
    hp48_key_set(code, true);
  } else {
    platform_ui_status("HP KEY CHORD FULL - RELEASE KEYS");
  }
}

static void begin_hp_action_with_context(uint16_t code,
                                         bool consumes_context) {
  begin_hp_action_for(code, consumes_context, HP_KEY_MIN_HOLD_MS);
}

static void begin_hp_action(uint16_t code) {
  begin_hp_action_with_context(code, false);
}

static void request_hp_action_release(uint16_t code) {
  if (!held_keys_request_release(&s_held_keys, code))
    hp48_key_set(code, false);
}

static bool service_hp_action_release(uint32_t now) {
  bool released = false;
  held_key_release_t release;
  while (held_keys_pop_due_release(&s_held_keys, now, &release)) {
    release_hp_action(release);
    released = true;
  }
  /* Do not drain the next controller event until this action has received its
   * full minimum down-time. The event remains safely queued in the STM32. */
  return released || held_keys_release_waiting(&s_held_keys);
}

static void set_hp_action(uint16_t code, bool pressed) {
  if (pressed) {
    if (s_pending_context_code != 0xffff) {
      if (hp48_prefix_latched(s_pending_context_code)) {
        /* Trust the HP ROM's annunciator, not the contextual overlay. */
        begin_hp_action_with_context(code, true);
      } else {
        /* The drawn tap may not have reached the ROM yet, and controller Caps
         * has no HP matrix event. Re-arm the prefix and wait for the ROM's
         * annunciator before delivering the action. */
        apply_prefix_transition(prefix_sequence_start(
            &s_prefix_sequence, s_pending_context_code, code));
      }
      if (code == 0x12 && s_pending_context_code == 0x15) {
        platform_ui_status("LIBRARY CATALOG - BLANK UNTIL ATTACHED");
        s_library_tools_page = 0;
      } else if (code == 0x12 && s_pending_context_code == 0x25) {
        platform_ui_status("PORT TOOLS - PRESS NXT FOR PINIT");
        s_library_tools_page = 1;
      } else {
        s_library_tools_page = 0;
      }
    } else {
      begin_hp_action(code);
      if (code == 0x70 && s_library_tools_page != 0) {
        s_library_tools_page = s_library_tools_page == 1 ? 2 : 1;
      } else if (code == 0x84 && s_library_tools_page == 2) {
        platform_ui_status(hp48_port2_attached()
                               ? "PINIT SENT - P2 READY; COMMAND IS SILENT"
                               : "PINIT SENT - NO PORT 2 CARD ATTACHED");
        s_library_tools_page = 0;
      } else {
        s_library_tools_page = 0;
      }
    }
  } else {
    request_hp_action_release(code);
  }
}

static void request_system_poweroff(bool hp_off_confirmed) {
  /* Refresh the version for a visible diagnostic, but do not use a transient
   * read failure to suppress the command. The bounded wait below recovers
   * safely if an old controller ignores REG_POWER_OFF. */
  uint8_t version_reply[2] = {0, 0};
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (read_reg(KBD_VERSION, version_reply, sizeof(version_reply)) &&
        version_reply[1] != 0) {
      s_controller_version = version_reply[1];
      break;
    }
    sleep_ms(25);
  }

  char message[48];
  if (hp_off_confirmed) {
    snprintf(message, sizeof(message), "KBD BIOS %u.%u - PMU OFF REQUEST",
             s_controller_version >> 4, s_controller_version & 0x0f);
  } else {
    snprintf(message, sizeof(message), "STATE SAFE - PICO OFF (BIOS %u.%u)",
             s_controller_version >> 4, s_controller_version & 0x0f);
  }
  platform_ui_status(message);

  /* Flush and unmount removable storage before the PMU countdown. Static PWM
   * produces no audible signal, but return it to its midpoint as well. */
  storage_shutdown();
  platform_sound_silence();

  /* The keyboard MCU blocks while its PMU countdown runs, so blank the panel
   * before sending REG_POWER_OFF. If power is not removed after a bounded
   * wait, resume and restore the panel instead of hanging. */
  keyboard_set_lcd_backlight(0);
  bool requested = false;
  for (int attempt = 0; attempt < 3 && !requested; ++attempt) {
    requested = write_reg(PICOCALC_REG_POWER_OFF,
                          PICOCALC_POWER_OFF_DELAY_SECONDS);
    if (!requested) sleep_ms(25);
  }
  if (requested) {
    printf("[HP48] PicoCalc PMU shutdown requested (HP LCD %s)\n",
           hp_off_confirmed ? "confirmed off" : "unstable; state saved");
    stdio_flush();
    uint64_t deadline_us = time_us_64() + 9000000ull;
    while (time_us_64() < deadline_us) sleep_ms(20);
    keyboard_set_lcd_backlight(192);
    power_sequence_reset(&s_poweroff);
    snprintf(message, sizeof(message), "PICO OFF TIMEOUT - KBD BIOS %u.%u",
             s_controller_version >> 4, s_controller_version & 0x0f);
    platform_ui_status(message);
    printf("[HP48] PicoCalc PMU did not remove power before timeout\n");
  } else {
    keyboard_set_lcd_backlight(192);
    power_sequence_reset(&s_poweroff);
    platform_ui_status("PICO POWER-OFF FAILED - STATE SAFE");
  }
}

/* HP OFF is a prefix sequence, not a simultaneous key chord. The sequence
 * advances one matrix transition per poll, verifies that the ROM actually
 * disabled its LCD, retries if necessary, and only then asks the PicoCalc PMU
 * to remove system power. */
static bool advance_poweroff_sequence(uint32_t now) {
  power_action_t action =
      power_sequence_poll(&s_poweroff, now, hp48_core_lcd_on());
  switch (action) {
    case POWER_ACTION_NONE:
      return false;
    case POWER_ACTION_PRESS_TEAL: {
      hp48_key_set(0x15, true);
      char message[48];
      snprintf(message, sizeof(message), "OFF ATTEMPT %u/%u: TEAL",
               (unsigned)power_sequence_attempts(&s_poweroff),
               (unsigned)POWER_SEQUENCE_MAX_ATTEMPTS);
      platform_ui_status(message);
      break;
    }
    case POWER_ACTION_RELEASE_TEAL:
      hp48_key_set(0x15, false);
      break;
    case POWER_ACTION_PRESS_ON:
      hp48_key_set(0x8000, true);
      platform_ui_status("STATE SAVED - SENDING HP OFF");
      break;
    case POWER_ACTION_RELEASE_ON:
      hp48_key_set(0x8000, false);
      platform_ui_status("WAITING FOR HP LCD OFF...");
      break;
    case POWER_ACTION_HP_OFF_DETECTED:
      platform_ui_status("HP LCD OFF - VERIFYING...");
      break;
    case POWER_ACTION_HP_OFF_REAWAKENED:
      hp48_key_set(0x15, false);
      hp48_key_set(0x8000, false);
      platform_ui_status("HP LCD WOKE - RETRYING OFF");
      break;
    case POWER_ACTION_HP_OFF_CONFIRMED:
      platform_ui_status("HP OFF CONFIRMED - REQUESTING PICO OFF");
      break;
    case POWER_ACTION_REQUEST_SYSTEM_OFF:
      request_system_poweroff(true);
      break;
    case POWER_ACTION_REQUEST_SYSTEM_OFF_UNCONFIRMED:
      hp48_key_set(0x15, false);
      hp48_key_set(0x8000, false);
      request_system_poweroff(false);
      break;
    case POWER_ACTION_FAILED:
      hp48_key_set(0x15, false);
      hp48_key_set(0x8000, false);
      platform_ui_status("HP OFF NOT CONFIRMED - STATE SAFE");
      break;
  }
  return true;
}

static void start_graceful_poweroff(const char *source) {
  if (power_sequence_active(&s_poweroff)) return;

  s_warm_start_pending = false;
  s_nav_poweroff_active = false;
  finish_hp_action();
  prefix_sequence_reset(&s_prefix_sequence);
  s_nav_active_code = 0xffff;
  platform_ui_nav_set_pressed(false);
  clear_pending_context();
  hp48_key_set(0x25, false);
  hp48_key_set(0x15, false);
  hp48_key_set(0x35, false);
  hp48_key_set(0x8000, false);
  platform_ui_status(source);
  if (state_save()) {
    power_sequence_start(&s_poweroff,
                         to_ms_since_boot(get_absolute_time()));
    platform_ui_status("STATE SAVED - STARTING OFF");
  } else {
    power_sequence_reset(&s_poweroff);
    platform_ui_status("SAVE FAILED - OFF CANCELED");
  }
}

/* A library attach needs the same non-destructive OFF/ON cycle as a real
 * calculator. Alt+Esc is a platform shortcut, so synthesize the HP
 * keys in their required order instead of treating it as a simultaneous
 * matrix chord. The normal input poll keeps running between every transition. */
static void start_warm_start(uint32_t now) {
  if (power_sequence_active(&s_poweroff)) {
    platform_ui_status("WARM START UNAVAILABLE DURING POWER-OFF");
    return;
  }

  finish_hp_action();
  prefix_sequence_reset(&s_prefix_sequence);
  s_nav_active_code = 0xffff;
  platform_ui_nav_set_pressed(false);
  clear_pending_context();
  hp48_key_set(0x25, false);
  hp48_key_set(0x15, false);
  hp48_key_set(0x35, false);
  hp48_key_set(0x8000, false);

  s_warm_start_pending = true;
  s_warm_start_deadline_ms = now + WARM_START_TIMEOUT_MS;
  apply_prefix_transition(
      prefix_sequence_start(&s_prefix_sequence, 0x15, 0x8000));
  platform_ui_status("WARM START - SENDING TEAL, OFF, ON");
}

static void handle_event(uint8_t state, uint8_t key) {
  bool down = state == K_PRESS || state == K_HOLD;
  bool up = state == K_RELEASE;
  if (!down && !up) return;

  printf("[HP48] key state=%u code=0x%02x\n", state, key);

  /* The controller emits one KEY_POWER press for a short physical power-key
   * tap (and no matching release). Long press remains the PMU's emergency
   * hardware shutdown. Route the short tap through the same state-saving,
   * HP-OFF-verifying sequence as the drawn OFF key. */
  if (key == PICOCALC_KEY_POWER) {
    if (state == K_PRESS)
      start_graceful_poweroff("POWER KEY - SAVING BEFORE OFF...");
    return;
  }

  /* Ctrl+Space latches the physical cursor cluster between UI navigation and
   * the calculator's four real arrow matrix keys. It is deliberately a
   * toggle, like Caps Lock, so games and stack tools can use repeated arrows
   * without holding a modifier. */
  if (key == ' ' && s_ctrl) {
    if (state == K_PRESS) {
      finish_hp_action();
      s_arrow_key_mode = !s_arrow_key_mode;
      platform_ui_set_arrow_key_mode(s_arrow_key_mode);
      platform_ui_status(
          s_arrow_key_mode ? "ARROW LOCK ON - PHYSICAL -> HP KEYS"
                           : "ARROW LOCK OFF - ARROWS MOVE CURSOR");
    }
    return;
  }

  /* Native games expose timing errors much more clearly than the ROM UI.
   * Ctrl+Left/Right changes the decoded-instruction pacing preset without
   * consuming an HP matrix key, allowing calibration on actual hardware. */
  if (s_ctrl && (key == KEY_LEFT || key == KEY_RIGHT)) {
    if (state == K_PRESS) {
      s_speed_adjustment = key == KEY_LEFT ? -1 : 1;
      hp48_core_platform_wake();
    }
    return;
  }

  /* The physical cursor is the user's finger. The full HP arrow cluster is
   * drawn and selectable, but the PicoCalc arrows themselves move the finger. */
  if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) {
    if (s_arrow_key_mode) {
      uint16_t arrow_code = direct_code(key);
      if (state == K_PRESS) {
        show_key_press(key, arrow_code);
        begin_hp_action(arrow_code);
      } else if (state == K_RELEASE) {
        request_hp_action_release(arrow_code);
      }
    } else if (state == K_PRESS) {
      platform_ui_nav_move(key == KEY_LEFT ? -1 : key == KEY_RIGHT ? 1 : 0,
                           key == KEY_UP ? -1 : key == KEY_DOWN ? 1 : 0);
      char message[48];
      snprintf(message, sizeof(message), "CURSOR %s - SPACE PRESSES",
               platform_ui_nav_selected_label());
      platform_ui_status(message);
    }
    return;
  }

  /* Space is the on-screen finger. Physical Enter remains a dedicated HP
   * ENTER key so typed entries can always be committed to the stack. */
  if (key == ' ') {
    if (state == K_PRESS && s_nav_active_code == 0xffff) {
      s_nav_active_code = platform_ui_nav_selected_code();
      platform_ui_nav_set_pressed(true);
      char message[48];
      snprintf(message, sizeof(message), "PRESS %s -> HP %s",
               platform_ui_nav_selected_label(),
               s_nav_active_code == 0x8000 ? "ON" : "KEY");
      platform_ui_status(message);
      if (s_nav_active_code == 0x8000 && s_pending_context_code == 0x15) {
        /* This handler can run from the Saturn SHUTDN/GetEvent loop, where
         * main() is unable to service a deferred request. Save synchronously
         * with no emulated keys held, then begin the ordered OFF sequence. */
        s_nav_poweroff_active = true;
        clear_pending_context();
        platform_ui_status("SAVING BEFORE OFF...");
        if (state_save()) {
          hp48_key_set(0x15, false);
          hp48_key_set(0x8000, false);
          power_sequence_start(&s_poweroff,
                               to_ms_since_boot(get_absolute_time()));
          platform_ui_status("STATE SAVED - STARTING OFF");
        } else {
          power_sequence_reset(&s_poweroff);
          platform_ui_status("SAVE FAILED - OFF CANCELED");
        }
      } else if (s_nav_active_code == 0x25 || s_nav_active_code == 0x15 ||
          s_nav_active_code == 0x35) {
        if (s_nav_active_code == 0x35) s_follow_controller_caps = false;
        /* Send the drawn context key itself to the ROM. Previously it only
         * changed our overlay, so a later T could arrive as bare COS. */
        hp48_key_set(s_nav_active_code, true);
        toggle_pending_context(s_nav_active_code,
                               s_nav_active_code == 0x25 ? "LSH"
                               : s_nav_active_code == 0x15 ? "RSH" : "ALPHA");
      } else {
        set_hp_action(s_nav_active_code, true);
      }
    } else if (state == K_RELEASE && s_nav_active_code != 0xffff) {
      if (s_nav_poweroff_active) {
        s_nav_poweroff_active = false;
      } else if (s_nav_active_code != 0x25 && s_nav_active_code != 0x15 &&
          s_nav_active_code != 0x35) {
        set_hp_action(s_nav_active_code, false);
      } else {
        hp48_key_set(s_nav_active_code, false);
      }
      s_nav_active_code = 0xffff;
      platform_ui_nav_set_pressed(false);
    }
    return;
  }

  /* Keep one unconditional path to the calculator's ENTER key. A pending
   * context is canceled here; shifted ENTER functions remain available by
   * highlighting the drawn ENTER key and pressing Space. */
  if (key == 0x0a) {
    if (state == K_PRESS) {
      if (s_lshift) s_lshift_used = true;
      if (s_rshift) s_rshift_used = true;
      if (!(s_text_mode && s_pending_context_code == 0x35)) {
        clear_pending_context();
      }
      show_key_press(key, 0x44);
    }
    if (state == K_PRESS) begin_hp_action(0x44);
    else if (state == K_RELEASE) request_hp_action_release(0x44);
    if (state == K_RELEASE && s_text_mode &&
        s_pending_context_code == 0x35)
      sync_context_ui();
    return;
  }

  uint16_t code = direct_code(key);
  if (state == K_PRESS) show_key_press(key, code);

  /* The STM32 both reports Shift and applies it to the following character.
   * Do not also hold an HP shift during that chord: Shift+8 is already '*',
   * and double-shifting HP multiply produces '['.  A shift pressed and
   * released on its own becomes the corresponding HP prefix-shift tap. */
  if (key == KEY_SHL) {
    if (down) {
      if (!s_lshift) {
        s_lshift_used = false;
        /* A pending context is canceled on the next physical press, not its
         * release. Mark this press used so release cannot latch it again. */
        if (s_pending_context_code == 0x25) {
          clear_pending_context();
          s_lshift_used = true;
          platform_ui_status("HP PURPLE PREFIX CANCELED");
        }
      }
      s_lshift = true;
    } else {
      s_lshift = false;
      if (!s_lshift_used) toggle_pending_context(0x25, "LSHIFT");
    }
    return;
  }
  if (key == KEY_SHR) {
    if (down) {
      if (!s_rshift) {
        s_rshift_used = false;
        if (s_pending_context_code == 0x15) {
          clear_pending_context();
          s_rshift_used = true;
          platform_ui_status("HP TEAL PREFIX CANCELED");
        }
      }
      s_rshift = true;
    } else {
      s_rshift = false;
      if (!s_rshift_used) toggle_pending_context(0x15, "RSHIFT");
    }
    return;
  }
  if (state == K_PRESS) {
    if (s_lshift) s_lshift_used = true;
    if (s_rshift) s_rshift_used = true;
  }

  /* Alt+Esc performs a warm OFF/ON cycle. Track the chord through its
   * release so Esc cannot leak through as a second, ordinary HP ON key if the
   * user releases Alt first. Right Shift+Esc deliberately falls through to
   * the calculator so the PicoCalc controller's BRK chord keeps working. */
  if (key == KEY_ESC && (s_alt || s_warm_start_chord)) {
    if (state == K_PRESS && s_alt) {
      s_warm_start_chord = true;
      start_warm_start(to_ms_since_boot(get_absolute_time()));
    } else if (state == K_RELEASE) {
      s_warm_start_chord = false;
    }
    return;
  }

  if (key == KEY_CTRL) {
    s_ctrl = down;
    return;
  }
  if (key == KEY_ALT) {
    s_alt = down;
    return;
  }
  if (s_ctrl && (key == KEY_F7 || key == KEY_F8 || key == KEY_F9 ||
                 key == KEY_F10)) {
    if (state == K_PRESS) {
      if (key == KEY_F7) s_file_cycle_request = true;
      if (key == KEY_F8) {
        s_file_import_request = true;
        platform_ui_status("IMPORT QUEUED - WAITING FOR HP IDLE");
      }
      if (key == KEY_F9) s_file_export_request = true;
      if (key == KEY_F10) s_save_request = true;
      /* A platform-only shortcut must still leave Saturn's SHUTDN loop so
       * main() can perform the requested SD/flash operation. Import is the
       * exception: it must run inside SHUTDN before the wake interrupt. */
      if (key != KEY_F8) hp48_core_platform_wake();
    }
    return;
  }
  if (key == KEY_CAPS && state == K_PRESS) {
    s_follow_controller_caps = true;
    bool enabled;
    if (!read_controller_caps(&enabled)) enabled = !s_text_mode;
    apply_controller_caps(enabled, false);
    platform_ui_status(enabled ? "CAPS -> ALPHA LOCK ON - TYPE A-Z"
                               : "CAPS -> ALPHA LOCK OFF");
    return;
  }
  if (s_ctrl && key == KEY_ESC && state == K_PRESS) {
    s_warm_start_pending = false;
    s_warm_start_chord = false;
    prefix_sequence_reset(&s_prefix_sequence);
    finish_hp_action();
    s_reset_request = true;
    hp48_core_platform_wake();
    return;
  }
  if (code != 0xffff) {
    /* A controller HOLD report leaves the matrix key down; replaying it would
     * incorrectly restart an ordered prefix sequence. */
    if (state == K_PRESS && s_arrow_key_mode && code == 0x40 &&
        s_pending_context_code == 0xffff) {
      /* Native presentations often scan DROP only between long effects.
       * Arrow lock denotes game-control mode, so stretch a quick physical
       * Backspace tap without delaying its initial matrix press. */
      begin_hp_action_for(code, false, HP_GAME_COMMAND_MIN_HOLD_MS);
    } else if (state == K_PRESS) {
      set_hp_action(code, true);
    } else if (state == K_RELEASE) {
      set_hp_action(code, false);
    }
  }
}

bool keyboard_init(void) {
  power_sequence_reset(&s_poweroff);
  prefix_sequence_reset(&s_prefix_sequence);
  held_keys_init(&s_held_keys);
  s_alt = false;
  s_warm_start_chord = false;
  s_warm_start_pending = false;
  setup_i2c();
  uint32_t boot_ms = to_ms_since_boot(get_absolute_time());
  if (boot_ms < 2500) sleep_ms(2500 - boot_ms);

  /* Clear any transaction left incomplete by the previous firmware or reset. */
  recover_i2c();

  uint8_t version_reply[2] = {0, 0};
  for (int attempt = 0; attempt < 50; ++attempt) {
    /* REG_VERSION is a two-byte reply: reserved/status, then version.  Reading
     * only its first byte leaves some controller firmware mid-transaction and
     * makes every later FIFO read fail. */
    if (read_reg(KBD_VERSION, version_reply, sizeof(version_reply))) {
      bool config_ok = write_reg(KBD_CONFIG, KBD_DEFAULT_CONFIG);
      bool backlight_ok = keyboard_set_lcd_backlight(192);
      s_controller_version = version_reply[1];
      printf("[HP48] PicoCalc controller v%u.%u, config %s, backlight %s, "
             "PMU off register %s\n",
             version_reply[1] >> 4, version_reply[1] & 0x0f,
             config_ok ? "ok" : "write failed",
             backlight_ok ? "on" : "write failed",
             picocalc_bios_supports_power_off(version_reply[1])
                 ? "expected" : "not expected");
      return true;
    }
    sleep_ms(100);
  }
  printf("[HP48] PicoCalc keyboard controller not found\n");
  return false;
}

bool keyboard_poll(void) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (now < s_next_poll_ms) return false;
  /* One event per 15 ms keeps taps responsive and drains the controller FIFO
   * faster than its 100 ms key-repeat interval, while still returning to the
   * Saturn CPU between a queued press and release. */
  s_next_poll_ms = now + 15;
  if (advance_poweroff_sequence(now)) return true;
  if (service_hp_action_release(now)) return true;
  if (prefix_sequence_active(&s_prefix_sequence)) {
    prefix_transition_t transition = prefix_sequence_advance(
        &s_prefix_sequence,
        hp48_prefix_latched(s_prefix_sequence.prefix_code));
    if (transition.kind == PREFIX_TRANSITION_PRESS &&
        !prefix_sequence_active(&s_prefix_sequence) &&
        transition.code == s_prefix_sequence.action_code) {
      begin_hp_action_with_context(
          transition.code, s_pending_context_code != 0xffff);
      if (s_warm_start_pending && transition.code == 0x8000)
        request_hp_action_release(transition.code);
    } else {
      apply_prefix_transition(transition);
    }
    return true;
  }
  if (s_warm_start_pending && held_keys_empty(&s_held_keys)) {
    if (!hp48_core_lcd_on()) {
      /* The first ON completed the teal OFF command. A second ordinary ON
       * wakes the sleeping ROM without clearing RAM, variables, or cards. */
      begin_hp_action(0x8000);
      request_hp_action_release(0x8000);
      s_warm_start_pending = false;
      platform_ui_status("WARM START COMPLETE - HP WAKING");
      return true;
    }
    if ((int32_t)(now - s_warm_start_deadline_ms) >= 0) {
      s_warm_start_pending = false;
      platform_ui_status("WARM START FAILED - HP DID NOT TURN OFF");
      return true;
    }
  }
  update_battery_status(now);
  update_controller_caps(now);
  /* Deliver exactly one queued event, then return to the Saturn CPU.  If a
   * complete tap (press + release) is drained here in one call, the ROM never
   * executes while its matrix bit is down and therefore sees no key at all. */
  uint8_t event[2];
  if (read_fifo(event) && event[0] != K_IDLE) {
    handle_event(event[0], event[1]);
    return true;
  }
  return false;
}

bool keyboard_take_save_request(void) { bool v = s_save_request; s_save_request = false; return v; }
bool keyboard_take_reset_request(void) { bool v = s_reset_request; s_reset_request = false; return v; }
bool keyboard_take_file_cycle_request(void) { bool v = s_file_cycle_request; s_file_cycle_request = false; return v; }
bool keyboard_take_file_import_request(void) { bool v = s_file_import_request; s_file_import_request = false; return v; }
bool keyboard_take_file_export_request(void) { bool v = s_file_export_request; s_file_export_request = false; return v; }
int keyboard_take_speed_adjustment(void) { int v = s_speed_adjustment; s_speed_adjustment = 0; return v; }
