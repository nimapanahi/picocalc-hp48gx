# HP 48GX revision R emulator for PicoCalc

[![CI](https://github.com/nimapanahi/picocalc-hp48gx/actions/workflows/ci.yml/badge.svg)](https://github.com/nimapanahi/picocalc-hp48gx/actions/workflows/ci.yml)

Current version: **1.0.0** (`v1.0.0`)

This is a standalone native emulator for a ClockworkPi PicoCalc fitted with the
standard Raspberry Pi Pico 2 W. It runs the public HP 48GX revision-R ROM,
drives the PicoCalc's 320×320 LCD directly, and draws a navigable HP 48GX-style
full 49-key keyboard below the calculator display. The PicoCalc arrow pad moves a visible
highlight like a finger, Space presses the highlighted HP key, and physical
Enter is always the calculator's real ENTER key. It
does not require PSRAM, MicroPython, Picoware, Wi-Fi, or an SD card.

> Naming note: HP made the **48GX** and the **49G**, but no model named 49GX.
> ROM revision R is the final HP 48G/GX ROM, and the embedded version text is
> `HP48-R`. This project therefore emulates the HP 48GX revision R.

## Make your installable UF2 (no compiler needed)

The included `hp48gx_picocalc_template.uf2` deliberately contains **no HP
ROM**. The small builder validates your ROM's model, revision, size, and
internal CRC before inserting it locally. Your ROM is not uploaded anywhere.

You need Python 3 and your `gxrom-r` file. Packed 512 KiB files, unpacked 1 MiB
nibble files, `.bz2`, and the usual `gxrom-r.zip` download are accepted.

### Windows

Drag the ROM file onto `MAKE_FIRMWARE.bat`.

### Linux or macOS

```sh
chmod +x make_firmware.sh
./make_firmware.sh /full/path/to/gxrom-r.zip
```

Either method creates `hp48gx_picocalc.uf2` beside the scripts. Do not flash
the file named `template`; only flash the newly created file.

## Install on the PicoCalc

1. Power the PicoCalc off and disconnect USB.
2. Hold the Pico 2 W's **BOOTSEL** button while connecting it to your computer.
3. Release BOOTSEL when the `RP2350` USB drive appears.
4. Copy `hp48gx_picocalc.uf2` to that drive. It will reboot automatically.

After reboot, allow about three seconds for the PicoCalc keyboard controller to
start. Version 1.0.0 explicitly turns the LCD backlight on, resets and validates
the keyboard bus, then drives the
ILI9488-style panel through RP2350 PIO using SPI mode 3 and three-byte RGB666
pixels at a conservative 10 MHz.

This is native firmware, so installing it replaces Picoware on the Pico 2 W.
Your Picoware files on the SD card are untouched. To return to Picoware, flash
its UF2 again.

## Using the on-screen keyboard

The drawn keyboard preserves the complete HP 48GX row layout, including its HP
arrow cluster and all six A–F softkeys. The PicoCalc arrows move the yellow
highlight; they do not directly press the drawn HP arrow keys. Move the
highlight onto an HP arrow or softkey and press Space to use it.

The current layout retains the enlarged 300×146 calculator LCD and compact 300×133
keyboard. The LCD is divided into six
50-pixel soft-menu zones, and each 48-pixel A–F key is centered exactly beneath
its corresponding zone. This makes the on-screen menu-to-softkey relationship
visually direct while retaining every one of the 49 keys.

The keyboard has four live views: base, purple left-shift, teal right-shift,
and Alpha. A left-shift tap turns the keys purple and shows their left-shift
legends; a right-shift tap turns them teal and shows their right-shift legends.
Select the drawn `ALPHA` key and press Space to show A–Z on their exact HP 48GX
key positions. These pending contexts are UI state while you navigate and do
not hold an HP matrix key down. The modifier and chosen key are sent together
only during the final key tap, then released cleanly.

The first press of the drawn `ALPHA` key selects Alpha for one letter. Press it
again before choosing a letter to turn on Alpha Lock; the footer changes from
`NAV` to `TXT`. A third press cancels Alpha Lock. Caps Lock is a direct shortcut
for Alpha Lock. The complete six-key A–F row remains drawn in every context. In
Alpha view those keys enter letters A–F; return to base view when you want them
to activate the calculator's current softkey menus.

| PicoCalc key | HP 48GX action |
|---|---|
| Arrow keys | Move the yellow on-screen highlight |
| Space | Hold/release the highlighted drawn HP key |
| Enter | Dedicated HP ENTER; commits an entry to the stack |
| Highlight drawn `ALPHA`, then Space | Alpha context for one A–Z key |
| Press drawn `ALPHA` again | Alpha Lock; a third press cancels it |
| Caps Lock | Toggle Alpha Lock directly |
| F1–F5 | Top soft keys A–E |
| Shift+F1 | F6 / top soft key F |
| Shift+F2, F3, F4, F5 | F7/MTH, F8/PRG, F9/CST, F10/VAR |
| Backspace | Backspace / DROP |
| Delete | DEL |
| Tab | NXT |
| Esc | ON / cancel / wake |
| Tap and release Left Shift by itself | HP left prefix and purple on-screen legends |
| Tap and release Right Shift by itself | HP right prefix and teal on-screen legends |
| Teal context, highlight `OFF`, then Space | Save, verify HP OFF, then soft-power-off the PicoCalc |
| Hold Shift with a printed symbol | That symbol directly; e.g. Shift+8 is HP multiply |
| 0–9, `.`, `+`, `-`, `*`, `/` | Matching HP number/operator key |
| A–Z | Optional shortcut to the HP key carrying that alpha letter |
| Ctrl (hold) | Modifier for save/reset shortcuts below |
| Ctrl+F10 | Save the complete calculator state to flash |
| Ctrl+Esc | Delete the saved state and cold-reset |

At the HP cold-start question `Try to Recover Memory?`, press **F1** for YES or
**Shift+F1** for F6/NO. Physical Enter always sends HP ENTER. To press the
drawn wide `ENTER` key with the on-screen finger instead, highlight it and
press Space.

The status strip names the active context and highlighted or pressed key.
Everything needed for equations and commands—including A–Z, softkeys, HP
arrows, shifts, and punctuation—can be entered through the drawn keyboard using
only the PicoCalc arrows and Space. Direct number, letter, operator, Backspace,
Delete, Tab, and F-key shortcuts remain available but are optional. Alt and Sym
are currently unused.

## Saving and turning off

To finish a session, select the teal context, move the highlight to `OFF`, and
press Space. Version 1.0.0 saves immediately inside the keyboard-event path,
so saving also works while the Saturn CPU is already in its normal idle
`SHUTDN` loop.
It then sends a deliberately paced teal press/release followed by ON
press/release and watches the emulated HP LCD power bit. If the HP ROM does not
turn its display off within six seconds, or turns off but wakes again during
the 600 ms stability check, the complete sequence is retried up to three times.

After `HP OFF CONFIRMED` appears, the firmware asks the PicoCalc keyboard
controller and its AXP2101 power manager to remove system power. The backlight
turns off while the PMU completes shutdown after its required six-second
safety delay. On the next physical power-on, the saved
calculator stack, variables, modes, and RAM are restored automatically. An
unstable HP LCD still proceeds to PicoCalc shutdown because state is already
safe; a failed save or failed PMU request leaves the unit running and displays
the failure instead of pretending shutdown succeeded.

Full system shutdown requires a PicoCalc keyboard BIOS that implements the
official `0x0E` power-off register (added upstream in August 2025 and present
in BIOS 1.6 and newer). Version 1.0.0 reports the detected controller version
immediately before requesting shutdown. Once state saving succeeds, it
requests PicoCalc shutdown even if the HP LCD never settles after three
attempts. If the PMU does not remove power within nine seconds, it restores the
backlight and reports a timeout instead of hanging.

## PicoCalc battery indicator

The top header shows `BAT nn%`, read from the official PicoCalc keyboard
controller battery register. A trailing `+` in teal means the batteries are
charging. Readings at or below 15% are purple. The controller refreshes its
fuel-gauge value periodically, so a new reading can take several seconds to
appear after power or charging changes.

## State and storage

The full Saturn CPU state and 256 KiB GX RAM are restored automatically after
a manual save or Save-and-OFF. The state occupies the last 256 KiB of the Pico
2 W's 4 MiB flash and is tied to the exact ROM. Saving happens only when you
request it, avoiding unnecessary flash wear. Reflashing the firmware may erase
the saved state.

## What is implemented

- Saturn CPU and G/GX memory controller, based on the GPL x48 core
- 256 KiB HP 48GX system RAM
- 131×64 LCD scaled proportionally to 300×146, with all six annunciators
- A–F keys centered exactly under the LCD's six equal soft-menu zones
- Full 49-key navigable HP 48GX keyboard with geometric four-way selection
- Purple, teal, and Alpha one-shot/lock contexts with changing legends and no
  continuously held HP modifier matrix key while navigating
- Verified and retryable HP teal→ON OFF followed by PicoCalc PMU soft shutdown
- PicoCalc battery percentage and charging indicator
- HP real-time clock, Timer 1/2 interrupts, ON/wake, and shutdown behavior
- Direct ILI9488 LCD and STM32 keyboard-controller drivers for PicoCalc
- ROM validation and a ROM-free UF2 patching workflow

Not currently emulated: plug-in RAM cards, serial/Kermit, infrared, or sound.
The Wi-Fi radio is unused. This build has been cross-compiled for `pico2_w`,
the UF2 metadata has been inspected, and the real revision-R ROM completed a
10.24-million-instruction boot smoke test with its LCD on and no illegal
instruction. Physical-device testing is still required because no PicoCalc was
connected during development.

## If the display is blank

- Confirm you flashed `hp48gx_picocalc.uf2`, not the ROM-free file whose name
  contains `template`.
- Power the PicoCalc fully off for ten seconds, then start it and wait five
  seconds. A normal boot briefly shows `HP48GX 1.0.0`, then the drawn
  keyboard appears below the HP display.
- If it is still blank, note whether the panel is completely unlit or is lit
  gray/black. That distinction identifies backlight/power versus LCD-data
  trouble.
- For detailed startup messages, connect the PicoCalc USB port to a computer
  after boot and open the Pico's USB serial port at any baud rate.

## Build from source instead

Install CMake, Ninja, an `arm-none-eabi` toolchain, Python 3, and the Raspberry
Pi Pico SDK 2.3.0. Set `PICO_SDK_PATH`, then run:

```sh
./build.sh /full/path/to/gxrom-r.zip
```

The result is `build/hp48gx_picocalc.uf2`. The configuration targets
`pico2_w`, runs the RP2350 at its stock 150 MHz, and reserves the final 256 KiB of flash
for state. A successful test build uses 272,324 bytes of SRAM and 1,154,192
bytes of flash including the 1 MiB unpacked ROM.

## License and provenance

The emulator is GPL-2.0-or-later, following Eddie C. Dost's x48 core. The
PicoCalc pin assignments and device protocols follow ClockworkPi's public
PicoCalc sources. The HP ROM is not part of this project or its license; the
template workflow inserts the copy you supply on your own machine. See
`THIRD_PARTY.md` for source links and the exact adapted-file list.
