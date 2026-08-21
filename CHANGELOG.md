# Changelog

## 1.0.0 - 2026-08-21

First stable release of the native HP 48GX revision-R emulator for PicoCalc
with Raspberry Pi Pico 2 W.

- Runs the HP 48GX revision-R ROM with persistent calculator state, real-time
  clock behavior, timers, annunciators, and an enlarged aligned LCD.
- Provides a complete navigable 49-key HP keyboard with six aligned softkeys,
  purple and teal prefix contexts, Alpha and Alpha Lock, and dependable
  physical Enter and direct-key shortcuts.
- Saves state before OFF, verifies and retries the HP teal→ON sequence, detects
  LCD reawakening, and requests full PicoCalc PMU shutdown through keyboard
  BIOS 1.6. A bounded timeout prevents a failed PMU request from hanging dark.
- Displays PicoCalc battery percentage, charging status, and a low-battery
  warning in the header.
- Includes a ROM-free UF2 template and local builder; no HP ROM is distributed
  in the repository or public release assets.

## 1.0.0-rc.10 - 2026-08-21

- Fixes the HP LCD-off race by requiring the emulated LCD to remain off
  continuously for 600 ms. If it wakes during verification, the complete
  teal→ON sequence is retried instead of accepting a momentary OFF state.
- Extends the per-attempt HP OFF processing window to six seconds.
- Guarantees PicoCalc shutdown after the calculator state has been saved: if
  the HP LCD never settles after three attempts, the firmware still asks the
  PMU to power off. This is safe because the restorable state was committed
  before the first OFF attempt.

## 1.0.0-rc.9 - 2026-08-21

- Removes the cached keyboard-power capability decision. After confirmed HP
  OFF, the firmware refreshes and displays the controller version, then sends
  the official power-off command even if that diagnostic read was transient.
- Adds a nine-second recovery timeout: if the PMU does not remove power, the
  RP2350 resumes and restores the backlight instead of remaining dark forever.
- Gives the HP ROM substantially longer prefix, settle, and ON-key intervals,
  plus a clean 250 ms start delay, to make teal→ON OFF reliable on hardware.

## 1.0.0-rc.8 - 2026-08-21

- Fixes a false `UPDATE KEYBOARD BIOS FOR PICO OFF` warning on the official
  PicoCalc keyboard BIOS 1.6. Power-off support is now determined from the
  controller's authoritative version byte (`0x16` or newer), rather than a
  transient boot-time read of the power-off command register.
- Keeps the safe version gate for older keyboard firmware and retains retry
  handling for the actual `0x0E` shutdown request.

## 1.0.0-rc.7 - 2026-08-20

- Makes drawn teal `OFF` deterministic: the firmware now uses longer key
  transitions, checks the emulated HP LCD state, and retries the complete
  teal→ON sequence up to three times instead of blindly darkening the panel.
- Saves before OFF as before, but proceeds only after the HP ROM confirms OFF.
  A failed save or three failed OFF attempts leaves the PicoCalc running with
  an explicit safe-state message.
- After HP OFF is confirmed, writes the official PicoCalc keyboard-controller
  power-off register. The AXP2101 PMU removes system power after its six-second
  safety delay, while the RP2350 immediately enters an idle halt loop.
- Probes for the PMU power-off register at boot and refuses to halt on older
  keyboard BIOS versions that do not implement it, preventing a false OFF.
- Adds persistent PicoCalc battery percentage to the top header. A trailing
  `+` and teal text indicate charging; low battery text changes to purple.

## 1.0.0-rc.6 - 2026-08-20

This is the first release using Semantic Versioning. Earlier `v1.x` and
`v2.x` names below were internal development snapshots; they are retained for
historical traceability and do not represent the public release sequence.

- Adds the missing apostrophe glyph, so the HP quote key between MTH and SIN
  now visibly shows `'` instead of appearing blank.
- Fixes an OFF deadlock when the Saturn CPU is already inside its idle
  `SHUTDN` loop. State saving now runs synchronously in the keyboard-event path
  instead of waiting for a main loop that cannot resume without an HP key
  interrupt.
- Continues with the ordered teal→ON sequence after saving and turns off the
  PicoCalc backlight after a 750 ms safe-power-off confirmation.
- Restores the backlight on any subsequent physical key press, allowing a
  software-OFF session to be woken without cycling physical power.

## aligned display v2.6 RC5

- Enlarges the emulated HP LCD from 262×128 to 300×146 while preserving its
  original aspect ratio to within one percent.
- Compacts the complete 49-key keyboard from about 310×151 to 300×133 so the
  larger calculator display and keyboard still fit cleanly on the 320×320 panel.
- Divides the LCD into six exact 50-pixel soft-menu zones and centers each
  48-pixel A–F key beneath its corresponding zone.
- Preserves the RC4 automatic save and correctly sequenced teal→ON OFF path.

## full context v2.5 RC4

- Fixes the drawn teal `OFF` key by sending it as four ordered transitions:
  teal down, teal up, ON down, and ON up, with Saturn CPU execution between
  each transition instead of an ineffective simultaneous chord.
- Automatically saves the complete calculator state before starting OFF, with
  no emulated keys held in the saved snapshot.
- Shows `STATE SAVED - SAFE TO POWER OFF` after the sequence. If flash saving
  fails, OFF is canceled so the user is not told it is safe to cut power.

## full context v2.4 RC3

- Adds a complete Alpha keyboard view with A–Z shown on the exact HP 48GX key
  positions while preserving the six drawn A–F softkeys.
- Makes the drawn ALPHA key one-shot on its first press, Alpha Lock on its
  second press, and canceled on its third press. Caps Lock toggles Alpha Lock
  directly.
- Keeps Alpha, purple, and teal as stable navigation contexts, applying the HP
  modifier only with the selected on-screen key and releasing it afterward.
- Keeps physical Enter unconditionally mapped to HP ENTER in every context.
- Supports complete on-screen entry using arrows plus Space; direct physical
  key shortcuts remain optional.

## full context v2.3 RC2

- Makes physical Enter a dedicated HP ENTER key, so entries and functions can
  always be committed to the stack.
- Moves the on-screen finger press to Space; arrows still move the highlight.
- Replaces the continuously held HP shift matrix key with a harmless pending
  UI context while navigating. Shift is applied only during the final key tap.
- Clears both shift and action after the chosen key release, preventing a
  broken context from contaminating later keys or turning ENTER into a shifted
  command.

## full context keyboard v2.2 RC1

- Changes the left- and right-prefix palettes to HP-style purple and teal.
- Cancels a latched physical prefix immediately on the second press of the
  same PicoCalc shift key, without relatching it on release.
- Adds the missing caret glyph to the UI font, making both the HP Up key `^`
  and the `Y^X` key label render correctly.

## full context keyboard v2.1

- Keeps an isolated orange or green shift tap latched while the PicoCalc
  arrows move the on-screen cursor.
- Consumes the latch only after the selected HP key completes its press and
  release, so the shifted context cannot expire before selection.
- Lets a second tap of the same shift cancel the pending prefix and applies
  the same latch behavior to the drawn LSH and RSH keys.

## full context keyboard v2.0

- Fits the complete 49-key HP 48GX keyboard into the 320x320 display,
  including all six A-F softkeys and the four-key HP cursor cluster.
- Changes every drawn key to its orange or green shifted legend when the HP
  ROM reports the corresponding prefix annunciator.
- Uses distinct base, orange, and green keyboard palettes, and automatically
  returns to base when the calculator consumes the prefix.
- Keeps the PicoCalc arrows as the movable on-screen finger and physical Enter
  as the selected-key press, while preserving all direct physical shortcuts.

## on-screen keyboard flavor v1.0

- Replaces the instruction legend with a compact HP 48GX keyboard mockup.
- Reserves the PicoCalc arrow pad for geometric movement of a highlighted
  on-screen key and uses physical Enter to hold/release that HP matrix key.
- Omits the HP arrow cluster and A–F softkeys from the drawing; physical arrows
  are navigation controls and physical F-keys retain direct softkey access.
- Preserves the responsive 15 ms, one-event-at-a-time keyboard transport and
  all display, ROM, state, and core fixes from the direct-key v1.7 flavor.

## v1.7 — PicoCalc symbol chords and responsive key timing

- Treats a held PicoCalc Shift as a symbol modifier because the keyboard
  controller already translates combinations such as Shift+8 into `*`.
  This prevents the same chord from also applying HP orange/green shift.
- Treats a left- or right-shift tap by itself as the HP orange or green prefix.
- Reduces FIFO polling latency from 50 ms to 15 ms while retaining one-event
  delivery so the Saturn CPU always runs between press and release.
- Reduces the per-key footer redraw area to remove unnecessary LCD-transfer
  latency while preserving the visible raw-key trace.

## v1.6 — wake the HP core on a physical key event

- Returns a real event result from the x48 `GetEvent` platform hook.
- Polls the keyboard inside `GetEvent`, after x48 clears its per-check
  interrupt flag, instead of consuming the event early in `pause`.
- This wakes the Saturn CPU from the ROM's `SHUTDN` instruction at prompts and
  during normal calculator idle, allowing the already-correct key matrix value
  to be processed.

## v1.5 — visible key trace and press/release delivery fix

- Shows every physical key press in the footer, including its name, raw
  PicoCalc controller code, and translated HP matrix code.
- Delivers one controller event at a time so the Saturn CPU executes while a
  key remains pressed. Previously, a queued press and release could both be
  consumed before the HP ROM ran, making recognized keys appear ineffective.

## v1.4 — keyboard transaction recovery

- Reads the controller version as the documented two-byte reply instead of
  leaving its second byte pending and blocking subsequent key events.
- Recovers a stuck PicoCalc I2C bus at startup and after failed transactions.
- Restores the keyboard controller's normal event configuration at boot.
- Polls at the controller-friendly 20 Hz rate and shows `KEYBOARD INPUT OK`
  after the first physical key event.

## v1.3 — ILI9488 pixel framing fix

- Retains the mode-3 RP2350 PIO transport that made the user's LCD respond.
- Restores the ILI9488 three-byte RGB666 pixel framing required by the panel.
- Restores the full ILI9488 gamma, power, frame-rate, and interface setup.

## v1.2 — current-panel display fix

- Replaces hardware SPI mode 0 with the known-working RP2350 PIO mode-3 path.
- Uses the ST7365P-compatible initialization sequence used by current PicoCalc
  firmware.
- Changes LCD pixel transport to RGB565 and lowers the display clock to 10 MHz.

## v1.1 — blank-screen startup fix

- Explicitly enables the PicoCalc LCD backlight through the STM32 controller.
- Starts the keyboard controller before drawing the UI and retries its probe.
- Reduces the LCD SPI clock from 50 MHz to ClockworkPi's proven 25 MHz rate.
- Adds USB serial startup diagnostics and an on-screen v1.1 boot status.

## v1.0

- Initial native HP 48GX revision-R emulator release for PicoCalc / Pico 2 W.
