# Third-party code and hardware references

## x48 0.6.4

The Saturn CPU, register, device, and memory implementation in `src/core` is
adapted from x48 0.6.4 by Eddie C. Dost and later contributors:

- Project: <https://sourceforge.net/projects/x48.berlios/>
- License: GNU General Public License, version 2 or later
- Imported/adapted files: `actions.c`, `device.c`, `emulate.c`, `memory.c`,
  `register.c`, `device.h`, `global.h`, `hp48.h`, `mmu.h`, and `timer.h`

The X11 UI, POSIX persistence, debugger UI, and serial backends were replaced
with small RP2350/PicoCalc-specific implementations. See `COPYING` for the GPL
text.

## Emu48 RPL object routines

The bounded HP object-size parser and stack import/export transaction in
`src/core/rpl_object.c` are adapted from Sebastien Carlier's GPL Emu48 RPL and
file-transfer routines:

- Project: <https://github.com/dgis/emu48android>
- Relevant sources: `app/src/main/cpp/core/rpl.c`, `files.c`, and `Emu48.h`
- License: GNU General Public License, version 2 or later

The PicoCalc adaptation replaces Emu48's full-file allocation with bounded,
rollback-safe streaming to fit RP2350 memory and uses the standard
`HPHP48-W` binary-object header.

## ClockworkPi PicoCalc

LCD pins, keyboard I2C pins/address/protocol, key codes, battery register
`0x0B`, PMU power-off register `0x0E`, and the ILI9488 setup sequence follow
ClockworkPi's public PicoCalc examples:

- <https://github.com/clockworkpi/PicoCalc>
- <https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/reg.h>
- <https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/picocalc_keyboard.ino>

No ClockworkPi source file is copied wholesale into this project.

## pico-fatfs-sd and FatFs

SD-card access uses the pinned `third_party/pico-fatfs-sd` submodule, a trimmed
RP2040/RP2350 port of the no-OS FatFs SD driver:

- Project: <https://github.com/inindev/pico-fatfs-sd>
- Driver license: Apache License 2.0
- Included FatFs version: R0.16, under the FatFs license included upstream

The dependency is kept as a Git submodule so its exact source revision and
license remain independently visible.

## Raspberry Pi Pico SDK

The firmware targets the official Pico SDK 2.3.0:

- <https://github.com/raspberrypi/pico-sdk>

The SDK itself is not included in this package.

## HP 48GX ROM

No Hewlett-Packard ROM data is included. The ROM-free UF2 contains a plainly
identifiable dummy marker and filler. `tools/make_uf2.py` validates and inserts
the end user's own revision-R image locally.
