#!/usr/bin/env python3

import hashlib
import importlib.util
from pathlib import Path
import sys


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "gen_content_font_whitelist",
    ROOT / "tools/gen_content_font_whitelist.py",
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def main() -> int:
    shift_jis = MODULE.shift_jis_codepoints()
    assert len(shift_jis) == MODULE.SHIFT_JIS_CODEPOINTS

    japanese = MODULE.japanese_codepoints()
    encoded = (
        "\n".join(str(value) for value in sorted(japanese)) + "\n"
    ).encode("ascii")
    assert len(japanese) == 7019
    assert hashlib.sha256(encoded).hexdigest() == (
        "03ed7275d88707301d979e9954cae669d0cb5ab2181348da43458dea1644662e"
    )

    required = "ー〜働込辻峠榊畑ㇰｱｰ♪♫♬♭♮♯"
    assert all(ord(char) in japanese for char in required)
    assert 0x3099 not in japanese
    assert 0x309A not in japanese

    extras = MODULE.extra_codepoints(
        ROOT / "tools/japanese-font-extra-chars.txt"
    )
    assert all(ord(char) in extras for char in "髙﨑𠮷濵彅栁桒")
    assert 0x20BB7 in extras

    print("content font whitelist tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
