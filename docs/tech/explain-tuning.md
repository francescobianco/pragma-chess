# Explain — tuning notes

"Explain" is one of the pillars of Pragma Chess and is tuned by analysing real
lines together with the author. This file holds how it works today, the
parameters, the ideas still to try and the feedback received, so that each
iteration builds on the previous ones.

**The logic lives in [smart/EXPLAIN.smart](../../smart/EXPLAIN.smart)**
(and the verdict in `smart/TUTOR.smart`), a SMART program every client runs:
a fix goes there, once. `PRAGMA_SMART_DIR=smart` makes `pragma-explain` and
the desktop client read the files of the checkout, so a change is tried
without building.

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
   one, so the output is reproducible. `--depth`, `--threads`, `--hash`
   change the searches; `--engine` picks the UCI engine (command or path,
   like the engine set in the desktop client), whose name is printed first.

   **Explain reacts to ticks**: the desktop client feeds EXPLAIN.smart every
   line of its live analysis, the tool every depth of its search after the
   move, through the same `explainTick`. `--ticks` shows what each tick made
   (shown, or held back because new arrows had not held yet).
   `--record <file>` appends the ticks of each move, with what was shown as
   `expect` lines; `--replay <file>` explains them again with no engine and
   says whether the expectations hold (exit status 4 if not). The desktop
   client records every move it explains with
   `PRAGMA_EXPLAIN_RECORD=<folder>` — the way to bring a wrong explanation
   from the board here, tick for tick.
3. Change `smart/EXPLAIN.smart` (or TUTOR.smart), with
   `PRAGMA_SMART_DIR=smart` to try it without building; keep the case as a
   record in `smart/tests/` (its `expect` lines say what must be shown; every
   client replays them) or as a test in `gui/qt/tests/tst_chessrules.cpp`
   with the engine lines written out; run `make test`.
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
   end of the probe) is where the evaluation stops depending on deep
   calculation. When no material or mate explains the evaluation, the arrows
   and the summary go up to that ply instead of a fixed two plies — and the
   summary says the assessment is positional, because this ply is *not* a
   material claim.
5. **Mate playback**: when the explained line is a forced mate that ends in
   checkmate (`MoveExplanation::playback`), the desktop plays it on the board
   — pieces slide, the 2 px board frame turns from neutral to red, input is
   blocked — and holds the mate, marking the mated king with a soft red glow
   and a small "#"; turning Explain off or moving to another move restores the
   position. The tool prints it as `playback`.
6. **Hints from the live analysis**: the desktop client passes the live
   analysis of the position on the board. A mate or a 0.00 (theoretical or
   technical draw) that the fixed-depth search missed, found at a depth at
   least as deep and with a playable line, replaces the searched evaluation
   (`ExplanationAnalysis::acceptsHint`); everything else is ignored, so the
   explanation stays reproducible. The tool takes the same hint with
   `--hint-mate`, `--hint-draw`, `--hint-line`, `--hint-depth`.
7. **Depth probe** (printed, not used yet): the depth from which a position's
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

- 2026-09-16 — King's Gambit line after 14…Bxc3: "there is a mate in 12 but
  Explain did not play it". Depth 20 sees +13; Stockfish finds the mate only
  from depth 28 (≈11 s single thread, mate in 14 from depth 30). Decision
  (author's idea): the live analysis guides Explain, only for mates and
  0.00. Mates are also replayed to the end (they were cut at 16 plies, so a
  long mate could not be played). Test `usesLiveAnalysisHints`.

- 2026-09-18 — 1.e4 e5 2.Qg4 Nf6 3.Qf5: "the explanation is not clear, it
  looks like material is lost". Nothing falls: the material probe says "not
  realized within 17 plies" and the drop (−0.9 → −2.2) is positional, Black
  develops with tempo against the queen. Two things said otherwise: the
  opponent's continuation was drawn as a red `Refutation` arrow, the colour of
  material falling, and the summary said "It becomes concrete after 3…Nc6" —
  the line probe's "concrete" (the shallow score already agrees with the deep
  one) read as "a piece is about to go". Decision: that sentence is only ever
  emitted by the two branches that found neither material nor mate, so it now
  says "No material is at stake: the assessment is positional, clear after
  …", and the error fallback draws `Reply` (blue) instead of `Refutation`.
  Red stays the colour of material falling. Test
  `positionalDropIsNotAMaterialLoss`.
- 2026-09-18 — Same position, on the longer line the desktop engine shows
  (3…Nc6 4.d3 d5 5.Qg5 … 26.Kh1): "I think the pawn capture is there". It is:
  19…Rxd4 and 24…gxh3 both take a pawn. Replaying the line and printing
  `ChessPosition::material()` at every ply settles it — the balance swings
  0, −100, 0, −300, **+200**, and *ends +200 for White*, who is nonetheless
  −2.2: Black's advantage is the attack, not material. So "No material is at
  stake" was wrong in the other direction, since pieces are captured all over
  the line. The wording is now "No material explains it: the assessment is
  positional", which is what the probe actually checked: no *lasting* gain,
  not that nothing is ever taken. A side can even end up ahead in material
  and still be losing.
- 2026-10-04 — The bundled engine is now Stockfish 18 with its small network
  only (packaging/README.md, "Bundled engine"), to keep the packages small.
  Compared with an official Stockfish 16 at depth 20, threads 1, the
  explanations hold: 3.Nxe5 in the Philidor −3.9 → −3.4, 3.Qf5 after 2.Qg4
  −2.2 → −2.1, 3.a4 in the King's Gambit −2.1 → −1.7 (still an inaccuracy
  and more), 4.Nxe5 after 3…Nd4 −0.5 → −0.6, the Opera game's 14.Rd1
  +6.0 → +6.4. The principal variations differ more than the scores. Tune
  with the bundled engine (`make stockfish`), which is what users have.
- 2026-10-05 — **SMART.** The logic is now `smart/EXPLAIN.smart` (and the
  verdict `smart/TUTOR.smart`), run by every client; the port was checked
  byte for byte against the old `pragma-explain` on five lines. The
  decision of 2026-09-16 (one fixed-depth `ExplanationSearch` for both) is
  reversed at the user's request: the separate search made Explain slow and
  drove the engine twice. Explain now reacts to the live analysis, tick by
  tick; stability comes from EXPLAIN.smart's memory instead (no answer
  under depth 8, the verdict only from depth 12 or as deep as the search
  before the move, other arrows only once they held for two depths), and
  reproducibility from recorded ticks (`--record`/`--replay`,
  `PRAGMA_EXPLAIN_RECORD`, `smart/tests`). Lost on the way: the line probe,
  so the positional sentence (TODO.md, "SMART e Spiega reattivo").
- 2026-10-05 — 1.d4 d5 2.Nf3 Nf6 3.Nc3 e6 4.Bg5 Bb4 5.a3 Bxc3+ 6.bxc3 c5
  7.dxc5 Qc7 8.Qd4 Nc6 9.Bxf6: "I don't understand what arrow 2 shows." It
  is White's recapture 10.Bxd4 (blue, a reply), where the sequence ends
  because material is counted once the exchanges are over: "Mistake (−0.8 →
  −4.0). Black wins the queen for 2 knights: 9…Nxd4 10.Bxd4. Better was
  9.Qe3." Open: whether to draw the loser's last recapture, "Mistake" for a
  queen left hanging (the winning-chances curve saturates when already
  worse), and "for 2 knights" counting the knight 9.Bxf6 itself took. Case
  recorded in `smart/tests/user-feedback.ticks`; nothing changed yet.
- 2026-10-05 — Same position: "the recapture is of little use to show; the
  evidence is that the queen was taken. With equal exchanges ending in an
  advantage it would have helped." Decision: in the material branches the
  moves a sequence ends with that the losing side plays are not drawn
  (`WithoutLoserTail`, EXPLAIN.smart); recaptures in the middle of the
  exchanges stay, mates are drawn whole, and the text keeps the full line
  since the balance counts them. 9.Bxf6 now shows 9.Qe3 dashed and
  9…Nxd4 red; no other recorded case or test changed.
- 2026-10-05 — Same game, 12.Bxe5 (…10.Bxd4 f6 11.e4 e5): "the tutor does
  not notice the move is an error, from −4.6 to −5.4". It gives a bishop for
  a pawn, but in a lost position the winning chances barely move (18.6 →
  12.9, under the 10 points of an inaccuracy) and Explain said "Good move …
  Black wins a bishop". Even in centipawns the engine sees only 0.8 pawns of
  loss (13.Bb5+ and play). Decision, in TUTOR.smart's Classify, the most
  severe of three measures: the winning chances as before; the centipawns
  lost (100/200/300 for inaccuracy/mistake/blunder) while the mover is not
  clearly winning after the move (+9 to +6 still passes); and the material
  the move hands over where the exchanges end (`Handed`: a piece for a pawn
  is a mistake, a rook or more a blunder), unless the evaluation lost under
  50 cp — a sacrifice the engine approves of. Classify and Judge take the
  position before the move for it. 12.Bxe5 is now "Mistake (−4.1 → −5.1).
  Black wins a bishop for a pawn: 12…fxe5. Better was 12.Be3." and stops a
  training game; 9.Bxf6 became a blunder; no other recorded case or test
  changed.
- 2026-10-06 — 1.f4 e6 2.Nf3 b6 3.e3 Bb7 4.b3 Bxf3, the user playing Black
  with the board turned: "it shows what I should have done, but not what I
  suffer after the capture — should Explain take the observer into
  account?" The observer is not the issue: the explanation already speaks
  to the side that moved ("Better was 4…Nf6"), and the score is White's as
  everywhere. The gap is that without material Explain has nothing to say
  about why: 5.Qxf3 hits the undefended rook on a8 (5…c6 is forced) and
  White keeps the bishop pair, while Explain only says "Main line: 5.Qxf3
  c6 …". Next: threats along the line (TODO.md). Fixed now: f3 was ringed
  in red as a piece lost, in an even exchange; the branches where no
  material explains the evaluation no longer ring anything (`ringing`).
- 2026-10-06 — Same position. The user suggested walking the line backwards
  from the forced capture instead of new tools; it does not apply here: the
  17 plies of the line keep the material level, because the engine's line
  holds the best defence and a threat parried is never a capture. So the
  clients give EXPLAIN.smart two generic primitives, `MOVES(p)` (the legal
  moves) and `PASS(p)` (the other side to move), and the threat logic is
  SMART: after each of the first 4 moves of the line, the captures the side
  that played it would have if the other side passed, worth ≥ 200 cp
  (taken piece, less the capturer if the target square can be retaken),
  new since the move; the next move parries it if the capture is gone
  after it. Ties are broken by UCI order, never by the order of the moves,
  which differs between clients. 4…Bxf3 now says "5.Qxf3 attacks the rook
  on a8: 5…c6 parries it."; no other recorded case changed. The user
  expects harder cases (forced tactics over several moves): TODO.md.
- 2026-10-06 — 1.f4 e6 2.Nf3 b6 3.e3 Bb7 4.b3 Bxf3 5.Qxf3 Qh4+, the user
  playing Black: "arrow 2 is of no use". It was 6…Qf6, Black's own queen
  going back after 6.g3. Decision: in the error branch where no material
  explains the drop, the answers of the side that suffers (the mover) at
  the end of the drawn plies are not drawn (`WithoutLoserTail`), the text
  says them; 3.a4 loses 4.Ke2, 4…Bxf3 loses 5…c6. When a position is
  explained without a move to judge, both sides' plan stays drawn. Open:
  the real point of 5…Qh4+ is the threat it ignores (the rook on a8 stays
  attacked), which Explain does not say yet (TODO.md).
- 2026-10-06 — Same position on the desktop: "I still see arrow 2". The
  app had been reopened on 5…Qh4+ with Explain on, so the position before
  the move had never been searched: no verdict ("+4.1. White is winning."),
  and the position-alone branch drew both sides' plan, 6…Qf6 included. Two
  fixes: the desktop searches the position before the move first, to depth
  16, whenever Explain lacks it (one engine, one position after the other);
  and a move not judged yet is still given to EXPLAIN.smart, which then
  draws the answer to it without the mover's answers.
- 2026-10-06 — 5…Qh4+ once more: "what I expect is the pawn's arrow, then at
  once an arrow of its threat to the queen, and the queen's arrow at the
  undefended rook; those +5 are the rook". Decision: a new arrow kind,
  `threat` (dashed red: a capture threatened, not played — red is still
  material, dashed says not yet), drawn for the threat ThreatText finds and
  for the threat the mistake leaves standing (`IgnoredThreatText`: a
  capture the opponent had before the move and makes within the first
  plies of the line). 5…Qh4+ now shows Nc6 (better), g2–g3, Qf3→a8 and
  g3→h4, and says "5…Qh4+ leaves the rook on a8 attacked: 7.Qxa8. 6.g3
  attacks the queen on h4: 6…Qf6 parries it." Note that the engine's line
  goes on 7…Qxa1 8.Qxb8+: the rook comes back, the knight is what is won.
- 2026-10-06 — 1.e4 e5 2.Nf3 Nc6 3.d4 exd4 4.Bc4 h6 5.c3 dxc3 6.Nxc3 Bb4
  7.O-O d6 8.Nd5 Bc5, the user playing Black: "it is not clear what is
  explained". Replaying the live search tick by tick (depth 28, 4 threads):
  the steady explanation ("Better was 8…a5. 9.b4 attacks the bishop on c5")
  was replaced at depths 20–21 by arrows taken ten plies down the line
  (f4–e6, c4–e6), pieces not on the board, then came back. Two decisions:
  (1) Tick's stability is a majority, not two depths in a row: other arrows
  only once they came in 3 of the last 4 depths; the verdict is shown at
  once (news, not flicker); an explanation the engine has not given for 4
  depths is dropped. Three depths in a row was tried and was worse (the
  engine oscillates, the good explanation came only at depth 24). (2) The
  noise came from material "won" far down the line: past 8 plies a gain
  must cover 80% of the swing (LONG_PLIES, LONG_SHARE); the Evergreen's
  knight eleven plies on covers it, 5…Qh4+'s pawn twelve moves on (−3
  pawns) does not, nor 3.Qf5's long line, which now says "4…g6 attacks the
  queen on f5: 5.Qg5 parries it". 5…Qh4+ is now steady on its threats from
  depth 12; 12.Bxe5 ends on the explanation without material (TODO.md).
- 2026-10-06 — 8…Bc5 again: "the bishop can just retreat, I do not see where
  the position is lost" and then "it is not only time: replay the whole line
  after my mistake, it is impossible that White is +2.4 and there is
  nothing". Right on both. At depths 18–22 the engine's line is quiet (9.b4
  Bb6 10.Bb2) and the honest text is the tempo: "9.b4 attacks the bishop on
  c5: 9…Bb6 moves it again, and White gains time. No material is lost: the
  evaluation is positional." (the last sentence only when the line really
  keeps the material: `KeepsMaterial`; when the answer takes the attacker,
  "takes the attacker"). From depth ~26 the engine finds the real reason:
  9.b4 Bb6 10.b5 Na5 11.Nxb6 axb6 12.Bxf7+ Kxf7 13.Ne5+ Kf8 14.Ng6+ Ke8
  15.Nxh8, the rook on h8 (…h6 weakened g6, b5 drove the knight from e5).
  Explain found it but drew one arrow, g6→h8, from a square the knight
  does not stand on yet. Decision: FocusWindow traces the piece that takes
  back to the square it stands on now, adds the winning side's capture or
  check just before (12.Bxf7+), and draws only those moves: c4–f7, f3–e5,
  e5–g6, g6–h8, rings on f7 and h8. 4.Nxe5 (after 3…Nd4) gains the same
  way: d8–g5, g5–e5 instead of the lone g5→e5. The Evergreen is unchanged
  (its rook takes from where it stands). Recorded: user-feedback.ticks.

### 8…Bc5 at depth 40: the double attack (6 October)

The user, Black: three arrows (8…a5, 9.b4, the threat on c5) do not
justify +0.8 → +3.7 with a pawn up. On their board the line was 9.b4 Bxb4
10.Nxb4 Nxb4 11.Qb3 a5 12.Bxf7+ Kf8: the point is 11.Qb3, on the knight on
b4 and on f7 behind the bishop. ThreatText now finds a double attack in the
first 8 plies (`DOUBLE_PLIES`; single threats stay in 4): a move that is no
capture and makes two new threats by the piece moved or one behind it on
the line (`IsBetween`), each ≥ `DOUBLE_GAIN` 100 and together ≥ 300. The
move is drawn (as a reply, or an idea when the position is explained alone)
with both threats, and the main line reaches it (`threatReach`). A king's
threats and those a piece uncovers by blocking a check (6.g3) do not count;
nor does a capture that forks (7.Qxa8: its material says it). 3.Qf5 gains
"6…Nd4 attacks the bishop on b5 and the pawn on c2". Recorded:
user-feedback.ticks. Still missing: what the king's position is worth.

### 11.Re1?? in training: exchanges that never stop (6 October)

The user, White in training, stopped by the tutor on 11.Re1 (−2.2 → −5.8),
asked Explain and read "No material is lost: the evaluation is positional".
They suspected the tutor's hidden reply made the state wrong: it does not
(the reply is held, `m_tutorReply`, the board and the analysis are the
position after 11.Re1). The fault was Explain's: at depth 45 the line is
11…Qb6 12.Nbc3 Bxf2+ 13.Kf1 Bxe1 14.Rxe1 Bxe2+ 15.Rxe2 Rxe2 16.Qxe2 Re8
17.Nxd5 Rxe2 18.Nxb6 …, a capture every other move, so no ply was settled
and held for HOLD_PLIES, and the last ply of the window (19.Bxc6) waits
for its recapture. A shallower line (14…a6) was explained right.
`FindRealization` now falls back, within the first LONG_PLIES, to the first
ply with enough material that the next move does not take back (14.Rxe1,
not 13…Bxe1), when the exchanges stop at least once after it and every
stop keeps enough; the last ply of the window is no stop. Recorded:
user-feedback.ticks.

### 13…Bd6?? in a lobby game: the knight in the way (7 October)

The user, Black, explained 13…Bd6 (Scandinavian, after 12…Bb4 13.a3) and
read "Blunder: White wins the queen for a bishop and a knight", with the
arrows 14…Qxd2+ 15.Qxd2 (and, at depth 20, 14…cxb5 15.Bxa5): a queen
falling across c3, where the white knight still stood. The text was right,
the arrows were not enough: the realization is 7 plies, `FocusWindow` drew
the biggest jump and the move before it, and 14.Nb5 — the knight stepping
aside so that the bishop of d2 attacks the queen, a discovered attack — is
neither a capture, a check, nor the move right before the jump. Drawing
the first move always was tried and dropped: a recorded case (12.b4 before
12…Bxf7+…) got an arrow that says nothing. `FocusWindow` now draws the
first move of a refutation or a threat when it uncovers an attack
(`Discovers`: with the turn passed back, a piece behind it takes across the
square it left), and the better move is then not drawn on top, as for any
focused refutation. Arrows: 14.Nb5 1, 14…cxb5 2, 15.Bxa5 3. Recorded:
user-feedback.ticks.

### 10.d4?: the guard driven away (7 October)

The user, White in training, after 10.d4 (+0.4 → −2.5): Explain drew only
11…Bxg2 and 12…Bxh1, and "the queen can take back on g2". It cannot: the
line is 10…Nf5 11.Qd3 Bxg2 12.Qxf5 Bxh1, the knight drives the queen of g3
away first. FocusWindow drew the biggest jump traced back to the bishop,
not the deflection. `GuardDriven(line, k, first)`: the capture moves[k]
takes what a piece of the losing side guarded until moves[k - 1] moved it
(had it stayed, it could take back on that square). Its way there from the
start of the line is drawn before the capture, with the winning side's
move that set it going: an attack it fled (10.d4: 10…Nf5 11.Qd3, the threat
on g3 dashed) or a bait it took (10.d3: 10…Nf5 11.Qh3 Bc5 12.Qxf5 — the
queen still guarded g2 from h3, and left it for the knight). Recorded:
user-feedback.ticks.

### 10.d3: a far win is no win when the loser grabbed a piece (7 October)

The user, after 10.d3 (+0.4 → −1.2): "why should the queen take the
knight, if it does not pay? Where is the evidence it is forced?" There is
none. After 10…Nf5 11.Qh3 Bc5, MultiPV at depth 22: 12.Qxf5 −1.06, 12.Kd1
−1.17, 12.Nge2 −1.27, 12.Kf1 −1.40: 12.Qxf5 is one choice among equals,
and Explain told the line through it ("Black wins a rook and a pawn",
13 plies on) as what 10.d3 lost. Now a realization past LONG_PLIES is a
win only if the losing side grabs no piece by choice on the way
(`IsForced`: a capture worth THREAT_GAIN that is no recapture and not out
of check); quiet moves are its best defence and do not count (the
Evergreen's 17…O-O). Otherwise the move is explained by its threats.
`Threats` counts a capture with check that cannot be taken back as a
threat (`ChecksUntaken`: 11…Bc5 and Bxf2+, the queen of b6 covering f2),
said "threatens Bxf2+, and the king cannot take back" when the king stands
beside (`KingBeside`); `ThreatText` says up to two single threats of the
same side, the second from a move that takes nothing, and draws a
threatening move no arrow shows yet. 10.d3 now: "10…Nf5 attacks the
queen on g3: 11.Qh3 parries it. 11…Bc5 threatens 12…Bxf2+, and the king
cannot take back." 3.a4 gains "4…Qe7 threatens 5…Qxe4+". `PASS` now
counts Black's pass as the end of a move in both clients (a White threat
after Black's move read "7.Qxb8+" for 8.Qxb8+).
