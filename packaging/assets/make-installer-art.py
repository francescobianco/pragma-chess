#!/usr/bin/env python3
"""Draws the artwork of the installers from the application icon.

    packaging/assets/make-installer-art.py

Writes, next to the packaging scripts that use them:

- macos/dmg-background.png, macos/dmg-background@2x.png   disk image window
- windows/wizard-image*.bmp, windows/wizard-small*.bmp    Inno Setup wizard (the
  first: the shoulder of every page, the welcome window's)

The generated files are committed, so building the installers needs no image
tools. Requires Pillow (apt install python3-pil) and the Lato font
(apt install fonts-lato).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
ICON = ROOT.parent / "gui" / "qt" / "data" / "icons" / "pragma-chess.png"
RICH_LOGO = ROOT.parent / "gui" / "qt" / "data" / "icons" / "pragma-chess-rich.png"
STUDY = ROOT.parent / "gui" / "qt" / "resources" / "welcome" / "chess-study.png"
BOOK_FONT = ROOT.parent / "gui" / "qt" / "resources" / "fonts" / "crimson-pro.ttf"
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
    """The shoulder on the left of every page of the wizard: the welcome
    window's (dialogs/WelcomeDialog), at the size of the installer — the
    picture of a study cut to fill it, a veil deep at the bottom, the rich
    logo on its tile, "Pragma" and "Chess" in the book face and a thin rule.
    """
    scale = width / 400  # The welcome's shoulder is 400 pixels wide.
    picture = Image.open(STUDY).convert("RGB")
    cover = max(width / picture.width, height / picture.height)
    picture = picture.resize((round(picture.width * cover), round(picture.height * cover)), Image.LANCZOS)
    left, top = (picture.width - width) // 2, (picture.height - height) // 2
    image = picture.crop((left, top, left + width, top + height)).convert("RGBA")
    # The veil: light at the top, deep at the bottom, for the name to read on.
    veil = Image.new("RGBA", (width, height))
    stops = [(0.0, 70), (0.45, 40), (0.72, 175), (1.0, 235)]
    for y in range(height):
        t = y / max(1, height - 1)
        for (t0, a0), (t1, a1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                alpha = a0 + (a1 - a0) * (t - t0) / (t1 - t0)
                break
        ImageDraw.Draw(veil).line([(0, y), (width, y)], fill=(12, 9, 6, round(alpha)))
    image.alpha_composite(veil)

    margin = round(32 * scale)
    size = scale * 1.35  # A little larger than the welcome's: the shoulder is smaller.
    title = ImageFont.truetype(str(BOOK_FONT), round(58 * size))
    tagline = font("Regular", max(8, round(12 * size)))
    ascent, descent = title.getmetrics()
    line = (ascent + descent) * 0.88
    tag_ascent, tag_descent = tagline.getmetrics()
    tagline_top = height - margin - (tag_ascent + tag_descent)
    chess_baseline = tagline_top - round(14 * size) - descent
    pragma_baseline = chess_baseline - line
    logo_side = round(88 * size)
    logo_top = round(pragma_baseline - ascent - 18 * size - logo_side)
    # The logo on a white tile, with the welcome's soft shadow under it.
    shadow = Image.new("RGBA", (width, height))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [margin - 2, logo_top + 1, margin + logo_side + 2, logo_top + logo_side + 5],
        radius=round(18 * size), fill=(0, 0, 0, 90))
    image.alpha_composite(shadow)
    logo = Image.open(RICH_LOGO).convert("RGBA").resize((logo_side, logo_side), Image.LANCZOS)
    image.alpha_composite(logo, (margin, logo_top))
    draw = ImageDraw.Draw(image)
    cream = (0xfa, 0xf6, 0xee, 255)
    draw.text((margin - 1, pragma_baseline), "Pragma", font=title, fill=cream, anchor="ls")
    draw.text((margin - 1, chess_baseline), "Chess", font=title, fill=cream, anchor="ls")
    # A thin rule, as a book's title page has, then the motto — in English,
    # the picture being one for every language.
    rule_y = tagline_top - round(6 * size)
    draw.line([(margin, rule_y), (margin + round(48 * size), rule_y)], fill=(0xe8, 0xc9, 0x8a, 200),
              width=max(1, round(size)))
    x = margin
    for character in "STUDY · TRAIN · PLAY":
        draw.text((x, tagline_top), character, font=tagline, fill=(0xfa, 0xf6, 0xee, 200))
        x += tagline.getlength(character) + 2.2 * size
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
    # Inno Setup picks the size closest to the screen's scaling; the shoulder
    # is as tall as the whole window (pragma-chess.iss stretches it there).
    for percent, (w, h) in {100: (164, 360), 150: (246, 540), 200: (328, 720)}.items():
        wizard_image(w, h).save(windows / f"wizard-image-{percent}.bmp")
    for percent, (w, h) in {100: (55, 58), 150: (83, 87), 200: (110, 116)}.items():
        wizard_small(w, h).save(windows / f"wizard-small-{percent}.bmp")


if __name__ == "__main__":
    main()
