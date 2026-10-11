<!-- Stockfish's wiki, page "Download and usage"
(https://github.com/official-stockfish/Stockfish/wiki/Download-and-usage):
the list of free GUIs. Authors added themselves in the past (jose in January
2025, JFXChess in May 2026, "shameless self-plug"), but on 2026-10-11 the
wiki takes changes from Stockfish's collaborators only (a push was refused,
403). So we ask: the request below goes to Stockfish's Discussions
(https://github.com/official-stockfish/Stockfish/discussions, category
General), with the two changes it proposes. Best after 0.4.0, which has Help
▸ Manage Extensions installing the official Stockfish. DISTRIBUTING.md,
"Stockfish's wiki". -->

## The request (Stockfish's Discussions)

Title: Add Pragma Chess to the GUIs in "Download and usage"?

> Hi, I am the author of Pragma Chess, a free and open source (MIT) chess
> GUI and database for Windows, macOS and Linux, written in C++ with Qt 6:
> https://yafb.net/pragma-chess/ — source:
> https://github.com/francescobianco/pragma-chess
>
> It bundles Stockfish (built from the release's source, with the small
> network to keep the installer small; the source ships with every release)
> and can install the official release at full strength. Would you consider
> listing it among the free GUIs of the wiki's "Download and usage" page? The
> wiki is open to collaborators only, so here are the two changes, ready to
> paste — or tell me if you would rather I did something else.
>
> (then the two blocks below)
>
> Thank you for Stockfish.

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
