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
FIXED_TIMESTAMP = 3876854400  # 2026-11-07, seconds since 1904 (TrueType epoch)

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


DIAGONAL = set("KVXYZ%/")


def scale_up(name, rows):
    if name == "/":
        return slash(12, BIG_ROWS, 4)
    width = len(rows[0])
    col_map = COL_MAPS.get(width) or [int(c * width / (width * 4 // 3)) for c in range(width * 4 // 3)]
    scaled = ["".join(rows[r][c] for c in col_map) for r in ROW_MAP]
    # diagonal strokes are staircases, which the corner rules would pick apart
    return scaled if name in DIAGONAL else round_corners(scaled)


def round_corners(rows):
    """Rounder curves for the big glyphs (Pebble fonts are 1-bit, so the
    smoothing has to be in the pixels).

    Outer corners: a 1 px corner cut becomes a 2-step curve
        .####     ..###
        #####  -> .####
        #####     #####
    Inner corners: a 1 px fillet becomes 2 px.
    Both only apply where the edges run on long enough that the corner is a
    real curve, so stem tips (I, 1, H legs) and narrow gaps (M, W) keep their
    shape, and deliberately square corners (no cut / no fillet) stay square.
    """
    h, w = len(rows), len(rows[0])
    src = [list(r) for r in rows]
    out = [list(r) for r in rows]

    def ink(y, x):
        return 0 <= y < h and 0 <= x < w and src[y][x] == "#"

    def run(y, x, dy, dx, want):
        n = 0
        while 0 <= y < h and 0 <= x < w and (src[y][x] == "#") == want:
            n, y, x = n + 1, y + dy, x + dx
        return n

    def clear(y, x, dy, dx, n):
        """n pixels from (y, x) along (dy, dx) are empty (off the glyph counts)."""
        return all(not ink(y + i * dy, x + i * dx) for i in range(n))

    for y in range(h):
        for x in range(w):
            for dy in (1, -1):  # direction into the glyph, vertically
                for dx in (1, -1):  # and horizontally
                    # outer corner: empty cut pixel with ink beside and below
                    # it, on the outside of both edges (open space along the
                    # edge, not the stepped inside of a curve like D's)
                    if (not ink(y, x) and not ink(y - dy, x) and not ink(y, x - dx)
                            and ink(y, x + dx) and ink(y + dy, x) and ink(y + dy, x + dx)
                            and run(y, x + dx, 0, dx, True) >= 5
                            and run(y + dy, x, dy, 0, True) >= 5
                            and clear(y - dy, x, 0, dx, 5)
                            and clear(y, x - dx, dy, 0, 5)):
                        out[y][x + dx] = "."
                        out[y + dy][x] = "."
                    # inner corner: a fillet pixel inside an opening
                    if (ink(y, x) and ink(y - dy, x) and ink(y, x - dx)
                            and not ink(y, x + dx) and not ink(y + dy, x)
                            and not ink(y + dy, x + dx)
                            and ink(y - dy, x + dx) and ink(y + dy, x - dx)
                            and run(y, x + dx, 0, dx, False) >= 4
                            and run(y + dy, x, dy, 0, False) >= 4):
                        out[y][x + dx] = "#"
                        out[y + dy][x] = "#"
    return ["".join(r) for r in out]


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
            cp = ord(key)
            name, char = (f"uni{cp:04X}" if cp <= 0xFFFF else f"u{cp:05X}"), key
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
    # fixed timestamps so rebuilding without glyph changes gives identical files
    fb.updateHead(created=FIXED_TIMESTAMP, modified=FIXED_TIMESTAMP)
    fb.font.recalcTimestamp = False

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
