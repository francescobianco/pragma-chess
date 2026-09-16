# Explain — tuning notes

"Explain" is one of the pillars of Pragma Chess and is tuned by analysing real
lines together with the author. This file holds how it works today, the
parameters, the ideas still to try and the feedback received, so that each
iteration builds on the previous ones.

Rule for every change: **enrich, don't replace.** The material/mate
explanation works; new techniques are added alongside it and must keep the
existing tests green.

## Workflow

1. The author pastes a line and says what the explanation should have shown.
2. Run it through the command line tool, with the trace:

   ```bash
   make build
   ./build/gui/qt/pragma-explain --trace "1.e4 e5 2.Nf3 d6 3.Nxe5"
   ./build/gui/qt/pragma-explain --all --board --file game.pgn
   ./build/gui/qt/pragma-explain --fen "<fen>" --ply 3 "1...Qh4+ 2.g3 Qe4+"
   ```

   Searches are fixed-depth, single thread, with the hash cleared before each
   one, so the output is reproducible. `--depth`, `--probe-depth`,
   `--probe-plies`, `--threads`, `--hash` change the searches; `--engine`
   picks the UCI engine (command or path, like the engine set in the desktop
   client), whose name is printed first.

   **The desktop client runs exactly the same searches** (`ExplanationSearch`
   with the default `ExplainSettings`, on the configured engine), so what the
   tool prints with default options is what "Explain" shows. Changing a
   default in `ExplainSettings` changes both.
3. Change `gui/qt/src/app/MoveExplanation.cpp` / `AdvantageProbe.cpp`, add the
   line as a test in `gui/qt/tests/tst_chessrules.cpp` (with the engine lines
   written out, so tests don't need an engine), run `make test`.
4. Record the feedback and the decision in the log below.

## How it works today

Input: the position before the last move and its evaluation (E0), the move,
the position on the board and its evaluation (E1) with the principal
variation (PV).

1. **Verdict** — winning chances (lichess curve) the mover gave away:
   ≥ 30 blunder, ≥ 20 mistake, ≥ 10 inaccuracy; the engine's first move is
   "best".
2. **Error** (inaccuracy or worse), in order:
   - E1 is a mate for the opponent → the mating line.
   - The move loses material: the move + PV replayed from the previous
     position until the opponent is up at least `max(80, 0.4 × drop)` cp, the
     exchanges are over (no check to answer, no capture or promotion next) and
     the gain holds for 4 more plies. Arrows up to there, the better move as a
     dashed arrow. **Focus:** when that takes more than 3 plies, only the
     move with the biggest material jump is drawn, with the move before it
     when that move is part of the tactic (a capture, a check, a piece
     stepping onto the square then taken, or clearing the capture's path);
     no better-move arrow; the summary keeps the whole line.
   - The move misses a mate or a win of material (same search on the PV of
     the previous position).
   - Otherwise the first plies of the PV.
3. **Not an error**: mate on the board; material the move itself won; material
   still to win in the PV (same realization search); otherwise the first plies
   of the PV with an assessment.
4. **Line probe enrichment**: every position of
   the PV is searched at a small depth; the first ply from which the shallow
   score agrees with the deep one (within 10 winning-chance points, until the
   end of the probe) is where the advantage *becomes concrete*. When no
   material or mate explains the evaluation, the arrows and the summary go up
   to that ply instead of a fixed two plies.
5. **Mate playback**: when the explained line is a forced mate that ends in
   checkmate (`MoveExplanation::playback`), the desktop plays it on the board
   — pieces slide, the 2 px board frame turns from neutral to red, input is
   blocked — and holds the mate, marking the mated king with a soft red glow
   and a small "#"; turning Explain off or moving to another move restores the
   position. The tool prints it as `playback`.
6. **Depth probe** (printed, not used yet): the depth from which a position's
   score stays with the final one — how far ahead the advantage lies.

Parameters (top of `MoveExplanation.cpp`, `AdvantageProbe.h`, `Explainer.cpp`):

| Name | Value | Meaning |
|---|---|---|
| `kRealizedShare` | 0.4 | share of the swing material must account for (engine scores value pieces above 1/3/3/5/9; 0.6 missed 3.Nxe5?? dxe5) |
| `kMinimumMaterial` | 80 cp | below this, no material explanation |
| `kHoldPlies` | 4 | plies the material gain must hold |
| `kMaxSearchPlies` | 16 | how far into the PV to look |
| `kMaxArrows` | 8 | arrows drawn at most |
| `kFocusAbove` / `kFocusPlies` | 3 / 2 | longer realizations are drawn as the decisive jump and the move before it |
| `kAgreement` | 10 | winning-chance points for "agrees" in the probes |
| probe depth | 2 | `--probe-depth` |
| `ExplainSettings` | depth 20, probe 2 × 12, 1 thread, 64 MB | searches, shared by the desktop client and the tool |

## Ideas from the author

- **Progressive fixed depth to find where the advantage materializes.** A +3
  often does not show in the next two moves: there is a sequence of forced,
  good moves (anything else is worse) and only later the real switch of
  value happens on the board. An infinite-depth engine sees the line and
  gives the value; whoever asks for an explanation wants the *theme* that
  makes it concrete. → implemented as the line probe and the depth probe
  (`AdvantageProbe`), to be tuned.

## Observations and open questions

- Depth-2 probe scores oscillate with the side to move (e.g. Fried Liver
  after 5…Nxd5: −0.4, +0.1, 0.0, 0.0, +0.9, +1.7, +0.6 …), so "agrees until
  the end of the probe" is fragile. Options: compare pairs of plies (after
  the reply), use the minimum over two plies for the side ahead, raise the
  depth, or require agreement for N plies instead of until the end.
- Themes to name, once the concrete ply is known: fork, pin, skewer,
  discovered attack, trapped piece, overloaded defender, promotion, mating
  net, back rank. The position at the concrete ply and the move leading to it
  are the input for recognizing them.
- Focused arrows belong to a position several moves ahead: on the current
  board a piece may not be there yet, or the path may still be blocked
  (15…Qf5: 21.Rxe5 crosses the knight still on e4). Worth watching.
- The better move is drawn from the previous position on the current board;
  confusing if the piece has moved. To be judged with real use.

## Feedback log

- 2026-09-16 — First version of Explain (material/mate realization).
- 2026-09-16 — Explain turns off when moving to another move; the user asks
  again at the next move.
- 2026-09-16 — Command line tool, trace, line and depth probes, figurines in
  the desktop client (letters on the command line and in the clipboard).
- 2026-09-16 — Evergreen game (Anderssen–Dufresne), 15…Qf5: "too many
  arrows, nothing is clear; show the two arrows that make the real advantage,
  the error loses a minor piece (from +2 to +5)". The refutation was drawn as
  all 11 plies up to 21.Rxe5. Decision: focus long realizations on the
  biggest material jump plus the move before it (20…Nxe5 21.Rxe5, knight on
  c6 ringed). Test `focusesLongRealizations`.
- 2026-09-16 — "The desktop says something different." The desktop explained
  from the live multi-thread analysis, so the arrows changed at every depth
  (a pawn, then the queen, then a rook…) and never matched the tool.
  Decision: one `ExplanationSearch` for both, fixed depth 20, one thread,
  cleared hash, line probe included; the desktop shows "Analyzing…" for a
  few seconds and then the same explanation as the tool. Focus: the move
  before the decisive capture is drawn only if it belongs to the tactic.
- 2026-09-16 — The engine is the configured one: `--engine` on the command
  line, the engine setting in the desktop client; the tool prints its name.
- 2026-09-16 — Evergreen game after 20…Nxe7 (mate in 4): pressing Explain on a
  forced mate plays the mate on the board, with a red board frame; turning
  Explain off or moving on puts the pieces back. The board always has a thin
  neutral frame and 4 px rounded corners.
- 2026-09-16 — The red frame was hard to see: the frame is now always 2 px
  (neutral, red in playback), and the mated king is marked lightly (red glow
  and a "#" badge) when the mate lands.
- 2026-09-16 — A king in check is marked too, lighter than a mate (glow and a
  "+" badge); both marks show on every board, not only in playback.
