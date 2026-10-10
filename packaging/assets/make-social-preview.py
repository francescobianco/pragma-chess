#!/usr/bin/env python3
"""Draws the GitHub social preview (1280x640): the site's hero screenshot
with a band carrying the logo, the name and the claim. Upload the result in
the repository's Settings > Social preview.

    python3 packaging/assets/make-social-preview.py
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
HERO = ROOT / "site/assets/screenshots/hero.png"
LOGO = ROOT / "gui/qt/data/icons/pragma-chess-rich.png"
OUT = ROOT / "packaging/assets/social-preview.png"
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
REGULAR = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

WIDTH, HEIGHT, BAND = 1280, 640, 150


def main():
    hero = Image.open(HERO).convert("RGB")
    # The window from its top, at the card's width.
    scale = WIDTH / hero.width
    hero = hero.resize((WIDTH, round(hero.height * scale)), Image.LANCZOS)
    card = hero.crop((0, 0, WIDTH, HEIGHT))

    band = Image.new("RGBA", (WIDTH, BAND), (24, 24, 24, 235))
    card.paste(band, (0, HEIGHT - BAND), band)
    draw = ImageDraw.Draw(card)
    logo = Image.open(LOGO).convert("RGBA").resize((110, 110), Image.LANCZOS)
    card.paste(logo, (40, HEIGHT - BAND + 20), logo)
    draw.text((175, HEIGHT - BAND + 22), "Pragma Chess", font=ImageFont.truetype(BOLD, 52), fill=(255, 255, 255))
    draw.text((177, HEIGHT - BAND + 90),
              "Open source chess database that explains the engine",
              font=ImageFont.truetype(REGULAR, 28), fill=(220, 220, 220))
    card.save(OUT, optimize=True)
    print(OUT)


if __name__ == "__main__":
    main()
