#!/usr/bin/env python3
"""Generate a compact bitmap glyph header from RobotoMono TTF for kernel HUD text."""

import pathlib
import sys

from PIL import Image, ImageDraw, ImageFont


FIRST_CHAR = 32
LAST_CHAR = 126
CANVAS_WIDTH = 8
CANVAS_HEIGHT = 8
FONT_SIZE = 10
DRAW_OFFSET_X = 0
DRAW_OFFSET_Y = -1


def render_glyph(font: ImageFont.FreeTypeFont, ch: str) -> tuple[int, list[int]]:
    image = Image.new("L", (CANVAS_WIDTH, CANVAS_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    draw.text((DRAW_OFFSET_X, DRAW_OFFSET_Y), ch, fill=255, font=font)

    pixels = image.load()
    min_x = CANVAS_WIDTH
    max_x = -1
    for y in range(CANVAS_HEIGHT):
        for x in range(CANVAS_WIDTH):
            if pixels[x, y] >= 64:
                if x < min_x:
                    min_x = x
                if x > max_x:
                    max_x = x

    if max_x < min_x:
        return 3, [0] * CANVAS_HEIGHT

    width = max_x - min_x + 1
    if width < 3:
        width = 3
    if width > 7:
        width = 7

    rows: list[int] = []
    for y in range(CANVAS_HEIGHT):
        bits = 0
        for x in range(width):
            src_x = min_x + x
            on = src_x < CANVAS_WIDTH and pixels[src_x, y] >= 64
            if on:
                bits |= 1 << (width - 1 - x)
        rows.append(bits)

    return width, rows


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: generate_roboto_mono_font_header.py <ttf-path> <out-header>", file=sys.stderr)
        return 1

    ttf_path = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2])

    font = ImageFont.truetype(str(ttf_path), FONT_SIZE)

    widths: list[int] = []
    rows_per_glyph: list[list[int]] = []
    for code in range(FIRST_CHAR, LAST_CHAR + 1):
        width, rows = render_glyph(font, chr(code))
        widths.append(width)
        rows_per_glyph.append(rows)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="ascii") as handle:
        handle.write("#ifndef FORTRESS_VIDEO_GENERATED_FROBOTO_MONO_GENERATED_HPP\n")
        handle.write("#define FORTRESS_VIDEO_GENERATED_FROBOTO_MONO_GENERATED_HPP\n\n")
        handle.write("#include \"Fortress/Core/FTypes.hpp\"\n\n")
        handle.write("namespace Fortress::Video::Generated {\n\n")
        handle.write("struct GRobotoMono {\n")
        handle.write(f"    static constexpr char FirstCharacter = '{chr(FIRST_CHAR)}';\n")
        handle.write(f"    static constexpr char LastCharacter = '{chr(LAST_CHAR)}';\n")
        handle.write(f"    static constexpr Fortress::Core::uint8 GlyphHeight = {CANVAS_HEIGHT}u;\n")
        handle.write("    static constexpr Fortress::Core::uint8 GlyphWidths[] = {\n")
        for i, width in enumerate(widths):
            end = "\n" if i % 16 == 15 else " "
            handle.write(f"        {width}u,{end}")
        handle.write("\n    };\n\n")

        handle.write("    static constexpr Fortress::Core::uint8 GlyphRows[][GlyphHeight] = {\n")
        for rows in rows_per_glyph:
            row_expr = ", ".join(f"0b{value:08b}" for value in rows)
            handle.write(f"        {{{row_expr}}},\n")
        handle.write("    };\n")
        handle.write("};\n\n")
        handle.write("} // namespace Fortress::Video::Generated\n\n")
        handle.write("#endif\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
