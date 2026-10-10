#!/usr/bin/env python3
"""Draws the rich logo: the logo's board with a P of pawns on it.

    scripts/make-rich-logo.py

The logo (gui/qt/data/icons/pragma-chess.png) is a 4x4 board; the rich one
puts pawns on seven of its squares so they draw a P — the left column, and
the bowl in the three rows from the top:

    1100
    1010
    1100
    1000

each pawn the negative of its square, white on the dark ones and black on
the light ones. It is shown where the logo is large (the welcome window);
small sizes, where a pawn would be a dot, keep the plain board. Writes
gui/qt/data/icons/pragma-chess-rich.png, committed like the other icons.
The pawn is the one of the folder and file icons (FolderIcon::pawnPath).
Requires Pillow (apt install python3-pil).
"""

from pathlib import Path

from PIL import Image, ImageDraw

ICONS = Path(__file__).resolve().parent.parent / "gui" / "qt" / "data" / "icons"
P = ["1100", "1010", "1100", "1000"]
SCALE = 4  # Drawn four times larger, then reduced: smooth edges.

# The board of the 300-pixel logo: the squares' edges, and what the frame
# leaves of the outer ones.
EDGES = [18, 84, 150, 216, 282]
INNER = (27, 273)


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
    draw = ImageDraw.Draw(big)
    for row, line in enumerate(P):
        for column, cell in enumerate(line):
            if cell != "1":
                continue
            # The part of the square the frame leaves visible.
            left = max(EDGES[column], INNER[0])
            right = min(EDGES[column + 1], INNER[1])
            top = max(EDGES[row], INNER[0])
            bottom = min(EDGES[row + 1], INNER[1])
            side = 0.74 * min(right - left, bottom - top)
            cx, cy = (left + right) / 2, (top + bottom) / 2
            if row in (0, 3) and column in (0, 3):
                # A corner square: the frame rounds it off, the pawn keeps clear.
                side *= 0.88
                cx += 3 if column == 0 else -3
                cy += 3 if row == 0 else -3
            x = cx - side / 2
            y = cy - side / 2
            dark = (row + column) % 2 == 0
            pawn(draw, x * SCALE, y * SCALE, side * SCALE, (255, 255, 255, 255) if dark else (0, 0, 0, 255))
    big.resize(logo.size, Image.LANCZOS).save(ICONS / "pragma-chess-rich.png", optimize=True)


if __name__ == "__main__":
    main()
