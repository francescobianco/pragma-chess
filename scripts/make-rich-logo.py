#!/usr/bin/env python3
"""Draws the rich logo: the logo's board with a P of pawns on it.

    scripts/make-rich-logo.py

The logo (gui/qt/data/icons/pragma-chess.png) is a 4x4 board; the rich one
puts pawns on seven of its squares so they draw a P — the second column,
and the bowl in the three rows from the top:

    0110
    0101
    0110
    0100

each pawn centred on its whole square, the same for all — the grid
commands, even where the frame covers part of the square — and the
negative of what lies under it: white on the dark squares and on the
frame, black on the light ones. It is shown where the logo is large (the
welcome window); small sizes, where a pawn would be a dot, keep the plain
board. Writes
gui/qt/data/icons/pragma-chess-rich.png, committed like the other icons.
The pawn is the one of the folder and file icons (FolderIcon::pawnPath).
Requires Pillow (apt install python3-pil).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

ICONS = Path(__file__).resolve().parent.parent / "gui" / "qt" / "data" / "icons"
P = ["0110", "0101", "0110", "0100"]
SCALE = 4  # Drawn four times larger, then reduced: smooth edges.

# The board of the 300-pixel logo: the squares' edges (the frame covers
# part of the outer ones).
EDGES = [18, 84, 150, 216, 282]


def cubic(p0, p1, p2, p3, steps=24):
    points = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        points.append((u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
                       u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]))
    return points


def pawn(draw, left, top, side, colour):
    """FolderIcon::pawnPath in the square at (left, top), `side` wide."""
    def at(x, y):
        return (left + x * side, top + y * side)

    def rounded(x, y, w, h, r):
        draw.rounded_rectangle([at(x, y), at(x + w, y + h)], radius=r * side, fill=colour)

    cx, cy, r = 0.5, 0.16, 0.15
    draw.ellipse([at(cx - r, cy - r), at(cx + r, cy + r)], fill=colour)  # The head.
    rounded(0.25, 0.345, 0.50, 0.105, 0.05)  # The collar.
    body = [at(0.40, 0.48), at(0.60, 0.48)]  # The body, flaring to the base.
    body += [at(*p) for p in cubic((0.60, 0.48), (0.61, 0.66), (0.70, 0.79), (0.77, 0.87))]
    body += [at(0.23, 0.87)]
    body += [at(*p) for p in cubic((0.23, 0.87), (0.30, 0.79), (0.39, 0.66), (0.40, 0.48))]
    draw.polygon(body, fill=colour)
    rounded(0.18, 0.90, 0.64, 0.10, 0.035)  # The base.


def main():
    logo = Image.open(ICONS / "pragma-chess.png").convert("RGBA")
    big = logo.resize((logo.width * SCALE, logo.height * SCALE), Image.NEAREST)
    # The pawns as a mask: each the same, centred on its whole square — the
    # grid commands, even where the frame covers the square.
    mask = Image.new("L", big.size, 0)
    draw = ImageDraw.Draw(mask)
    for row, line in enumerate(P):
        for column, cell in enumerate(line):
            if cell != "1":
                continue
            size = EDGES[1] - EDGES[0]
            side = 0.74 * size
            x = EDGES[column] + (size - side) / 2
            y = EDGES[row] + (size - side) / 2
            pawn(draw, x * SCALE, y * SCALE, side * SCALE, 255)
    # Each pawn is the negative of what lies under it: white on the dark
    # squares and on the frame, black on the light squares.
    rgb, alpha = big.convert("RGB"), big.getchannel("A")
    inverted = ImageOps.invert(rgb)
    painted = Image.composite(inverted, rgb, mask)
    painted.putalpha(alpha)
    painted.resize(logo.size, Image.LANCZOS).save(ICONS / "pragma-chess-rich.png", optimize=True)


if __name__ == "__main__":
    main()
