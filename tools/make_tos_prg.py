#!/usr/bin/env python3
"""Wrap a flat 68k text image in a minimal absolute Atari TOS PRG header."""

from pathlib import Path
import argparse
import struct


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    text = args.input.read_bytes()
    if not text:
        raise SystemExit("input text image is empty")

    # TOS PRG header, big-endian:
    # branch=0x601a, text, data, bss, symbols, reserved, flags, absflag.
    header = struct.pack(">HLLLLLLH", 0x601A, len(text), 0, 0, 0, 0, 0, 1)
    args.output.write_bytes(header + text)


if __name__ == "__main__":
    main()
