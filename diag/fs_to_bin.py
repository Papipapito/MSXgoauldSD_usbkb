#!/usr/bin/env python3
"""
fs_to_bin.py — Convert a Gowin ASCII bitstream (.fs) to a raw binary (.bin).

The Gowin .fs format:
  - Header lines starting with '//' (comments) — skipped.
  - Data lines containing only '0' and '1' characters.
  - Bits within each line are packed MSB-first into bytes.
  - Lines are concatenated in order; total bits must be divisible by 8.

This is a fallback for when the Gowin build does NOT produce impl/pnr/project.bin.
The normal Gowin EDA flow (gw_sh / IDE) already emits project.bin alongside
project.fs; prefer that file.  Only use this converter when project.bin is absent.

Usage:
    python3 fs_to_bin.py <input.fs> [output.bin]

    If output.bin is omitted, the output file is the input path with .bin extension.

Sanity check:
    The script prints the total bit count and output byte count.
    Expected for GW2AR-18 C: ~7 259 344 bits → 907 418 bytes.
"""

import sys
import os


def fs_to_bin(fs_path: str, bin_path: str) -> None:
    bits: list[int] = []

    with open(fs_path, "r", encoding="ascii", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\r\n")
            if line.startswith("//"):
                continue  # skip header comments
            if not line:
                continue  # skip blank lines
            # Validate: only '0' and '1' allowed
            if not all(c in "01" for c in line):
                raise ValueError(f"Unexpected characters in .fs line: {line!r}")
            bits.extend(int(c) for c in line)

    total_bits = len(bits)
    if total_bits % 8 != 0:
        raise ValueError(
            f"Total bit count {total_bits} is not divisible by 8 — "
            "the .fs file may be truncated or corrupted."
        )

    total_bytes = total_bits // 8
    print(f"  Total bits : {total_bits}")
    print(f"  Total bytes: {total_bytes}")

    # Pack bits MSB-first into bytes
    buf = bytearray(total_bytes)
    for i in range(total_bytes):
        byte_val = 0
        for bit_pos in range(8):
            byte_val = (byte_val << 1) | bits[i * 8 + bit_pos]
        buf[i] = byte_val

    with open(bin_path, "wb") as out:
        out.write(buf)

    written = os.path.getsize(bin_path)
    print(f"  Written    : {written} bytes -> {bin_path}")
    if written != total_bytes:
        raise RuntimeError(f"Size mismatch: expected {total_bytes}, wrote {written}")


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <input.fs> [output.bin]", file=sys.stderr)
        sys.exit(1)

    fs_path = sys.argv[1]
    if len(sys.argv) >= 3:
        bin_path = sys.argv[2]
    else:
        base, _ = os.path.splitext(fs_path)
        bin_path = base + ".bin"

    if not os.path.isfile(fs_path):
        print(f"ERROR: Input file not found: {fs_path}", file=sys.stderr)
        sys.exit(1)

    print(f"Converting: {fs_path} -> {bin_path}")
    fs_to_bin(fs_path, bin_path)
    print("Done.")


if __name__ == "__main__":
    main()
