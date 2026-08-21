#!/usr/bin/env python3
"""Insert a user-supplied HP 48GX revision-R ROM into the ROM-free UF2."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path

from prepare_rom import ROM_NIBBLES, TEMPLATE_MARKER, load_rom, rom_version, validate_crc


UF2_MAGIC_0 = 0x0A324655
UF2_MAGIC_1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_BLOCK_BYTES = 512


def parse_blocks(raw: bytes) -> tuple[list[bytearray], dict[int, tuple[int, int]]]:
    if len(raw) == 0 or len(raw) % UF2_BLOCK_BYTES:
        raise SystemExit("The template is not a valid UF2 file.")
    blocks = [bytearray(raw[i:i + UF2_BLOCK_BYTES])
              for i in range(0, len(raw), UF2_BLOCK_BYTES)]
    locations: dict[int, tuple[int, int]] = {}
    for block_no, block in enumerate(blocks):
        magic0, magic1, _flags, target, size = struct.unpack_from("<IIIII", block, 0)
        (magic_end,) = struct.unpack_from("<I", block, 508)
        if magic0 != UF2_MAGIC_0 or magic1 != UF2_MAGIC_1 or magic_end != UF2_MAGIC_END:
            raise SystemExit(f"Invalid UF2 framing in block {block_no}.")
        if size > 476:
            raise SystemExit(f"Invalid UF2 payload size in block {block_no}.")
        for offset in range(size):
            address = target + offset
            if address in locations:
                raise SystemExit("The UF2 has overlapping payload blocks.")
            locations[address] = (block_no, 32 + offset)
    return blocks, locations


def find_marker(blocks: list[bytearray], locations: dict[int, tuple[int, int]]) -> int:
    matches: list[int] = []
    for address, (block_no, offset) in locations.items():
        if blocks[block_no][offset] != TEMPLATE_MARKER[0]:
            continue
        for i, expected in enumerate(TEMPLATE_MARKER):
            where = locations.get(address + i)
            if where is None or blocks[where[0]][where[1]] != expected:
                break
        else:
            matches.append(address)
    if len(matches) != 1:
        raise SystemExit(f"Expected one ROM-template marker; found {len(matches)}.")
    return matches[0]


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Make an installable PicoCalc UF2 using your HP 48GX revision-R ROM."
    )
    parser.add_argument("template", type=Path, help="ROM-free hp48gx_picocalc_template.uf2")
    parser.add_argument("rom", type=Path, help="gxrom-r, gxrom-r.zip contents, or gxrom-r.bz2")
    parser.add_argument("output", type=Path, nargs="?", default=Path("hp48gx_picocalc.uf2"))
    args = parser.parse_args()

    rom = load_rom(args.rom)
    version = rom_version(rom)
    if rom[0x29] != 0 or version != "HP48-R" or not validate_crc(rom):
        raise SystemExit("ROM validation failed: an intact HP 48GX revision-R image is required.")

    blocks, locations = parse_blocks(args.template.read_bytes())
    rom_start = find_marker(blocks, locations)
    for i, value in enumerate(rom):
        where = locations.get(rom_start + i)
        if where is None:
            raise SystemExit("The UF2 template does not contain the complete ROM slot.")
        blocks[where[0]][where[1]] = value

    args.output.write_bytes(b"".join(blocks))
    digest = hashlib.sha256(args.rom.read_bytes()).hexdigest()
    print(f"Created {args.output} with ROM {version} (input SHA-256 {digest})")


if __name__ == "__main__":
    main()
