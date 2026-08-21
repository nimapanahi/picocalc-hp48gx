#!/usr/bin/env python3
"""Validate an HP 48GX revision-R ROM and emit an unpacked Saturn nibble blob."""

from __future__ import annotations

import argparse
import bz2
import hashlib
import zipfile
from pathlib import Path


ROM_NIBBLES = 0x100000
PACKED_BYTES = ROM_NIBBLES // 2
VERSION_ADDR = 0x7FFBF
TEMPLATE_MARKER = b"HP48GX_PICOCALC_ROM_TEMPLATE_V1!"


def load_rom(path: Path) -> bytes:
    raw = path.read_bytes()
    if raw.startswith(b"PK\x03\x04"):
        with zipfile.ZipFile(path) as archive:
            candidates = [name for name in archive.namelist()
                          if not name.endswith("/") and archive.getinfo(name).file_size in
                          (PACKED_BYTES, ROM_NIBBLES)]
            if len(candidates) != 1:
                raise SystemExit("The ZIP must contain exactly one 512 KiB packed or 1 MiB unpacked ROM.")
            raw = archive.read(candidates[0])
    if raw.startswith(b"BZh"):
        raw = bz2.decompress(raw)
    if len(raw) == PACKED_BYTES:
        out = bytearray(ROM_NIBBLES)
        for i, value in enumerate(raw):
            out[i * 2] = value & 0x0F
            out[i * 2 + 1] = value >> 4
        return bytes(out)
    if len(raw) == ROM_NIBBLES and all(value <= 0x0F for value in raw):
        return raw
    raise SystemExit(
        f"Unsupported ROM format: {len(raw)} bytes. Expected a 524288-byte "
        "packed GX ROM or a 1048576-byte unpacked nibble ROM."
    )


def rom_version(rom: bytes) -> str:
    chars = []
    for i in range(6):
        at = VERSION_ADDR + i * 2
        chars.append(rom[at] | (rom[at + 1] << 4))
    return bytes(chars).split(b"\0", 1)[0].decode("ascii", "replace").strip()


def validate_crc(rom: bytes) -> bool:
    work = bytearray(rom)
    work[0x100:0x140] = b"\0" * 0x40
    d0, d1 = 0, 0x40000
    for _segment in range(ROM_NIBBLES // 0x80000):
        crc = 0
        for _ in range(0x4000):
            for nib in work[d0:d0 + 16]:
                crc = (crc >> 4) ^ (((crc ^ nib) & 0xF) * 0x1081)
            d0 += 16
            for nib in work[d1:d1 + 16]:
                crc = (crc >> 4) ^ (((crc ^ nib) & 0xF) * 0x1081)
            d1 += 16
        if (((crc | 0xF0000) + 1) & 0xFFFFF) != 0:
            return False
        d0 += 0x40000
        d1 += 0x40000
    return True


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--template", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("input", type=Path, nargs="?")
    parser.add_argument("output_bin", type=Path)
    parser.add_argument("output_asm", type=Path)
    args = parser.parse_args()

    if args.template:
        if args.input is not None:
            raise SystemExit("Do not supply a ROM when creating a template build.")
        rom = TEMPLATE_MARKER + bytes([0x0A]) * (ROM_NIBBLES - len(TEMPLATE_MARKER))
        version = "TEMPLATE"
    else:
        if args.input is None:
            raise SystemExit("A ROM path is required.")
        rom = load_rom(args.input)
        version = rom_version(rom)
        if rom[0x29] != 0:
            raise SystemExit("This image identifies as an HP 48S/SX ROM, not an HP 48G/GX ROM.")
        if version != "HP48-R":
            raise SystemExit(f"Expected HP 48GX ROM revision R; detected revision {version!r}.")
        if not validate_crc(rom):
            raise SystemExit("The ROM's internal CRC check failed; the image is corrupt or modified.")

    args.output_bin.parent.mkdir(parents=True, exist_ok=True)
    args.output_bin.write_bytes(rom)
    rom_path = args.output_bin.resolve().as_posix().replace('"', '\\"')
    args.output_asm.write_text(
        '.section .rodata.hp48_rom,"a",%progbits\n'
        '.balign 4\n'
        '.global hp48_rom\n'
        '.global hp48_rom_end\n'
        '.type hp48_rom, %object\n'
        'hp48_rom:\n'
        f'.incbin "{rom_path}"\n'
        'hp48_rom_end:\n'
        '.size hp48_rom, hp48_rom_end-hp48_rom\n',
        encoding="utf-8",
    )
    if args.template:
        print("Generated ROM-free UF2 template payload")
    else:
        print(f"ROM revision {version}; SHA-256 {hashlib.sha256(args.input.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
