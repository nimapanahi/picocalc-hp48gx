# On-screen keyboard

The PicoCalc arrow pad moves a yellow selection cursor over the complete
49-key HP 48GX keyboard. Space presses and releases the selected key. Physical
Enter always sends the unmodified HP ENTER key.

Press `Ctrl+Space` to lock the physical PicoCalc arrows to the HP 48GX arrow
keys. The footer shows teal `ARR`; Left/Down/Right/Up now send the calculator's
`<`, `V`, `>`, and `^` keys instead of moving the yellow cursor. Press
`Ctrl+Space` again to return to on-screen keyboard navigation.

Physical HP actions are tracked independently. In arrow-lock mode, holding
Enter while pressing an arrow keeps both matrix keys down at the same time;
native games such as Android therefore receive their real dig chord. A quick
physical Backspace tap in arrow-lock mode is extended to 300 ms so a native
presentation loop cannot miss its HP `DROP` command.

Native Saturn speed defaults to 500,000 decoded instructions per second.
Press `Ctrl+Left` to select a slower preset or `Ctrl+Right` for a faster one;
the footer reports 20k, 25k, 33k, 40k, 50k, 66k, 83k, 100k, 125k, 166k,
200k, 250k, 333k, 500k, 667k, 1M, 1.5M, or 2M IPS, followed by unpaced MAX.
These shortcuts do not send an HP arrow key, even while arrow lock is enabled.
`Ctrl+Left` steps down normally from MAX.

## Contexts

- Base: ordinary white HP key legends.
- Purple: HP left-shift legends. Select LSH or tap physical Left Shift.
- Teal: HP right-shift legends. Select RSH or tap physical Right Shift.
- Alpha: A-Z on the exact HP 48GX letter positions. Select ALPHA once for one
  letter, twice for Alpha Lock, and a third time to cancel. Caps Lock toggles
  Alpha Lock directly. The controller Caps bit is polled so automatic hardware
  lock changes, the `TXT`/`NAV` status, header, and drawn legends stay aligned.
  A drawn ALPHA press is delivered to the HP ROM immediately, so its Alpha
  annunciator remains authoritative while the cursor moves to the letter.
Before every contextual action, the emulator checks the ROM annunciator. If
  the ROM has not latched the shown context, it re-sends the prefix and waits
  for confirmation before sending the selected key.

Every ordinary action key also has a 75 ms minimum HP matrix down-time. Quick PicoCalc
press/release events therefore remain visible to the Saturn keyboard scanner;
synthetic prefixes are held across several polls before their annunciator is
checked and retried. Multiple held action keys keep independent release
deadlines instead of forcing the preceding key up.

Vertical navigation chooses the nearest keyboard row before comparing
horizontal distance. From `7`, Up selects the wide `ENTER` key; its base,
purple, and teal labels are `ENTER`, `EQUATION`, and `MATRIX` respectively.

Contexts remain visible while the cursor moves. A drawn context key is pressed
and released immediately so the HP ROM latches it; the chosen action is sent
later as a separate key. Physical modifier contexts use an ordered synthetic
tap because their controller events do not represent HP matrix keys.

Purple/left-shift plus drawn `2` is labeled `PORTS` and opens the management
menu beginning with the `PORTS` softkey. Teal/right-shift plus drawn `2` remains
the attached-library catalog.

## Softkeys

The 300-pixel calculator LCD has six 50-pixel menu zones. Each 48-pixel A-F
key is centered directly under its matching menu zone. In Alpha context those
six keys enter A-F; return to Base to activate the current calculator menus.

## Save and OFF

Briefly press the physical PicoCalc power button, or select the Teal context,
move to OFF, and press Space. The emulator saves the
complete Saturn state and GX RAM, sends the ordered teal-to-ON OFF sequence,
and checks that the HP ROM turns its LCD off. It retries up to three times if
OFF is missed. Once OFF is observed, the final PMU request follows on the next
polling passes without the former 600 ms verification and 350 ms status waits.
The panel is then blanked and the PicoCalc AXP2101 removes system power after
the keyboard BIOS's mandatory six-second countdown. Because state was already
saved, PicoCalc shutdown still proceeds if the HP LCD never settles. Use the
physical PicoCalc power button to start again.

The keyboard controller reports a short physical power press to the emulator,
so it follows the complete graceful sequence above. A long press remains the
controller/PMU emergency cutoff and cannot guarantee that the latest state or
SD-card changes were saved.

BIOS 1.6 and newer provide the official PMU power-off register. Immediately
before shutdown, the emulator refreshes and displays the detected BIOS version.
If power is not removed within nine seconds, it restores the backlight and
reports a timeout rather than remaining dark indefinitely.

## Battery

The top header continuously shows the PicoCalc battery percentage. A teal `+`
means charging; purple text warns at 15% or below.

## Direct shortcuts

Numbers, arithmetic operators, letters, Backspace, Delete, Tab, F1-F10, Esc,
and the two physical shifts retain direct mappings. Ctrl+F7 selects the next
SD inbox file, Ctrl+F8 queues it for import at HP idle, Ctrl+F9 exports stack level 1, and Ctrl+F10
saves state. Ctrl+Esc clears saved state and cold-resets the calculator.
Physical Esc sends HP `ON/CANCEL`; in Kahla, press Esc once to exit the game.

Alt+Esc is the dedicated warm-start shortcut. It sends the ordered HP
teal/right-shift, `OFF`, then `ON` sequence while preserving calculator RAM,
variables, and the active card. Use it after storing a library; unlike
Ctrl+Esc, it does not clear the saved state or cold-boot the ROM. Right
Shift+Esc is not intercepted and retains the PicoCalc controller's `BRK`
behavior.
