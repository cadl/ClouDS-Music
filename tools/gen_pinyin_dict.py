#!/usr/bin/env python3
"""Generate the compact PYIN dictionary consumed by source/ime_pinyin.c.

The format and ranking strategy follow the MIT-licensed Fishason/DSSH project.
The source data is a pinned rime-ice snapshot distributed under LGPL-3.0.
"""

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "data" / "pinyin_dict_src"
OUTPUT = ROOT / "romfs" / "pinyin_dict.bin"
FILES = [
    "8105.dict.yaml",
    "41448.dict.yaml",
    "others.dict.yaml",
    "base.dict.yaml",
    "ext.dict.yaml",
    "tencent.dict.yaml",
]
COMMON_CHARACTER_FILE = "8105.dict.yaml"
# The original 300k-word table used about 13 MiB of heap once the IME opened.
# Reserve every pronunciation of GB2312 characters from the 8105 table, then
# fill the remaining slots with the highest-frequency entries. This keeps
# common polyphonic characters usable without growing the fixed 80k budget.
MAX_FULL_ENTRIES = 80_000
MAX_ABBREVIATIONS = 20_000
ABBREVIATION_WEIGHT = 0.30
ALWAYS_KEEP_WORDS = {
    "网易云", "网易云音乐", "音乐", "歌曲", "歌手", "专辑",
    "周杰伦", "陈奕迅", "林俊杰", "邓紫棋", "五月天", "孙燕姿", "王菲",
}


def entries_from(path):
    after_header = False
    with path.open(encoding="utf-8") as stream:
        for raw in stream:
            line = raw.rstrip("\n")
            if not after_header:
                if line == "...":
                    after_header = True
                continue
            if not line or line.startswith("#"):
                continue
            columns = line.split("\t")
            if len(columns) < 2:
                continue
            word = columns[0].strip()
            syllables = tuple(part for part in
                              columns[1].replace("'", "").split() if part)
            pinyin = "".join(syllables)
            if not word or not pinyin or not pinyin.isascii() or not pinyin.isalpha():
                continue
            try:
                weight = int(columns[2]) if len(columns) > 2 else 1
            except ValueError:
                weight = 1
            yield word, pinyin.lower(), max(weight, 1), syllables


def intern(pool, offsets, value, encoding):
    if value not in offsets:
        offsets[value] = len(pool)
        pool.extend(value.encode(encoding))
        pool.append(0)
    return offsets[value]


def is_gb2312_character(word):
    if len(word) != 1:
        return False
    try:
        return len(word.encode("gb2312")) == 2
    except UnicodeEncodeError:
        return False


def main():
    values = {}
    preferred_entries = set()
    for filename in FILES:
        path = SOURCE / filename
        if not path.exists():
            raise SystemExit(f"missing {path}; run tools/fetch-ime-assets.sh")
        for word, pinyin, weight, syllables in entries_from(path):
            key = (word, pinyin)
            if (filename == COMMON_CHARACTER_FILE and
                    is_gb2312_character(word)):
                preferred_entries.add(key)
            previous = values.get(key)
            if previous is None or weight > previous[0]:
                values[key] = weight, syllables
        print(f"parsed {filename}: {len(values):,} unique entries")

    ranked = [(pinyin, word, weight, syllables)
              for (word, pinyin), (weight, syllables) in values.items()]
    ranked.sort(key=lambda item: -item[2])
    required_entries = preferred_entries | {
        (word, pinyin) for pinyin, word, _, _ in ranked
        if word in ALWAYS_KEEP_WORDS
    }
    if len(required_entries) > MAX_FULL_ENTRIES:
        raise SystemExit("required dictionary entries exceed full-entry budget")
    full = [item for item in ranked
            if (item[1], item[0]) in required_entries]
    for item in ranked:
        if (item[1], item[0]) not in required_entries:
            full.append(item)
        if len(full) == MAX_FULL_ENTRIES:
            break
    print(f"reserved {len(preferred_entries):,} GB2312 character readings")

    abbreviations = {}
    required_abbreviations = {}
    for pinyin, word, weight, syllables in full:
        if len(syllables) < 2:
            continue
        abbreviation = "".join(syllable[0] for syllable in syllables)
        if abbreviation == pinyin:
            continue
        key = (word, abbreviation)
        adjusted = max(1, int(weight * ABBREVIATION_WEIGHT))
        abbreviations[key] = max(adjusted, abbreviations.get(key, 0))
        if word in ALWAYS_KEEP_WORDS:
            required_abbreviations[key] = abbreviations[key]

    # Initial-only matching is a convenience feature, not the primary lookup
    # path.  Keeping every abbreviation nearly doubles the dictionary table
    # and penalizes all 3DS models even though the long tail is rarely useful
    # for music search.  Preserve the highest-frequency abbreviations while
    # retaining every full-pinyin entry above.
    abbreviations = dict(sorted(
        abbreviations.items(), key=lambda item: -item[1]
    )[:MAX_ABBREVIATIONS])
    abbreviations.update(required_abbreviations)

    combined = {(pinyin, word): weight
                for pinyin, word, weight, _ in full}
    for (word, pinyin), weight in abbreviations.items():
        combined[(pinyin, word)] = max(weight, combined.get((pinyin, word), 0))
    ordered = sorted(
        ((pinyin, word, weight) for (pinyin, word), weight in combined.items()),
        key=lambda item: (item[0], -item[2], item[1]),
    )

    pinyin_pool = bytearray()
    word_pool = bytearray()
    pinyin_offsets = {}
    word_offsets = {}
    table = bytearray()
    for pinyin, word, weight in ordered:
        pinyin_offset = intern(pinyin_pool, pinyin_offsets, pinyin, "ascii")
        word_offset = intern(word_pool, word_offsets, word, "utf-8")
        table.extend(struct.pack("<III", pinyin_offset, word_offset, weight))

    header_size = 28
    pinyin_offset = header_size + len(table)
    word_offset = pinyin_offset + len(pinyin_pool)
    header = struct.pack(
        "<4sIIIIII", b"PYIN", 1, len(ordered),
        pinyin_offset, len(pinyin_pool), word_offset, len(word_pool),
    )
    OUTPUT.parent.mkdir(exist_ok=True)
    OUTPUT.write_bytes(header + table + pinyin_pool + word_pool)
    print(f"wrote {OUTPUT}: {len(ordered):,} entries, "
          f"{OUTPUT.stat().st_size / 1024 / 1024:.2f} MiB")


if __name__ == "__main__":
    main()
