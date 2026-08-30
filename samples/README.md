# SD individual-file hardware test

`PICOCALC.48G` is a standard HP 48 binary string object with the eight-byte
`HPHP48-W` header. It contains the string `"PICOCALC"` and no executable code.
It is generated deterministically by `tools/make_sample_objects.py`.

1. Boot `2.1.0` once with the SD card installed so the firmware creates
   `/HP48GX/PROGRAMS/INBOX` and `OUTBOX`.
2. Use the emulator's normal Save-and-OFF path before removing the SD card.
3. Copy `PICOCALC.48G` into `/HP48GX/PROGRAMS/INBOX`, reinstall the card, and
   boot the PicoCalc.
4. Press Ctrl+F7. The footer should show `INBOX: PICOCALC.48G`.
5. Press Ctrl+F8. The footer should report a successful import and stack level
   1 should display `"PICOCALC"`.
6. Without changing level 1, press Ctrl+F9. The footer should report a new
   `OUTBOX/STKnnn.48G` file.
7. Save-and-OFF, read the SD card on a computer, and compare the new export to
   `PICOCALC.48G`. They should be byte-for-byte identical. The original inbox
   file must still be present and unchanged.

SHA-256 for `PICOCALC.48G`:

```text
24e5a560e583e12d8bf95c4842cdb3e88f790e5b2c17c005d886521553d24c35  PICOCALC.48G
```

Fresh 2.1 installations create the non-covered `PORT1.CRD` automatically.
Copy `PORT1.MODE` to `/HP48GX/PROGRAMS` while powered off to select it
explicitly, or copy `PORT2.MODE` to select an existing covered `PORT2.CRD`.
Never install both markers. An unmerged Port 1 accepts old machine-language
libraries stored to port number `1`; the stock `MERGE1` command instead turns
the whole card into expanded user memory.
