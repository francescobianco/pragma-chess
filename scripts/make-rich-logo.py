#!/usr/bin/env python3
"""Draws the rich logo: the logo's board with chess pieces on it.

    scripts/make-rich-logo.py

The logo (gui/qt/data/icons/pragma-chess.png) is a 4x4 board; the rich one
puts pieces on four of its squares (N knight, P pawn, R rook):

    0N00
    000P
    0N00
    0R00

each centred on its whole square, the same size for all — the grid
commands, even where the frame covers part of the square — and the
negative of what lies under it: white on the dark squares and on the
frame, black on the light ones. It is shown where the logo is large (the
welcome window); small sizes, where a pawn would be a dot, keep the plain
board. Writes
gui/qt/data/icons/pragma-chess-rich.png, committed like the other icons.
The pawn is the one of the folder and file icons (FolderIcon::pawnPath);
the knight and the rook are drawn in its style.
Requires Pillow (apt install python3-pil).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

ICONS = Path(__file__).resolve().parent.parent / "gui" / "qt" / "data" / "icons"
# The pieces on the board, by row from the top: N knight, P pawn, R rook.
BOARD = ["0N00", "000P", "0N00", "0R00"]
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


def rook(draw, left, top, side, colour):
    """A rook in the pawn's style: the crown with its notches, the body, the base."""
    def at(x, y):
        return (left + x * side, top + y * side)

    crown = [(0.26, 0.04), (0.36, 0.04), (0.36, 0.13), (0.45, 0.13), (0.45, 0.04), (0.55, 0.04), (0.55, 0.13),
             (0.64, 0.13), (0.64, 0.04), (0.74, 0.04), (0.74, 0.27), (0.26, 0.27)]
    draw.polygon([at(*p) for p in crown], fill=colour)
    body = [(0.35, 0.31), (0.65, 0.31), (0.69, 0.84), (0.31, 0.84)]
    draw.polygon([at(*p) for p in body], fill=colour)
    draw.rounded_rectangle([at(0.18, 0.88), at(0.82, 1.0)], radius=0.035 * side, fill=colour)


def knight(draw, left, top, side, colour, hole):
    """A knight in the pawn's style, facing left: the head and neck, the base."""
    def at(x, y):
        return (left + x * side, top + y * side)

    head = [(0.30, 0.84), (0.31, 0.70), (0.36, 0.60), (0.44, 0.50), (0.38, 0.49), (0.30, 0.53), (0.24, 0.56),
            (0.16, 0.55), (0.12, 0.49), (0.13, 0.43), (0.20, 0.34), (0.28, 0.24), (0.36, 0.16), (0.42, 0.11),
            (0.46, 0.01), (0.52, 0.09), (0.60, 0.13), (0.68, 0.20), (0.74, 0.30), (0.78, 0.44), (0.78, 0.60),
            (0.75, 0.74), (0.72, 0.84)]
    draw.polygon([at(*p) for p in head], fill=colour)
    draw.ellipse([at(0.36, 0.23), at(0.43, 0.30)], fill=hole)  # The eye.
    draw.rounded_rectangle([at(0.18, 0.88), at(0.82, 1.0)], radius=0.035 * side, fill=colour)


def main():
    logo = Image.open(ICONS / "pragma-chess.png").convert("RGBA")
    big = logo.resize((logo.width * SCALE, logo.height * SCALE), Image.NEAREST)
    # The pieces as a mask: each the same size, centred on its whole square
    # — the grid commands, even where the frame covers the square.
    mask = Image.new("L", big.size, 0)
    draw = ImageDraw.Draw(mask)
    for row, line in enumerate(BOARD):
        for column, cell in enumerate(line):
            if cell == "0":
                continue
            size = EDGES[1] - EDGES[0]
            side = 0.74 * size
            x = (EDGES[column] + (size - side) / 2) * SCALE
            y = (EDGES[row] + (size - side) / 2) * SCALE
            if cell == "P":
                pawn(draw, x, y, side * SCALE, 255)
            elif cell == "N":
                knight(draw, x, y, side * SCALE, 255, 0)
            elif cell == "R":
                rook(draw, x, y, side * SCALE, 255)
    # Each piece is the negative of what lies under it: white on the dark
    # squares and on the frame, black on the light squares.
    rgb, alpha = big.convert("RGB"), big.getchannel("A")
    inverted = ImageOps.invert(rgb)
    painted = Image.composite(inverted, rgb, mask)
    painted.putalpha(alpha)
    painted.resize(logo.size, Image.LANCZOS).save(ICONS / "pragma-chess-rich.png", optimize=True)


if __name__ == "__main__":
    main()
