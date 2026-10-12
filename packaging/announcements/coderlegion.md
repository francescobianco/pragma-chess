<!-- CoderLegion (https://coderlegion.com), the user's account "yafb"
(DISTRIBUTING.md, "CoderLegion"). A developers' community, small and
friendly: groups Open Source (1.6k members) and C/C++ Community (287);
self-promotion allowed when relevant. Three pieces, a week apart, each
shared to the Open Source and C/C++ groups; answer every comment.
1. the Launch; 2. the article on the Elo calibration; 3. the call for
translators and contributors (the kind of post that gets the most there).
Pictures: site/assets/screenshots/explain.png, main.png. -->

# 1. Launch

**Name:** Pragma Chess

**Tagline:** Open source chess database with an engine that explains *why* — C++20 and Qt 6, Windows, macOS, Linux

**Tags:** opensource, cpp, qt, chess

**Link:** https://github.com/francescobianco/pragma-chess — site: https://yafb.net/pragma-chess/

**Text:**

I've been building Pragma Chess: a free, MIT-licensed chess database and
analysis app, native on Windows, macOS and Linux (C++20, Qt 6 Widgets — no
web view), with an Android companion.

The feature I care most about is **Explain**: press E on a move and, instead
of just a score, the board shows *why* it is good or bad — arrows on the
material that falls and the reply that makes the difference. It doesn't run
a second engine: every line of the live analysis feeds a small program that
compares the search before and after the move and replays the engine's line
to where the advantage becomes concrete.

What else is in it:

- game databases in SQLite: PGN, lichess.org and chess.com imports, ChessBase
  files; search by position on 2.3 million games with a ~0.5 GB footprint;
- any UCI engine (Stockfish bundled), capped to a share of the CPU;
- training against the engine with a tutor that stops you on mistakes;
- online play on lichess.org and freechess.org, and after each game an
  estimated Elo for both sides;
- English, Italian, Spanish — and a Chinese translation just started by a
  first-time contributor.

It's young (0.3.0, 0.4.0 coming at the end of October). Feedback, bug
reports and contributors are very welcome — there are good first issues and
a guide for translators in CONTRIBUTING.md.

---

# 2. Article (a week later)

**Title:** I estimated chess players' Elo from one game — then calibrated it on 14,307 rated games

**Tags:** opensource, cpp, datascience, chess

After an online game, Pragma Chess evaluates every move with the engine and
shows an estimated Elo for both sides. My first version was a curve I found
quoted around the web — Elo ≈ 3100 · e^(−0.01 · average centipawn loss) — and
it rated Paul Morphy's famous Opera game at **2904**. A friend's comment: "Carlsen
is barely 2800."

What I learned fixing it:

1. **Short games lie.** A 17-move miniature of forced moves against weak
   opponents has a tiny average loss. So an estimate above an average player
   is drawn towards 1500 by the number of moves — weight = moves / (moves + 12).
   Only *above*: a few blunders are evidence, a few good moves may just be easy.
2. **Lost positions hide blunders.** Evaluations are bounded (±10 pawns, so a
   mate doesn't swamp the average) — but once a game is lost beyond the bound,
   further blunders cost nothing and a weak player looks perfect. Moves in a
   position decided before *and* after are not judged.
3. **Then measure instead of guessing.** The lichess open database has both
   players' ratings, and some games carry the server's own Stockfish evaluation
   after every move (`[%eval]`). A short script (no engine run, 14 seconds)
   measured 14,307 players' games across 800–2343:

   | Rating | Typical loss per move |
   |---|---|
   | 1000–1199 | 1.13 pawns |
   | 1400–1599 | 0.77 |
   | 1800–1999 | 0.51 |
   | 2200–2399 | 0.21 |

4. **One game says little** — the correlation between one game's loss and the
   rating is only ≈0.36. A regression would therefore predict 1500–1700 for
   everyone. So the curve goes the other way, through each band's typical loss:
   **Elo = 5189 − 849 · ln(ACPL)**. Someone who plays like an 1100 is called 1100.
5. **Counting blunders didn't help much** (0.39 vs 0.36): they carry the same
   information as the average loss.

The code is C++ (`app/PlayStrength`, pure functions with unit tests) and the
calibration script is in `scripts/calibrate-play-strength.py`:
https://github.com/francescobianco/pragma-chess — the whole story, with the
mistakes, is in `tech/game-evaluation.md`. I'd love to hear how you'd
model it better — the next step would be weighing how hard each position was,
as Ken Regan's Intrinsic Performance Rating does.

---

# 3. Discussion (a week later)

**Title:** Looking for translators (and a few C++/Qt contributors) for an open source chess app

**Text:**

Pragma Chess speaks English, Italian and Spanish, and a first-time contributor
has just started Chinese. Translating needs no programming: one XML file for
the interface (Qt Linguist or any editor) and one Markdown file for the guide;
a partial translation is fine. If you speak German, French, Portuguese,
Russian, Japanese… pick your language in
https://github.com/francescobianco/pragma-chess/issues/4.

For C++/Qt people there are small, well-bounded issues: a new board style
(SVG pieces with a free license, #5), reading ChessBase annotators (#7),
checking a CPU cap on Windows (#8). CONTRIBUTING.md explains how to build and
where to start. Beginners welcome — I'll review and guide.
