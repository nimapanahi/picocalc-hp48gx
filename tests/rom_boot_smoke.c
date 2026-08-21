#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "core_runtime.h"
#include "device.h"
#include "timer.h"

extern int enter_debugger;
extern unsigned long instructions;

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s ROM.unpacked.bin\n", argv[0]);
    return 2;
  }

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
  set_accesstime();
  for (int block = 0; block < 2500 && !enter_debugger; ++block) {
    hp48_core_run(4096);
  }

  printf("instructions=%lu pc=%05x display=%s contrast=%u halted=%d\n",
         instructions, (unsigned)saturn.PC,
         display.on ? "on" : "off", (unsigned)display.contrast,
         enter_debugger);
  free(rom);
  return (enter_debugger || instructions < 1000000ul) ? 1 : 0;
}
