# HP 48GX revision R emulator for PicoCalc

[![CI](https://github.com/nimapanahi/picocalc-hp48gx/actions/workflows/ci.yml/badge.svg)](https://github.com/nimapanahi/picocalc-hp48gx/actions/workflows/ci.yml)

Current stable version: **2.0.0** (`v2.0.0`)

This is a standalone native emulator for a ClockworkPi PicoCalc fitted with the
standard Raspberry Pi Pico 2 W. It runs the public HP 48GX revision-R ROM,
drives the PicoCalc's 320×320 LCD directly, and draws a navigable HP 48GX-style
full 49-key keyboard below the calculator display. The PicoCalc arrow pad moves a visible
highlight like a finger, Space presses the highlighted HP key, and physical
Enter is always the calculator's real ENTER key. It
does not require PSRAM, MicroPython, Picoware, Wi-Fi, or an SD card to run.
Version 2.0 adds optional SD-card storage, native sound, Port 1/2 card support,
temporal grayscale, and the completed physical/drawn-keyboard workflow.

> **Release status:** 2.0.0 is feature complete for the project's planned main
> feature set. It is not universally compatible with every HP 48 third-party
> game or application. See [game compatibility tests](docs/GAME_TESTS.md) for
> confirmed successes and known failures before installing a specific title.

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
start. The firmware explicitly turns the LCD backlight on, resets and validates
the keyboard bus, then drives the
ILI9488-style panel through RP2350 PIO using SPI mode 3 and three-byte RGB666
pixels at the PicoCalc reference rate of 25 MHz.

Version 2.0.0 defaults active Saturn execution to 500,000 decoded
instructions per second. After the wall-time timer fixes, physical Phoenix
testing found this rate smoother without making gameplay run faster. Sound is
reconstructed from calculator-time speaker edges and is no longer tied to the
selected host throughput or its 256-instruction batches. Press `Ctrl+Left` to slow
execution or `Ctrl+Right` to speed it up through 20k, 25k, 33k, 40k, 50k,
66k, 83k, 100k, 125k, 166k, 200k, 250k, 333k, 500k, 667k, 1M, 1.5M, and 2M
presets, followed by an unpaced `MAX` setting; the footer reports the selected
rate. `Ctrl+Left` always steps back down from MAX.
Scaled HP frames use a single-core cooperative PIO row queue, with no LCD DMA,
second-core worker, inter-core lock, or DMA-abort path. After the first complete
frame, only the scaled span containing actual changed pixels in each changed
source row is transferred. At most one source-row span is sent per emulator
loop, allowing Saturn execution, native LCD scan polling, sound, and input to
continue between bounded transfers.

Native grayscale games alternate two one-bit HP frame planes. Version 2.0.0 models
the GX LCD's 4096 Hz line down-counter and samples native full-screen writers
at a complete 64-line scan boundary. It detects temporal plane cycles and
renders paper, middle, and ink shades on the color LCD. Established grayscale
pairing keeps two independently tracked planes, so repeated captures of the
same selected plane cannot expose foreground-only or background-only frames
on the slower physical panel. Ordinary animation still keeps only its newest
frame. Version 2.0.0 drops the stored opposite plane when alternation stops, so
a monochrome title or game-over screen cannot retain the previous playfield as
a background artifact. WarioLandHP 2.0 reaches gameplay in the corrected exact-ROM test;
WarioLandHP 3.1 remains a known core compatibility failure.

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
key positions. A drawn context key is sent immediately as a complete HP key
tap, so the calculator ROM latches the matching annunciator while you navigate.
The chosen key is then delivered separately, just as on the physical HP 48GX.

The first press of the drawn `ALPHA` key selects Alpha for one letter. Press it
again before choosing a letter to turn on Alpha Lock; the footer changes from
`NAV` to `TXT`. A third press cancels Alpha Lock. Caps Lock is a direct shortcut
for Alpha Lock, synchronized against the controller's real lock bit. The
complete six-key A–F row remains drawn in every context. In
Alpha view those keys enter letters A–F; return to base view when you want them
to activate the calculator's current softkey menus.

| PicoCalc key | HP 48GX action |
|---|---|
| Arrow keys | Move the yellow on-screen highlight |
| Ctrl+Space | Toggle physical arrows between keyboard navigation and HP arrow keys |
| Ctrl+Left / Ctrl+Right | Slow down / speed up Saturn execution |
| Alt+Esc | Warm OFF/ON cycle; preserves memory and attaches stored libraries |
| Right Shift+Esc | PicoCalc `BRK`; passed through to the calculator |
| Space | Hold/release the highlighted drawn HP key |
| Enter | Dedicated HP ENTER; commits an entry to the stack |
| Enter + locked arrow | Simultaneous HP matrix chord for game actions such as Android digging |
| Highlight drawn `ALPHA`, then Space | Alpha context for one A–Z key |
| Press drawn `ALPHA` again | Alpha Lock; a third press cancels it |
| Caps Lock | Toggle Alpha Lock directly |
| F1–F5 | Top soft keys A–E |
| Shift+F1 | F6 / top soft key F |
| Shift+F2, F3, F4, F5 | F7/MTH, F8/PRG, F9/CST, F10/VAR |
| Backspace | Backspace / DROP; stretched to a reliable 300 ms tap in arrow-lock game mode |
| Delete | DEL |
| Tab | NXT |
| Esc | ON / cancel / wake |
| Tap and release Left Shift by itself | HP left prefix and purple on-screen legends |
| Tap and release Right Shift by itself | HP right prefix and teal on-screen legends |
| Teal context, highlight `OFF`, then Space | Save, verify HP OFF, then soft-power-off the PicoCalc |
| Short press physical power button | Gracefully save, verify HP OFF, then soft-power-off |
| Hold Shift with a printed symbol | That symbol directly; e.g. Shift+8 is HP multiply |
| 0–9, `.`, `+`, `-`, `*`, `/` | Matching HP number/operator key |
| A–Z | Optional shortcut to the HP key carrying that alpha letter |
| Ctrl (hold) | Modifier for save/reset shortcuts below |
| Ctrl+F7 | Select the next HP file in the SD-card `INBOX` |
| Ctrl+F8 | Import the selected file onto stack level 1 |
| Ctrl+F9 | Export stack level 1 to a new numbered file in `OUTBOX` |
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

To finish a session, briefly press the physical PicoCalc power button. You can
also select the teal context, move the highlight to `OFF`, and press Space.
Version 2.0.0 saves immediately inside the keyboard-event path,
so saving also works while the Saturn CPU is already in its normal idle
`SHUTDN` loop.
It then sends a deliberately paced teal press/release followed by ON
press/release and watches the emulated HP LCD power bit. If the HP ROM does not
turn its display off within six seconds, the complete sequence is retried up
to three times. Once OFF is seen, 2.0.0 issues the final power request on the
next polling passes without the previous 600 ms stability and 350 ms status
pauses.

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
in BIOS 1.6 and newer). Version 2.0.0 reports the detected controller version
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

The full Saturn CPU state and 128 KiB GX built-in RAM are restored automatically after
a manual save or Save-and-OFF. The state occupies the last 256 KiB of the Pico
2 W's 4 MiB flash and is tied to the exact ROM. Saving happens only when you
request it, avoiding unnecessary flash wear. Reflashing the firmware may erase
the saved state.

### Version 2.0.0 SD-card storage

Insert a FAT32 or exFAT SD card before boot. The firmware mounts it without
formatting it and creates this non-destructive directory layout:

```text
/HP48GX/
└── PROGRAMS/
    ├── README.TXT
    ├── FILES.TXT
    ├── PORT2.TXT
    ├── PORT2.CRD
    ├── PORT1.MODE (optional compatibility marker)
    ├── PORT1.CRD  (created only in compatibility mode)
    ├── INBOX/
    └── OUTBOX/
```

For individual programs and objects, copy standard binary HP 48 files with an
`HPHP48-x` header into `INBOX` while the PicoCalc is powered off. The final
header byte is producer/ROM-revision metadata; common values such as A, E, R,
and W are accepted. Press
Ctrl+F7 to cycle through the files, Ctrl+F8 to put the selected object on stack
level 1, and Ctrl+F9 to export the current level-1 object. Exports are named
`OUTBOX/STK000.48G` through `STK999.48G`; the first unused number is chosen, so
an older file is never overwritten. Imports leave their source files intact.
Ctrl+F7 only selects; it does not load the file. Ctrl+F8 queues the import until
the HP ROM reaches its normal idle state, then reports the result. After it
reports an import to level 1, store that object under a backup name such as `:2: MYPROG`
and press Ctrl+F10 or Save-and-OFF to persist the changed Port 2 card image.

A directory object is not executable directly from stack level 1, so `EVAL`
is intentionally a no-op. To install one, enter a global name such as `'KAH'`
above it and execute `STO`. Press `VAR`, select the new `KAH` directory, then
select its program softkey. For Kahla that softkey is `Kala`.

The individual-file path is separate from the native card-image path. It
validates object boundaries before committing and streams data directly
between the SD card and HP temporary memory to conserve RP2350 SRAM. The
firmware imports binary objects only; it does not compile plain-text UserRPL.
If the object does not initially fit, the firmware asks the stock revision-R
ROM to garbage-collect unused temporary objects once before reporting that HP
memory is full. The complete CPU and scheduler state is restored afterward.
For a first hardware check, `samples/PICOCALC.48G` is a safe string object that
should display `"PICOCALC"` at stack level 1 after import; `samples/README.md`
contains the complete round-trip test.

`PORT2.CRD` is a standard packed 128 KiB x48-compatible image that the HP ROM
sees as a writable Port 2 RAM card. It is imported at boot. Ctrl+F10 and the
normal Save-and-OFF path export any changes. Updates are first written to
`PORT2.NEW`; the prior valid image is retained as `PORT2.BAK` so an interrupted
save can be recovered on the next boot.

Some early machine-language libraries, including the tested
`TETRISGX.LIB` 3.0, are not safe when executed from the GX's covered Port 2.
To use one, power off, create a file named `PORT1.MODE` in this `PROGRAMS`
folder (its contents do not matter), and boot. The footer reports `P1 NEW` or
`P1 READY`, and the firmware uses a separate `PORT1.CRD`. Import the library,
put `1` above it, execute `STO`, and warm-start the calculator before opening
the Library catalog. Remove `PORT1.MODE` while powered off to return to the
unchanged Port 2 image. Only one card is mounted at a time to stay within the
Pico 2 W's SRAM budget.

On the first boot with a card, the emulator creates a blank image. Initialize
that card once with purple/left-shift → drawn `2` → `NXT` → `PINIT`. Teal/
right-shift → drawn `2` opens the catalog of attached libraries instead; its
six softkeys are correctly blank before a library is installed and attached.
`PINIT` is silent on a card that is already in the canonical empty state; 2.0.0
shows an explicit footer confirmation when that softkey is delivered.
Programs and other calculator objects can then be stored as backup objects on
the active virtual card. Create the backup name with teal/right-shift plus the
drawn `+` key labeled `::`, type the port, press Space, and enter the name
(`:1: NAME` with `PORT1.MODE`, otherwise `:2: NAME`), then use `STO`. The
import footer now shows the matching active port. The card image
can also be moved between the PicoCalc and
x48-compatible desktop tools while the PicoCalc is fully powered off.

The boot footer reports the corresponding `P1` or `P2` ready, new, recovered,
no-SD, bad-image, or I/O status. The emulator remains usable when
no card is installed or it cannot be mounted. It will not overwrite a
pre-existing `PORT2.CRD` of the wrong size. Do not remove the SD card while the
emulator is running. See `docs/SD_CARD.md` for the complete workflow and
limits.

## Sound

Version 2.0.0 uses the PicoCalc stereo PWM audio hardware on GP26 and
GP27 for the HP 48GX one-bit speaker output. A high 2.6 kHz startup beep
confirms the complete output path before the calculator boots. ROM and native
game edge spacing is measured in calculator instructions and reconstructed by
an asynchronous 25 kHz timer, so core batching and LCD transfers cannot turn
the square wave into a low chirp. Output uses the configured 60% setting for
both the startup diagnostic and HP-generated sounds.

## What is implemented

- Saturn CPU and G/GX memory controller, based on the GPL x48 core
- 128 KiB HP 48GX built-in calculator RAM (stored internally as a 256 KiB
  one-byte-per-nibble array)
- 131×64 LCD scaled proportionally to 300×146, with all six annunciators
- A–F keys centered exactly under the LCD's six equal soft-menu zones
- Full 49-key navigable HP 48GX keyboard with row-aware four-way selection
- Purple, teal, and Alpha one-shot/lock contexts with changing legends and no
  continuously held HP modifier matrix key while navigating
- Physical short-power or HP teal→ON OFF followed by PicoCalc PMU soft shutdown
- PicoCalc battery percentage and charging indicator
- HP one-bit speaker output on both PicoCalc speakers
- Individual standard HP 48 binary-object inbox/import and numbered exports
- SD-backed writable HP Port 2 card, or optional non-covered Port 1
  compatibility card, with recoverable full-image import/export
- HP real-time clock, Timer 1/2 interrupts, ON/wake, and shutdown behavior
- Direct ILI9488 LCD and STM32 keyboard-controller drivers for PicoCalc
- ROM validation and a ROM-free UF2 patching workflow

Not currently emulated: simultaneous Port 1 and Port 2 cards, serial/Kermit,
infrared, plain-text UserRPL compilation, or an on-screen file-picker dialog.
Individual files are selected
with Ctrl+F7 and transferred with Ctrl+F8/F9; the complete native Port 2 image
remains available as a second storage workflow.
The Wi-Fi radio is unused. This build has been cross-compiled for `pico2_w`,
the UF2 metadata has been inspected, and the real revision-R ROM completed a
10.24-million-instruction boot smoke test with its LCD on and no illegal
instruction. A second real-ROM regression proves the exact Kahla directory
changes stack depth from 0 to 1, remains present after another ROM key, stores
as `KAH`, opens through `VAR`, launches `Kala`, draws the game, and produces
sound. Exact-ROM tests also run Pac Man GX and AstroNUT directories for 20
seconds each and install/launch the GX-converted Frog48 library from Port 1.
An exact `TETRISGX.LIB` regression reproduces revision R's fatal
loop from covered Port 2 and verifies that the same library installs and
launches without that loop from the packed Port 1 compatibility card.
Exact-ROM tests supplement, but do not replace, physical PicoCalc testing of
each candidate's panel timing, keyboard controller, SD card, and audio path.

## If the display is blank

- Confirm you flashed `hp48gx_picocalc.uf2`, not the ROM-free file whose name
  contains `template`.
- Power the PicoCalc fully off for ten seconds, then start it and wait five
  seconds. The firmware briefly shows `HP48GX 2.0.0`, then the drawn
  keyboard appears below the HP display.
- If it is still blank, note whether the panel is completely unlit or is lit
  gray/black. That distinction identifies backlight/power versus LCD-data
  trouble.
- For detailed startup messages, connect the PicoCalc USB port to a computer
  after boot and open the Pico's USB serial port at any baud rate.

## Build from source instead

Install CMake, Ninja, an `arm-none-eabi` toolchain, Python 3, and the Raspberry
Pi Pico SDK 2.3.0. Initialize the pinned SD-card dependency, set
`PICO_SDK_PATH`, then run:

```sh
git submodule update --init --recursive
./build.sh /full/path/to/gxrom-r.zip
```

The result is `build/hp48gx_picocalc.uf2`. The configuration targets
`pico2_w`, runs the RP2350 at its stock 150 MHz, and reserves the final 256 KiB
of flash for state. The 2.0.0 test build uses 430,560 bytes of static
SRAM and 1,241,300 bytes of flash including the 1 MiB unpacked ROM.

## License and provenance

The emulator is GPL-2.0-or-later, following Eddie C. Dost's x48 core. The
PicoCalc pin assignments and device protocols follow ClockworkPi's public
PicoCalc sources. The HP ROM is not part of this project or its license; the
template workflow inserts the copy you supply on your own machine. See
`THIRD_PARTY.md` for source links and the exact adapted-file list.
