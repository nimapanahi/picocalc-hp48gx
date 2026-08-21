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

short rom_is_new = 1;
long ram_size = RAM_SIZE_GX;
long port1_size = 0;
long port1_mask = 0;
short port1_is_ram = 0;
long port2_size = 0;
long port2_mask = 0;
short port2_is_ram = 0;

static uint8_t s_ram[RAM_SIZE_GX];

static void start_runtime_timers(void) {
  reset_timer(IDLE_TIMER);
  restart_timer(T1_TIMER);
  restart_timer(RUN_TIMER);
  sched_timer1 = saturn.t1_tick;
  sched_timer2 = saturn.t2_tick;
  set_t1 = saturn.timer1;
}

void init_saturn(void) {
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
  init_saturn();
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
  init_saturn();
  memset(s_ram, 0, sizeof(s_ram));
  saturn.rom = rom;
  saturn.ram = s_ram;
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

void hp48_core_run(unsigned instruction_budget) {
  while (instruction_budget-- && !enter_debugger) {
    step_instruction();
    if (schedule_event-- <= 0) schedule();
  }
}

bool hp48_core_lcd_on(void) { return display.on != 0; }
uint8_t *hp48_ram_data(void) { return s_ram; }
size_t hp48_ram_size(void) { return sizeof(s_ram); }

int serial_init(void) { return 0; }
void serial_baud(int baud) { (void)baud; }
void transmit_char(void) { saturn.tcs &= (word_4)~0x01; }
void receive_char(void) {}
