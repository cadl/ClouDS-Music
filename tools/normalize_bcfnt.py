#!/usr/bin/env python3
"""Normalize mkbcfnt's uninitialized default-width field."""

import argparse
from pathlib import Path
import struct


def normalize(data: bytearray) -> tuple[bytes, bytes]:
    if len(data) < 52 or data[:4] != b"CFNT":
        raise ValueError("not a BCFNT file")
    header_size = struct.unpack_from("<H", data, 6)[0]
    file_size = struct.unpack_from("<I", data, 12)[0]
    if header_size + 32 > len(data) or file_size != len(data):
        raise ValueError("invalid BCFNT header")
    finf = header_size
    if data[finf:finf + 4] != b"FINF":
        raise ValueError("BCFNT has no FINF section")

    alter_index = struct.unpack_from("<H", data, finf + 10)[0]
    default_offset = finf + 12
    cwdh = struct.unpack_from("<I", data, finf + 20)[0]
    while cwdh:
        if (cwdh < 8 or cwdh + 8 > len(data) or
                data[cwdh - 8:cwdh - 4] != b"CWDH"):
            raise ValueError("invalid BCFNT CWDH pointer")
        start, end, next_cwdh = struct.unpack_from("<HHI", data, cwdh)
        widths_end = cwdh + 8 + (end - start + 1) * 3
        if end < start or widths_end > len(data):
            raise ValueError("invalid BCFNT CWDH range")
        if start <= alter_index <= end:
            replacement_offset = cwdh + 8 + (alter_index - start) * 3
            old = bytes(data[default_offset:default_offset + 3])
            new = bytes(data[replacement_offset:replacement_offset + 3])
            data[default_offset:default_offset + 3] = new
            return old, new
        cwdh = next_cwdh
    raise ValueError("replacement glyph width not found")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Copy the BCFNT replacement glyph width into FINF."
    )
    parser.add_argument("font", type=Path)
    args = parser.parse_args()
    data = bytearray(args.font.read_bytes())
    old, new = normalize(data)
    args.font.write_bytes(data)
    print(
        f"normalized {args.font}: default width "
        f"{tuple(old)} -> {tuple(new)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
