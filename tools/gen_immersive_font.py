#!/usr/bin/env python3
"""Generate a compact native-size monochrome font for the 3DS UI."""

import argparse
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont


MAGIC = b"IMBF"
VERSION = 1
HEADER = struct.Struct("<4sHHHHI")

CJK_GRID_RANGES = (
    (0x3400, 0x4DBF),
    (0x4E00, 0x9FFF),
    (0xAC00, 0xD7A3),
    (0xF900, 0xFAFF),
    (0x20000, 0x323AF),
)


def read_codepoints(path: Path) -> list[int]:
    values = {
        int(line) for line in path.read_text(encoding="ascii").splitlines()
        if line.strip()
    }
    if not values:
        raise ValueError(f"{path}: empty codepoint whitelist")
    invalid = [value for value in values if value < 0 or value > 0x10FFFF]
    if invalid:
        raise ValueError(f"{path}: invalid Unicode codepoint {invalid[0]}")
    return sorted(values)


def pack_bitmap(image: Image.Image, width: int, height: int) -> bytes:
    row_bytes = (width + 7) // 8
    packed = bytearray(row_bytes * height)
    for y in range(height):
        for x in range(width):
            if image.getpixel((x, y)):
                offset = y * row_bytes + x // 8
                packed[offset] |= 0x80 >> (x & 7)
    return bytes(packed)


def is_cjk_grid_glyph(codepoint: int) -> bool:
    return any(first <= codepoint <= last
               for first, last in CJK_GRID_RANGES)


def align_cjk_vertical(image: Image.Image, codepoint: int,
                       target_center2: int | None) -> tuple[Image.Image, int]:
    """Correct full-pixel CJK visual-center drift without moving punctuation."""
    if target_center2 is None or not is_cjk_grid_glyph(codepoint):
        return image, 0
    bounds = image.getbbox()
    if bounds is None:
        return image, 0
    center2 = bounds[1] + bounds[3] - 1
    difference2 = target_center2 - center2
    if -1 <= difference2 <= 1:
        return image, 0
    shift = 1 if difference2 > 0 else -1
    aligned = Image.new(image.mode, image.size, 0)
    aligned.paste(image, (0, shift))
    return aligned, shift


def render_glyph(font: ImageFont.FreeTypeFont,
                 codepoint: int, width: int, height: int,
                 baseline: int,
                 cjk_center2: int | None) -> tuple[int, bytes, int]:
    char = chr(codepoint)
    image = Image.new("1", (width, height), 0)
    draw = ImageDraw.Draw(image)
    draw.text((0, baseline), char, font=font, fill=1, anchor="ls")
    image, shift = align_cjk_vertical(image, codepoint, cjk_center2)
    advance = round(font.getlength(char))
    advance = max(1, min(width, advance))
    return advance, pack_bitmap(image, width, height), shift


def generate(font_path: Path, whitelist: Path, output: Path,
             font_pixels: int, width: int, height: int,
             baseline: int, cjk_center2: int | None) -> None:
    if (font_pixels <= 0 or width <= 0 or height <= 0 or
            baseline < 0 or baseline > height):
        raise ValueError("font and glyph dimensions must be positive")
    if (cjk_center2 is not None and
            (cjk_center2 < 0 or cjk_center2 >= height * 2)):
        raise ValueError("CJK center must be inside the glyph cell")
    codepoints = read_codepoints(whitelist)
    font = ImageFont.truetype(str(font_path), font_pixels)
    bitmap_bytes = ((width + 7) // 8) * height
    entry = struct.Struct(f"<IB3x{bitmap_bytes}s")
    shifted_up = 0
    shifted_down = 0
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("wb") as stream:
        stream.write(HEADER.pack(
            MAGIC, VERSION, width, height,
            bitmap_bytes, len(codepoints)))
        for codepoint in codepoints:
            advance, bitmap, shift = render_glyph(
                font, codepoint, width, height, baseline, cjk_center2)
            shifted_up += shift < 0
            shifted_down += shift > 0
            stream.write(entry.pack(codepoint, advance, bitmap))
    expected = HEADER.size + len(codepoints) * entry.size
    if output.stat().st_size != expected:
        raise RuntimeError(f"{output}: generated size does not match format")
    print(
        f"wrote {output}: {len(codepoints):,} glyphs, "
        f"{output.stat().st_size:,} bytes, "
        f"CJK shifts {shifted_up:,} up/{shifted_down:,} down"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a compact monochrome native-size font"
    )
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--whitelist", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--font-pixels", type=int, default=24)
    parser.add_argument("--glyph-width", type=int, default=24)
    parser.add_argument("--glyph-height", type=int, default=32)
    parser.add_argument("--baseline", type=int, default=26)
    parser.add_argument(
        "--cjk-center2", type=int,
        help=("twice the target visible center for Han/Hangul glyphs; "
              "only full-pixel drift is corrected and shifts are capped at 1px")
    )
    args = parser.parse_args()
    generate(args.font, args.whitelist, args.output,
             args.font_pixels, args.glyph_width,
             args.glyph_height, args.baseline, args.cjk_center2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
