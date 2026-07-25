#!/usr/bin/env python3
"""Generate the 3,500-syllable whitelist for the Korean point fonts."""

import argparse
from collections import Counter
from pathlib import Path
import unicodedata

from fontTools.ttLib import TTFont


HANGUL_BASE = 0xAC00
HANGUL_END = 0xD7A3
HANGUL_VOWELS = 21
HANGUL_TRAILING = 28
HANGUL_PER_INITIAL = HANGUL_VOWELS * HANGUL_TRAILING
KS_X_1001_SYLLABLES = 2350
REPLACEMENT_CODEPOINT = 0x003F


def ks_x_1001_syllables() -> set[int]:
    """Return the strict KS X 1001 precomposed Hangul repertoire."""
    result = set()
    for codepoint in range(HANGUL_BASE, HANGUL_END + 1):
        try:
            chr(codepoint).encode("iso2022_kr")
        except UnicodeEncodeError:
            continue
        result.add(codepoint)
    if len(result) != KS_X_1001_SYLLABLES:
        raise RuntimeError(
            "Python's strict KS X 1001 mapping changed: "
            f"expected {KS_X_1001_SYLLABLES}, found {len(result)}"
        )
    return result


def syllable_parts(codepoint: int) -> tuple[int, int, int]:
    offset = codepoint - HANGUL_BASE
    initial = offset // HANGUL_PER_INITIAL
    vowel = (offset % HANGUL_PER_INITIAL) // HANGUL_TRAILING
    trailing = offset % HANGUL_TRAILING
    return initial, vowel, trailing


def frequency_scores(path: Path) -> tuple[Counter, list[Counter]]:
    syllables = Counter()
    parts = [Counter(), Counter(), Counter()]
    with path.open(encoding="utf-8") as stream:
        for line_number, raw_line in enumerate(stream, 1):
            line = raw_line.rstrip("\n")
            try:
                word, encoded_count = line.rsplit(" ", 1)
                count = int(encoded_count)
            except ValueError as error:
                raise ValueError(
                    f"{path}:{line_number}: invalid frequency row"
                ) from error
            if count <= 0:
                raise ValueError(
                    f"{path}:{line_number}: frequency must be positive"
                )
            for char in unicodedata.normalize("NFC", word):
                codepoint = ord(char)
                if codepoint < HANGUL_BASE or codepoint > HANGUL_END:
                    continue
                syllables[codepoint] += count
                for index, part in enumerate(syllable_parts(codepoint)):
                    parts[index][part] += count
    return syllables, parts


def component_score(codepoint: int, parts: list[Counter]) -> int:
    initial, vowel, trailing = syllable_parts(codepoint)
    # Add-one smoothing gives every legal modern syllable a deterministic
    # score even when the subtitle corpus never observed that combination.
    return ((parts[0][initial] + 1) *
            (parts[1][vowel] + 1) *
            (parts[2][trailing] + 1))


def generate(font_path: Path, frequency_path: Path,
             output_path: Path, target: int) -> None:
    if target < KS_X_1001_SYLLABLES:
        raise ValueError(
            f"target must retain all {KS_X_1001_SYLLABLES} KS X 1001 syllables"
        )
    font = TTFont(font_path, lazy=True)
    try:
        font_codepoints = set(font.getBestCmap())
    finally:
        font.close()

    base = ks_x_1001_syllables()
    if REPLACEMENT_CODEPOINT not in font_codepoints:
        raise ValueError("font is missing the '?' replacement glyph")
    missing_base = sorted(base - font_codepoints)
    if missing_base:
        values = ", ".join(f"U+{value:04X}" for value in missing_base[:8])
        raise ValueError(f"font is missing KS X 1001 glyphs: {values}")

    observed, parts = frequency_scores(frequency_path)
    candidates = [
        codepoint for codepoint in range(HANGUL_BASE, HANGUL_END + 1)
        if codepoint in font_codepoints and codepoint not in base
    ]
    candidates.sort(key=lambda codepoint: (
        0 if observed[codepoint] else 1,
        -observed[codepoint],
        -component_score(codepoint, parts),
        codepoint,
    ))

    extra_count = target - len(base)
    if len(candidates) < extra_count:
        raise ValueError(
            f"font only provides {len(candidates)} non-KS Hangul syllables; "
            f"need {extra_count}"
        )
    selected_extras = candidates[:extra_count]
    selected_syllables = base | set(selected_extras)
    selected = sorted(selected_syllables | {REPLACEMENT_CODEPOINT})
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        "\n".join(str(codepoint) for codepoint in selected) + "\n",
        encoding="ascii",
    )
    observed_extras = sum(1 for value in selected_extras if observed[value])
    print(
        f"wrote {output_path}: {len(selected_syllables):,} Hangul syllables "
        f"({len(base):,} KS X 1001 + {observed_extras:,} observed + "
        f"{extra_count - observed_extras:,} component-ranked) + '?' replacement"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate the Korean point-font codepoint whitelist."
    )
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--frequency", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--target", type=int, default=3500)
    args = parser.parse_args()
    generate(args.font, args.frequency, args.output, args.target)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
