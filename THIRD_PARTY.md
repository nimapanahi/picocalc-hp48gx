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

## ClockworkPi PicoCalc

LCD pins, keyboard I2C pins/address/protocol, key codes, battery register
`0x0B`, PMU power-off register `0x0E`, and the ILI9488 setup sequence follow
ClockworkPi's public PicoCalc examples:

- <https://github.com/clockworkpi/PicoCalc>
- <https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/reg.h>
- <https://github.com/clockworkpi/PicoCalc/blob/master/Code/picocalc_keyboard/picocalc_keyboard.ino>

No ClockworkPi source file is copied wholesale into this project.

## Raspberry Pi Pico SDK

The firmware targets the official Pico SDK 2.3.0:

- <https://github.com/raspberrypi/pico-sdk>

The SDK itself is not included in this package.

## HP 48GX ROM

No Hewlett-Packard ROM data is included. The ROM-free UF2 contains a plainly
identifiable dummy marker and filler. `tools/make_uf2.py` validates and inserts
the end user's own revision-R image locally.
