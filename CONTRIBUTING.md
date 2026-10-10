# Contributing to Pragma Chess

Thank you for wanting to help. There are many ways, and most need no code:

- **Tell us what breaks**, or what you miss: an
  [issue](https://github.com/francescobianco/pragma-chess/issues/new/choose)
  for a problem, the
  [Discussions](https://github.com/francescobianco/pragma-chess/discussions)
  for questions and ideas.
- **Tell us where Explain is wrong.** It is the heart of the program, and
  every wrong explanation you send becomes a test (see below).
- **Translate** the interface and the guide into your language (see below).
- **Spread the word**: a post in your club, your forum, your language.
- **Write code**: fixes, features, packages for your system.

Pragma Chess is MIT licensed; what you contribute is under the same license.
Be kind: we are all here because we like chess.

## Reporting a wrong explanation

Explain (the button between the arrows under the board, key **E**) draws why
the evaluation of the last move changed. When it says something wrong or
silly, open an issue with the *Explain is wrong* form and give:

1. the moves (or a FEN) and the move explained — **Edit ▸ Copy ▸ Game as
   PGN** copies the game;
2. what Explain showed (a screenshot is perfect) and what you would expect;
3. if you can, **Engine ▸ Copy Explain's Ticks** right after: it copies what
   the engine said to Explain, so we can replay exactly what you saw, with
   no engine, and keep it as a test (`smart/tests/`).

The logic is written in a small language, `smart/EXPLAIN.smart`, the same
on desktop, command line and phone; `docs/tech/explain-tuning.md` tells
how it is tuned.

## Translating

The interface is English and Italian today. A new language takes a few
files, and none of them is code. Open an issue with the *Translation* form
first, so two people do not translate the same language.

1. **The interface**: `gui/qt/translations/pragma-chess_<code>.ts`, where
   `<code>` is the language's two letters (`de`, `fr`, `es`, `pt_BR`…).
   Start from the Italian file: copy it, change `language="it_IT"` in its
   second line, and translate each `<translation>` with
   [Qt Linguist](https://doc.qt.io/qt-6/linguist-translators.html) (in
   `qt6-tools-dev-tools` on Debian and Ubuntu) or any text editor. Keep the
   `&` (the letter underlined in menus), the `%1`, `%2` and the `…` as they
   are. Once the file is there, the language appears in **Options ▸ Switch
   Language** by itself.
2. **The guide** (Help ▸ Pragma Chess Guide, F1, and the pages on the web
   site): `gui/qt/resources/help/guide_<code>.md`, from `guide_en.md`. Keep
   every `# Title {#id}` line's id as it is, and write menu and button names
   exactly as your translation of the interface shows them. Until it exists,
   the guide is shown in English.
3. Optionally **the web site**: `site/content/<code>.json`, from `en.json`.

A partial translation is welcome: what is not translated stays in English,
and someone can finish it later. Send it as a pull request, or attach the
files to the issue if you do not use Git.

When the application's texts change, the files are refreshed with
`lupdate` (see AGENTS.md, "Conventions"); the new texts arrive as
`type="unfinished"` for the translators to fill.

## Building and changing the code

On Debian/Ubuntu or macOS with Homebrew:

```bash
make deps     # Qt 6, CMake, Ninja and the rest
make build    # build/ (Debug)
make run      # build and start the client
make test     # build and run the tests
```

Then:

- Read [AGENTS.md](AGENTS.md) — written for coding agents, it is the most
  complete map of the code for humans too — and [DESIGN.md](DESIGN.md) for
  the vision. The layers: the core library (`gui/qt/src/app`, Qt Core only:
  rules, databases, engines, Explain) knows nothing of widgets; the widgets
  only talk to it.
- Follow the style of the code around your change: C++20, Qt 6 Widgets,
  `Q_SIGNALS`/`Q_SLOTS`/`Q_EMIT`, `m_member`, `kConstant`, texts the user
  sees through `tr()`. New source files go into `gui/qt/CMakeLists.txt`.
- Add a test for logic you change (`gui/qt/tests/tst_chessrules.cpp`, no
  display needed); `make test` must stay green, and the build must not add
  compiler warnings.
- A user-visible change gets a line in [CHANGELOG.md](CHANGELOG.md) under
  *Unreleased*, and a paragraph in every guide if it is a new feature.
- Keep a pull request to one thing; say what it changes for the user.

Issues labelled
[good first issue](https://github.com/francescobianco/pragma-chess/labels/good%20first%20issue)
are a good place to start; ask in the issue before starting something big.
