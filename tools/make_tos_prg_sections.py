#!/usr/bin/env python3
"""Build an absolute TOS PRG from separate TEXT/DATA images plus BSS size."""
from pathlib import Path
import argparse
import struct


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("text", type=Path)
    parser.add_argument("data", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--bss", type=int, default=0)
    args = parser.parse_args()

    text = args.text.read_bytes()
    data = args.data.read_bytes() if args.data.exists() else b""
    if not text:
        raise SystemExit("TEXT image is empty")
    if args.bss < 0:
        raise SystemExit("BSS size must be non-negative")

    header = struct.pack(">HLLLLLLH", 0x601A, len(text), len(data), args.bss, 0, 0, 0, 1)
    args.output.write_bytes(header + text + data)


if __name__ == "__main__":
    main()
