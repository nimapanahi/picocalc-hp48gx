# HP48GX PicoCalc game compatibility tests

These unmodified hpcalc.org objects have been exercised against the exact
revision-R ROM used by 2.1.0. The host regression records import,
command invocation, display/audio activity, and fatal emulator halts. Those
signals are not by themselves proof of gameplay: for libraries it must select
the documented game command and inspect the resulting LCD state. The harness
installs through `STO`, warm-starts, and opens the ROM's LIBRARY catalog; it
does not bypass the menu with a synthetic ROMPTR. Physical PicoCalc testing
remains the final verdict on grayscale quality and playability. Put objects in
`/HP48GX/PROGRAMS/INBOX` while the PicoCalc is powered off. Ctrl+F7 cycles
files and Ctrl+F8 imports the selected object to stack level 1.

## Android 2.01 (recommended complete game/editor test)

Use the unmodified revision-R `Android.lib` object from the official archive,
renamed `ANDROID201.LIB` so it is unambiguous beside the unrelated HP 49
version: <https://www.hpcalc.org/details/459>. It is library 1698 and includes
the ANDROID game, the EDITANDRO level editor, and an information command.

1. Use the fresh 2.1 Port 1 card, or select it explicitly with
   `/HP48GX/PROGRAMS/PORT1.MODE` before boot. Keep it unmerged for libraries.
2. Import `ANDROID201.LIB`, put `1` above the library object, and execute `STO`.
3. Warm-start with physical Alt+Esc.
4. Open teal/right-shift -> drawn `2` (`LIBRARY`), select ANDROID, then select
   its first `ANDROID` softkey. Press Enter at the presentation screen.
5. Toggle arrow lock with Ctrl+Space. The physical arrows now send Android's
   HP `K`/`P`/`Q`/`R` movement keys. Hold Enter with an arrow to dig, Delete
   pauses, Backspace retries, and Esc returns to the presentation. Backspace
   at the presentation exits.

The second library softkey, `EDITANDRO`, opens the included level editor and is
the companion application test. Move with the locked arrows, use `+`/`-` to
choose blocks, `+/-` to draw, and Enter to save and exit.

The exact revision-R ROM test installs library 1698 through the real catalog,
reaches the presentation, starts a level, draws a clean playfield, accepts the
four movement keys and simultaneous Enter-plus-arrow dig chords, and remains
halt-free through the 33-second control loop. Version 2.0.0 also prevents the
presentation from retaining a stale temporal gameplay plane.
Android does not exercise the calculator audio path, so pair it with 1st Demo.

## 1st Demo (recommended graphics/audio stress application)

Use the official `FSTDEMO.DIR` object as `FSTDEMO.48G`; only the filename
extension changes because `.DIR` is not listed by the PicoCalc INBOX browser:
<https://www.hpcalc.org/details/1069>. The 41 KiB directory exercises
digitized sound, 3-D animation, Flight Simulator graphics, and parallax
scrolling.

1. Import `FSTDEMO.48G` to stack level 1.
2. Put the global name `'DEMO'` above it and execute `STO`.
3. Press `VAR`, enter `DEMO`, and select its first softkey. Press Enter at
   `The 1st Demo` title.
4. At `CHOOSE GRAPHIC CARD`, select `External Hybrid` (HP Down twice) and
   press Enter. Toggle physical arrow lock with Ctrl+Space first if needed.
5. Back in the DEMO directory, select the first `GO` softkey.

The exact-ROM path reaches the title and device form, detects the internal
beeper, then runs the 3-D sequence. Its 20-second observation window records
184 changed LCD frames and 210 additional audio edges without a fatal halt.

## Pac Man GX (primary graphics test)

Use the GX directory object repackaged as `PACMANGX.48G` from the official Pac
Man archive:
<https://www.hpcalc.org/details/550>.

1. Import `PACMANGX.48G`.
2. With the directory on level 1, enter the global name `'PACMAN'` above it
   and execute `STO`.
3. Press `VAR`, select `PACMAN`, then select its first game softkey.
4. Press a key to start. Controls are `4` left, `6` right, `8` up, `2` down,
   and Backspace to exit.

The exact-ROM test runs this assembly hardware-scrolling path for 20 seconds,
presses all four directions, observes more than 500 changed LCD frames, and does not
halt.

## Frog48 (Port 1 library test)

`FROG48.LIB` is library 1256, the GX conversion contained in AS-Games:
<https://www.hpcalc.org/details/223>.

1. Use the fresh 2.1 Port 1 card, or select it explicitly with
   `/HP48GX/PROGRAMS/PORT1.MODE` before boot. Keep it unmerged for libraries.
2. Import `FROG48.LIB`, put `1` above the library object, and execute `STO`.
3. Warm-start with physical Alt+Esc (or teal/right-shift then `ON`).
4. Open teal/right-shift → drawn `2` (`LIBRARY`), select library 1256, then
   select `FrOgGeR48`.

## WarioLandHP (grayscale triage)

Use `WARIOHP31.LIB` first. It is the full WarioLandHP 3.1 library 1693 from
the official archive and contains the more complex levels, grayscale, and
sound: <https://www.hpcalc.org/details/608>. `WARIOHP20.LIB` is the smaller
library 1697 fallback from <https://www.hpcalc.org/details/607>.

1. For the reproducible emulator test, use the fresh 2.1 Port 1 card or select
   it explicitly with `/HP48GX/PROGRAMS/PORT1.MODE` before boot.
2. Import one Wario library, put `1` above the library object, and execute
   `STO`. The original 3.1 booklet instead suggests memory Port 0 (`0 STO`);
   the 2.0.0 validation tests both placements, but neither makes 3.1 playable.
3. Warm-start with physical Alt+Esc (or teal/right-shift then `ON`).
4. Open teal/right-shift → drawn `2` (`LIBRARY`), select the Wario library,
   then select its game command. Press `ENTER` to start.

Controls are physical `G`/`H` for left/right, `K` to jump, and `N` to fire
when Super Wario. `+/-` pauses, physical `Esc` (the emulated HP `ON` key)
leaves play, and Backspace/`DROP` returns to the presentation. The star-ship
stage uses `K`/`Q` for up/down and `H`/`G` for right/left. Alpha is not needed
for these direct game controls.

The corrected 2.0.0 path invalidates the earlier Wario numbers: the old
direct ROMPTR shortcut never launched the game, so it had counted ROM HOME and
error-screen activity. Through the real menu path, version 2.0 reaches title
and actual gameplay. Its raw emulated LCD output is a clean A/B/A temporal
grayscale sequence. Version 2.0.0 keeps two independently tracked temporal
planes and drains physical dirty rows cooperatively, so repeated captures do
not expose a raw plane and long panel transfers do not stop gameplay polling.

Version 3.1 still fails. It produces the same dense bad terminal frame seen on
hardware, then produces no further LCD changes and returns to a bad ROM/error
state even though the instruction decoder does not report an illegal opcode.
Installing it in author-documented Port 0 or compatibility Port 1 does not
resolve the failure. Treat 3.1 as unsupported in 2.0.0. The standalone
`WARIOHP.SE` object is not a recommended test file.

The Wario triage was still productive. Physical testing confirms that the
timer, pacing, Port 1, and cooperative display changes it drove now let Phoenix
and Tetris run correctly and smoothly. Keep both as positive regression tests
when changing the native-game path further.

## Mission: Impossible (rejected test)

Use `MISSIMP.LIB`, an extension-only rename of the unmodified `MISSIMP.GX`
object from the official archive: <https://www.hpcalc.org/details/543>. It is
library 1266 and occupies about 18 KiB.

1. Use the fresh 2.1 Port 1 card, or select it explicitly with
   `/HP48GX/PROGRAMS/PORT1.MODE` before boot. Keep it unmerged for libraries.
2. Import `MISSIMP.LIB`, put `1` above the library object, and execute `STO`.
3. Press physical Alt+Esc for the warm OFF/ON cycle.
4. Select `LEVEL` once to create the `levels.IMP` directory, enter it, and
   recall a level such as `L0` so its `Library Data` object is on stack level 1.
5. Reopen library 1266 and select `IMP`.

Do not use this as a compatibility target. The earlier test accidentally
selected `INFOS.IMP`, the first library softkey, and counted its help and error
screens as gameplay. With the correct `IMP` softkey and required level object,
the exact-ROM emulator reproduces `Insufficient Memory` on an otherwise clean
128 KiB GX and after merging the 128 KiB Port 1 card. That matches the physical
PicoCalc failure and rules out the user's remaining variables as its cause.

## AstroNUT (UserRPL comparison)

Use the directory object repackaged as `ASTRONUT.48G` from the official
AstroNUT 1.1 archive:
<https://www.hpcalc.org/details/464>.

1. Import `ASTRONUT.48G`.
2. Enter the global name `'ASTRONUT'` above it and execute `STO`.
3. Press `VAR`, select `ASTRONUT`, then select `PLAY`.
4. Controls are `8` for upward thrust, `4` for left thrust, and `6` for right
   thrust.

AstroNUT is all UserRPL and its exact-ROM run remains halt-free, but physical
2.0.0 development testing showed severe display glitches and unplayable
output. Keep it as a known compatibility limitation rather than a recommended
game test.
