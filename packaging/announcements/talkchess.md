<!-- The launch thread on TalkChess (DISTRIBUTING.md, "TalkChess";
POSITIONING.md, step 2: En Croissant's first post was there). Post it in
General Topics after the 0.4.0 release, as the author, and stay for the
replies: answer each one, open an issue for each bug. The picture is
site/assets/screenshots/explain.png (attach it, or link it from the site).
Needs a TalkChess account. -->

Subject: Pragma Chess – a new free chess GUI and database, with an "Explain" button

---

Hi all,

I have been writing a chess GUI and game database, Pragma Chess, and it is
now usable enough to ask for your opinion. It is free and open source (MIT),
C++20 with Qt 6 Widgets — a native desktop program, not a web page in a
window — for Windows, macOS and Linux, with an Android companion app in
testing.

Site and downloads: https://yafb.net/pragma-chess/
Source: https://github.com/francescobianco/pragma-chess

**What it does**

- Databases in SQLite files: import PGN (a converter that uses every core:
  about 6 000 games a second here), lichess.org and chess.com accounts,
  lichess studies, ChessBase files (`.cbh`, and the `.2cbh` of ChessBase 17
  with variations and annotations).
- Search by position, whatever the move order, and by line, from an index of
  Polyglot keys built on every core and kept on disk next to the database. On
  2.3 million lichess games the window is ready in under 6 s, in about half
  a GB, and a position is found in 0.3–2 s.
- Any UCI engine, Stockfish bundled. Each engine gets a "computing power"
  level that caps how much of the machine it takes (a systemd scope with
  CPUQuota on Linux, a job object on Windows) without touching depth or
  time: the analysis is only slower, never weaker.
- Polyglot books: read, written and edited from the GUI (weights adjusted
  move by move; a repertoire flag in one of the `learn` bits, which engines
  ignore, so the book stays a normal book).
- Training against the engine, which plays from the book by weight, with a
  tutor that stops you on a mistake; play on lichess.org (Board API) and
  freechess.org (telnet, style 12) from the same board.

**Explain**

This is the part I would most like your opinion on. Press E on a move and the
program draws on the board why the evaluation changed: red arrows where
material falls, blue ones for the reply that makes the difference, a forced
mate played out when it is short enough to be useful to the one who asks.

It runs no engine of its own. Every line the live analysis reports is fed,
as a "tick", to a small program that judges it: it compares the search after
the move with the deepest one known before it (the position before is searched
to depth 16 first when nothing that deep is known), then replays the
principal variation to the point where the evaluation becomes concrete —
material won once exchanges, checks and recaptures are over and kept for a few
plies, or a mate. An arrow is shown only once it appears in 3 of the last 4
depths, since the PV oscillates. Where the drop has no material behind it, it
is drawn and said as positional, not as a refutation.

The judgement is written in a tiny BASIC-like language rather than in C++, so
the desktop, the command line tool and the Android app run the same files;
recorded sequences of ticks are the regression tests. There is a command line
tool, `pragma-explain --trace "1.e4 e5 2.Nf3 d6 3.Nxe5"`, that prints every
step.

It is version 0.4.0 and certainly rough in places. I would be glad to hear
what breaks, what an engine author would want from a GUI, and above all where
Explain says something silly — a position and the move are enough for me to
turn it into a test.

Thanks for reading,
Francesco
