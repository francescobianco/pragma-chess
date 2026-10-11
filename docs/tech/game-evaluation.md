# The Evaluation of the Game

*Valutazione della partita.* An internal process, not in the menus yet: it
evaluates every position of a game with the engine, so the course of the
game (the Engine panel's graph) can be drawn whole, and from those
evaluations it estimates how strongly each side played. It is meant to be
reused: today it runs when an online game ends; tomorrow a menu entry, a
whole database, the phone.

## When it runs: after the game, never during it

Decided on 2026-10-11. While a game is played online the engine, Explain and
Training are off (`MainWindow::m_onlinePlay`, the flag against cheating): an
engine running on the same computer during the game, even unseen, is what
lichess and FICS forbid, and the processor belongs to the player and the
clock. So the evaluation starts when the game is over
(`MainWindow::onlineGameFinished` → `evaluateGame`), and a new online game
stops it at once (`setOnlinePlay(true)`). Afterwards there is the whole game
to work on, which is what the passes need.

## How: passes, shallow to deep

`app/GameEvaluation` (core library) replays the game's main line from its
start and searches each position with **an engine process of its own**
(the project's engine and its Computing Power, `EnginePower`, as the
analysis is given; never the live analysis' process), in passes:
`passDepths()` = 8, 12, 16, 20, 24. The first pass covers the whole game in
about a second, so the course is complete at once; each later pass refines
it. Within a pass the positions go in game order; `nextStep` (pure,
unit-tested) chooses the next one.

- A position with no legal moves is not searched: mate or stalemate is known
  (`finalEvaluation`), at the last depth, for good.
- A position already known as deep is not searched again: `setKnownDepth`
  reads the Explainer's store, so a game evaluated before resumes where it
  stopped.
- Every result goes to `Explainer::setEvaluation`, the store of the deepest
  evaluation of every position (kept across restarts in AppLocalData's
  `explain-evaluations.txt`, at most 5 000 positions): the course reads it,
  so it fills in by itself, and Explain judges with it.

Measured on 2026-10-11, Morphy's Opera game (34 positions), bundled
Stockfish 18 at Medium (10% of 22 cores): depth 8 and 12 within a second,
16 within a few seconds, 20 after about 3 minutes; 24 takes longer. The
course is whole from the first second.

Development API: `POST /api/evaluation {"start": true}` evaluates the game
on the board, `{"stop": true}`; `GET /api/evaluation` says `running`,
`positions` and `completedDepth`.

## The strength each side showed

`app/PlayStrength` (pure, unit-tested), computed by `MainWindow::updateCourse`
once every position of the line has an evaluation, and shown by
`EvaluationBox` in the score's place while the engine is not analysing: two
rows, the dot of each colour, in the order of the board, the numbers behind
them in the tooltip. Nothing is shown during an online game.

- **Loss of a move**: the evaluation before it minus the one after it, for
  the side that moved, each bounded to ±1000 centipawns (a mate counts as a
  large advantage, no larger: one move into a mate does not swamp the
  average). Average over the side's moves = ACPL.
- **Moves in a decided position are not judged**: beyond the bound before
  the move and on the same side of it after (still lost, still won). Lost
  beyond the bound, a further blunder would not show and a weak player would
  look perfect; won, the moves are easy. A move that changes the outcome is
  judged.
- **Accuracy**: lichess's, from the winning chances (`winChances`,
  `moveAccuracy`), averaged; shown in the tooltip, not used for the Elo yet.
- **Elo**: `eloFromLoss` = 5189 − 849 · ln(ACPL), between 100 and 2850 (a
  game without loss is worth what the best players reach, not more):
  calibrated on rated games, below. ACPL 30 ≈ 2300, 50 ≈ 1870, 70 ≈ 1580,
  100 ≈ 1280, 150 ≈ 930, 200 ≈ 690, 300 ≈ 345 — **on lichess's scale**,
  which runs above FIDE's for most players. Then `eloFromGame` draws an
  estimate **above** an average player (1500) down towards it by the number
  of moves: weight = moves / (moves + 12) — a miniature of forced moves
  against a weak opponent says little, a long game speaks for itself. One
  below 1500 stays where it is: a few good moves may be easy ones, a few
  blunders are blunders. No estimate under 8 judged moves of a side.

History: the first curve had a ceiling of 3100 and no weighting, and put
Morphy at 2904 in the Opera game — not credible (the user: "Carlsen is
barely 2800"). With the ceiling and the weighting: Morphy ≈2180, the Duke
and the Count ≈1543. Then the weighting drew weak players up to 1500 as
well, and a lost position hid their further blunders: a player who gets
nothing right must measure under 1000 (the user). Weighting only from above,
decided moves left out, floor 100: Morphy ≈2131, the Duke and the Count
≈1508; 1.5 pawns lost a move over 10 moves ≈640.

A game the user found "terrible" showed 1500 (2026-10-11): Black lost 41
moves to a stronger player with 33 judged moves, most of them fine or easy,
and seven mistakes, two of them blunders (Stockfish, depth 16): ACPL 70. By
the old curve ≈1420. The question was whether that is right — so the curve
was measured instead of guessed.

## Calibration on rated games

`scripts/calibrate-play-strength.py` reads the stress corpus
(`~/.cache/pragma-chess-stress`, lichess 2013), keeps the classical and rapid
games the lichess server analysed — a `[%eval]` after every move, Stockfish
— and measures each player as PlayStrength does. 2013-01 to 2013-12:
**14 307 players' games, rating 803–2343**, in 14 s, no engine run.

| Lichess rating | Players | Median ACPL | Mistakes (≥100) a move | Blunders (≥300) a move |
|---|---|---|---|---|
| 800–999 | 33 | 148 | 0.33 | 0.15 |
| 1000–1199 | 369 | 113 | 0.29 | 0.13 |
| 1200–1399 | 1 672 | 97 | 0.26 | 0.10 |
| 1400–1599 | 4 356 | 77 | 0.20 | 0.08 |
| 1600–1799 | 5 371 | 61 | 0.16 | 0.06 |
| 1800–1999 | 2 142 | 51 | 0.14 | 0.04 |
| 2000–2199 | 343 | 39 | 0.10 | 0.03 |
| 2200–2399 | 21 | 21 | 0.05 | 0.01 |

What it says:

- **One game says little.** The correlation between one game's ACPL and the
  player's rating is ≈0.36–0.40. A regression (rating from ACPL) therefore
  draws every prediction towards the mean — every player 1500–1700 — and
  weak play would never measure weak. Adding the rates of mistakes and
  blunders (the user's idea: count the blunders) improves it only a little
  (0.39 against 0.36): they carry the same information as the average loss.
- **So the curve goes the other way**: through each band's typical loss
  (fit 3 in the script), Elo = 5189 − 849·ln(ACPL). A player who loses what
  1100s lose is called 1100, whatever one game's noise; its error on a
  single game is large (mean absolute error ≈430), the estimate of a level,
  not a measurement.
- The user's game, ACPL 70, is what a lichess player of ≈1580 typically
  shows: many sound moves and a few heavy mistakes is how 1500s lose. Under
  1000 is a loss of ≈1.6 pawns a move and more — pieces left hanging move
  after move.
- The ratings are lichess's of 2013 (Glicko), the evaluations its server's
  Stockfish of the time; the clients evaluate with today's engines, which
  find more and lose less on the same moves. Repeat the calibration with a
  recent month (lichess's files from 2017 on carry far more [%eval]) to
  bring it up to date.

Still open: weigh how hard each position was, as Ken Regan's Intrinsic
Performance Rating does (multi-PV: the loss of a move against the spread of
the good moves there); it needs several lines per position, not one.
