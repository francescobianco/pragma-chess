#!/usr/bin/env python3
"""Generates the application icons for every platform from one image.

    scripts/make-icons.py [source.png]

The source (default gui/qt/data/icons/pragma-chess.png) should be square with
a transparent background. Writes, next to it:

- hicolor/<size>x<size>/apps/<app id>.png  Linux icon theme (installed by CMake)
- pragma-chess.ico                          Windows executable icon
- pragma-chess.icns                         macOS bundle icon

The generated files are committed, so building needs no image tools.
Requires Pillow (apt install python3-pil).
"""

import sys
from pathlib import Path

from PIL import Image

APP_ID = "io.github.francescobianco.PragmaChess"
ICONS = Path(__file__).resolve().parent.parent / "gui" / "qt" / "data" / "icons"
LINUX_SIZES = [16, 22, 24, 32, 48, 64, 128, 256]
WINDOWS_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ICONS / "pragma-chess.png"
    image = Image.open(source).convert("RGBA")
    if image.width != image.height:
        sys.exit(f"{source} is not square ({image.width}x{image.height})")

    for size in LINUX_SIZES:
        target = ICONS / "hicolor" / f"{size}x{size}" / "apps" / f"{APP_ID}.png"
        target.parent.mkdir(parents=True, exist_ok=True)
        image.resize((size, size), Image.LANCZOS).save(target, optimize=True)

    image.save(ICONS / "pragma-chess.ico", sizes=[(s, s) for s in WINDOWS_SIZES])
    image.save(ICONS / "pragma-chess.icns")
    print(f"Icons written to {ICONS}")


if __name__ == "__main__":
    main()
