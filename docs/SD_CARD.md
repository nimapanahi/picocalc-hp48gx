# SD-card storage

## Version 2.1.0

The PicoCalc SD socket is connected to the Pico 2 W through SPI0:

| Signal | GPIO |
|---|---:|
| MISO | GP16 |
| CS | GP17 |
| SCK | GP18 |
| MOSI | GP19 |
| Card detect | GP22, active low |

At boot, the firmware checks card detect, mounts volume `0:` through FatFs,
and creates `/HP48GX/PROGRAMS`, `INBOX`, and `OUTBOX`. It also creates
`README.TXT`, `FILES.TXT`, `CARDS.TXT`, and the legacy `PORT2.TXT` only when
each file does not already exist. It never formats the card or modifies
unrelated files.

Supported filesystems are FAT12/16/32 and exFAT. FAT32 is recommended for the
widest compatibility with other PicoCalc firmware.

### Individual HP objects

Place standard binary HP 48 object files in:

```text
/HP48GX/PROGRAMS/INBOX
```

Files must use the conventional eight-byte `HPHP48-x` binary header. The final
byte records producer/ROM-revision metadata rather than a calculator model, so
values used by real HP tools and archives—such as A, E, R, and W—are accepted.
Supported file extensions are `.48G`, `.48`, `.48P`, `.HP`, `.LIB`, and `.BIN`,
matched without regard to case. Extract ZIP archives first. Plain-text UserRPL
is not compiled by the firmware.

- Ctrl+F7 selects the next supported inbox file and wraps after the last one.
- Ctrl+F8 queues the selected object for import onto stack level 1. The
  transfer runs when the HP ROM reaches its normal `SHUTDN` idle state so its
  stack pointers cannot be overwritten by an in-progress command.
- Ctrl+F9 exports stack level 1 to the first unused numbered file under
  `/HP48GX/PROGRAMS/OUTBOX`.

Selection does not copy the object into calculator memory. After Ctrl+F7 shows
the desired filename, press Ctrl+F8. `IMPORT QUEUED` may appear briefly; wait
for `IMPORTED ... -> L1`. With merged Port 1, the persistence hint tells you
to press Ctrl+F10 because the object is already in expanded user memory. With
an independent card it suggests `STO :1: NAME` or `STO :2: NAME`. Use that
backup identifier only when you want a port backup object. Ctrl+F10 or
Save-and-OFF writes the complete active card image to SD.

If level 1 contains a directory, `EVAL` does not enter it. Enter a global name
such as `'KAH'` above the directory and execute `STO`; then press `VAR` and its
`KAH` softkey to make it current. Kahla's game starts from the `Kala` softkey
inside that directory.

Exports use `STK000.48G` through `STK999.48G`. An existing numbered export is
never overwritten. A matching `STKnnn.NEW` is created only when neither name
exists, then renamed after its contents have been flushed. Imported source
files and pre-existing temporary names are never renamed, modified, or
deleted.

The transfer implementation uses the stock 48GX RPL memory layout and the same
binary object convention as established desktop HP emulators. It streams
between FatFs and HP temporary-object memory instead of allocating a second
object-sized RP2350 buffer. A malformed or incomplete import rolls the RPL
allocation back before returning an error. If the object does not initially
fit, the stock revision-R `=GARBAGECOL` routine is allowed one bounded pass.
The emulator restores every Saturn register and its scheduler position after
the pass, retaining only the ROM's intended HP RAM compaction. If the object
still does not fit, the transfer is rejected without changing the stack.

### Virtual RAM-card image and slot selection

The firmware presents one writable 128 KiB RAM card to the HP ROM. Fresh 2.1
installations use non-covered Port 1 so it can expand user memory. An existing
2.0 Port 2 image remains in Port 2 automatically. The backing files are:

```text
/HP48GX/PROGRAMS/PORT1.CRD
/HP48GX/PROGRAMS/PORT2.CRD
```

Each file uses the standard packed x48 card layout: each byte contains two
Saturn nibbles, low nibble first. Its exact size is 131,072 bytes. A compatible
image placed at either path while the PicoCalc is powered off is imported on
the next boot. A file of any other size is rejected and left untouched.

Selection is deterministic and non-destructive:

- `PORT1.MODE` explicitly selects `PORT1.CRD`.
- `PORT2.MODE` explicitly selects `PORT2.CRD`.
- With neither marker, an existing `PORT2.CRD` keeps the 2.0 behavior.
- Otherwise an existing or new `PORT1.CRD` is selected.
- If both markers exist, no card is attached and the footer reports
  `CARD MODE ERR`.

Create or remove markers only while fully powered off. Only one slot is active
at a time because the standard Pico 2 W cannot hold two additional packed
128 KiB images while retaining the full emulator feature set.

If no selected image exists, the emulator creates an all-zero card. The HP ROM
may initially report invalid card data; this is expected for an uninitialized
RAM card. Run `PINIT` once with purple/left-shift → drawn `2` → `NXT` →
`PINIT`. Teal/right-shift → drawn `2` is the attached-library catalog and is
normally blank on a new card. After initialization, an independent card can
store an HP backup object under `:1: NAME` or `:2: NAME` for the active slot.

To recall a known backup directly, create a backup identifier rather than text
that merely looks like one:

1. Select the teal/right-shift context, select the drawn `+` key (now labeled
   `::`), and press Space. This invokes the HP 48GX backup-name entry key.
2. Type the active slot number, press Space to separate the port field, then
   enter the name with Alpha—for example, `TEST`—and press physical Enter.
   During keyboard entry this appears as `:1: TEST` or `:2: TEST`; omitting
   that Space does not select the name field correctly.
3. Select teal/right-shift and then the drawn `STO` key, now labeled `RCL`.

The stored contents replace the identifier on level 1. `EVAL` would execute
the recalled object instead.

The easier menu route is purple/left-shift → drawn `2` (`PORTS`) → the `PORTS`
softkey → the active port softkey. Bare object softkeys evaluate their objects;
teal/right-shift plus an object softkey recalls it to the stack.

`PINIT` has no stack result and an all-zero new card is already the ROM's
canonical empty state, so the command may make no visible calculator change.
The emulator footer confirms `PINIT SENT - P1 READY; COMMAND IS SILENT` or the
corresponding Port 2 status when the F2 softkey is delivered from the PINIT
menu page.

Press Ctrl+F10 to export changes without turning off. Save-and-OFF also exports
the card before requesting PicoCalc power removal. Wait for `STATE SAVED` or
the normal shutdown to finish before touching the SD card.

### Merge Port 1 with built-in user memory

Port 1 can be independent backup/library storage or merged user memory, but
not both at once. With Port 1 active and writable, execute the stock HP 48GX
revision-R command `MERGE1`. The ROM moves any existing Port 1 backup objects
or libraries into Port 0 and changes Port 1 to `SYSRAM`. Variables,
directories, stack objects, and Ctrl+F8 imports can then use the combined
256 KiB physical memory (128 KiB built in plus the 128 KiB card).

The release regression types `MEM`, executes `MERGE1`, and types `MEM` again.
It requires the available-memory value to grow, the Port 1 image to change,
the image to become dirty, and the revision-R ROM to remain halt-free. Press
Ctrl+F10 immediately after merging and after later changes; Save-and-OFF does
the same card export before shutdown.

To return the card to independent Port 1 memory, put an empty list `{ }` on
the stack and execute `FREE1`. The calculator must have at least 128 KiB free
to separate a 128 KiB card. Never switch slots, replace `PORT1.CRD`, or remove
the SD card while Port 1 is merged; that is equivalent to removing physical
system RAM from a running HP 48GX and can destroy user memory.

### Unmerged Port 1 for machine-language libraries

The GX's Port 2 is a covered/banked port. Some early machine-language
libraries can be stored there but cannot safely execute there. The exact
TetrisGX 3.0 library is one: revision R enters an invalid RPL-stream loop when
its first command runs from Port 2, while the identical object and command do
not enter that loop from the non-covered Port 1.

Version 2.1.0 uses Port 1 on a fresh installation. On an upgraded installation
that retained Port 2, select the non-covered card explicitly:

1. Save and fully power off the PicoCalc, then put the SD card in a computer.
2. Create `/HP48GX/PROGRAMS/PORT1.MODE` and remove `PORT2.MODE` if present.
   The Port 1 marker may be empty; only its filename matters.
3. Reinstall the SD card and boot. The footer should report `P1 NEW` the first
   time and `P1 READY` later. The firmware creates a separate 131,072-byte
   `PORT1.CRD` and never modifies `PORT2.CRD` while this marker exists.
4. Import `TETRISGX.LIB` with Ctrl+F7 and Ctrl+F8. Put `1` above the library
   object, execute `STO`, and warm-start with physical Alt+Esc (or
   teal/right-shift then `ON`).
5. Open teal/right-shift → drawn `2` (`LIBRARY`), select the Tetris library,
   and select its `TETRIS` command.

Save with Ctrl+F10 or Save-and-OFF as usual. Port 1 uses `PORT1.NEW` and
`PORT1.BAK` for the same interrupted-write protection as Port 2. To return to
the original Port 2 card, fully power off, remove `PORT1.MODE`, and create
`PORT2.MODE`. Keep this library card unmerged; `STO :1:` is unavailable after
`MERGE1` changes it into system RAM.

### Removing TetrisGX 3.0 from the boot library catalog

TetrisGX is not copied from `INBOX` again at each boot. Library 900 is already
stored in `PORT1.CRD`, and its configuration object automatically attaches it
to `HOME` during a warm start. Removing the inbox source alone therefore does
not uninstall it.

1. Go to `HOME`, enter `900`, and press Enter.
2. Open purple/left-shift → drawn `2` (`PORTS`) and select `DETAC`/`DETHC`
   (use `NXT` if necessary). This detaches library 900 from `HOME`.
3. Enter the library identifier `:1: 900`: use teal/right-shift → drawn `+`
   (`::`), type `1`, press Space, type `900`, and press physical Enter.
4. Use purple/left-shift → drawn `EEX` (`PURG`) to remove it from Port 1.
5. Press Ctrl+F10 and wait for `STATE SAVED` so the updated `PORT1.CRD` is
   written to the SD card.

If `Object In Use` appears, return to `HOME`, remove any recalled Tetris
objects from the stack, warm-start the calculator, and repeat detach then
purge before launching Tetris again.

### Interrupted-write protection

The emulator writes the full active image to `.NEW`, flushes and closes it,
moves the previous valid `.CRD` to `.BAK`, and only then promotes the new
image. This applies to either `PORT1` or `PORT2`. On boot, a missing current
image is recovered from a complete temporary image or the previous backup. A
bad-size current image is never overwritten.

The card is unmounted before the PicoCalc PMU shutdown request. Removing a card
while the emulator is running is unsupported and may lose changes made since
the last Ctrl+F10 save.

## Known 2.1 limits

- Only one virtual card is mounted at a time; simultaneous Port 1 and Port 2
  remain outside the feature set on the standard Pico 2 W.
- The inbox imports binary HP objects and does not compile plain-text UserRPL.
- File selection uses Ctrl+F7/F8/F9 rather than an on-screen file picker.
- Older machine-language software can depend on undocumented timing, memory,
  display, or port behavior. See [GAME_TESTS.md](GAME_TESTS.md) for validated
  titles and known incompatibilities.
- Removing an SD card while powered is unsupported. Physical card switching
  and power-loss recovery should still be treated cautiously with valuable
  calculator data.
