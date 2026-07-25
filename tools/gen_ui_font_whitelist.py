#!/usr/bin/env python3
"""Generate the compact BCFNT whitelist used by fixed UI copy."""

import argparse
from pathlib import Path


def direct_string_codepoints(text: str) -> set[int]:
    """Collect direct Unicode characters from C strings, ignoring comments."""
    result = set()
    index = 0
    while index < len(text):
        if text.startswith("//", index):
            newline = text.find("\n", index + 2)
            index = len(text) if newline < 0 else newline + 1
            continue
        if text.startswith("/*", index):
            close = text.find("*/", index + 2)
            index = len(text) if close < 0 else close + 2
            continue
        if text[index] == "'":
            index += 1
            while index < len(text) and text[index] != "'":
                index += 2 if text[index] == "\\" else 1
            index += index < len(text)
            continue
        if text[index] != '"':
            index += 1
            continue
        index += 1
        while index < len(text) and text[index] != '"':
            if text[index] == "\\":
                index += 2
                continue
            if ord(text[index]) >= 0x80:
                result.add(ord(text[index]))
            index += 1
        index += index < len(text)
    return result


def source_codepoints(paths: list[Path]) -> set[int]:
    """Return direct Unicode characters used inside C string literals."""
    result = set(range(0x20, 0x7F))
    for path in paths:
        text = path.read_text(encoding="utf-8")
        result.update(direct_string_codepoints(text))
    # The point renderer and BCFNT both use a visible missing-glyph marker.
    result.add(0x25A1)
    return result


def generate(font_path: Path, source_paths: list[Path], output: Path) -> None:
    from fontTools.ttLib import TTFont

    requested = source_codepoints(source_paths)
    with TTFont(font_path, lazy=True) as font:
        available = set(font.getBestCmap())
    missing = sorted(requested - available)
    if missing:
        values = ", ".join(f"U+{value:04X}" for value in missing)
        raise ValueError(f"fixed UI characters missing from font: {values}")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "\n".join(str(codepoint) for codepoint in sorted(requested)) + "\n",
        encoding="ascii",
    )
    print(f"wrote {output}: {len(requested):,} fixed UI codepoints")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate the fixed UI codepoint whitelist for mkbcfnt."
    )
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--source", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.font, args.source, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
