# On-screen keyboard

The PicoCalc arrow pad moves a yellow selection cursor over the complete
49-key HP 48GX keyboard. Space presses and releases the selected key. Physical
Enter always sends the unmodified HP ENTER key.

## Contexts

- Base: ordinary white HP key legends.
- Purple: HP left-shift legends. Select LSH or tap physical Left Shift.
- Teal: HP right-shift legends. Select RSH or tap physical Right Shift.
- Alpha: A-Z on the exact HP 48GX letter positions. Select ALPHA once for one
  letter, twice for Alpha Lock, and a third time to cancel. Caps Lock toggles
  Alpha Lock directly.

Contexts remain visible while the cursor moves. The corresponding HP modifier
is applied only with the selected action key, avoiding a continuously held
matrix key.

## Softkeys

The 300-pixel calculator LCD has six 50-pixel menu zones. Each 48-pixel A-F
key is centered directly under its matching menu zone. In Alpha context those
six keys enter A-F; return to Base to activate the current calculator menus.

## Save and OFF

Select the Teal context, move to OFF, and press Space. The emulator saves the
complete Saturn state and GX RAM, sends the ordered teal-to-ON OFF sequence,
and checks that the HP ROM keeps its LCD off continuously for 600 ms. It
retries up to three times if OFF is missed or the LCD wakes during that check.
It then blanks the panel and asks the PicoCalc AXP2101 power manager to remove
system power after six seconds. Because state was already saved, PicoCalc
shutdown still proceeds if the HP LCD never settles. Use the physical PicoCalc
power button to start again.

BIOS 1.6 and newer provide the official PMU power-off register. Immediately
before shutdown, the emulator refreshes and displays the detected BIOS version.
If power is not removed within nine seconds, it restores the backlight and
reports a timeout rather than remaining dark indefinitely.

## Battery

The top header continuously shows the PicoCalc battery percentage. A teal `+`
means charging; purple text warns at 15% or below.

## Direct shortcuts

Numbers, arithmetic operators, letters, Backspace, Delete, Tab, F1-F10, Esc,
and the two physical shifts retain direct mappings. Ctrl+F10 saves state;
Ctrl+Esc clears saved state and cold-resets the calculator.
