# Opening names

`opening-names.pdb` is the Opening Names database seeded into the user's
Databases folder on first launch, ready made so that the first launch is not
spent building it. Each game in it is a named line: Event is the name, ECO the
code, and the name belongs to the position where the line ends.

`lichess-openings.tsv` is what it was built from, the named openings of
[lichess-org/chess-openings](https://github.com/lichess-org/chess-openings), a
public domain (CC0) collection. It is still shipped, and the app falls back to
building the database from it if the ready-made one cannot be copied.

To rebuild `opening-names.pdb` after updating the TSV, let the fallback do it:

```bash
rm gui/qt/resources/openings/opening-names.pdb
touch gui/qt/CMakeLists.txt && make build          # builds without the seed
rm -rf /tmp/pragma-seed
PRAGMA_CHESS_DIR=/tmp/pragma-seed ./build/gui/qt/pragma-chess   # quit once it opens
cp "/tmp/pragma-seed/Databases/Opening Names.pdb" gui/qt/resources/openings/opening-names.pdb
make build
```
