#!/usr/bin/env python3
"""Build the Jarl Mini pixel fonts from tools/jarl_mini_glyphs.txt.

Each glyph is a 17-row pixel grid ('#' = ink). Every horizontal run of ink
becomes a rectangle on an exact pixel grid (64 font units per pixel, em =
glyph height), so Pebble's 1-bit glyph rasterizer reproduces the grid exactly
when the font resource is built at that pixel height.

Two fonts are written:

  resources/fonts/jarl_mini.ttf      13x17 glyphs, 2 px gap (build at 17 px)
  resources/fonts/jarl_mini_big.ttf  18x24 glyphs, 3 px gap (build at 24 px)

The big font is derived from the small glyphs, not drawn separately. The
glyphs are built on a grid of 3 px strokes (columns 3|2|3|2|3, rows
3|4|3|4|3), so each band is resized on its own: strokes become 4 px and the
gaps grow, while the outermost pixel of every band stays a single pixel so
rounded corners keep their 1 px cut. Glyphs that don't scale well can be
redrawn by hand in tools/jarl_mini_big_overrides.txt.

Lowercase letters reuse the uppercase glyphs.

Usage (needs fontTools):
    uvx --with fonttools python tools/build_mini_font.py
"""

import os
import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

PX = 64  # font units per pixel

HERE = os.path.dirname(os.path.abspath(__file__))
FONTS = os.path.join(HERE, "..", "resources", "fonts")
SRC = os.path.join(HERE, "jarl_mini_glyphs.txt")
BIG_OVERRIDES = os.path.join(HERE, "jarl_mini_big_overrides.txt")

SMALL_ROWS, BIG_ROWS = 17, 24

# For each band of source pixels, which source pixel each target pixel copies.
# The first and last pixel of a band stay single so 1 px details (rounded
# corners, inner corners) don't double up.
ROW_MAP = (
    [0, 1, 1, 2]                      # top bar        rows 0-2   -> 4
    + [3 + i for i in (0, 1, 1, 2, 2, 3)]   # upper gap  rows 3-6   -> 6
    + [7 + i for i in (0, 1, 1, 2)]         # middle bar rows 7-9   -> 4
    + [10 + i for i in (0, 1, 1, 2, 2, 3)]  # lower gap  rows 10-13 -> 6
    + [14 + i for i in (0, 1, 1, 2)]        # bottom bar rows 14-16 -> 4
)
COL_MAPS = {
    13: (
        [0, 1, 1, 2]                  # left stroke    cols 0-2   -> 4
        + [3 + i for i in (0, 1, 1)]      # left gap   cols 3-4   -> 3
        + [5 + i for i in (0, 1, 1, 2)]   # centre     cols 5-7   -> 4
        + [8 + i for i in (0, 0, 1)]      # right gap  cols 8-9   -> 3
        + [10 + i for i in (0, 1, 1, 2)]  # right stroke cols 10-12 -> 4
    ),
    3: [0, 1, 1, 2],                  # narrow punctuation -> 4
    6: [0, 1, 1, 2, 3, 4, 4, 5],      # degree sign, space  -> 8
}
assert len(ROW_MAP) == BIG_ROWS and len(COL_MAPS[13]) == 18


def read_glyphs(path, rows_expected):
    glyphs, cur = {}, None
    if not os.path.exists(path):
        return glyphs
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("char "):
                cur = line[5:]
                glyphs[cur] = []
            elif cur is not None and line and set(line) <= set("#."):
                glyphs[cur].append(line)
    for name, rows in glyphs.items():
        if len(rows) != rows_expected or len({len(r) for r in rows}) != 1:
            sys.exit(f"{os.path.basename(path)}: glyph {name!r}: expected {rows_expected} rows of equal width")
    return glyphs


def slash(width, height, stroke):
    """A '/' with a `stroke`-wide diagonal from bottom-left to top-right."""
    span = width - stroke
    return [
        "".join("#" if x0 <= c < x0 + stroke else "." for c in range(width))
        for x0 in (round((height - 1 - r) * span / (height - 1)) for r in range(height))
    ]


def scale_up(name, rows):
    if name == "/":
        return slash(12, BIG_ROWS, 4)
    width = len(rows[0])
    col_map = COL_MAPS.get(width) or [int(c * width / (width * 4 // 3)) for c in range(width * 4 // 3)]
    return ["".join(rows[r][c] for c in col_map) for r in ROW_MAP]


def draw(rows):
    height = len(rows)
    pen = TTGlyphPen(None)
    for y, row in enumerate(rows):
        top = (height - y) * PX  # row 0 is the top of the em box
        bottom = top - PX
        x = 0
        while x < len(row):
            if row[x] != "#":
                x += 1
                continue
            start = x
            while x < len(row) and row[x] == "#":
                x += 1
            # clockwise outer contour (TrueType)
            pen.moveTo((start * PX, bottom))
            pen.lineTo((start * PX, top))
            pen.lineTo((x * PX, top))
            pen.lineTo((x * PX, bottom))
            pen.closePath()
    return pen.glyph()


def build(glyphs, height, gap, family, out):
    names = [".notdef"]
    outlines = {".notdef": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (0, 0)}
    cmap = {}

    for key, rows in glyphs.items():
        if key == "space":
            name, char = "space", " "
            advance = len(rows[0]) * PX
        else:
            name, char = f"uni{ord(key):04X}", key
            advance = (len(rows[0]) + gap) * PX
        names.append(name)
        outlines[name] = draw(rows)
        # left side bearing must equal the outline's xMin, or FreeType shifts
        # the glyph to line up with it (e.g. '1' and 'I' start at x = 5 px)
        ink_cols = [x for row in rows for x, p in enumerate(row) if p == "#"]
        lsb = min(ink_cols) * PX if ink_cols else 0
        metrics[name] = (advance, lsb)
        cmap[ord(char)] = name
        if char.isalpha() and char.isupper():
            cmap[ord(char.lower())] = name

    fb = FontBuilder(height * PX, isTTF=True)
    fb.setupGlyphOrder(names)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(outlines)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=height * PX, descent=0)
    fb.setupNameTable({"familyName": family, "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=height * PX, sTypoDescender=0, usWinAscent=height * PX, usWinDescent=0)
    fb.setupPost()

    os.makedirs(FONTS, exist_ok=True)
    fb.save(out)
    print(f"wrote {os.path.relpath(out)}: {len(glyphs)} glyphs, {len(cmap)} codepoints")


def main():
    small = read_glyphs(SRC, SMALL_ROWS)
    big = {name: scale_up(name, rows) for name, rows in small.items()}
    big.update(read_glyphs(BIG_OVERRIDES, BIG_ROWS))

    build(small, SMALL_ROWS, 2, "Jarl Mini", os.path.join(FONTS, "jarl_mini.ttf"))
    build(big, BIG_ROWS, 3, "Jarl Mini Big", os.path.join(FONTS, "jarl_mini_big.ttf"))

    if "--dump-big" in sys.argv:
        for name, rows in big.items():
            print(f"\nchar {name}\n" + "\n".join(rows))


if __name__ == "__main__":
    main()
