# Opening books

`pragma-openings.bin` is the Polyglot book seeded into the user's Books folder
on first launch. It is built with `pragma-book` from the named openings of
[lichess-org/chess-openings](https://github.com/lichess-org/chess-openings),
a public domain (CC0) collection: each move weighs as many named openings as
pass through it.

To rebuild it:

```bash
for f in a b c d e; do
  curl -sfLO https://raw.githubusercontent.com/lichess-org/chess-openings/master/$f.tsv
done
./build/gui/qt/pragma-book -o gui/qt/resources/books/pragma-openings.bin a.tsv b.tsv c.tsv d.tsv e.tsv
```
