#!/usr/bin/env python3
"""Add the touch control icons to FanFont.ttf (needs fontTools: pip install fonttools).

Paths are the Figma icons (file "Homewind", 32 px frames), drawn at 1 px = 32 units
around the centre of the existing icons (512, 448), so they match icon "36" (size 32).
Re-running replaces the glyphs. Afterwards run generate_fonts.sh.

  '7' plus (1241:4990)  '8' minus (1241:4989)  '9' bolt (1241:4991)  ':' gear (1241:4992)
"""
import math
import os

from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.svgLib.path import parse_path
from fontTools.ttLib import TTFont

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "FanFont.ttf")
SCALE = 32
CENTRE = (512, 448)


def ring(cx, cy, r):
    return f"M{cx - r} {cy}A{r} {r} 0 1 0 {cx + r} {cy}A{r} {r} 0 1 0 {cx - r} {cy}Z"


# (char, glyph name, filled paths, hole paths) in SVG pixels, y down
ICONS = [
    ("7", "plus", ["M12 1H20V12H31V20H20V31H12V20H1V12H12V1Z"], []),
    ("8", "minus", ["M1 12H31V20H1V12Z"], []),
    ("9", "bolt", ["M17.25 3.5L6 18.5H13.5L12.25 28.5L23.5 13.5H16L17.25 3.5Z"], []),
    (":", "gear", [
        "M17.8332 5H14.1665V10.5H17.8332V5Z",
        "M25.0747 9.51851L22.4819 6.92578L18.5928 10.8149L21.1856 13.4076L25.0747 9.51851Z",
        "M27 17.8337V14.167H21.5V17.8337H27Z",
        "M22.482 25.0742L25.0747 22.4814L21.1856 18.5924L18.5929 21.1851L22.482 25.0742Z",
        "M14.1668 27H17.8335V21.5H14.1668V27Z",
        "M6.92534 22.4815L9.51807 25.0742L13.4072 21.1851L10.8144 18.5924L6.92534 22.4815Z",
        "M5 14.1663L5 17.833H10.5V14.1663H5Z",
        "M9.51802 6.92583L6.92529 9.51855L10.8144 13.4076L13.4071 10.8149L9.51802 6.92583Z",
        # ring: circle r 5.5 with stroke 3.66667
        ring(16, 16, 5.5 + 3.66667 / 2),
    ], [ring(16, 16, 5.5 - 3.66667 / 2)]),
]


def contours(svg_path):
    """SVG path -> list of contours in font units (each a RecordingPen value list)."""
    rec = RecordingPen()
    t = (SCALE, 0, 0, -SCALE, CENTRE[0] - 16 * SCALE, CENTRE[1] + 16 * SCALE)
    parse_path(svg_path, TransformPen(rec, t))
    out, cur = [], []
    for op, args in rec.value:
        cur.append((op, args))
        if op in ("closePath", "endPath"):
            out.append(cur)
            cur = []
    return out


def area(contour):
    pts = [p for _, args in contour for p in args]
    return sum(x0 * y1 - x1 * y0 for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1])) / 2


def reverse(contour):
    pts = []
    for op, args in contour:
        if op in ("moveTo", "lineTo"):
            pts.append(("on", args[0]))
        elif op == "qCurveTo":
            pts.extend(("off", p) for p in args[:-1])
            pts.append(("on", args[-1]))
        elif op == "curveTo":
            raise ValueError("cubic curves are converted before reversing")
    pts = pts[::-1]
    out = [("moveTo", (pts[0][1],))]
    offs = []
    for kind, p in pts[1:] + pts[:1]:
        if kind == "off":
            offs.append(p)
        else:
            out.append(("qCurveTo", (*offs, p)) if offs else ("lineTo", (p,)))
            offs = []
    out.append(("closePath", ()))
    return out


def to_quadratic(contour):
    """Arcs come in as cubics; TrueType needs quadratics."""
    from fontTools.pens.cu2quPen import Cu2QuPen
    rec = RecordingPen()
    pen = Cu2QuPen(rec, max_err=1.0, reverse_direction=False)
    for op, args in contour:
        getattr(pen, op)(*args)
    return rec.value


def build(filled, holes):
    pen = TTGlyphPen(None)
    for paths, clockwise in ((filled, True), (holes, False)):
        for svg in paths:
            for c in contours(svg):
                c = to_quadratic(c)
                # TrueType: filled contours clockwise (negative area with y up)
                if (area(c) < 0) != clockwise:
                    c = reverse(c)
                for op, args in c:
                    getattr(pen, op)(*args)
    return pen.glyph()


def main():
    font = TTFont(FONT)
    order = font.getGlyphOrder()
    glyf = font["glyf"]
    cmap = font.getBestCmap()
    for char, name, filled, holes in ICONS:
        # post format 3 stores no names: a glyph added before comes back named after its char
        name = cmap.get(ord(char), name)
        glyph = build(filled, holes)
        glyph.recalcBounds(glyf)
        if name not in order:
            order.append(name)
        glyf[name] = glyph
        font["hmtx"][name] = (1024, glyph.xMin)
        for table in font["cmap"].tables:
            if table.isUnicode():
                table.cmap[ord(char)] = name
        print(f"{char} {name}: {glyph.xMin},{glyph.yMin} .. {glyph.xMax},{glyph.yMax}")
    font.setGlyphOrder(order)
    font.save(FONT)


if __name__ == "__main__":
    main()
