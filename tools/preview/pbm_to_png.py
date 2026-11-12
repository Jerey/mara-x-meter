#!/usr/bin/env python3
"""Converts binary PBM (P4) files to PNG using only the standard library."""
import struct
import sys
import zlib
from pathlib import Path


def read_pbm(path: Path):
    raw = path.read_bytes()
    assert raw.startswith(b"P4"), "only P4 PBM is supported"
    header, _, rest = raw[2:].lstrip().partition(b"\n")
    width, height = (int(v) for v in header.split())
    return width, height, rest


def write_png(path: Path, width: int, height: int, bits: bytes):
    row_bytes = (width + 7) // 8
    rows = bytearray()
    for y in range(height):
        row = bits[y * row_bytes : (y + 1) * row_bytes]
        rows.append(0)  # filter type none
        # PNG 1 bit grayscale: 1 = white, PBM: 1 = black -> invert.
        rows.extend(b ^ 0xFF for b in row)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 1, 0, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main():
    for arg in sys.argv[1:]:
        src = Path(arg)
        width, height, bits = read_pbm(src)
        dst = src.with_suffix(".png")
        write_png(dst, width, height, bits)
        print(f"wrote {dst}")


if __name__ == "__main__":
    main()
