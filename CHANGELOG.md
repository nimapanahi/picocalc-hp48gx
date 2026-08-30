# Changelog

## 2.1.0 - 2026-08-30

- Promotes the writable 128 KiB virtual Port 1 RAM card from the 2.0
  compatibility path into a supported user-memory expansion. A fresh SD card
  now creates `PORT1.CRD`; the stock revision-R `MERGE1` command merges it
  with the calculator's built-in 128 KiB user memory.
- Preserves upgrades safely: an existing 2.0 `PORT2.CRD` remains selected
  automatically. `PORT1.MODE` and the new `PORT2.MODE` explicitly select a
  slot, while conflicting markers fail without attaching or modifying either
  image.
- Makes imported-object guidance merge-aware. Once Port 1 is merged, imports
  direct the user to Ctrl+F10 rather than suggesting an invalid `:1:` backup
  destination; Save-and-OFF persists both built-in and merged memory.
- Adds `CARDS.TXT` to every newly prepared SD layout with the Port 1,
  `MERGE1`, upgrade, slot-selection, save, and recovery workflow.
- Adds an exact revision-R `MERGE1` regression that verifies the ROM expands
  available user memory, writes the virtual card, keeps the expected memory
  controllers configured, and does not halt. The calculator validation video
  now shows `MEM` before and after the merge.

## 2.0.0 - 2026-08-24

Feature-complete release of the project's planned native HP 48GX PicoCalc
feature set. This status covers the emulator, input, display, sound, storage,
power, and validation goals; it does not promise universal compatibility with
every third-party HP 48 game or application.

- Adds FAT/exFAT SD storage with validated individual HP binary-object import
  and export, recoverable packed card images, writable Port 2, and an optional
  non-covered Port 1 compatibility mode for older machine-language libraries.
- Adds native HP beeper reconstruction on both PicoCalc speakers, a 60% output
  level, stable calculator-time pitch, a high-pitched startup diagnostic, and
  timer/pacing corrections that keep sound independent of core throughput.
- Adds cooperative partial-row LCD transfers, scan-counter-aware temporal
  grayscale composition, clean monochrome scene transitions, and pacing from
  20k through 2M instructions per second plus an unpaced maximum mode.
- Completes physical and drawn-keyboard interaction with synchronized Alpha
  Lock/Caps Lock, reliable shift contexts, physical-arrow lock, simultaneous
  game chords, direct Backspace/DROP handling, Alt+Esc warm start, Ctrl+Esc
  cold reset, and graceful short-power-button shutdown.
- Preserves calculator state, variables, modes, active card data, battery
  status, and attached libraries across the supported save, OFF, and warm-start
  workflows.
- Adds exact revision-R ROM validation for calculator/CAS operations, matrices,
  multi-function 2D plots, 3D parametric surfaces, SD transfers, card/library
  behavior, graphics, sound, input chords, and long-running native programs.
- Confirms Phoenix, TetrisGX from Port 1, Pac Man GX, Frog48, Android 2.01, and
  1st Demo as positive compatibility and regression targets.

Known compatibility limits:

- WarioLandHP is not a supported target. Version 3.1 reaches a dense corrupted
  terminal frame and stops; the version 2.0 grayscale path has also proved
  device-sensitive and is not used as a release acceptance test.
- AstroNUT remains halt-free in the exact-ROM core but has severe physical LCD
  glitches and is unplayable on the tested PicoCalc.
- Mission: Impossible reports `Insufficient Memory` even with the tested Port 1
  configuration and is not a supported compatibility target.
- Other third-party machine-language games or applications may expose timing,
  memory-layout, display, or unsupported peripheral assumptions. Serial/Kermit,
  infrared, simultaneous Port 1 and Port 2 cards, and plain-text UserRPL
  compilation are outside the 2.0.0 feature set.

## 1.1.0-alpha.dev.6 - 2026-08-23

- Allows independently held HP matrix keys instead of forcibly releasing the
  previous key on every new press. Android's `ENTER` plus locked-arrow digging
  controls now reach the ROM as real simultaneous chords, while each key
  retains the minimum hold time that prevents missed quick taps.
- Makes physical Backspace more reliable as the HP `DROP` game command while
  arrow lock is enabled. Its initial press remains immediate, but a quick tap
  is held for 300 ms so native presentation loops cannot miss it between
  effects.
- Collapses both temporal-grayscale planes immediately when a hinted frame is
  a full-screen scene cut, and expires the pair when its program stops
  alternating. Android's presentation/game-over screen no longer keeps a
  stale gameplay plane as a persistent background artifact.
- Adds focused independently-held-key and grayscale-to-monochrome regressions,
  plus an exact-ROM Android profile that exercises all movement directions,
  simultaneous dig chords, `ON`, and `DROP` without a fatal halt.
- Makes a viewable exact-ROM graphics/audio validation MP4 a required private
  hardware-test artifact. Optional host capture now records changed LCD frames
  and beeper edges, and a reproducible video builder labels and packages the
  Android control/scene test with the 1st Demo graphics/sound sequence.
- Adds a deterministic exact-ROM calculator profile covering polynomial
  factoring, symbolic differentiation/integration/solving, 2x2 inverse and
  multiplication, four-function plotting, and HP's 3D parametric-surface
  example. Labeled capture markers and a separate reproducible calculator/CAS
  MP4 make those results inspectable before hardware flashing.

## 1.1.0-alpha.dev.5 - 2026-08-23

- Moves the dedicated warm-start shortcut from Right Shift+Esc to Alt+Esc.
  The PicoCalc controller already uses Right Shift+Esc for the calculator's
  `BRK` action, so that chord now passes through unchanged. Ctrl+Esc remains
  the separate saved-state-clearing cold reset.
- Corrects the Mission: Impossible compatibility result. The earlier harness
  invoked its first `INFOS.IMP` softkey and mistook help/error-screen activity
  for a running game. The corrected test invokes `IMP` with a real level
  `Library Data` object and reproduces `Insufficient Memory` on clean built-in
  RAM and after merging the 128 KiB Port 1 card. Mission: Impossible is
  withdrawn as a recommended compatibility test.
- Extends the exact-ROM library harness with explicit setup-softkey and
  command-softkey selection so future tests cannot silently assume that a
  library's first command launches its game.

## 1.1.0-alpha.dev.4 - 2026-08-23

- Adds physical Right Shift+Esc as a dedicated non-destructive warm-start
  shortcut. It sends HP teal/right-shift then ON (the `OFF` command), waits
  for the ROM to turn its LCD off, and sends an ordinary ON to wake it.
  RAM, variables, and the active virtual card are preserved; Ctrl+Esc remains
  the separate saved-state-clearing cold reset.
- Records the hardware result of the Wario investigation: although Wario is no
  longer a practical target, the resulting timer, display, pacing, and Port 1
  corrections make Phoenix and Tetris run correctly and smoothly. Their
  current behavior is now a regression baseline for subsequent changes.

## 1.1.0-alpha.dev.3 - 2026-08-23

- Fixes the extreme WarioLandHP 2.0 slowdown shown by the alpha.dev.2
  physical recordings. Persistent A/B composition stopped the raw-plane
  flashes, but some grayscale frames still spent nearly a complete 42 ms
  transfer with Saturn execution and LCD scan polling entirely paused.
- Converts the proven single-core PIO display path into a cooperative dirty-row
  queue. It sends at most one scaled HP source-row span per emulator loop,
  allowing 256 Saturn instructions plus input, sound, timers, and scan-counter
  polling to run between bounded panel transfers.
- Keeps the stable 25 MHz PIO byte transport and newest-frame queue without
  restoring the DMA, second-core worker, inter-core lock, or abort paths that
  previously froze hardware. Full and palette-changing frames remain correct,
  but are drained incrementally instead of as one blocking operation.

## 1.1.0-alpha.dev.2 - 2026-08-23

- Fixes the remaining WarioLandHP 2.0 physical display failure seen in the
  alpha.dev.1 hardware video. The game was running, but repeated captures of
  one temporal bitplane caused the mountain/background and foreground planes
  to replace one another for several panel refreshes at a time.
- Latches two independently tracked grayscale planes after scan-counter or
  A/B/A evidence is established. Each new capture updates the nearest plane,
  while every physical refresh continues to render the stable combined pair;
  two consecutive A updates can no longer be mistaken for an A/A pair.
- Adds a focused panel-queue regression covering repeated hinted same-plane
  updates, delayed capture, the next opposite-plane update, absence of raw
  one-bit refreshes while latched, and return to ordinary rendering after the
  game stops producing grayscale evidence.

## 1.1.0-alpha.dev.1 - 2026-08-23

- Corrects the exact-ROM library test so it installs, warm-starts, opens the
  ROM's LIBRARY catalog, selects the library, and invokes its first command.
  The old direct ROMPTR shortcut never launched Wario and made dev.32's Wario
  frame/audio counts false positives from HOME and error-screen activity.
- Confirms with the corrected path that WarioLandHP 2.0 reaches actual
  gameplay and continuously produces a clean alternating A/B/A LCD sequence
  in the core. WarioLandHP 3.1 instead reproduces the bad terminal screen and
  stops producing LCD changes; it remains a known incompatibility.
- Keeps established grayscale pairing alive across bounded panel-transfer
  gaps and raises grayscale panel presentation from 12.5 to 20 frames per
  second. This prevents an occasional missed A/B/A sample from exposing one
  raw bitplane as a title-screen flash.
- Implements the GX's read-only LINECOUNT-high mirrors at `0x12A` through
  `0x12D`, and honors DA19 when the 512 KiB GX ROM is addressed. Focused host
  regressions cover both hardware behaviors.
- Adds a Port 0 option to the exact-ROM library harness so author-required
  main-RAM installations can be distinguished from Port 1/2 behavior.
- Removes the unsuccessful forced-grayscale Wario compatibility override. It
  made the corrupted display opaque but did not fix WarioLandHP 3.1, while
  the preceding physical WarioLandHP 2.0 build remained stuck on a flickering
  title screen.
- Simplifies LCD line-counter synchronization and removes unused UI and
  keyboard compatibility wrappers left behind by earlier experiments.
- Makes the exact-ROM host harness use deterministic instruction-driven time,
  eliminating workstation wall-clock variance from game and display tests.
- Removes temporary diagnostics and stale generated build/output artifacts.
- Starts the 1.1.0 alpha series; this milestone supersedes the planned
  `1.1.0-dev.33` name.

## 1.1.0-dev.32 - 2026-08-22

Thirty-second development prototype, incorporating the twenty-eighth physical
PicoCalc test.

- Records dev.31 as a failed WarioLandHP hardware test: version 3.1 still
  hung with the same garbled screen. Inspection of the original Wario source
  identified its direct polling of the GX LCD line counter as the missing
  hardware behavior, rather than another generic frame-rate problem.
- Replaces x48's read-count LCD-register stub with the GX controller's 4096 Hz,
  six-bit scan-line down-counter. The two register nibbles now form one
  coherent sample, the counter follows display enable/disable, and native
  writers are captured at the next complete 64-line scan boundary.
- Adds `Ctrl+F6` as a session-only forced-grayscale compatibility switch for
  interrupt-driven games whose plane synchronization cannot be inferred from
  ordinary frame differences. Automatic mode remains the default and the
  switch is reset at every boot.
- Extends the exact revision-R harness with sub-scanline wall-time steps and
  deliberately held presentation input. Wario 3.1 now completes a 30-second
  direct-library run with roughly 760 changed frames and 16,170 audio edges;
  Wario 2.0, Pac Man GX, Phoenix, and the deterministic host suite remain
  halt-free. **Correction:** alpha.dev.1 found that this direct-ROMPTR test
  never entered Wario; those Wario counts were HOME/error-screen activity.

## 1.1.0-dev.31 - 2026-08-22

Thirty-first development prototype, incorporating the twenty-seventh physical
PicoCalc test.

- Treats WarioLandHP 3.1's garbled physical output as a failed grayscale
  test, despite its halt-free exact-ROM execution. Continuous full-screen
  writers are now captured after a complete visible-memory sweep instead of
  only at an arbitrary maximum-age point that can split a frame between old
  and new contents.
- Recognizes the high-difference A/B/A bitplane signature used by HP grayscale
  games and composites the two planes into paper, middle, and ink shades on
  the ILI9488. The same-plane similarity requirement keeps ordinary animation
  on the newest-frame path, avoiding the old muddy sprite trails.
- Raises both reconstructed HP audio and the startup reference beep from 30%
  to 60% of the dev.5 PWM swing, without changing the 2.6 kHz reconstruction.
- Removes the firmware's 600 ms HP-LCD stability pause and 350 ms confirmation
  display before the final PMU request. The official keyboard BIOS still
  enforces its own six-second minimum PMU countdown; eliminating that hardware
  controller delay requires a separately flashed keyboard BIOS.
- Makes the post-import persistence hint follow the active virtual card:
  `STO :1: NAME` in `PORT1.MODE`, otherwise `STO :2: NAME`.
- Extends the exact-ROM game harness with direct Wario `ENTER/G/H/K/N` input,
  display-page diagnostics, and deterministic grayscale presentation tests.
  Phoenix, Pac Man GX, TetrisGX, Wario 2.0, and the complete host suite remain
  free of fatal core halts.

## 1.1.0-dev.30 - 2026-08-22

Thirtieth development prototype, incorporating the twenty-sixth physical
PicoCalc test.

- Fixes the low-pitched ROM chirp that remained in dev.29. Hardware proved
  that direct speaker transitions were still being grouped inside each
  256-instruction Saturn batch and separated by the pacer's sleep, so the
  physical speaker reproduced the batching envelope instead of the HP square
  wave.
- Reconstructs calculator-time pitch from the exact decoded-instruction span
  between OUT-bit transitions, then emits a continuous waveform from a 25 kHz
  asynchronous timer. The 97-instruction revision-R diagnostic half-cycle is
  now synthesized at approximately 2.58 kHz regardless of LCD transfer time
  or the selected host throughput. A 20 ms edge hold bridges core batches
  without blocking Saturn execution.
- Keeps the previously hardware-selected 30% stereo amplitude. The 2.6 kHz
  startup diagnostic now uses the same asynchronous output path as ROM sound,
  making it a direct reference for the reconstruction rather than a separate
  blocking implementation.
- Adds a deterministic sound-timing model and rollover/silence/clamping tests.
- Tests three additional unmodified hpcalc.org objects against the exact
  revision-R ROM: `pacmangx.dir` runs its assembly hardware-scrolling path for
  20 seconds with numeric controls and more than 500 changing frames; the GX-converted
  Frog48 library 1256 installs and launches from Port 1; and AstroNUT runs for
  20 seconds with more than 180 changing frames. None enters the debugger or fatal RPL
  loop.
- Documents how to detach and purge TetrisGX library 900 from Port 1 so it no
  longer auto-attaches to HOME at boot.

## 1.1.0-dev.29 - 2026-08-22

Twenty-ninth development prototype, incorporating the twenty-fifth physical
PicoCalc test.

- Identifies the remaining TetrisGX 3.0 failure as covered-port execution, not
  TIMER2 or display timing. The exact 8021-byte `TETRISGX.LIB` consistently
  enters revision R's invalid four-instruction RPL-stream loop from Port 2;
  the same imported object, ROM, command, and 30-second run avoid that path on
  a non-covered Port 1 card.
- Adds an optional SD-backed 128 KiB Port 1 compatibility mode for older
  machine-language libraries. If `HP48GX/PROGRAMS/PORT1.MODE` exists at boot,
  the firmware mounts and persists `PORT1.CRD`/`.NEW`/`.BAK`; otherwise the
  established `PORT2.CRD` workflow is unchanged. Only one virtual card is
  mounted at a time, reusing the existing packed 128 KiB buffer rather than
  consuming a second RP2350 allocation.
- Converts every Port 1 read, write, and CRC path in the inherited GX memory
  controller to the packed-card accessor, and adds reset, mapping, persistence,
  recovery, and exact Tetris library launch regressions for the new slot.
- Raises the hardware default from 125k to the subsequently validated 500k
  decoded instructions per second. The ROM diagnostic waveform measured at
  97 instructions per half-cycle therefore moves from roughly 644 Hz to
  2.58 kHz, matching the requested high beep instead of a low chirp. The
  startup diagnostic is likewise 2.6 kHz, while the hardware-tested 30%
  amplitude remains unchanged.
- Keeps all slower and faster live speed presets, Port 2 storage, Ctrl+Esc
  recovery, and the stable single-core display/timer implementation.

## 1.1.0-dev.28 - 2026-08-22

Twenty-eighth development prototype, incorporating the twenty-fourth physical
PicoCalc test.

- Fixes the TIMER2 reconciliation failure exposed by TetrisGX 3.0. A trace of
  the exact `TETRISGX.LIB` showed that gameplay repeatedly programs and polls
  the low TIMER2 nibbles; it does not poll the LCD line counter. The simplified
  PicoCalc timer port could replace the game's short programmed deadline with
  an older, larger ACCESSTIME-derived value at the next scheduler adjustment,
  trapping the game before startup or its next block.
- Restores x48's essential monotonic down-counter rule with explicit 32-bit
  wraparound ordering. Wall-clock reconciliation may advance TIMER2, but can
  never move it backward or overwrite a newer native-game deadline. When the
  instruction estimate is already ahead, its tick interval is slowed toward
  real time as intended.
- Adds a deterministic regression that programs a short TIMER2 value against
  an older calculator clock, runs beyond a scheduler adjustment, and proves
  the programmed countdown is never replaced by the stale larger value.
- Removes the extra 8 kHz busy-wait previously inserted at every HP speaker
  edge. Paced Saturn execution and timer waits already provide the waveform's
  timing; counting it twice made ROM diagnostic beeps low and chirpy and also
  blocked the emulation loop during sound-heavy native code. Stereo amplitude
  remains at the hardware-tested 30% level.
- Retains dev.27's uncapped speed selector and dev.26's stable single-core,
  bounded 25 MHz PIO dirty-span display path. The complete host suite and the
  exact TetrisGX ROM-R timer trace complete without a core halt; physical
  gameplay remains the decisive verification for this hardware-timing fix.

## 1.1.0-dev.27 - 2026-08-22

Twenty-seventh development prototype, incorporating the twenty-third physical
PicoCalc test.

- Removes the 500k IPS ceiling exposed after dev.26 became stable and playable.
  Hardware showed progressively smoother animation through 500k without a
  corresponding increase in game speed, indicating that 500k had become a
  throughput cap rather than a useful upper timing limit.
- Replaces integral microseconds-per-instruction pacing with exact IPS rates
  and a carried fractional numerator. This supports sub-microsecond rates
  without cumulative rounding drift.
- Adds 667k, 1M, 1.5M, and 2M IPS steps above 500k. One final `MAX` setting
  disables pacing completely so hardware can establish its natural ceiling.
- Retains the 256-instruction execution quantum in every mode, including MAX,
  so keyboard polling, Ctrl+Left/Right, sound, timers, and display capture are
  still serviced frequently.
- Keeps 125k as the boot default and preserves all existing 20k-500k steps.
  The footer reports the rounded selected rate or `CPU MAX`.
- Adds deterministic fractional-rate, no-drift, and unpaced regressions. The
  complete host suite and firmware cross-build pass.

## 1.1.0-dev.26 - 2026-08-22

Twenty-sixth development prototype, incorporating the twenty-second physical
PicoCalc test.

- Reverts dev.25's entire second-core display design. Hardware sometimes
  passed the memory-recovery screen, but usually froze there, and surviving
  boots produced markedly low-pitched sound. The LCD, Saturn, keyboard, and
  sound now run on one core again with no multicore worker, inter-core mutex,
  flash lockout, LCD DMA, or DMA abort.
- Retains the stable bounded 25 MHz PIO byte transport, but replaces full-frame
  game writes with synchronous dirty spans. Each changed 131-pixel source row
  finds its actual first and last changed pixel and transfers only that scaled
  rectangle; unchanged rows and margins produce no panel traffic.
- Keeps complete 300x146 output for the first frame and palette changes. Even
  that worst-case blocking transfer has bounded PIO FIFO waits and cannot trap
  the firmware behind a cross-core bus lock.
- Raises the pacing lag window from 20 ms to 75 ms, covering the approximately
  42 ms worst-case panel frame. The Saturn repays that bounded display time so
  animation and ROM speaker pitch retain the selected average rate; genuinely
  long storage, flash, UI, or SHUTDN stalls still rebase timing.
- Reduces static SRAM to 407,520 bytes. The firmware cross-build and complete
  host suite pass after the rollback.

## 1.1.0-dev.25 - 2026-08-22

Twenty-fifth development prototype, incorporating the twenty-first physical
PicoCalc test.

- Removes the experimental LCD DMA path completely. Hardware showed a total
  firmware freeze—not merely a stopped game or stale panel—and the Pico SDK's
  RP2350 DMA abort routine contains its own unbounded wait for the channel BUSY
  bit. That made dev.23's recovery watchdog capable of becoming the deadlock.
- Restores the stable, blocking PIO panel transport, still at the PicoCalc
  reference rate of 25 MHz, and runs complete HP framebuffer transfers on the
  RP2350's second core. Saturn execution, input, sound, and 125k IPS pacing
  remain on core 0 and never wait for a game frame.
- Serializes ordinary UI drawing and core-1 framebuffer drawing with a
  cross-core LCD-bus mutex. PIO FIFO waits remain bounded and restart the
  transport on timeout; there is no LCD DMA channel or abort operation.
- Registers the display worker as a multicore flash-lockout victim and parks
  it before state erase/program operations, preserving safe Save-and-OFF and
  state-clear behavior while XIP is unavailable.
- Reduces static SRAM use from dev.24's 437,448 bytes to 410,716 bytes by
  removing the 28.8 KiB DMA staging buffer. The complete host suite plus ROM-R
  Port 2, library/PINIT, RPL round-trip, Kahla, and 15-second Phoenix
  regressions pass.

## 1.1.0-dev.24 - 2026-08-22

Twenty-fourth development prototype, incorporating the twentieth physical
PicoCalc test.

- Removes the remaining unbounded display wait exposed by dev.23's changed DMA
  scheduling. At the end of every command and frame, the driver cleared the
  PIO `TXSTALL` flag and waited forever for it to be asserted again; when the
  state machine was already stalled on an empty FIFO, that transition was not
  guaranteed and the entire firmware could stop after an arbitrary frame.
- Replaces the `TXSTALL` wait with a bounded FIFO drain plus a fixed two
  microsecond output-shifter allowance. A byte takes less than 0.4 us at the
  current panel rate.
- Bounds both full-FIFO writes and empty-FIFO drains at five milliseconds. On
  timeout, the driver clears and restarts the PIO transport instead of blocking
  the Saturn, input, and sound forever.
- Keeps dev.23's deferred ten-second battery redraw and per-DMA-chunk watchdog,
  so every panel-side wait now has a recovery path.
- Retains the hardware-calibrated 125k IPS default and the complete 20k-500k
  preset range.

## 1.1.0-dev.23 - 2026-08-22

Twenty-third development prototype, incorporating the nineteenth physical
PicoCalc test.

- Sets the default Saturn rate to 125k decoded instructions per second, the
  midpoint of the repeatedly observed 100k-150k range that matches real
  Phoenix timing. All 20k-500k calibration presets remain available.
- Removes a synchronous LCD operation from the only periodic task at the
  repeatable failure boundary: the battery header refresh runs every ten
  seconds and previously forced an in-progress game-frame DMA transfer to
  drain inside the keyboard poll. It now marks the header dirty and draws it
  only after the panel becomes naturally idle.
- Adds a 25 ms watchdog to each approximately 2.3 ms display DMA chunk. A
  genuinely wedged transfer is aborted and its cached image invalidated so the
  newest queued frame can retry instead of stopping the calculator forever.
- Extends the real revision-R Phoenix regression through 15 seconds of virtual
  wall time at 100k IPS with 60 movement/weapon transitions. The run continues
  rendering after the physical failure window and never enters the debugger
  or halts.
- Adds a deterministic busy-panel regression proving that a battery update is
  deferred until DMA is idle.

## 1.1.0-dev.22 - 2026-08-22

Twenty-second development prototype, incorporating the eighteenth physical
PicoCalc test.

- Replaces dev.18's OR accumulation of every queued frame with a newest-frame
  queue. Dev.21's continuous captures exposed that accumulation as heavy
  trails: moving sprites, weapons, and old backgrounds were merged into one
  muddy bitmap while the panel was busy.
- Raises the PIO display link from the original conservative 10 MHz to the
  25 MHz rate used by ClockworkPi's PicoCalc LCD examples. A complete scaled HP
  frame now takes roughly 42 ms instead of 105 ms, and panel starts are paced
  at 50 ms instead of 125 ms.
- Extends live calibration below the too-fast 100k floor. Dev.22 starts at 50k
  decoded instructions per second and exposes 20k, 25k, 33k, 40k, 50k, 66k,
  83k, 100k, 125k, 166k, 200k, 250k, 333k, and 500k presets.
- Reduces each unpaced Saturn burst from 4,096 to 256 decoded instructions so
  wall-clock timers, sound, input, and framebuffer capture are interleaved at
  millisecond scale even at the new lower rates.
- Updates the queued-display and default pacing regressions; the complete host
  suite and real ROM-R game launch tests remain enabled.

## 1.1.0-dev.21 - 2026-08-22

Twenty-first development prototype, incorporating the seventeenth physical
PicoCalc test.

- Fixes the intermittent native-game display revealed after CPU pacing made
  Phoenix's sound run at the correct rate. The continuously written
  framebuffer could previously wait 262,144 decoded instructions—about 1.31
  seconds at 200k IPS—before a snapshot was queued.
- Completes a framebuffer snapshot after two milliseconds of real-time write
  quiet or after a maximum pending age of 40 milliseconds. The instruction
  thresholds remain as a deterministic fallback for unpaced host tests.
- Services pending snapshots inside the pacer's one-millisecond waits, so the
  display can capture a complete game frame while the Saturn is deliberately
  idle without changing CPU or sound timing.
- Adds host regressions for both a completed frame and a continuously written
  frame that can only be captured by the wall-time deadline.

## 1.1.0-dev.20 - 2026-08-22

Twentieth development prototype, incorporating the sixteenth physical
PicoCalc test.

- Replaces dev.19's still-too-fast 500,000 decoded-instruction rate with a
  conservative 200,000-instruction default. A real GX's approximately 4 MHz
  oscillator does not execute one decoded instruction per clock; measured
  display-memory operations commonly require roughly 18–36 processor cycles.
- Adds live native-game calibration presets. `Ctrl+Left` slows the Saturn and
  `Ctrl+Right` speeds it up through 100k, 125k, 166k, 200k, 250k, 333k, and
  500k decoded instructions per second. The footer reports the selected rate,
  and changing it starts a fresh timing epoch without a catch-up burst.
- Extends deterministic pacing tests across the 200k default, live rate
  changes, accumulated deadlines, and long-stall recovery. Display DMA,
  keyboard service, and ROM-R game regressions remain enabled.

## 1.1.0-dev.19 - 2026-08-22

Nineteenth development prototype, incorporating the fifteenth physical
PicoCalc test.

- Restores x48's intended speed emulation in the native PicoCalc loop: each
  decoded Saturn instruction receives a two-microsecond budget, capping active
  execution at approximately 500,000 instructions per second instead of the
  many-million-instruction unpaced rate exposed by dev.18's display fix.
- Discards more than 20 ms of accumulated pacing debt after a blocking
  operation or SHUTDN interval, preventing visible and audible catch-up bursts.
  Pacing waits use one-millisecond slices so display DMA and keyboard input
  remain serviced.
- Moves scaled HP framebuffer transfers to PIO-paced DMA in eight-row chunks.
  The Saturn, reconstructed audio, and controls continue between chunks rather
  than pausing for approximately 105 ms on every complete 10 MHz panel frame.
- Adds deterministic pacing and busy-panel queue regressions while retaining
  the ROM-R Phoenix, Kahla, Port 2, prefix, import, and sound coverage.

## 1.1.0-dev.18 - 2026-08-22

Eighteenth development prototype, incorporating the fourteenth physical
PicoCalc test.

- Restores the display-RAM invalidation path required by native Saturn games.
  Writes through `disp_draw_nibble` and `menu_draw_nibble` now queue complete
  framebuffer snapshots after a quiet interval, with a bounded fallback for
  continuously rendered animation.
- Decouples the slow 10 MHz ILI9488 transfer from emulator display-register
  activity. ROM snapshots are accumulated in RAM and the physical panel is
  paced to one frame start every 125 ms, so graphics can execute while the
  previous panel frame remains visible.
- Combines transient clear/draw and rapidly alternated grayscale planes before
  each panel update, then follows with the newest clean plane to prevent stale
  ghost pixels on ordinary calculator screen transitions.
- Adds regressions for direct active-framebuffer writes, display-plane
  accumulation, panel pacing, clean-frame recovery, and the exact ROM-R
  `PHOENIX.GX` graphics-and-sound launch path.

## 1.1.0-dev.17 - 2026-08-22

Seventeenth development prototype, incorporating the thirteenth physical
PicoCalc test.

- Adds a latched `Ctrl+Space` physical-arrow mode. When enabled, PicoCalc
  Left/Down/Right/Up send the HP 48GX `<`, `V`, `>`, and `^` matrix keys;
  pressing `Ctrl+Space` again returns them to on-screen cursor navigation.
- Shows a persistent teal `ARR` footer marker while physical-arrow mode is
  locked, independent of later status messages. Toggle messages spell out the
  active behavior.
- Documents physical `Esc` as HP `ON/CANCEL`, including exiting Kahla.
- Adds a revision-R ROM launch regression for the original hpcalc.org
  `PHOENIX.GX` shooter. Three consecutive runs imported and launched the
  program, exercised movement, phaser, torpedo, and shield input, observed
  changing graphics and sustained sound transitions, and did not halt.

## 1.1.0-dev.16 - 2026-08-22

Sixteenth development prototype, incorporating the twelfth physical PicoCalc
test.

- Corrects dev.15's claim that `EVAL` opens a raw directory object on stack
  level 1. HP 48 directories must be stored under a global name and entered
  through `VAR`; `EVAL` on the raw directory is correctly a no-op.
- Detects directory objects after Ctrl+F8 and replaces the generic Port 2
  footer with `DIR L1: 'NAME' ENTER STO; VAR -> NAME`.
- Replaces the false directory-open assertion with the complete real-ROM Kahla
  workflow: import, survive another key, enter `'KAH'`, `STO`, open `VAR`,
  enter `KAH`, launch `Kala`, accept its form, render the board, and produce
  sound without halting.

## 1.1.0-dev.15 - 2026-08-22

Fifteenth development prototype, incorporating the eleventh physical PicoCalc
test.

- Replaces dev.14's false-positive `NXT` redraw workaround. Ctrl+F8 now queues
  the transfer until the stock ROM is stopped in its `SHUTDN` idle loop, then
  commits the RPL object and wakes the calculator only after its stack pointers
  are complete. The ROM can no longer restore its pre-import empty stack.
- Adds a real revision-R ROM regression using the exact `KAHLA.48G` file. It
  proves stack depth changes from 0 to 1, the main stack display changes, and
  the object survives a subsequent ordinary ROM key. Its `EVAL` assertion was
  incomplete and is superseded by dev.16's actual installation/launch test.
- Makes a short press of the physical PicoCalc power button initiate the same
  graceful save, verified HP OFF, storage unmount, and PMU power-off sequence
  as the on-screen OFF key. The controller's long press remains the emergency
  hardware shutdown.

## 1.1.0-dev.14 - 2026-08-22

Fourteenth development prototype, incorporating the tenth physical PicoCalc
test.

- Attempted to fix successful Ctrl+F8 imports disappearing before they become visible.
  The former synthetic ON/ATTN redraw could abort to the ROM's pre-import data
  stack pointer, effectively discarding the newly inserted object.
- Used a held `NXT` action to redraw the stack, but the test only observed a
  menu change and failed to prove that the imported object survived ROM
  execution. Dev.15 supersedes this workaround and its false-positive test.

## 1.1.0-dev.13 - 2026-08-22

Thirteenth development prototype, incorporating the ninth physical PicoCalc
test.

- Verifies the exact 8,021-byte hpcalc.org `TETRISGX.LIB` through the real RPL
  importer. Its `HPHP48-D` header and odd-nibble `DOLIB` payload are valid.
- Replaces the generic alternate-header regression with the exact
  `TETRISGX.LIB` filename and `HPHP48-D` signature reported from hardware.
- Adds an optional real-file path to the core smoke test so downloaded HP48
  objects can be validated without checking copyrighted programs into source.

## 1.1.0-dev.12 - 2026-08-22

Twelfth development prototype, incorporating the eighth physical PicoCalc
test.

- Fixes rejection of genuine hpcalc.org/serial-compatible HP48 binaries. The
  eighth byte in `HPHP48-x` is producer/ROM-revision metadata, not a calculator
  model; the importer no longer incorrectly accepts only `HPHP48-W`.
- Retains the strict `HPHP48-` signature, bounded streaming, and complete RPL
  object validation, so broad revision support does not admit arbitrary files.
- Adds a regression importing a valid `HPHP48-R` object alongside the existing
  `HPHP48-W` round-trip coverage.

## 1.1.0-dev.11 - 2026-08-22

Eleventh development prototype, incorporating the seventh physical PicoCalc
test.

- Guarantees every HP action a minimum 75 ms matrix down-time, so a quick
  PicoCalc press/release cannot pass between Saturn keyboard scans.
- Holds every synthetic Alpha/purple/teal prefix across multiple input polls
  before release, then retains dev.10's annunciator check and bounded retries.
- Changes the inbox footer to distinguish Ctrl+F7 selection from Ctrl+F8 stack
  import and to direct users to `STO :2: NAME` for Port 2 persistence.
- Documents that Ctrl+F10 or Save-and-OFF is required to export the modified
  emulated card back to `PORT2.CRD` on the SD card.

## 1.1.0-dev.10 - 2026-08-22

Tenth development prototype, incorporating the sixth physical PicoCalc test.

- Makes the HP ROM annunciator authoritative for contextual actions. If Alpha,
  purple, or teal is shown but the ROM has not latched it, the firmware sends
  an ordered prefix and waits for the matching annunciator before the action.
- Labels purple/left-shift `2` as `PORTS`; it opens the management soft menu
  beginning with PORTS. Teal/right-shift `2` remains LIBRARY.
- Corrects direct backup-name instructions to include Space between the port
  field and name during keyboard entry (`:2: TEST`).
- Removes the dev.9 direct-recall probe after hardware feedback exposed its
  false positive: it had observed the pre-existing stack object and dirty bit.

## 1.1.0-dev.9 - 2026-08-22

Ninth development prototype, incorporating the fifth physical PicoCalc test.

- Delivers drawn ALPHA, purple-shift, and teal-shift presses to the HP ROM when
  their context is selected. Context navigation no longer risks sending a bare
  base key such as COS when the overlay displays `T`.
- Documents direct Port 2 backup-name entry; dev.10 corrects that workflow from
  subsequent physical-device feedback.

## 1.1.0-dev.8 - 2026-08-21

Eighth development prototype, incorporating the fourth physical PicoCalc test.

- Fixes Caps/Alpha divergence by atomically deriving the `TXT`/`NAV` marker,
  context header, status message, and drawn legends from the keyboard's actual
  text-lock and pending-prefix state.
- Reads the STM32 keyboard controller's Caps Lock bit on physical Caps presses
  and every 250 ms while hardware Caps owns the lock. This catches automatic
  controller lock changes instead of leaving a stale emulator-only Alpha view.
- Fixes four-way selection of the wide ENTER key. Vertical movement now chooses
  the nearest row first, making Up from `7` or ALPHA select ENTER; Space then
  sends ENTER, purple EQUATION, or teal MATRIX according to the drawn context.
- Adds a host navigation test covering reachability of ENTER and all three of
  its context labels.

## 1.1.0-dev.7 - 2026-08-21

Seventh development prototype, incorporating the third physical PicoCalc test.

- Removes the shifted-menu display stall by caching the last HP bitmap and
  transmitting only changed source rows to the scaled ILI9488 region. A soft
  menu redraw now sends roughly one eighth of the former pixel payload, while
  annunciator-only changes send no unchanged HP pixels.
- Tracks entry into the purple/left-shift Port tools and its second page, then
  confirms F2 with `PINIT SENT - P2 READY; COMMAND IS SILENT` (or reports that
  no Port 2 card is attached).
- Extends the real revision-R ROM test through F2/PINIT and proves that the ROM
  touches the attached writable card. The test also documents the stock ROM's
  intentional behavior: PINIT has no stack result and an already-empty card
  can retain the same final image checksum.

## 1.1.0-dev.6 - 2026-08-21

Sixth development prototype, incorporating the second physical PicoCalc test.

- Clarifies and tests the HP ROM's two distinct shifted `2` functions:
  teal/right-shift opens the attached-library catalog (correctly blank when no
  library is attached), while purple/left-shift opens the Port/Library tools
  whose next page contains `PINIT`.
- Shows a footer hint for each shifted `2` path so a blank catalog is not
  mistaken for a failed key or undetected RAM card.
- Reduces both reconstructed HP sound and the startup diagnostic to 30% of
  dev.5's output amplitude while preserving the 8 kHz ROM pacing and stable
  880 Hz startup tone.
- Extends the real revision-R ROM test through purple/left-shift → `2` → `NXT`
  and verifies that both the Port tools and `PINIT` menu pages are non-empty.

## 1.1.0-dev.5 - 2026-08-21

Fifth development prototype toward 1.1.0, incorporating the first physical
PicoCalc feedback from dev.4.

- Fixes drawn and physical prefix actions by delivering an ordered prefix tap
  with Saturn execution between prefix-down, prefix-up, and action-down. This
  makes teal/right-shift → `2` open the HP `LIBRARY` menu instead of sending an
  ineffective simultaneous matrix chord.
- Adds a real revision-R ROM test proving that the teal annunciator latches and
  teal → `2` changes the HP soft menu to `LIBRARY`.
- Reconstructs short ROM speaker OUT runs at x48's 8 kHz byte-stream rate so
  HP `BEEP` waveforms are not compressed into RP2350-speed chirps. Long static
  keyboard-scan runs are coalesced so audio pacing does not slow normal boot.
- Replaces the artificial startup chirp with a stable 880 Hz, 140 ms diagnostic
  beep before handing the speaker path to the HP ROM.
- Adds host-backed Port 2 persistence tests covering first-card creation, dirty
  saves, backup rotation, complete and incomplete interrupted-write recovery,
  corrupt-current preservation, and operation without an SD card.

## 1.1.0-dev.4 - 2026-08-21

Fourth development prototype toward 1.1.0.

- Runs the stock revision-R `=GARBAGECOL` routine once when an individual-file
  import does not initially fit, before reporting `HP MEMORY FULL`.
- Executes garbage collection as a bounded transaction and restores the exact
  Saturn registers, hardware return stack, scheduler position, and debugger
  state while retaining only the intended HP RAM compaction.
- Adds a real-ROM test that creates an unreachable valid temporary object,
  verifies that collection reclaims it, and confirms the level-1 object still
  exports byte-for-byte correctly.
- Adds a host-backed FatFs integration test covering deterministic file
  selection, wrapping, header stripping, streaming import, numbered exports,
  an empty inbox, and preservation of existing `.48G` and `.NEW` files.
- Adds the deterministic `samples/PICOCALC.48G` import/export hardware test and
  generator while retaining all dev.3 inbox/outbox, Port 2, and sound features.

## 1.1.0-dev.3 - 2026-08-21

Third development prototype toward 1.1.0.

- Adds individual standard HP 48 binary-object import from
  `/HP48GX/PROGRAMS/INBOX` directly onto stack level 1.
- Adds non-overwriting stack-level-1 export to numbered
  `/HP48GX/PROGRAMS/OUTBOX/STKnnn.48G` files.
- Adds Ctrl+F7 to cycle the selected inbox file, Ctrl+F8 to import it, and
  Ctrl+F9 to export stack level 1. Ctrl+F10 remains full state/card save.
- Streams objects between SD and HP memory, avoiding a second large RP2350 RAM
  buffer. Invalid imports roll back the temporary-object allocation safely.
- Preserves dev.2's writable Port 2 image workflow and dev.1's stereo sound.

## 1.1.0-dev.2 - 2026-08-21

Second development prototype toward 1.1.0.

- Adds a writable 128 KiB HP 48GX Port 2 RAM card backed by
  `/HP48GX/PROGRAMS/PORT2.CRD` on the PicoCalc SD card.
- Imports the standard packed x48-compatible card image at boot and exports
  calculator changes on Ctrl+F10 and Save-and-OFF.
- Creates a blank card image on first use. The HP ROM can initialize it with
  `PINIT`, after which programs and other objects can be stored as native Port
  2 backup objects such as `:2:NAME`.
- Uses `PORT2.NEW` and `PORT2.BAK` for recoverable updates. An invalid existing
  `PORT2.CRD` is never overwritten, and an SD write failure cancels OFF.
- Keeps dev.1 stereo sound and its startup chirp enabled.

## 1.1.0-dev.1 - 2026-08-21

First development prototype toward 1.1.0.

- Enables the PicoCalc stereo PWM audio path on the official GP26/GP27 pins
  and routes the HP 48GX one-bit speaker output to both speakers.
- Plays a brief startup chirp so physical testing can confirm the audio path
  independently of an HP sound command.
- Mounts an optional FAT32 or exFAT SD card on the official SPI0 pins and
  creates `/HP48GX/PROGRAMS/README.TXT` without formatting, deleting, or
  overwriting existing files.
- Reports SD readiness or a specific card/mount/folder failure in the footer;
  calculator operation continues normally without an SD card.
- Adds the pinned RP2350-compatible FatFs/SD SPI dependency. Actual HP object
  import/export remains planned for later 1.1 prototypes.

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
