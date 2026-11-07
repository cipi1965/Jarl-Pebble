#!/usr/bin/env python3
"""Build resources/fonts/jarl_mini.ttf from tools/jarl_mini_glyphs.txt.

Each glyph is a 17-row pixel grid ('#' = ink). Every horizontal run of ink
becomes a rectangle on an exact pixel grid (64 font units per pixel, em =
17 px), so Pebble's 1-bit glyph rasterizer reproduces the grid exactly when
the font resource is built at 17 px.

Lowercase letters reuse the uppercase glyphs.

Usage (needs fontTools):
    uvx --with fonttools python tools/build_mini_font.py
"""

import os
import sys

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

ROWS = 17
PX = 64  # font units per pixel
GAP = 2  # pixels between glyphs (matches the original day-name bitmaps)

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "jarl_mini_glyphs.txt")
OUT = os.path.join(HERE, "..", "resources", "fonts", "jarl_mini.ttf")


def read_glyphs(path):
    glyphs, cur = {}, None
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("char "):
                cur = line[5:]
                glyphs[cur] = []
            elif cur is not None and line and set(line) <= set("#."):
                glyphs[cur].append(line)
    for name, rows in glyphs.items():
        if len(rows) != ROWS or len({len(r) for r in rows}) != 1:
            sys.exit(f"glyph {name!r}: expected {ROWS} rows of equal width")
    return glyphs


def draw(rows):
    pen = TTGlyphPen(None)
    for y, row in enumerate(rows):
        top = (ROWS - y) * PX  # row 0 is the top of the em box
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


def main():
    glyphs = read_glyphs(SRC)

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
            advance = (len(rows[0]) + GAP) * PX
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

    fb = FontBuilder(ROWS * PX, isTTF=True)
    fb.setupGlyphOrder(names)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(outlines)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ROWS * PX, descent=0)
    fb.setupNameTable({"familyName": "Jarl Mini", "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=ROWS * PX, sTypoDescender=0, usWinAscent=ROWS * PX, usWinDescent=0)
    fb.setupPost()

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    fb.save(OUT)
    print(f"wrote {os.path.relpath(OUT)}: {len(glyphs)} glyphs, {len(cmap)} codepoints")


if __name__ == "__main__":
    main()
