# SMART tests

Each `.ticks` file holds records of moves explained: the ticks EXPLAIN.smart
was fed (the engine's lines, depth after depth) and, in `expect` lines, what
the explanation must end up showing. Every client replays them — the desktop
suite (`tst_chessrules::replaysRecordedTicks`) and the Android one — so the
interpreters, and the chess the clients give the programs, stay the same.

- A new case: `pragma-explain --record smart/tests/<name>.ticks "<moves>"`
  records a move with what is shown now; the desktop client records every
  move it explains with `PRAGMA_EXPLAIN_RECORD=<folder>`.
- After a deliberate change to EXPLAIN.smart, `pragma-explain --replay
  <file>` says which records explain otherwise; fix the program, or update
  their `expect` lines when the new explanation is the better one.
