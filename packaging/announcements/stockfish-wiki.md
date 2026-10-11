<!-- Stockfish's wiki, page "Download and usage"
(https://github.com/official-stockfish/Stockfish/wiki/Download-and-usage):
the list of free GUIs, which their authors add themselves (jose's author in
January 2025; JFXChess's in May 2026, "shameless self-plug"). Open the page,
Edit, and make the two changes below; commit message: "Added Pragma Chess
GUI information (author's self-plug)". Any GitHub account can edit it.
DISTRIBUTING.md, "Stockfish's wiki". -->

## 1. In the table under "Download a Chess GUI › Free › Computer"

The JFXChess row has an empty right cell: replace its two lines with

```markdown
| **[JFXChess](https://github.com/asdfjkl/jfxchess/releases)** ([source code](https://github.com/asdfjkl/jfxchess)) <br>[How to install Stockfish](#jfxchess) | **[Pragma Chess](https://yafb.net/pragma-chess/)** ([source code](https://github.com/francescobianco/pragma-chess)) <br>[How to install Stockfish](#pragma-chess) |
| ![jfxchess](https://raw.githubusercontent.com/asdfjkl/jerry/master/jfxchess.png) | ![pragma-chess](https://yafb.net/pragma-chess/assets/screenshots/main-thumb.png) |
```

## 2. Under "Install in a Chess GUI", after the jfxchess section, before `---`

```markdown
### Pragma Chess

Stockfish is already bundled with Pragma Chess (built with the small network only, to keep the installer small). To use another version of Stockfish, such as the official release at full strength:

1. Menu `Engine` > `Manage Engines…`
2. click on `Add`, then `Browse…` and select the Stockfish executable of your choice
3. set `Threads` and `Hash`, then click `Use This Engine`
```
