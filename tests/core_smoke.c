#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "core_runtime.h"
#include "device.h"
#include "hp48.h"
#include "hp48_emu.h"
#include "mmu.h"
#include "rpl_object.h"
#include "timer.h"

extern int test_sound_level;
extern int test_sound_calls;
extern int test_display_present_calls;
extern int test_display_last_ink_bits;
extern uint64_t test_platform_time_advance_us;
extern unsigned long instructions;
extern void write_dev_mem(long addr, int val);
extern int read_dev_mem(long addr);

static unsigned read_display_line_counter(void) {
  unsigned low = (unsigned)read_dev_mem(0x128);
  unsigned high = (unsigned)read_dev_mem(0x129);
  return low | ((high & 0x3u) << 4);
}

static void import_hp_binary(const char *path) {
  FILE *file = fopen(path, "rb");
  assert(file);
  assert(fseek(file, 0, SEEK_END) == 0);
  long file_size = ftell(file);
  assert(file_size > 8);
  assert(fseek(file, 0, SEEK_SET) == 0);

  uint8_t *contents = malloc((size_t)file_size);
  assert(contents);
  assert(fread(contents, 1, (size_t)file_size, file) == (size_t)file_size);
  assert(fgetc(file) == EOF);
  assert(fclose(file) == 0);
  assert(memcmp(contents, "HPHP48-", 7) == 0);

  size_t payload_size = (size_t)file_size - 8u;
  unsigned depth_before = hp48_rpl_stack_depth();
  hp48_rpl_import_t import;
  assert(hp48_rpl_import_begin(&import, payload_size) == HP48_RPL_OK);
  for (size_t offset = 0; offset < payload_size;) {
    size_t chunk = payload_size - offset;
    if (chunk > 256u) chunk = 256u;
    assert(hp48_rpl_import_write(&import, contents + 8u + offset, chunk) ==
           HP48_RPL_OK);
    offset += chunk;
  }
  assert(hp48_rpl_import_commit(&import) == HP48_RPL_OK);
  assert(hp48_rpl_stack_depth() == depth_before + 1u);

  hp48_rpl_export_t export;
  assert(hp48_rpl_export_begin(&export) == HP48_RPL_OK);
  assert(export.packed_bytes == payload_size);
  free(contents);
}

int main(int argc, char **argv) {
  assert(argc <= 2);
  static uint8_t rom[0x100000];
  assert(!hp48_core_init(rom, sizeof(rom) - 1));
  assert(hp48_core_init(rom, sizeof(rom)));
  assert(hp48_ram_size() == 0x40000);
  assert(!hp48_core_lcd_on());
  assert(!hp48_core_is_sleeping());
  hp48_key_set(0x44, true);
  assert(saturn.keybuf.rows[4] & (1 << 4));
  hp48_key_set(0x44, false);
  assert(!(saturn.keybuf.rows[4] & (1 << 4)));
  saturn.OUT[2] = 0x08;
  check_out_register();
  assert(test_sound_calls == 1 && test_sound_level == 1);
  saturn.OUT[2] = 0x00;
  check_out_register();
  assert(test_sound_calls == 2 && test_sound_level == 0);

  /* DA19 shares the high LINECOUNT nibble and controls whether a GX exposes
   * the upper half of its ROM. Clearing it mirrors the lower ROM half. */
  rom[0x00000] = 0x03;
  rom[0x80000] = 0x0c;
  write_dev_mem(0x129, 0x03);
  assert(read_nibble(0x80000) == 0x03);
  write_dev_mem(0x129, 0x0b);
  assert(read_nibble(0x80000) == 0x0c);
  rom[0x00000] = 0;
  rom[0x80000] = 0;

  /* The physical GX LCD exposes a coherent six-bit 4096 Hz down-counter.
   * WarioHP polls it to synchronize its grayscale interrupt handler; the
   * inherited x48 read-count stub advanced at CPU speed instead. */
  write_dev_mem(0x128, 0x0f);
  write_dev_mem(0x129, 0x03);
  write_dev_mem(0x100, 0x08);
  unsigned line_before = read_display_line_counter();
  test_platform_time_advance_us += 1000;
  unsigned line_after = read_display_line_counter();
  unsigned line_delta = (line_before - line_after) & 0x3fu;
  assert(line_delta >= 3u && line_delta <= 5u);
  write_dev_mem(0x100, 0x00);
  unsigned line_frozen = read_display_line_counter();
  unsigned line_high = read_dev_mem(0x129);
  assert(read_dev_mem(0x12a) == line_high);
  assert(read_dev_mem(0x12b) == line_high);
  assert(read_dev_mem(0x12c) == line_high);
  assert(read_dev_mem(0x12d) == line_high);
  unsigned long unknown_before = saturn.unknown;
  write_dev_mem(0x12a, 0);
  assert(saturn.unknown == unknown_before);
  test_platform_time_advance_us += 2000;
  assert(read_display_line_counter() == line_frozen);

  /* A native program may load a short TIMER2 frame deadline while the ROM's
   * ACCESSTIME still describes the long-running calculator clock. A
   * wall-clock reconciliation must never restore that older, larger value. */
  saturn.timer2 = 0x2000;
  saturn.t2_tick = 16;
  set_accesstime();
  saturn.timer2 = 0x0800;
  t1_t2_ticks timer_ticks = get_t1_t2();
  assert(timer_ticks.t2_ticks == 0x0800u);
  assert(saturn.t2_tick == 17);
  saturn.t2_ctrl = 0x01;
  hp48_core_run(9000);
  assert((uint32_t)saturn.timer2 <= 0x0800u);

  hp48_port2_attach_blank();
  assert(hp48_port2_attached());
  assert((saturn.card_status & 0x05) == 0x05);
  assert(hp48_port2_packed_data() != NULL);
  hp48_port2_packed_data()[0] = 0xa5;
  hp48_port2_mark_clean();
  assert(hp48_port2_read_nibble(0) == 0x05);
  assert(hp48_port2_read_nibble(1) == 0x0a);
  hp48_port2_write_nibble(0, 0x05);
  assert(!hp48_port2_dirty());
  hp48_port2_write_nibble(1, 0x03);
  assert(hp48_port2_packed_data()[0] == 0x35);
  assert(hp48_port2_dirty());

  saturn.mem_cntl[MCTL_PORT2_GX].config[0] = 0xb0000;
  saturn.bank_switch = 0;
  write_nibble(0xb0010, 0x0c);
  assert(read_nibble(0xb0010) == 0x0c);
  assert(hp48_port2_read_nibble(0x10) == 0x0c);

  hp48_core_run(32);
  hp48_core_reset();
  assert(saturn.PC == 0);
  assert(!hp48_core_is_sleeping());
  assert(hp48_port2_attached());
  assert(hp48_port2_read_nibble(0x10) == 0x0c);

  hp48_port2_detach();
  hp48_port1_attach_blank();
  assert(hp48_port1_attached() && !hp48_port2_attached());
  assert((saturn.card_status & 0x0a) == 0x0a);
  saturn.mem_cntl[MCTL_PORT1_GX].config[0] = 0xa0000;
  write_nibble(0xa0010, 0x09);
  assert(read_nibble(0xa0010) == 0x09);
  assert(hp48_port1_read_nibble(0x10) == 0x09);
  hp48_core_reset();
  assert(hp48_port1_attached());
  assert(hp48_port1_read_nibble(0x10) == 0x09);

  /* Model the post-boot RPL memory pointers and round-trip the standard
   * "PICOCALC" string used by the SD-card hardware test. */
  saturn.mem_cntl[MCTL_SysRAM_GX].config[0] = 0x80000;
  saturn.mem_cntl[MCTL_SysRAM_GX].config[1] = 0xc0000;
  write_nibbles(0x806ee, 0x81000, 5); /* TEMPTOP */
  write_nibbles(0x806f3, 0x81100, 5); /* RSKTOP */
  write_nibbles(0x806f8, 0x86000, 5); /* DSKTOP */
  write_nibbles(0x806fd, 0x86005, 5); /* EDITLINE: empty stack */
  write_nibbles(0x807ed, 0x00fcc, 5); /* AVMEM */
  assert(!hp48_rpl_port1_is_merged());
  write_nibbles(0x806f8, 0xd6000, 5);
  assert(hp48_rpl_port1_is_merged());
  write_nibbles(0x806f8, 0x86000, 5);

  /* A native program's direct active-framebuffer write must produce a full
   * queued snapshot. Dev17's empty nibble callbacks silently lost this path. */
  display.on = 1;
  display.disp_start = 0x90000;
  display.offset = 0;
  display.lines = 63;
  display.nibs_per_line = 34;
  display.disp_end = display.disp_start + 64 * display.nibs_per_line;
  device.display_touched = 0;
  int display_presents_before = test_display_present_calls;
  write_nibble(display.disp_start, 1);
  assert(test_display_present_calls == display_presents_before);
  hp48_core_service_display();
  assert(test_display_present_calls == display_presents_before);
  test_platform_time_advance_us += 3000;
  hp48_core_service_display();
  assert(test_display_present_calls == display_presents_before + 1);
  assert(test_display_last_ink_bits == 1);

  /* A game that continuously rewrites display RAM never reaches the quiet
   * interval. It must still produce a snapshot on the 40 ms wall-time cap. */
  display_presents_before = test_display_present_calls;
  write_nibble(display.disp_start + 1, 1);
  hp48_core_service_display();
  for (int millisecond = 0; millisecond < 50; ++millisecond) {
    ++instructions;
    write_nibble(display.disp_start + 1, millisecond & 1);
    test_platform_time_advance_us += 1000;
    hp48_core_service_display();
  }
  assert(test_display_present_calls > display_presents_before);

  uint8_t string_object[] = {
      0x2c, 0x2a, 0x50, 0x01, 0x00, 0x50, 0x49,
      0x43, 0x4f, 0x43, 0x41, 0x4c, 0x43,
  };
  hp48_rpl_import_t import;
  assert(hp48_rpl_import_begin(&import, sizeof(string_object)) == HP48_RPL_OK);
  assert(hp48_rpl_import_write(&import, string_object, 4) == HP48_RPL_OK);
  assert(hp48_rpl_import_write(&import, string_object + 4,
                               sizeof(string_object) - 4) == HP48_RPL_OK);
  assert(hp48_rpl_import_commit(&import) == HP48_RPL_OK);
  assert(hp48_rpl_stack_depth() == 1);
  assert(!hp48_rpl_stack_level1_is_directory());

  hp48_rpl_export_t export;
  uint8_t roundtrip[sizeof(string_object)] = {0};
  assert(hp48_rpl_export_begin(&export) == HP48_RPL_OK);
  assert(export.packed_bytes == sizeof(string_object));
  assert(hp48_rpl_export_read(&export, 0, roundtrip, 5) == HP48_RPL_OK);
  assert(hp48_rpl_export_read(&export, 5, roundtrip + 5,
                              sizeof(roundtrip) - 5) == HP48_RPL_OK);
  assert(memcmp(roundtrip, string_object, sizeof(string_object)) == 0);

  uint32_t temp_before = (uint32_t)read_nibbles(0x806ee, 5);
  uint32_t return_before = (uint32_t)read_nibbles(0x806f3, 5);
  hp48_rpl_import_t rejected;
  uint8_t too_short = 0;
  assert(hp48_rpl_import_begin(&rejected, 1) == HP48_RPL_OK);
  assert(hp48_rpl_import_write(&rejected, &too_short, 1) == HP48_RPL_OK);
  assert(hp48_rpl_import_commit(&rejected) == HP48_RPL_BAD_OBJECT);
  assert((uint32_t)read_nibbles(0x806ee, 5) == temp_before);
  assert((uint32_t)read_nibbles(0x806f3, 5) == return_before);

  /* An optional real HPHP48-x file gives the core validator a copyright-safe
   * integration probe: the test data stays outside the source tree. */
  if (argc == 2) import_hp_binary(argv[1]);

  hp48_port2_detach();
  assert(!hp48_port2_attached());
  assert(!hp48_port1_attached());
  assert((saturn.card_status & 0x05) == 0);
  return 0;
}
