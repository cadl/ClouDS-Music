#!/usr/bin/env python3

import hashlib
import importlib.util
from pathlib import Path
import struct
import sys


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "normalize_bcfnt", ROOT / "tools/normalize_bcfnt.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def fixture() -> bytearray:
    data = bytearray(96)
    data[:4] = b"CFNT"
    struct.pack_into("<H", data, 6, 20)
    struct.pack_into("<I", data, 12, len(data))
    data[20:24] = b"FINF"
    struct.pack_into("<H", data, 30, 1)
    data[32:35] = b"xyz"
    struct.pack_into("<I", data, 40, 64)
    data[56:60] = b"CWDH"
    struct.pack_into("<HHI", data, 64, 0, 1, 0)
    struct.pack_into("<bBB", data, 72, 0, 3, 4)
    struct.pack_into("<bBB", data, 75, -1, 5, 6)
    return data


def main() -> int:
    data = fixture()
    old, new = MODULE.normalize(data)
    assert old == b"xyz"
    assert new == bytes((0xFF, 5, 6))
    assert data[32:35] == new

    font = bytearray((ROOT / "romfs/ui-menu-font.bcfnt").read_bytes())
    old, new = MODULE.normalize(font)
    assert old == new == bytes((0, 6, 6))
    assert hashlib.sha256(font).hexdigest() == (
        "50188f89a2526b1c34664a51aeee0517e3a9939f78f67cc0501df16f857e4907"
    )
    print("BCFNT normalization tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
