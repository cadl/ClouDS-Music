#!/usr/bin/env python3

import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile


sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "gen_ui_font_whitelist", ROOT / "tools/gen_ui_font_whitelist.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        first = Path(directory) / "first.c"
        second = Path(directory) / "second.c"
        first.write_text(
            '// "注释不进入字库"\nconst char *a = "设置";\n', encoding="utf-8"
        )
        second.write_text(
            'const char *b = u8"歌词 ←→";\n', encoding="utf-8"
        )
        values = MODULE.source_codepoints([first, second])

    assert ord("设") in values
    assert ord("置") in values
    assert ord("歌") in values
    assert ord("词") in values
    assert ord("←") in values
    assert ord("→") in values
    assert ord("注") not in values
    assert ord("A") in values
    assert 0x25A1 in values

    project_values = MODULE.source_codepoints(
        [ROOT / "source/i18n.c", ROOT / "source/ui.c"]
    )
    encoded = (
        "\n".join(str(value) for value in sorted(project_values)) + "\n"
    ).encode("ascii")
    assert len(project_values) == 499
    assert hashlib.sha256(encoded).hexdigest() == (
        "ba65879db604f69ed415791b2bcabf9f2d8b121f9c4b2c0d63d195a2b0f1563e"
    )
    print("ui font whitelist tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
