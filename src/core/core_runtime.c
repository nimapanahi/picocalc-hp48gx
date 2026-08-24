#include "core_runtime.h"

#include <string.h>

#include "device.h"
#include "hp48_emu.h"
#include "timer.h"

#define X48_MAGIC 0x48503438u

saturn_t saturn;

int verbose = 0;
int quiet = 1;
int throttle = 0;
int opt_gx = 1;
char *progname = "hp48gx-picocalc";
int enter_debugger = 0;
int in_debugger = 0;
extern int hp48_core_sleeping;

short rom_is_new = 1;
long ram_size = RAM_SIZE_GX;
long port1_size = 0;
long port1_mask = 0;
short port1_is_ram = 0;
long port2_size = 0;
long port2_mask = 0;
short port2_is_ram = 0;

static uint8_t s_ram[RAM_SIZE_GX];
static uint8_t s_card_packed[HP48_PORT2_PACKED_SIZE];
static hp48_card_slot_t s_card_slot;
static bool s_card_dirty;

static void clear_card_configuration(void) {
  port1_size = 0;
  port1_mask = 0;
  port1_is_ram = 0;
  port2_size = 0;
  port2_mask = 0;
  port2_is_ram = 0;
  saturn.port1 = NULL;
  saturn.port2 = NULL;
  saturn.card_status &= (word_4)~0x0fu;
}

static void configure_port1(void) {
  if (s_card_slot != HP48_CARD_PORT1) return;
  clear_card_configuration();
  port1_size = HP48_PORT2_NIBBLE_SIZE;
  port1_mask = HP48_PORT2_NIBBLE_SIZE - 1u;
  port1_is_ram = 1;
  /* Port 1 present and writable. Its packed backing shares the same bounded
   * SRAM allocation as Port 2, so only one virtual card is mounted at once. */
  saturn.card_status |= 0x0au;
  device.card_status_touched = 1;
}

static void configure_port2(void) {
  if (s_card_slot != HP48_CARD_PORT2) return;
  clear_card_configuration();
  port2_size = HP48_PORT2_NIBBLE_SIZE;
  port2_mask = HP48_PORT2_NIBBLE_SIZE - 1u;
  port2_is_ram = 1;
  /* Port 2 present and writable in the GX card-status register. Memory access
   * goes through the packed accessors below, not through this legacy pointer. */
  saturn.port2 = NULL;
  saturn.card_status |= 0x05u;
  device.card_status_touched = 1;
}

static void start_runtime_timers(void) {
  reset_timer(IDLE_TIMER);
  restart_timer(T1_TIMER);
  restart_timer(RUN_TIMER);
  sched_timer1 = saturn.t1_tick;
  sched_timer2 = saturn.t2_tick;
  set_t1 = saturn.timer1;
}

void init_saturn(void) {
  hp48_core_sleeping = 0;
  memset(&saturn, 0, sizeof(saturn));
  saturn.PC = 0;
  saturn.magic = X48_MAGIC;
  saturn.version[0] = VERSION_MAJOR;
  saturn.version[1] = VERSION_MINOR;
  saturn.version[2] = PATCHLEVEL;
  saturn.version[3] = COMPILE_VERSION;
  saturn.t1_tick = 8192;
  saturn.t2_tick = 16;
  saturn.hexmode = HEX;
  saturn.rstkp = -1;
  saturn.intenable = 1;
  saturn.kbd_ien = 1;
  saturn.timer2 = 0x2000;
  for (int i = 0; i < NR_MCTL; ++i) {
    saturn.mem_cntl[i].unconfigured = (i == 0) ? 1 : ((i == 5) ? 0 : 2);
  }
  memset(&device, 0, sizeof(device));
  device.display_touched = 1;
  device.contrast_touched = 1;
  device.ann_touched = 1;
  dev_memory_init();
}

bool hp48_core_init(const uint8_t *unpacked_rom, size_t rom_size) {
  if (!unpacked_rom || rom_size != 0x100000u) return false;
  s_card_slot = HP48_CARD_NONE;
  s_card_dirty = false;
  init_saturn();
  clear_card_configuration();
  memset(s_ram, 0, sizeof(s_ram));
  saturn.rom = unpacked_rom;
  saturn.ram = s_ram;
  saturn.port1 = NULL;
  saturn.port2 = NULL;
  init_display();
  start_runtime_timers();
  return true;
}

void hp48_core_reset(void) {
  const uint8_t *rom = saturn.rom;
  hp48_card_slot_t restore_card = s_card_slot;
  init_saturn();
  memset(s_ram, 0, sizeof(s_ram));
  saturn.rom = rom;
  saturn.ram = s_ram;
  if (restore_card == HP48_CARD_PORT1) configure_port1();
  else if (restore_card == HP48_CARD_PORT2) configure_port2();
  init_display();
  start_runtime_timers();
  enter_debugger = 0;
}

void hp48_key_set(uint16_t code, bool pressed) {
  if (code == 0x8000u) {
    for (int i = 0; i < 9; ++i) {
      if (pressed) saturn.keybuf.rows[i] |= 0x8000;
      else saturn.keybuf.rows[i] &= (short)~0x8000;
    }
    if (pressed) do_kbd_int();
    return;
  }
  int row = code >> 4;
  int mask = 1 << (code & 0x0f);
  if (row < 0 || row >= 9) return;
  if (pressed) {
    if ((saturn.keybuf.rows[row] & mask) == 0) {
      saturn.keybuf.rows[row] |= mask;
      if (saturn.kbd_ien) do_kbd_int();
    }
  } else {
    saturn.keybuf.rows[row] &= (short)~mask;
  }
}

bool hp48_prefix_latched(uint16_t code) {
  int annunciator = code == 0x25 ? ANN_LEFT
                    : code == 0x15 ? ANN_RIGHT
                    : code == 0x35 ? ANN_ALPHA : 0;
  return annunciator != 0 &&
         (saturn.annunc & annunciator) == annunciator;
}

unsigned hp48_core_run(unsigned instruction_budget) {
  unsigned display_service_budget = 4096;
  unsigned executed = 0;
  while (executed < instruction_budget && !enter_debugger) {
    step_instruction();
    ++executed;
    if (schedule_event-- <= 0) schedule();
    if (--display_service_budget == 0) {
      display_service(0);
      display_service_budget = 4096;
    }
  }
  display_service(0);
  return executed;
}

void hp48_core_service_display(void) { display_service(0); }
bool hp48_core_lcd_on(void) { return display.on != 0; }
bool hp48_core_is_sleeping(void) { return hp48_core_sleeping != 0; }
void hp48_core_platform_wake(void) { do_kbd_int(); }
uint8_t *hp48_ram_data(void) { return s_ram; }
size_t hp48_ram_size(void) { return sizeof(s_ram); }

void hp48_port2_attach_blank(void) {
  memset(s_card_packed, 0, sizeof(s_card_packed));
  s_card_slot = HP48_CARD_PORT2;
  s_card_dirty = true;
  configure_port2();
}

void hp48_port1_attach_blank(void) {
  memset(s_card_packed, 0, sizeof(s_card_packed));
  s_card_slot = HP48_CARD_PORT1;
  s_card_dirty = true;
  configure_port1();
}

void hp48_port2_detach(void) {
  s_card_slot = HP48_CARD_NONE;
  s_card_dirty = false;
  clear_card_configuration();
  device.card_status_touched = 1;
}

bool hp48_port2_attached(void) { return s_card_slot == HP48_CARD_PORT2; }
bool hp48_port1_attached(void) { return s_card_slot == HP48_CARD_PORT1; }
bool hp48_port2_dirty(void) {
  return s_card_slot != HP48_CARD_NONE && s_card_dirty;
}
void hp48_port2_mark_clean(void) { s_card_dirty = false; }
hp48_card_slot_t hp48_card_slot(void) { return s_card_slot; }

uint8_t *hp48_port2_packed_data(void) {
  return s_card_slot != HP48_CARD_NONE ? s_card_packed : NULL;
}

static uint8_t read_card_nibble(hp48_card_slot_t slot, size_t offset) {
  if (s_card_slot != slot) return 0;
  offset &= HP48_PORT2_NIBBLE_SIZE - 1u;
  uint8_t packed = s_card_packed[offset >> 1];
  return (offset & 1u) ? packed >> 4 : packed & 0x0fu;
}

static void write_card_nibble(hp48_card_slot_t slot, size_t offset,
                              uint8_t value) {
  if (s_card_slot != slot) return;
  offset &= HP48_PORT2_NIBBLE_SIZE - 1u;
  uint8_t *packed = &s_card_packed[offset >> 1];
  uint8_t next = (offset & 1u)
                     ? (uint8_t)((*packed & 0x0fu) | ((value & 0x0fu) << 4))
                     : (uint8_t)((*packed & 0xf0u) | (value & 0x0fu));
  if (*packed != next) {
    *packed = next;
    s_card_dirty = true;
  }
}

uint8_t hp48_port1_read_nibble(size_t offset) {
  return read_card_nibble(HP48_CARD_PORT1, offset);
}

void hp48_port1_write_nibble(size_t offset, uint8_t value) {
  write_card_nibble(HP48_CARD_PORT1, offset, value);
}

uint8_t hp48_port2_read_nibble(size_t offset) {
  return read_card_nibble(HP48_CARD_PORT2, offset);
}

void hp48_port2_write_nibble(size_t offset, uint8_t value) {
  write_card_nibble(HP48_CARD_PORT2, offset, value);
}

int serial_init(void) { return 0; }
void serial_baud(int baud) { (void)baud; }
void transmit_char(void) { saturn.tcs &= (word_4)~0x01; }
void receive_char(void) {}
