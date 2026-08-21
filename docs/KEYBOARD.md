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
briefly confirms that power-off is safe, and turns off the PicoCalc backlight.
Any new physical key press restores the backlight. Use Esc/ON afterward if the
HP ROM also needs to be awakened.

## Direct shortcuts

Numbers, arithmetic operators, letters, Backspace, Delete, Tab, F1-F10, Esc,
and the two physical shifts retain direct mappings. Ctrl+F10 saves state;
Ctrl+Esc clears saved state and cold-resets the calculator.
