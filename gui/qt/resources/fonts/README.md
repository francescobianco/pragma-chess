# Figurine font

`pragma-figurine.otf` ("Pragma Figurine") draws the figurines of the move list
(♔ ♕ ♖ ♗ ♘), the same used in the variations of books typeset with LaTeX's
`skak` package. It holds only those five glyphs; every other character comes
from the interface font.

It is derived from **SkakNew-Figurine** 1.3 by Ulrich Dirr (after designs by
Piet Tutelaers, Torben Hoffmann and Dirk Bächle), distributed on CTAN as
[fonts/chess/skaknew](https://ctan.org/pkg/skaknew) under the LaTeX Project
Public License 1.2. As the LPPL asks of a modified version, it is renamed: the
glyphs are unchanged, only mapped from the letters K Q R B N to the Unicode
chess symbols U+2654–U+2658.

To rebuild it, download `skaknew.zip` from CTAN and run:

```bash
scripts/make-figurine-font.py skaknew/SkakNew-Figurine.otf
```

# Paragraph font

`crimson-pro.ttf` is **Crimson Pro**, by The Crimson Pro Project Authors
(designed by Jacques Le Bailly), from
[google/fonts](https://github.com/google/fonts/tree/main/ofl/crimsonpro),
under the SIL Open Font License 1.1 (`crimson-pro-OFL.txt`). It sets the
paragraphs written between the moves of a chapter, as a book would.

It is the ExtraLight instance (weight 200) of the variable font, made static
so every platform draws it the same:

```bash
python3 -c 'from fontTools.ttLib import TTFont; from fontTools.varLib import instancer
instancer.instantiateVariableFont(TTFont("CrimsonPro[wght].ttf"), {"wght": 200},
    updateFontNames=True).save("crimson-pro.ttf")'
```
