<!-- A page for https://www.chessprogramming.org (DISTRIBUTING.md, "Chess
programming wiki"): factual, in the wiki's style, linked from the GUI page.
Needs an account there. -->

'''Pragma Chess''',
a free and open source [[GUI|chess GUI]] and game database by Francesco Bianco, written in [[Cpp|C++20]] with [[Qt]] 6 Widgets,
released under the [[Free Software#MIT|MIT License]] for [[Windows]], [[Mac OS]] and [[Linux]], with an [[Android]] companion app.
First released in September 2026.

=Features=
* Game databases in SQLite files (.pdb), import from [[Portable Game Notation|PGN]], [[ChessBase]] files and online accounts (lichess.org, chess.com)
* [[Position|Position]] and line search with a [[Zobrist Hashing|Polyglot key]] index, built on all cores and kept on disk
* [[UCI]] engines, [[Stockfish]] bundled
* ''Explain'': replays the engine's [[Principal Variation|principal variation]] to where the evaluation becomes concrete (material won once exchanges are over, or a mate) and draws it on the board as arrows
* [[Opening Book|Polyglot opening books]], their weights edited from the GUI
* Training against the engine with a tutor judging the user's moves
* Online play on lichess.org (Board API) and the [[FICS|Free Internet Chess Server]]

=See also=
* [[GUI]]
* [[Scid]]
* [[ChessX]]

=External Links=
* [https://yafb.net/pragma-chess/ Pragma Chess] home page
* [https://github.com/francescobianco/pragma-chess Pragma Chess] on GitHub

'''[[GUI|Up one Level]]'''
