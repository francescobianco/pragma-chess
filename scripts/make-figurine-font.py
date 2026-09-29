#!/usr/bin/env python3
"""Builds gui/qt/resources/fonts/pragma-figurine.otf from SkakNew-Figurine.otf.

SkakNew-Figurine (Ulrich Dirr, CTAN fonts/chess/skaknew, LPPL) draws the
figurines on the letters K Q R B N. The desktop client writes SAN with the
Unicode chess symbols (U+2654..U+2658), so this keeps only those five glyphs,
maps them to the Unicode code points and renames the font, as the LPPL asks of
a modified version. Text falls back to the interface font for everything else.

Usage: scripts/make-figurine-font.py path/to/SkakNew-Figurine.otf
Needs fontTools (pip install fonttools, or apt install python3-fonttools).
"""
import sys
from pathlib import Path

from fontTools import subset
from fontTools.ttLib import TTFont

FIGURINES = {"K": 0x2654, "Q": 0x2655, "R": 0x2656, "B": 0x2657, "N": 0x2658}
FAMILY = "Pragma Figurine"
OUTPUT = Path(__file__).resolve().parent.parent / "gui/qt/resources/fonts/pragma-figurine.otf"


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    font = TTFont(sys.argv[1])
    options = subset.Options()
    options.name_IDs = ["*"]
    options.notdef_outline = True
    subsetter = subset.Subsetter(options)
    subsetter.populate(glyphs=list(FIGURINES))
    subsetter.subset(font)

    for table in font["cmap"].tables:
        table.cmap = {code: letter for letter, code in FIGURINES.items()}

    names = font["name"]
    for record in list(names.names):
        if record.nameID in (1, 4, 16, 21):
            names.setName(FAMILY, record.nameID, record.platformID, record.platEncID, record.langID)
        elif record.nameID in (6, 20):
            names.setName(FAMILY.replace(" ", ""), record.nameID, record.platformID, record.platEncID, record.langID)
        elif record.nameID == 3:
            names.setName(f"{FAMILY}; derived from SkakNew-Figurine 1.3", record.nameID,
                          record.platformID, record.platEncID, record.langID)
    font["CFF "].cff.fontNames = [FAMILY.replace(" ", "")]
    font["OS/2"].usFirstCharIndex = min(FIGURINES.values())
    font["OS/2"].usLastCharIndex = max(FIGURINES.values())

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    font.save(OUTPUT)
    print(f"wrote {OUTPUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
