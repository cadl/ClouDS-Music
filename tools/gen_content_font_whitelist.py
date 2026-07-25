#!/usr/bin/env python3
"""Generate the Chinese/Latin whitelist used by the 3DS point fonts."""

import argparse
from pathlib import Path
import struct

from fontTools.ttLib import TTFont


DICTIONARY_HEADER = struct.Struct("<4sIIIIII")


def extra_codepoints(path: Path | None) -> set[int]:
    if path is None:
        return set()
    result = set()
    for line_number, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), 1):
        content = line.split("#", 1)[0]
        for char in content:
            if not char.isspace():
                result.add(ord(char))
        if "\0" in content:
            raise ValueError(f"{path}:{line_number}: NUL is not allowed")
    if not result:
        raise ValueError(f"{path}: empty extra-character list")
    return result


def traditional_codepoints(path: Path | None,
                           source_codepoints: set[int]) -> set[int]:
    if path is None:
        return set()
    result = set()
    for line_number, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), 1):
        fields = line.split("#", 1)[0].split()
        if not fields:
            continue
        if len(fields) < 2 or len(fields[0]) != 1:
            raise ValueError(
                f"{path}:{line_number}: expected one source and targets"
            )
        if ord(fields[0]) not in source_codepoints:
            continue
        for value in fields[1:]:
            result.update(ord(char) for char in value)
    return result


def candidate_codepoints(dictionary: Path) -> set[int]:
    data = dictionary.read_bytes()
    if len(data) < DICTIONARY_HEADER.size:
        raise ValueError(f"{dictionary}: truncated PYIN header")
    magic, version, _, _, _, word_offset, word_size = \
        DICTIONARY_HEADER.unpack_from(data)
    if magic != b"PYIN" or version != 1:
        raise ValueError(f"{dictionary}: unsupported PYIN dictionary")
    word_end = word_offset + word_size
    if word_offset < DICTIONARY_HEADER.size or word_end > len(data):
        raise ValueError(f"{dictionary}: invalid word pool")

    result = set()
    for encoded in data[word_offset:word_end].split(b"\0"):
        if encoded:
            result.update(ord(char) for char in encoded.decode("utf-8"))
    return result


def gb2312_codepoints() -> set[int]:
    result = set()
    for codepoint in range(0x80, 0x10000):
        try:
            chr(codepoint).encode("gb2312")
        except UnicodeEncodeError:
            continue
        result.add(codepoint)
    return result


def requested_codepoints(dictionary: Path, source: Path) -> set[int]:
    result = candidate_codepoints(dictionary) | gb2312_codepoints()
    result.update(range(0x20, 0x7F))
    result.update(range(0xA0, 0x180))
    result.update(range(0x2000, 0x2070))
    result.update({0x2190, 0x2191, 0x2192, 0x2193, 0x25A1})
    result.update(
        ord(char) for char in source.read_text(encoding="utf-8")
        if ord(char) <= 0xFFFF
    )
    return result


def generate(font_path: Path, dictionary: Path,
             source: Path, output: Path, extra_path: Path | None,
             traditional_maps: list[Path]) -> None:
    with TTFont(font_path, lazy=True) as font:
        cmap_codepoints = {
            codepoint for codepoint in font.getBestCmap()
            if codepoint <= 0xFFFF
        }
    candidates = candidate_codepoints(dictionary)
    base = requested_codepoints(dictionary, source)
    extras = extra_codepoints(extra_path)
    requested = base | extras
    traditional = set()
    for traditional_map in traditional_maps:
        traditional |= traditional_codepoints(
            traditional_map, requested | traditional)
    if traditional_maps and not traditional:
        raise ValueError("no Traditional mappings matched the base repertoire")
    requested |= traditional
    missing_extras = sorted(extras - cmap_codepoints)
    if missing_extras:
        values = ", ".join(f"U+{value:04X}" for value in missing_extras)
        raise ValueError(f"extra characters missing from font: {values}")
    codepoints = requested & cmap_codepoints
    missing_candidates = sorted(candidates - cmap_codepoints)
    extension = codepoints - (base & cmap_codepoints)
    extra_added = len(extras & extension)
    traditional_added = len((traditional & extension) - extras)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "\n".join(str(codepoint) for codepoint in sorted(codepoints)) + "\n",
        encoding="ascii",
    )
    print(
        f"wrote {output}: {len(codepoints):,} codepoints "
        f"(+{len(extension):,} extension: {extra_added:,} curated, "
        f"{traditional_added:,} Traditional)"
    )
    if missing_candidates:
        values = ", ".join(f"U+{value:04X}" for value in missing_candidates)
        print(f"system-font fallback required for: {values}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate the Chinese/Latin 3DS point-font whitelist."
    )
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--dictionary", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--extra", type=Path)
    parser.add_argument("--traditional-map", type=Path, action="append",
                        default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.font, args.dictionary, args.source, args.output,
             args.extra, args.traditional_map)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
