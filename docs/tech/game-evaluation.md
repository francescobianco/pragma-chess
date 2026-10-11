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
- **Accuracy**: lichess's, from the winning chances (`winChances`,
  `moveAccuracy`), averaged; shown in the tooltip, not used for the Elo yet.
- **Elo**: `eloFromLoss` = 2850 · e^(−0.01 · ACPL), the shape of a fit often
  quoted for engine-judged games, with a ceiling of 2850 (a game without
  loss is worth what the best players reach, not more). Then
  `eloFromGame` draws it towards an average player, 1500, by the number of
  moves: weight = moves / (moves + 12). A miniature of forced moves against a
  weak opponent says little, a long game speaks for itself. No estimate under
  10 moves of a side.

History: the first curve had a ceiling of 3100 and no weighting, and put
Morphy at 2904 in the Opera game — not credible (the user: "Carlsen is
barely 2800"). With the ceiling and the weighting: Morphy ≈2180, the Duke
and the Count ≈1543.

### Still to do: calibrate on rated games

The curve and the weight are reasonable, not measured. The scientific step
is to fit them on games with known ratings: the lichess open database we
use as the stress corpus (`docs/tech/large-databases.md`) has both players'
ratings on every game. Evaluate a sample across the rating range at a fixed
depth, then fit Elo against ACPL (or accuracy) and the number of moves, and
replace `eloFromLoss`/`kPriorMoves` with the fit. The reference for a
rigorous model is Ken Regan's Intrinsic Performance Rating, which also
weighs how hard each position was (multi-PV); it would need several lines
per position, not one.
