#!/usr/bin/env python3
"""Build a TOS PRG from separate TEXT/DATA images plus BSS and relocations."""
from pathlib import Path
import argparse
import struct


def encode_relocations(offsets: list[int]) -> bytes:
    if not offsets:
        return b""
    offsets = sorted(offsets)
    if offsets[0] <= 0:
        raise SystemExit("first relocation offset must be positive")
    out = bytearray(struct.pack(">L", offsets[0]))
    previous = offsets[0]
    for current in offsets[1:]:
        if current <= previous:
            raise SystemExit("relocation offsets must be strictly increasing")
        delta = current - previous
        while delta > 254:
            out.append(1)
            delta -= 254
        if delta == 1:
            raise SystemExit("TOS relocation encoding cannot represent a literal delta of 1")
        out.append(delta)
        previous = current
    out.append(0)
    return bytes(out)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("text", type=Path)
    parser.add_argument("data", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--bss", type=int, default=0)
    parser.add_argument("--reloc", type=lambda value: int(value, 0), action="append", default=[])
    args = parser.parse_args()

    text = args.text.read_bytes()
    data = args.data.read_bytes() if args.data.exists() else b""
    if not text:
        raise SystemExit("TEXT image is empty")
    if args.bss < 0:
        raise SystemExit("BSS size must be non-negative")

    reloc = encode_relocations(args.reloc)
    absolute = 0 if args.reloc else 1
    header = struct.pack(">HLLLLLLH", 0x601A, len(text), len(data), args.bss, 0, 0, 0, absolute)
    args.output.write_bytes(header + text + data + reloc)


if __name__ == "__main__":
    main()
