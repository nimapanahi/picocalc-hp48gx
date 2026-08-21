#include "keyboard.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include "core_runtime.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include "picocalc_protocol.h"
#include "platform_ui.h"
#include "power_sequence.h"
#include "state.h"

#define KBD_I2C i2c1
#define KBD_SDA 6
#define KBD_SCL 7
#define KBD_ADDR 0x1f
#define KBD_FIFO 0x09
#define KBD_VERSION 0x01
#define KBD_CONFIG 0x02
#define KBD_LCD_BACKLIGHT 0x05
#define KBD_WRITE_MASK 0x80
#define KBD_DEFAULT_CONFIG 0xd2

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
static bool s_text_mode;
static bool s_lshift;
static bool s_rshift;
static bool s_lshift_used;
static bool s_rshift_used;
static bool s_save_request;
static bool s_nav_poweroff_active;
static bool s_reset_request;
static uint8_t s_controller_version;
static uint32_t s_next_poll_ms;
static uint32_t s_next_battery_ms;
static uint16_t s_pending_context_code = 0xffff;
static uint16_t s_nav_active_code = 0xffff;
static power_sequence_t s_poweroff;

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

static void clear_pending_context(void) {
  s_pending_context_code = 0xffff;
  platform_ui_set_prefix_preview(0xffff);
}

static void set_pending_context(uint16_t code) {
  s_pending_context_code = code;
  platform_ui_set_prefix_preview(code);
}

static void toggle_pending_context(uint16_t code, const char *name) {
  if (code == 0x35) {
    if (s_pending_context_code != 0x35) {
      s_text_mode = false;
      platform_ui_set_text_mode(false);
      set_pending_context(0x35);
      platform_ui_status("ALPHA ONE-SHOT - SELECT A-Z");
    } else if (!s_text_mode) {
      s_text_mode = true;
      platform_ui_set_text_mode(true);
      platform_ui_status("ALPHA LOCK ON - SELECT A-Z");
    } else {
      s_text_mode = false;
      platform_ui_set_text_mode(false);
      clear_pending_context();
      platform_ui_status("ALPHA LOCK CANCELED");
    }
    return;
  }

  if (s_text_mode) {
    s_text_mode = false;
    platform_ui_set_text_mode(false);
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

/* A pending context is visual/UI state while the user navigates; it is not a
 * continuously held HP matrix key. Apply its modifier and the action together
 * only for the duration of the chosen key tap. Alpha Lock then re-arms Alpha;
 * all one-shot contexts return to the base keyboard. */
static void set_hp_action(uint16_t code, bool pressed) {
  if (pressed) {
    if (s_pending_context_code != 0xffff) {
      hp48_key_set(s_pending_context_code, true);
    }
    hp48_key_set(code, true);
  } else {
    hp48_key_set(code, false);
    if (s_pending_context_code != 0xffff) {
      hp48_key_set(s_pending_context_code, false);
      if (s_text_mode) {
        set_pending_context(0x35);
      } else {
        clear_pending_context();
      }
    }
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
    snprintf(message, sizeof(message), "KBD BIOS %u.%u - PICO OFF IN 6S",
             s_controller_version >> 4, s_controller_version & 0x0f);
  } else {
    snprintf(message, sizeof(message), "STATE SAFE - PICO OFF (BIOS %u.%u)",
             s_controller_version >> 4, s_controller_version & 0x0f);
  }
  platform_ui_status(message);
  sleep_ms(350);

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
      platform_ui_status("HP OFF CONFIRMED - PICO OFF IN 6S");
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

static void handle_event(uint8_t state, uint8_t key) {
  bool down = state == K_PRESS || state == K_HOLD;
  bool up = state == K_RELEASE;
  if (!down && !up) return;

  printf("[HP48] key state=%u code=0x%02x\n", state, key);

  /* The physical cursor is the user's finger. The full HP arrow cluster is
   * drawn and selectable, but the PicoCalc arrows themselves move the finger. */
  if (key == KEY_LEFT || key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN) {
    if (state == K_PRESS) {
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
    hp48_key_set(0x44, down);
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

  if (key == KEY_CTRL) {
    s_ctrl = down;
    return;
  }
  if (key == KEY_ALT) return;
  if (key == KEY_CAPS && state == K_PRESS) {
    s_text_mode = !s_text_mode;
    platform_ui_set_text_mode(s_text_mode);
    if (s_text_mode) {
      set_pending_context(0x35);
      platform_ui_status("ALPHA LOCK ON - SELECT A-Z");
    } else {
      clear_pending_context();
      platform_ui_status("ALPHA LOCK OFF");
    }
    return;
  }
  if (s_ctrl && key == KEY_F10 && state == K_PRESS) {
    s_save_request = true;
    return;
  }
  if (s_ctrl && key == KEY_ESC && state == K_PRESS) {
    s_reset_request = true;
    return;
  }
  if (code != 0xffff) {
    set_hp_action(code, down);
  }
}

bool keyboard_init(void) {
  power_sequence_reset(&s_poweroff);
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
  update_battery_status(now);
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
bool keyboard_text_mode(void) { return s_text_mode; }
