#!/usr/bin/env python3
"""Create the deterministic HP 48 binary object used for SD transfer tests."""

from __future__ import annotations

import argparse
from pathlib import Path


HEADER = b"HPHP48-W"
DOCSTR = 0x02A2C


def nibbles(value: int, count: int) -> list[int]:
    return [(value >> (4 * index)) & 0xF for index in range(count)]


def pack_nibbles(values: list[int]) -> bytes:
    if len(values) & 1:
        values = values + [0]
    return bytes(values[index] | (values[index + 1] << 4)
                 for index in range(0, len(values), 2))


def hp48_string(text: str) -> bytes:
    encoded = text.encode("latin-1")
    body: list[int] = nibbles(DOCSTR, 5)
    body.extend(nibbles(5 + len(encoded) * 2, 5))
    for byte in encoded:
        body.extend(nibbles(byte, 2))
    return HEADER + pack_nibbles(body)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a standard HPHP48-W string-object test file."
    )
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_bytes(hp48_string("PICOCALC"))
    print(f"Created {args.output} (stack result: \"PICOCALC\")")


if __name__ == "__main__":
    main()
