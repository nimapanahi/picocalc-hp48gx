# Contributing

Thank you for helping improve the HP 48GX emulator for PicoCalc.

## Development workflow

1. Open an issue for user-visible changes or hardware-specific bugs.
2. Create a short-lived branch from `main`.
3. Keep changes focused and update `CHANGELOG.md` when behavior changes.
4. Run `tests/run_host_tests.sh` and build the ROM-free template before a pull
   request.
5. Never commit an HP ROM, an unpacked ROM, calculator state, or a UF2 that
   contains an HP ROM.

Hardware reports should include the Pico board model, PicoCalc hardware or
keyboard-controller version if known, firmware version, exact input sequence,
and a photograph or USB serial trace when relevant.

Code derived from another project must retain its applicable license notices
and be recorded in `THIRD_PARTY.md`.
