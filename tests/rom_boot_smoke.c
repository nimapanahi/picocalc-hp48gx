#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_runtime.h"
#include "device.h"
#include "hp48_emu.h"
#include "rpl_object.h"
#include "timer.h"

extern int enter_debugger;
extern int test_sound_calls;
extern int test_sound_short_runs;
extern int test_sound_short_samples;
extern int test_sound_transitions;
extern int test_display_present_calls;
extern int test_display_bitmap_changes;
extern int test_display_blank_changes;
extern int test_display_nonblank_changes;
extern int test_display_last_ink_bits;
extern int test_display_grayscale_hint_calls;
extern unsigned long instructions;
extern uint64_t test_platform_time_advance_us;
extern _Bool test_platform_time_instruction_driven;
extern void test_set_instruction_time_rate(unsigned us_per_instruction);
extern void (*test_getevent_hook)(void);
extern void test_capture_marker(const char *label);

static void run_realtime_us(uint64_t duration_us,
                            unsigned us_per_instruction) {
  /* Keep execution blocks below one 4096 Hz LCD scanline. Host platform time
   * is derived from the instruction count, making native line-counter waits
   * deterministic instead of dependent on the workstation's load. */
  const unsigned quantum = 64;
  test_set_instruction_time_rate(us_per_instruction);
  while (duration_us != 0 && !enter_debugger) {
    unsigned budget = quantum;
    uint64_t quantum_us = (uint64_t)budget * us_per_instruction;
    if (quantum_us > duration_us) {
      budget = (unsigned)(duration_us / us_per_instruction);
      if (budget == 0) {
        test_platform_time_advance_us += duration_us;
        break;
      }
      quantum_us = (uint64_t)budget * us_per_instruction;
    }
    hp48_core_run(budget);
    duration_us -= quantum_us;
  }
}

static uint32_t menu_checksum(void) {
  uint32_t checksum = 2166136261u;
  for (uint32_t offset = 0; offset < 0x110u; ++offset) {
    checksum ^= (uint32_t)read_nibble(display.menu_start + offset);
    checksum *= 16777619u;
  }
  return checksum;
}

static uint32_t display_checksum(void) {
  uint32_t checksum = 2166136261u;
  for (long address = display.disp_start; address < display.disp_end;
       ++address) {
    checksum ^= (uint32_t)read_nibble(address);
    checksum *= 16777619u;
  }
  for (long address = display.menu_start; address < display.menu_end;
       ++address) {
    checksum ^= (uint32_t)read_nibble(address);
    checksum *= 16777619u;
  }
  return checksum;
}

static uint32_t main_display_checksum(void) {
  uint32_t checksum = 2166136261u;
  for (long address = display.disp_start; address < display.disp_end;
       ++address) {
    checksum ^= (uint32_t)read_nibble(address);
    checksum *= 16777619u;
  }
  return checksum;
}

static unsigned menu_nonzero_nibbles(void) {
  unsigned count = 0;
  for (uint32_t offset = 0; offset < 0x110u; ++offset) {
    if (read_nibble(display.menu_start + offset) != 0) ++count;
  }
  return count;
}

static uint32_t port2_checksum(void) {
  uint32_t checksum = 2166136261u;
  const uint8_t *card = hp48_port2_packed_data();
  if (!card) return 0;
  for (size_t offset = 0; offset < HP48_PORT2_PACKED_SIZE; ++offset) {
    checksum ^= card[offset];
    checksum *= 16777619u;
  }
  return checksum;
}

static void tap_key(uint16_t code, unsigned settle_instructions) {
  hp48_key_set(code, true);
  /* Model a deliberate physical press. Even 100k-instruction pulses can pass
   * between scans while INPUT or a game is transitioning between RPL loops. */
  hp48_core_run(300000);
  hp48_key_set(code, false);
  hp48_core_run(settle_instructions);
}

static void quick_tap_key(uint16_t code, unsigned settle_instructions) {
  hp48_key_set(code, true);
  /* A graphics command can finish before a normal synthetic hold is released.
   * Keep this tap to one ROM scheduling quantum so the key cannot repeat as a
   * form/menu action after the command has already completed. */
  hp48_core_run(4096);
  hp48_key_set(code, false);
  hp48_core_run(settle_instructions);
}

static bool tap_prefix(uint16_t code) {
  for (int attempt = 0; attempt < 3 && !hp48_prefix_latched(code); ++attempt) {
    hp48_key_set(code, true);
    hp48_core_run(100000);
    hp48_key_set(code, false);
    for (int block = 0; block < 100 && !hp48_prefix_latched(code); ++block)
      hp48_core_run(4096);
  }
  return hp48_prefix_latched(code);
}

static uint16_t softkey_code(unsigned softkey) {
  static const uint16_t codes[] = {0x14, 0x84, 0x83, 0x82, 0x81, 0x80};
  return softkey >= 1 && softkey <= 6 ? codes[softkey - 1] : 0xffff;
}

static uint16_t alpha_code(char letter) {
  static const uint16_t codes[] = {
      0x14, 0x84, 0x83, 0x82, 0x81, 0x80, 0x24, 0x74, 0x73,
      0x72, 0x71, 0x70, 0x04, 0x64, 0x63, 0x62, 0x61, 0x60,
      0x34, 0x54, 0x53, 0x52, 0x51, 0x50, 0x43, 0x42,
  };
  return letter >= 'A' && letter <= 'Z' ? codes[letter - 'A'] : 0xffff;
}

static bool type_alpha_word(const char *word) {
  for (; *word; ++word) {
    uint16_t code = alpha_code(*word);
    if (code == 0xffff || !tap_prefix(0x35)) return false;
    tap_key(code, 250000);
  }
  return true;
}

static bool type_command(const char *command, unsigned settle_instructions) {
  if (!type_alpha_word(command)) return false;
  tap_key(0x44, settle_instructions);
  return true;
}

static uint16_t digit_code(char digit) {
  static const uint16_t codes[] = {
      0x03, 0x13, 0x12, 0x11, 0x23,
      0x22, 0x21, 0x33, 0x32, 0x31,
  };
  return digit >= '0' && digit <= '9' ? codes[digit - '0'] : 0xffff;
}

static bool type_algebraic(const char *expression) {
  tap_key(0x04, 250000); /* Opening quote enters algebraic entry mode. */
  for (; *expression; ++expression) {
    uint16_t code = digit_code(*expression);
    if (code != 0xffff) {
      tap_key(code, 150000);
      continue;
    }
    code = alpha_code(*expression);
    if (code != 0xffff) {
      if (!tap_prefix(0x35)) return false;
      tap_key(code, 150000);
      continue;
    }
    switch (*expression) {
      case '+': code = 0x00; break;
      case '-': code = 0x10; break;
      case '*': code = 0x20; break;
      case '/': code = 0x30; break;
      case '^': code = 0x51; break;
      case '.': code = 0x02; break;
      default: return false;
    }
    tap_key(code, 150000);
  }
  tap_key(0x44, 1000000);
  return true;
}

static bool type_name(const char *name) {
  tap_key(0x04, 250000);
  if (!type_alpha_word(name)) return false;
  tap_key(0x44, 1000000);
  return true;
}

static bool clear_stack(void) {
  if (!tap_prefix(0x25)) return false;
  tap_key(0x41, 1000000); /* Left-shift DEL: CLEAR. */
  return true;
}

static void enter_matrix_2x2(unsigned a, unsigned b, unsigned c, unsigned d) {
  tap_prefix(0x15);
  tap_key(0x44, 1000000); /* Right-shift ENTER: MatrixWriter. */
  tap_key(digit_code((char)('0' + a)), 250000);
  tap_key(0x44, 250000);
  tap_key(digit_code((char)('0' + b)), 250000);
  tap_key(0x44, 250000);
  tap_key(0x61, 250000); /* Start the next row. */
  tap_key(digit_code((char)('0' + c)), 250000);
  tap_key(0x44, 250000);
  tap_key(digit_code((char)('0' + d)), 250000);
  tap_key(0x44, 250000);
  tap_key(0x44, 1500000); /* Accept the completed matrix. */
}

static hp48_rpl_status_t import_hp_binary(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) return HP48_RPL_BAD_OBJECT;
  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return HP48_RPL_BAD_OBJECT;
  }
  long file_size = ftell(file);
  if (file_size <= 8 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return HP48_RPL_BAD_OBJECT;
  }
  uint8_t *contents = malloc((size_t)file_size);
  if (!contents) {
    fclose(file);
    return HP48_RPL_NO_MEMORY;
  }
  bool read_ok = fread(contents, 1, (size_t)file_size, file) ==
                     (size_t)file_size &&
                 fgetc(file) == EOF && fclose(file) == 0;
  if (!read_ok || memcmp(contents, "HPHP48-", 7) != 0) {
    free(contents);
    return HP48_RPL_BAD_OBJECT;
  }

  size_t payload_size = (size_t)file_size - 8u;
  hp48_rpl_import_t import;
  hp48_rpl_status_t status = hp48_rpl_import_begin(&import, payload_size);
  for (size_t offset = 0; status == HP48_RPL_OK && offset < payload_size;) {
    size_t chunk = payload_size - offset;
    if (chunk > 256u) chunk = 256u;
    status = hp48_rpl_import_write(&import, contents + 8u + offset, chunk);
    offset += chunk;
  }
  if (status == HP48_RPL_OK) status = hp48_rpl_import_commit(&import);
  else if (import.active) hp48_rpl_import_abort(&import);
  free(contents);
  return status;
}

static const char *s_idle_import_path;
static hp48_rpl_status_t s_idle_import_status;
static bool s_idle_import_done;
static unsigned s_idle_import_depth_before;
static unsigned s_idle_import_depth_after;
static uint32_t s_idle_import_screen_before;
static uint32_t s_idle_import_main_before;

static void idle_import_hook(void) {
  if (s_idle_import_done || !s_idle_import_path ||
      !hp48_core_is_sleeping())
    return;

  s_idle_import_depth_before = hp48_rpl_stack_depth();
  s_idle_import_screen_before = display_checksum();
  s_idle_import_main_before = main_display_checksum();
  s_idle_import_status = import_hp_binary(s_idle_import_path);
  s_idle_import_depth_after = hp48_rpl_stack_depth();
  s_idle_import_done = true;
  if (s_idle_import_status == HP48_RPL_OK) hp48_key_set(0x8000, true);
}

static void print_usage(const char *program) {
  fprintf(stderr,
          "usage: %s ROM.unpacked.bin [--port0|--port1|--port2] "
          "[--rpl-roundtrip] "
          "[--prefix-library] [--launch-directory HPHP48-file] "
          "[--launch-program HPHP48-file] [--launch-library HPHP48-file] "
          "[--directory-enter-count 1..8] [--directory-form-down 0..8] "
          "[--directory-start-softkey 1..6] "
          "[--library-setup 1..6] [--library-command 1..6] "
          "[--library-profile android] [--calculator-profile]\n",
          program);
}

int main(int argc, char **argv) {
  bool use_port0 = false;
  bool use_port2 = false;
  bool use_port1 = false;
  bool test_rpl = false;
  bool test_library = false;
  const char *launch_directory = NULL;
  const char *launch_program = NULL;
  const char *launch_library = NULL;
  unsigned directory_enter_count = 1;
  unsigned directory_form_down = 0;
  unsigned directory_start_softkey = 0;
  unsigned library_command = 1;
  bool library_profile_android = false;
  bool calculator_profile = false;
  unsigned library_setup[6];
  unsigned library_setup_count = 0;
  test_platform_time_instruction_driven = 1;
  if (argc < 2) {
    print_usage(argv[0]);
    return 2;
  }
  for (int i = 2; i < argc; ++i) {
    if (strcmp(argv[i], "--port0") == 0) use_port0 = true;
    else if (strcmp(argv[i], "--port1") == 0) use_port1 = true;
    else if (strcmp(argv[i], "--port2") == 0) use_port2 = true;
    else if (strcmp(argv[i], "--rpl-roundtrip") == 0) test_rpl = true;
    else if (strcmp(argv[i], "--prefix-library") == 0) test_library = true;
    else if (strcmp(argv[i], "--launch-directory") == 0 && i + 1 < argc)
      launch_directory = argv[++i];
    else if (strcmp(argv[i], "--directory-enter-count") == 0 &&
             i + 1 < argc)
      directory_enter_count = (unsigned)strtoul(argv[++i], NULL, 0);
    else if (strcmp(argv[i], "--directory-form-down") == 0 && i + 1 < argc)
      directory_form_down = (unsigned)strtoul(argv[++i], NULL, 0);
    else if (strcmp(argv[i], "--directory-start-softkey") == 0 &&
             i + 1 < argc)
      directory_start_softkey = (unsigned)strtoul(argv[++i], NULL, 0);
    else if (strcmp(argv[i], "--launch-program") == 0 && i + 1 < argc)
      launch_program = argv[++i];
    else if (strcmp(argv[i], "--launch-library") == 0 && i + 1 < argc)
      launch_library = argv[++i];
    else if (strcmp(argv[i], "--library-setup") == 0 && i + 1 < argc &&
             library_setup_count < 6)
      library_setup[library_setup_count++] = (unsigned)strtoul(argv[++i], NULL, 0);
    else if (strcmp(argv[i], "--library-command") == 0 && i + 1 < argc)
      library_command = (unsigned)strtoul(argv[++i], NULL, 0);
    else if (strcmp(argv[i], "--library-profile") == 0 && i + 1 < argc &&
             strcmp(argv[i + 1], "android") == 0) {
      library_profile_android = true;
      ++i;
    }
    else if (strcmp(argv[i], "--calculator-profile") == 0)
      calculator_profile = true;
    else {
      print_usage(argv[0]);
      return 2;
    }
  }
  if (softkey_code(library_command) == 0xffff) {
    fprintf(stderr, "--library-command must be between 1 and 6\n");
    return 2;
  }
  if (directory_enter_count < 1 || directory_enter_count > 8) {
    fprintf(stderr, "--directory-enter-count must be between 1 and 8\n");
    return 2;
  }
  if (directory_form_down > 8) {
    fprintf(stderr, "--directory-form-down must be between 0 and 8\n");
    return 2;
  }
  if (directory_start_softkey != 0 &&
      softkey_code(directory_start_softkey) == 0xffff) {
    fprintf(stderr, "--directory-start-softkey must be between 1 and 6\n");
    return 2;
  }
  for (unsigned i = 0; i < library_setup_count; ++i) {
    if (softkey_code(library_setup[i]) == 0xffff) {
      fprintf(stderr, "--library-setup must be between 1 and 6\n");
      return 2;
    }
  }
  if ((use_port0 ? 1 : 0) + (use_port1 ? 1 : 0) + (use_port2 ? 1 : 0) > 1) {
    fprintf(stderr, "select only one virtual card slot\n");
    return 2;
  }
  if (launch_library && !use_port0 && !use_port1) use_port2 = true;

  FILE *fp = fopen(argv[1], "rb");
  if (!fp) {
    perror(argv[1]);
    return 2;
  }
  uint8_t *rom = malloc(0x100000u);
  if (!rom || fread(rom, 1, 0x100000u, fp) != 0x100000u || fgetc(fp) != EOF) {
    fprintf(stderr, "expected exactly 1,048,576 unpacked ROM nibbles\n");
    fclose(fp);
    free(rom);
    return 2;
  }
  fclose(fp);

  if (!hp48_core_init(rom, 0x100000u)) return 1;
  if (use_port1) hp48_port1_attach_blank();
  if (use_port2) hp48_port2_attach_blank();
  set_accesstime();
  for (int block = 0; block < 2500 && !enter_debugger; ++block) {
    hp48_core_run(4096);
  }

  printf("instructions=%lu pc=%05x display=%s contrast=%u card=%s "
         "audio_samples=%d audio_edges=%d short_runs=%d/%d halted=%d\n",
         instructions, (unsigned)saturn.PC,
         display.on ? "on" : "off", (unsigned)display.contrast,
         hp48_port1_attached() ? "port1" : hp48_port2_attached() ? "port2"
                                                                  : "off",
         test_sound_calls,
         test_sound_transitions, test_sound_short_runs,
         test_sound_short_samples, enter_debugger);

  if ((test_rpl || test_library || launch_directory || launch_program ||
       launch_library || calculator_profile) &&
      !enter_debugger) {
    /* A completely blank GX RAM pauses at "Try to Recover Memory?". Select
     * the rightmost NO softkey so the ROM finishes creating its RPL heaps. */
    if (read_nibbles(0x806ee, 5) == 0) {
      hp48_key_set(0x80, true);
      hp48_core_run(20000);
      hp48_key_set(0x80, false);
      hp48_core_run(2000000);
    }
    hp48_core_run(1000000);
  }

  if (test_library && !enter_debugger) {
    uint32_t menu_before = menu_checksum();
    unsigned nonzero_before = menu_nonzero_nibbles();

    /* The ROM recognizes the teal prefix as a tap completed before the
     * action key, never as a simultaneous matrix chord. */
    if (!tap_prefix(0x15) || (saturn.annunc & ANN_RIGHT) != ANN_RIGHT) {
      fprintf(stderr, "Teal prefix did not latch in the HP ROM\n");
      free(rom);
      return 1;
    }
    tap_key(0x12, 400000);

    uint32_t menu_after = menu_checksum();
    unsigned nonzero_after = menu_nonzero_nibbles();
    if ((saturn.annunc & ANN_RIGHT) == ANN_RIGHT ||
        menu_after == menu_before) {
      fprintf(stderr, "Teal then 2 did not open the LIBRARY menu\n");
      free(rom);
      return 1;
    }
    printf("prefix-library-catalog=ok menu=%08x/%u->%08x/%u\n",
           menu_before, nonzero_before, menu_after, nonzero_after);

    /* Right-shift LIBRARY is only the catalog of attached libraries, so a
     * fresh calculator legitimately shows six blank softkeys. Left-shift 2
     * opens the management menu containing PORTS, and NXT exposes PINIT. */
    tap_key(0x8000, 400000);
    if (!tap_prefix(0x25) || (saturn.annunc & ANN_LEFT) != ANN_LEFT) {
      fprintf(stderr, "Purple prefix did not latch in the HP ROM\n");
      free(rom);
      return 1;
    }
    tap_key(0x12, 400000);
    uint32_t tools_menu = menu_checksum();
    unsigned tools_nonzero = menu_nonzero_nibbles();
    tap_key(0x70, 400000);
    uint32_t pinit_menu = menu_checksum();
    unsigned pinit_nonzero = menu_nonzero_nibbles();
    if ((saturn.annunc & ANN_LEFT) == ANN_LEFT || tools_nonzero == 0 ||
        pinit_nonzero == 0 || tools_menu == pinit_menu) {
      fprintf(stderr, "Purple then 2, NXT did not expose Port tools/PINIT\n");
      free(rom);
      return 1;
    }
    printf("prefix-library-tools=ok menu=%08x/%u->%08x/%u\n",
           tools_menu, tools_nonzero, pinit_menu, pinit_nonzero);
    printf("prefix-library-audio samples=%d edges=%d short=%d/%d\n",
           test_sound_calls, test_sound_transitions, test_sound_short_runs,
           test_sound_short_samples);
    if (use_port2) {
      uint32_t card_before = port2_checksum();
      hp48_port2_mark_clean();
      tap_key(0x84, 6000000); /* F2: PINIT on this menu page. */
      uint32_t card_after = port2_checksum();
      printf("pinit-card dirty=%d checksum=%08x->%08x pc=%05x\n",
             hp48_port2_dirty(), card_before, card_after,
             (unsigned)saturn.PC);
      printf("pinit-audio samples=%d edges=%d short=%d/%d mctl4=%05x/%05x "
             "bank=%u menu=%08x\n",
             test_sound_calls, test_sound_transitions, test_sound_short_runs,
             test_sound_short_samples,
             (unsigned)saturn.mem_cntl[MCTL_PORT2_GX].config[0],
             (unsigned)saturn.mem_cntl[MCTL_PORT2_GX].config[1],
             (unsigned)saturn.bank_switch, menu_checksum());
      /* A zero-filled RAM card is already the ROM's canonical empty state.
       * PINIT still probes/writes it (setting dirty), but intentionally leaves
       * no stack result and may have the same final checksum. */
      if (!hp48_port2_dirty()) {
        fprintf(stderr, "PINIT did not touch the attached Port 2 card\n");
        free(rom);
        return 1;
      }
    }
  }

  if (test_rpl && !enter_debugger) {
    static const uint8_t object[] = {
        0x2c, 0x2a, 0x50, 0x01, 0x00, 0x50, 0x49,
        0x43, 0x4f, 0x43, 0x41, 0x4c, 0x43,
    };
    unsigned depth_before = hp48_rpl_stack_depth();
    hp48_rpl_import_t import;
    hp48_rpl_status_t status = hp48_rpl_import_begin(&import, sizeof(object));
    if (status == HP48_RPL_OK)
      status = hp48_rpl_import_write(&import, object, sizeof(object));
    if (status == HP48_RPL_OK) status = hp48_rpl_import_commit(&import);
    if (status != HP48_RPL_OK) {
      fprintf(stderr, "RPL import failed: %s\n", hp48_rpl_status_label(status));
      fprintf(stderr,
              "RPL pointers TEMPTOP=%05lx RSKTOP=%05lx DSKTOP=%05lx "
              "EDITLINE=%05lx AVMEM=%05lx\n",
              read_nibbles(0x806ee, 5), read_nibbles(0x806f3, 5),
              read_nibbles(0x806f8, 5), read_nibbles(0x806fd, 5),
              read_nibbles(0x807ed, 5));
      free(rom);
      return 1;
    }
    uint32_t temp_before_orphan = (uint32_t)read_nibbles(0x806ee, 5);
    uint32_t return_before_orphan = (uint32_t)read_nibbles(0x806f3, 5);
    hp48_rpl_import_t orphan;
    status = hp48_rpl_import_begin(&orphan, sizeof(object));
    if (status == HP48_RPL_OK)
      status = hp48_rpl_import_write(&orphan, object, sizeof(object));
    if (status != HP48_RPL_OK) {
      fprintf(stderr, "Could not create RPL GC test orphan\n");
      free(rom);
      return 1;
    }
    /* Intentionally omit commit/abort: this models an unreachable temporary
     * object so the ROM collector has real space to recover. */
    orphan.active = false;
    uint32_t temp_after_orphan = (uint32_t)read_nibbles(0x806ee, 5);
    uint32_t return_after_orphan = (uint32_t)read_nibbles(0x806f3, 5);
    if (temp_after_orphan <= temp_before_orphan ||
        return_after_orphan <= return_before_orphan) {
      fprintf(stderr, "RPL GC test orphan did not consume memory\n");
      free(rom);
      return 1;
    }

    saturn_t before_gc = saturn;
    unsigned depth_after_import = hp48_rpl_stack_depth();
    if (!hp48_rpl_collect_garbage() ||
        memcmp(&before_gc, &saturn, sizeof(saturn)) != 0 ||
        hp48_rpl_stack_depth() != depth_after_import ||
        (uint32_t)read_nibbles(0x806ee, 5) >= temp_after_orphan ||
        (uint32_t)read_nibbles(0x806f3, 5) >= return_after_orphan) {
      fprintf(stderr, "RPL garbage-collection transaction failed\n");
      fprintf(stderr,
              "before orphan temp=%05x return=%05x; after GC temp=%05lx "
              "return=%05lx\n",
              temp_before_orphan, return_before_orphan,
              read_nibbles(0x806ee, 5), read_nibbles(0x806f3, 5));
      free(rom);
      return 1;
    }
    hp48_rpl_export_t export;
    uint8_t roundtrip[sizeof(object)] = {0};
    status = hp48_rpl_export_begin(&export);
    if (status == HP48_RPL_OK)
      status = hp48_rpl_export_read(&export, 0, roundtrip, sizeof(roundtrip));
    if (status != HP48_RPL_OK || export.packed_bytes != sizeof(object) ||
        memcmp(object, roundtrip, sizeof(object)) != 0 ||
        hp48_rpl_stack_depth() != depth_before + 1u) {
      fprintf(stderr, "RPL export/stack round-trip failed\n");
      free(rom);
      return 1;
    }
    printf("rpl-roundtrip=ok gc=ok depth=%u->%u\n", depth_before,
           hp48_rpl_stack_depth());
  }

  if (calculator_profile && !enter_debugger) {
    uint32_t home_before = display_checksum();
    unsigned depth_before = hp48_rpl_stack_depth();
    if (!type_algebraic("X^4-5*X^2+4")) {
      fprintf(stderr, "Could not enter the factor expression\n");
      free(rom);
      return 1;
    }
    uint32_t factor_input = display_checksum();
    test_capture_marker("cas_factor_input");
    if (!type_command("FACTOR", 5000000)) {
      fprintf(stderr, "Could not execute FACTOR\n");
      free(rom);
      return 1;
    }
    uint32_t factor_result = display_checksum();
    test_capture_marker("cas_factor_result");
    if (!clear_stack() || !type_algebraic("X^5-3*X^2+7*X") ||
        !type_name("X") || !tap_prefix(0x15)) {
      fprintf(stderr, "Could not enter the differentiation test\n");
      free(rom);
      return 1;
    }
    tap_key(0x34, 5000000); /* Right-shift SIN: DIFF. */
    uint32_t diff_result = display_checksum();
    test_capture_marker("cas_diff_result");
    if (!clear_stack() || !type_algebraic("3*X^2+2*X+1") ||
        !type_name("X") || !type_command("RISCH", 8000000)) {
      fprintf(stderr, "Could not execute the integration test\n");
      free(rom);
      return 1;
    }
    uint32_t integrate_result = display_checksum();
    test_capture_marker("cas_integrate_result");
    if (!clear_stack() || !type_algebraic("X^2-5*X+6") ||
        !type_name("X") || !type_command("QUAD", 8000000)) {
      fprintf(stderr, "Could not execute the quadratic solve test\n");
      free(rom);
      return 1;
    }
    uint32_t solve_result = display_checksum();
    test_capture_marker("cas_solve_result");
    if (!clear_stack()) {
      fprintf(stderr, "Could not clear the CAS results\n");
      free(rom);
      return 1;
    }
    enter_matrix_2x2(1, 2, 3, 4);
    tap_key(0x50, 5000000); /* 1/X: matrix inverse. */
    uint32_t inverse_result = display_checksum();
    test_capture_marker("matrix_inverse_result");
    if (!clear_stack()) {
      fprintf(stderr, "Could not clear the inverse result\n");
      free(rom);
      return 1;
    }
    enter_matrix_2x2(1, 2, 3, 4);
    enter_matrix_2x2(5, 6, 7, 8);
    tap_key(0x20, 5000000); /* Matrix multiplication. */
    uint32_t product_result = display_checksum();
    test_capture_marker("matrix_product_result");
    if (!clear_stack()) {
      fprintf(stderr, "Could not clear the matrix result\n");
      free(rom);
      return 1;
    }
    if (!type_command("RAD", 1000000) ||
        !type_command("TEACH", 12000000)) {
      fprintf(stderr, "Could not configure and open the HP examples\n");
      free(rom);
      return 1;
    }
    uint32_t taught = display_checksum();
    unsigned depth_after = hp48_rpl_stack_depth();
    tap_key(0x72, 1000000); /* VAR: EXAMPLES should be the first variable. */
    uint32_t variables = display_checksum();
    tap_key(0x14, 2000000);
    uint32_t examples = display_checksum();
    tap_key(0x83, 1500000); /* EQNS directory. */
    if (!tap_prefix(0x15)) {
      fprintf(stderr, "Could not open the equation plot test\n");
      free(rom);
      return 1;
    }
    tap_key(0x32, 2000000); /* Right-shift 8: PLOT application. */
    uint32_t equation_plot_form = display_checksum();
    test_capture_marker("multiple_plot_form");
    tap_key(0x84, 1500000); /* CHOOS equations from the EQNS directory. */
    uint32_t equation_chooser = display_checksum();
    test_capture_marker("multiple_plot_chooser");
    /* The chooser opens on PPAR, the plot-parameter list stored beside HP's
     * examples. Move to ONE so only algebraic functions enter the EQ list. */
    quick_tap_key(0x61, 400000);
    for (int equation = 0; equation < 4; ++equation) {
      quick_tap_key(0x83, 400000); /* CHK ONE, TWO, THREE, and FOUR. */
      if (equation != 3) quick_tap_key(0x61, 400000);
    }
    tap_key(0x80, 1500000); /* OK: place the checked list in EQ. */
    test_capture_marker("multiple_plot_selected");
    /* Use the ROM's current view and sequential mode. The picture retains all
     * four curves while the capture shows each computed graph being added. */
    uint32_t multiple_plot_ready = display_checksum();
    test_capture_marker("multiple_plot_ready");
    tap_key(0x81, 1000000); /* ERASE */
    int multiple_changes_before = test_display_bitmap_changes;
    quick_tap_key(0x80, 50000000); /* DRAW without a repeated CANCL scan. */
    uint32_t multiple_plot = display_checksum();
    int multiple_changes =
        test_display_bitmap_changes - multiple_changes_before;
    test_capture_marker("multiple_plot_result");
    /* Run the ordinary Function plot before the parametric-surface example.
     * PSUR intentionally leaves its specialized PPAR fields installed, which
     * must not leak into this independent multi-equation regression. */
    quick_tap_key(0x80, 2000000);   /* PICTURE CANCL to the input form. */
    quick_tap_key(0x8000, 2000000); /* CANCEL form to the EQNS directory. */
    if (!tap_prefix(0x25)) {
      fprintf(stderr, "Could not leave the EQNS directory\n");
      free(rom);
      return 1;
    }
    tap_key(0x04, 1500000); /* UP to EXAMPLES. */
    tap_key(0x84, 2000000); /* PLOTS directory in HP's EXAMPLES. */
    uint32_t plots = display_checksum();
    tap_key(0x70, 1000000);
    uint32_t plots_next = display_checksum();
    test_capture_marker("parametric_surface_start");
    int surface_changes_before = test_display_bitmap_changes;
    quick_tap_key(0x82, 30000000); /* PSUR parametric-surface example. */
    uint32_t surface = display_checksum();
    int surface_changes = test_display_bitmap_changes - surface_changes_before;
    test_capture_marker("parametric_surface_result");
    printf("calculator-cas-matrix=ok factor=%08x->%08x diff=%08x "
           "integrate=%08x solve=%08x inverse=%08x product=%08x\n",
           factor_input, factor_result, diff_result, integrate_result,
           solve_result, inverse_result, product_result);
    printf("calculator-teach=ok depth=%u->%u screen=%08x->%08x "
           "var=%08x examples=%08x plots=%08x/%08x surface=%08x/%d "
           "multi-form=%08x/%08x ready=%08x plot=%08x/%d "
           "pc=%05x halted=%d\n",
           depth_before, depth_after, home_before, taught, variables,
           examples, plots, plots_next, surface, surface_changes,
           equation_plot_form, equation_chooser, multiple_plot_ready,
           multiple_plot, multiple_changes,
           (unsigned)saturn.PC, enter_debugger);
    if (factor_input == factor_result || factor_result == diff_result ||
        diff_result == integrate_result || integrate_result == solve_result ||
        inverse_result == product_result || home_before == taught ||
        taught == variables || variables == examples ||
        examples == plots || plots_next == surface || surface_changes <= 0 ||
        multiple_plot_ready == multiple_plot || multiple_changes <= 400 ||
        enter_debugger) {
      fprintf(stderr, "TEACH did not install and open the HP examples\n");
      free(rom);
      return 1;
    }
  }

  if (launch_directory && !enter_debugger) {
    s_idle_import_path = launch_directory;
    s_idle_import_done = false;
    test_getevent_hook = idle_import_hook;
    for (int block = 0; block < 2500 && !s_idle_import_done &&
                        !enter_debugger; ++block)
      hp48_core_run(4096);
    test_getevent_hook = NULL;
    /* Match the platform's deliberate 75 ms ON hold. Releasing ON at the
     * instant SHUTDN returns can wake the core without letting the ROM scan
     * the key and redraw its stack. */
    hp48_core_run(300000);
    hp48_key_set(0x8000, false);
    hp48_core_run(2000000);

    if (!s_idle_import_done || s_idle_import_status != HP48_RPL_OK ||
        s_idle_import_depth_after != s_idle_import_depth_before + 1u ||
        hp48_rpl_stack_depth() != s_idle_import_depth_after ||
        !hp48_rpl_stack_level1_is_directory()) {
      fprintf(stderr, "Idle RPL import failed or vanished: %s depth=%u->%u->%u\n",
              hp48_rpl_status_label(s_idle_import_status),
              s_idle_import_depth_before, s_idle_import_depth_after,
              hp48_rpl_stack_depth());
      free(rom);
      return 1;
    }

    uint32_t stack_screen = display_checksum();
    uint32_t stack_main = main_display_checksum();
    /* A subsequent ordinary ROM key must not restore the pre-import stack.
     * This is the regression that made dev.14 claim success while L1 stayed
     * visibly empty on hardware. */
    tap_key(0x70, 600000);
    if (hp48_rpl_stack_depth() != s_idle_import_depth_after) {
      fprintf(stderr, "Imported object vanished after the first ROM key\n");
      free(rom);
      return 1;
    }

    uint32_t post_nxt_screen = display_checksum();
    tap_key(0x63, 1500000); /* A raw directory object is not executable. */
    if (hp48_rpl_stack_depth() != s_idle_import_depth_after) {
      fprintf(stderr, "EVAL unexpectedly consumed the directory object\n");
      free(rom);
      return 1;
    }

    /* Install it exactly as a physical HP 48 user must: put a global name
     * 'KAH' above the directory, STO it in HOME, open VAR, and select KAH.
     * The name is deliberately generic; the first directory command launches
     * Kala, Pac Man GX, and AstroNUT in their respective test packages. */
    tap_key(0x04, 300000); /* Quote/name entry. */
    if (!tap_prefix(0x35)) {
      fprintf(stderr, "Alpha did not latch for directory name KAH\n");
      free(rom);
      return 1;
    }
    tap_key(0x71, 300000); /* K */
    if (!tap_prefix(0x35)) {
      fprintf(stderr, "Alpha did not re-latch for directory name KAH\n");
      free(rom);
      return 1;
    }
    tap_key(0x14, 300000); /* A */
    if (!tap_prefix(0x35)) {
      fprintf(stderr, "Alpha did not re-latch for directory name KAH\n");
      free(rom);
      return 1;
    }
    tap_key(0x74, 300000); /* H */
    tap_key(0x44, 1000000); /* Finish 'KAH'. */
    unsigned named_depth = hp48_rpl_stack_depth();
    tap_key(0x64, 2000000); /* STO directory as KAH. */
    unsigned stored_depth = hp48_rpl_stack_depth();
    tap_key(0x72, 600000); /* VAR menu. */
    uint32_t var_screen = display_checksum();
    tap_key(0x14, 1500000); /* First softkey: enter KAH directory. */
    uint32_t directory_screen = display_checksum();
    tap_key(0x14, 8000000); /* First directory softkey: launch the game. */
    uint32_t prompt_screen = display_checksum();
    int sound_before = test_sound_transitions;
    uint32_t running_screen = prompt_screen;
    for (unsigned press = 0; press < directory_enter_count; ++press) {
      tap_key(0x44, 12000000); /* Advance title/setup forms with ENTER. */
      running_screen = display_checksum();
      if (press == 0) {
        for (unsigned down = 0; down < directory_form_down; ++down)
          tap_key(0x61, 1000000);
      }
    }
    if (directory_start_softkey != 0) {
      tap_key(softkey_code(directory_start_softkey), 12000000);
      running_screen = display_checksum();
    }
    if (running_screen == prompt_screen &&
        test_sound_transitions == sound_before) {
      tap_key(0x44, 12000000);
      running_screen = display_checksum();
    }

    /* Exercise ordinary HP numeric controls for 20 seconds. Pac Man uses
     * 4/6/8/2; other directory games safely ignore keys they do not bind.
     * Display progress, not sound, is the compatibility requirement because
     * the Pac Man GX directory itself is silent. */
    int long_run_changes_before = test_display_bitmap_changes;
    static const uint16_t directory_controls[] = {0x23, 0x21, 0x32, 0x12};
    for (int interval = 0; interval < 80 && !enter_debugger; ++interval) {
      uint16_t code = directory_controls[interval % 4];
      hp48_key_set(code, true);
      run_realtime_us(50000, 10);
      hp48_key_set(code, false);
      run_realtime_us(200000, 10);
    }
    int long_run_changes =
        test_display_bitmap_changes - long_run_changes_before;

    printf("launch-directory=ok idle-import depth=%u->%u name=%u stored=%u "
           "screen=%08x->%08x eval=%08x var=%08x dir=%08x prompt=%08x "
           "run=%08x main=%08x->%08x audio=%d->%d realtime20s=%d "
           "pc=%05x halted=%d\n",
           s_idle_import_depth_before, s_idle_import_depth_after,
           named_depth, stored_depth, s_idle_import_screen_before,
           stack_screen, post_nxt_screen, var_screen, directory_screen,
           prompt_screen, running_screen, s_idle_import_main_before,
           stack_main, sound_before, test_sound_transitions,
           long_run_changes, (unsigned)saturn.PC, enter_debugger);
    if (enter_debugger || s_idle_import_main_before == stack_main ||
        named_depth != s_idle_import_depth_after + 1u || stored_depth != 0 ||
        var_screen == directory_screen || directory_screen == prompt_screen ||
        prompt_screen == running_screen || long_run_changes <= 0) {
      fprintf(stderr, "External directory did not install, open, and run\n");
      free(rom);
      return 1;
    }
  }

  if (launch_program && !enter_debugger) {
    s_idle_import_path = launch_program;
    s_idle_import_done = false;
    test_getevent_hook = idle_import_hook;
    for (int block = 0; block < 2500 && !s_idle_import_done &&
                        !enter_debugger; ++block)
      hp48_core_run(4096);
    test_getevent_hook = NULL;
    hp48_core_run(300000);
    hp48_key_set(0x8000, false);
    hp48_core_run(2000000);

    if (!s_idle_import_done || s_idle_import_status != HP48_RPL_OK ||
        s_idle_import_depth_after != s_idle_import_depth_before + 1u ||
        hp48_rpl_stack_depth() != s_idle_import_depth_after ||
        hp48_rpl_stack_level1_is_directory()) {
      fprintf(stderr, "Program import failed or vanished: %s depth=%u->%u->%u\n",
              hp48_rpl_status_label(s_idle_import_status),
              s_idle_import_depth_before, s_idle_import_depth_after,
              hp48_rpl_stack_depth());
      free(rom);
      return 1;
    }

    uint32_t stack_screen = display_checksum();
    int sound_before = test_sound_transitions;
    int presents_before = test_display_present_calls;
    int changes_before = test_display_bitmap_changes;
    tap_key(0x63, 6000000); /* EVAL standalone program/code object. */
    uint32_t title_screen = display_checksum();
    tap_key(0x44, 12000000); /* ENTER starts title-driven games. */
    uint32_t started_screen = display_checksum();
    tap_key(0x32, 4000000); /* Phoenix default 8: move right. */
    uint32_t moved_screen = display_checksum();
    tap_key(0x21, 4000000); /* Phoenix default 6: fire phaser. */
    tap_key(0x31, 4000000); /* Phoenix default 9: fire torpedo. */
    tap_key(0x30, 4000000); /* Phoenix default /: shields. */
    uint32_t action_screen = display_checksum();

    /* Instruction-only execution barely advances the HP wall-clock timers.
     * Exercise the game beyond the physical 7-10 second failure window at
     * 100k IPS, including repeated movement and weapon matrix transitions. */
    int long_run_changes_before = test_display_bitmap_changes;
    int long_run_sound_before = test_sound_transitions;
    static const uint16_t controls[] = {
        0x32, 0x21, 0x31, 0x30, /* Phoenix: move, fire, torpedo, shield. */
        0x44, 0x24, 0x74, 0x71, 0x64, /* ENTER and Wario G/H/K/N. */
    };
    for (int interval = 0; interval < 72 && !enter_debugger; ++interval) {
      uint16_t code = controls[interval % 9];
      hp48_key_set(code, true);
      run_realtime_us(75000, 10);
      hp48_key_set(code, false);
      run_realtime_us(175000, 10);
    }
    int long_run_changes =
        test_display_bitmap_changes - long_run_changes_before;
    int long_run_sound = test_sound_transitions - long_run_sound_before;

    printf("launch-program=ok depth=%u->%u stack=%08x title=%08x "
           "start=%08x move=%08x action=%08x audio=%d->%d "
           "lcd=%d/%d->%d/%d blank=%d drawn=%d ink=%d "
           "realtime15s=%d/%d pc=%05x halted=%d\n",
           s_idle_import_depth_before, s_idle_import_depth_after,
           stack_screen, title_screen, started_screen, moved_screen,
           action_screen, sound_before, test_sound_transitions,
           presents_before, changes_before, test_display_present_calls,
           test_display_bitmap_changes, test_display_blank_changes,
           test_display_nonblank_changes, test_display_last_ink_bits,
           long_run_changes, long_run_sound, (unsigned)saturn.PC,
           enter_debugger);
    if (enter_debugger || stack_screen == title_screen ||
        title_screen == started_screen ||
        (started_screen == moved_screen && moved_screen == action_screen) ||
        test_sound_transitions <= sound_before || long_run_changes <= 0) {
      fprintf(stderr, "Standalone game did not render, react, and produce sound\n");
      free(rom);
      return 1;
    }
  }

  if (launch_library && !enter_debugger) {
    s_idle_import_path = launch_library;
    s_idle_import_done = false;
    test_getevent_hook = idle_import_hook;
    for (int block = 0; block < 2500 && !s_idle_import_done &&
                        !enter_debugger; ++block)
      hp48_core_run(4096);
    test_getevent_hook = NULL;
    hp48_core_run(300000);
    hp48_key_set(0x8000, false);
    hp48_core_run(2000000);

    if (!s_idle_import_done || s_idle_import_status != HP48_RPL_OK ||
        hp48_rpl_stack_depth() != s_idle_import_depth_before + 1u) {
      fprintf(stderr, "Library import failed: %s depth=%u->%u\n",
              hp48_rpl_status_label(s_idle_import_status),
              s_idle_import_depth_before, hp48_rpl_stack_depth());
      free(rom);
      return 1;
    }

    /* HP library installation uses the destination port number above the
     * library object, followed by STO. A warm OFF/ON attaches its config
     * object before the right-shift LIBRARY catalog is opened. Port 0 is
     * important for libraries whose authors require execution from main RAM. */
    tap_key(use_port0 ? 0x03 : use_port1 ? 0x13 : 0x12, 300000);
    tap_key(0x44, 1000000); /* ENTER */
    unsigned install_depth = hp48_rpl_stack_depth();
    hp48_port2_mark_clean();
    tap_key(0x64, 8000000); /* STO */
    unsigned stored_depth = hp48_rpl_stack_depth();
    bool stored_dirty = hp48_port2_dirty();
    if (!tap_prefix(0x15)) {
      fprintf(stderr, "Teal prefix did not latch for library warm start\n");
      free(rom);
      return 1;
    }
    tap_key(0x8000, 3000000); /* OFF, then host wake models ON. */
    hp48_core_run(3000000);

    uint32_t before_catalog = display_checksum();
    if (!tap_prefix(0x15)) {
      fprintf(stderr, "Teal prefix did not latch for LIBRARY\n");
      free(rom);
      return 1;
    }
    tap_key(0x12, 3000000);
    uint32_t catalog = display_checksum();
    unsigned catalog_names = menu_nonzero_nibbles();
    tap_key(0x14, 3000000); /* First attached library. */
    uint32_t library_menu = display_checksum();
    unsigned command_names = menu_nonzero_nibbles();
    test_display_grayscale_hint_calls = 0;
    for (unsigned i = 0; i < library_setup_count; ++i)
      tap_key(softkey_code(library_setup[i]), 3000000);
    unsigned setup_depth = hp48_rpl_stack_depth();
    tap_key(softkey_code(library_command), 12000000);
    uint32_t launched = display_checksum();
    unsigned long launched_instructions = instructions;
    int launched_sound = test_sound_transitions;
    int launched_changes = test_display_bitmap_changes;

    /* Presentation screens scan ENTER between effects. Long presses avoid
     * phase-locking a short synthetic pulse inside a delay loop. */
    int start_attempts = library_profile_android ? 1 : 5;
    for (int attempt = 0; attempt < start_attempts && !enter_debugger;
         ++attempt) {
      hp48_key_set(0x44, true);
      run_realtime_us(500000, 2);
      hp48_key_set(0x44, false);
      run_realtime_us(250000, 2);
    }
    static const uint16_t library_controls[] = {
        0x44, 0x24, 0x74, 0x71, 0x64, /* ENTER, Wario G/H/K/N. */
    };
    static const uint16_t android_controls[] = {
        0x62, 0x60, 0x71, 0x61, /* Android P/R/K/Q: left/right/up/down. */
    };
    for (int interval = 0; interval < 120 && !enter_debugger; ++interval) {
      uint16_t code = library_profile_android
                          ? android_controls[interval % 4]
                          : library_controls[interval % 5];
      bool dig = library_profile_android && interval % 5 == 0;
      if (dig) hp48_key_set(0x44, true);
      hp48_key_set(code, true);
      run_realtime_us(75000, 2);
      hp48_key_set(code, false);
      if (dig) hp48_key_set(0x44, false);
      run_realtime_us(175000, 2);
    }

    uint32_t presentation = 0;
    uint32_t exited = 0;
    if (library_profile_android && !enter_debugger) {
      tap_key(0x8000, 3000000); /* Android ON: return to presentation. */
      presentation = display_checksum();
      tap_key(0x40, 6000000); /* Android DROP: leave presentation. */
      exited = display_checksum();
    }

    printf("launch-library-invoked install=%u stored=%u dirty=%d command=%u "
           "setup=%u/%u "
           "screen=%08x->%08x/%u->%08x/%u->%08x "
           "run33s instr=%lu->%lu lcd=%d->%d audio=%d->%d "
           "gray-hints=%d pc=%05x t1=%x/%x t2=%08x/%x rstkp=%d "
           "int=%d/%d android-exit=%08x->%08x halted=%d\n",
           install_depth, stored_depth, stored_dirty, library_command,
           library_setup_count, setup_depth, before_catalog, catalog,
           catalog_names, library_menu, command_names, launched,
           launched_instructions, instructions, launched_changes,
           test_display_bitmap_changes, launched_sound, test_sound_transitions,
           test_display_grayscale_hint_calls, (unsigned)saturn.PC,
           saturn.timer1 & 0x0f, saturn.t1_ctrl & 0x0f,
           (unsigned)saturn.timer2, saturn.t2_ctrl & 0x0f, saturn.rstkp,
           saturn.intenable, saturn.int_pending, presentation, exited,
           enter_debugger);
    if (install_depth < 2 || stored_depth != 0 ||
        (!use_port0 && !stored_dirty) ||
        before_catalog == catalog || catalog_names == 0 ||
        catalog == library_menu || command_names == 0 || enter_debugger ||
        saturn.PC == 0x15970u || saturn.PC == 0x15972u ||
        saturn.PC == 0x15974u || saturn.PC == 0x159eeu ||
        (library_profile_android && presentation == exited)) {
      fprintf(stderr, "Library did not install, attach, and invoke command\n");
      free(rom);
      return 1;
    }
  }
  free(rom);
  return (enter_debugger || instructions < 1000000ul) ? 1 : 0;
}
