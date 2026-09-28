#!/usr/bin/env python3
"""Draws the artwork of the installers from the application icon.

    packaging/assets/make-installer-art.py

Writes, next to the packaging scripts that use them:

- macos/dmg-background.png, macos/dmg-background@2x.png   disk image window
- windows/wizard-image*.bmp, windows/wizard-small*.bmp    Inno Setup wizard

The generated files are committed, so building the installers needs no image
tools. Requires Pillow (apt install python3-pil) and the Lato font
(apt install fonts-lato).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
ICON = ROOT.parent / "gui" / "qt" / "data" / "icons" / "pragma-chess.png"
FONTS = Path("/usr/share/fonts/truetype/lato")

PAPER = (246, 243, 236)
LIGHT_SQUARE = (238, 233, 222)
DARK_SQUARE = (226, 219, 204)
INK = (38, 38, 42)
MUTED = (120, 116, 108)
ACCENT = (53, 132, 228)  # the blue of Explain's reply arrows


def font(name, size):
    return ImageFont.truetype(str(FONTS / f"Lato-{name}.ttf"), size)


def checker(image, box, square):
    """Paints a faint chessboard over box, fading towards its top edge."""
    x0, y0, x1, y1 = box
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    rows = (y1 - y0 + square - 1) // square
    for row in range(rows):
        alpha = int(255 * (row + 1) / rows)
        for column in range((x1 - x0 + square - 1) // square):
            colour = DARK_SQUARE if (row + column) % 2 else LIGHT_SQUARE
            top = y1 - (rows - row) * square
            draw.rectangle([x0 + column * square, top, x0 + (column + 1) * square - 1,
                            top + square - 1], fill=colour + (alpha,))
    image.alpha_composite(overlay)


def arrow(draw, start, end, width, colour):
    (x0, y), (x1, _) = start, end
    head = width * 3
    draw.line([(x0, y), (x1 - head, y)], fill=colour, width=width)
    draw.polygon([(x1, y), (x1 - head, y - head * 0.8), (x1 - head, y + head * 0.8)], fill=colour)


def dmg_background(scale):
    """The window of the disk image: the app on the left, Applications on the right.

    Icons sit at (170, 190) and (490, 190) in a 660x400 window (see macos/build-dmg.sh).
    """
    width, height = 660 * scale, 400 * scale
    image = Image.new("RGBA", (width, height), PAPER + (255,))
    checker(image, (0, height - 70 * scale, width, height), 35 * scale)
    draw = ImageDraw.Draw(image)
    title = font("Bold", 26 * scale)
    subtitle = font("Regular", 14 * scale)
    draw.text((width // 2, 52 * scale), "Pragma Chess", font=title, fill=INK, anchor="mm")
    draw.text((width // 2, 84 * scale), "Drag the app to Applications to install it",
              font=subtitle, fill=MUTED, anchor="mm")
    arrow(draw, (262 * scale, 190 * scale), (398 * scale, 190 * scale), 4 * scale, ACCENT)
    return image.convert("RGB")


def wizard_image(width, height):
    """The tall picture on the left of the first and last pages of the wizard."""
    image = Image.new("RGBA", (width, height), PAPER + (255,))
    checker(image, (0, height * 3 // 5, width, height), width // 4)
    logo = Image.open(ICON).convert("RGBA")
    side = width * 3 // 5
    logo = logo.resize((side, side), Image.LANCZOS)
    image.alpha_composite(logo, ((width - side) // 2, height // 6))
    draw = ImageDraw.Draw(image)
    draw.text((width // 2, height // 6 + side + width // 7), "Pragma Chess",
              font=font("Bold", width // 8), fill=INK, anchor="mm")
    return image.convert("RGB")


def wizard_small(width, height):
    """The icon at the top right of the inner pages of the wizard."""
    image = Image.new("RGBA", (width, height), (255, 255, 255, 255))
    logo = Image.open(ICON).convert("RGBA")
    side = min(width, height) * 7 // 8
    logo = logo.resize((side, side), Image.LANCZOS)
    image.alpha_composite(logo, ((width - side) // 2, (height - side) // 2))
    return image.convert("RGB")


def main():
    macos, windows = ROOT / "macos", ROOT / "windows"
    dmg_background(1).save(macos / "dmg-background.png")
    dmg_background(2).save(macos / "dmg-background@2x.png")
    # Inno Setup picks the size closest to the screen's scaling.
    for percent, (w, h) in {100: (164, 314), 150: (246, 471), 200: (328, 628)}.items():
        wizard_image(w, h).save(windows / f"wizard-image-{percent}.bmp")
    for percent, (w, h) in {100: (55, 58), 150: (83, 87), 200: (110, 116)}.items():
        wizard_small(w, h).save(windows / f"wizard-small-{percent}.bmp")


if __name__ == "__main__":
    main()
