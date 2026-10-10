# SMART

SMART is the small language the chess judgement of Pragma Chess is written
in. The files of this folder hold it:

- `EXPLAIN.smart` — Explain: the arrows, lost pieces and summary that justify
  the evaluation of the position on the board;
- `TUTOR.smart` — the tutor of a training game: whether the move just played
  was an error;
- `INSIGHT.smart` — the plans of the engine's line, drawn over the position
  at its end while the Engine panel's eye is held: the routes of the pieces,
  the king's march, the pawns that run. A reading of the line, called once
  per line (`Insight`), not per tick.

Every client runs these same files — the desktop client, the command line
tool `pragma-explain` and the Android app —, each with its own interpreter
(C++ in `gui/qt/src/app/smart`, Kotlin in the app). So Explain is the same
everywhere not because the code is shared but because the behaviour is: a
position explained badly is fixed here, once, and the fix reaches all of them.

## Reacting, not driving

A SMART program never drives an engine. The client runs its engine as it
always does — the live analysis of the desktop client, the search of the
command line tool — and hands every line the engine reports to the program,
a *tick*. The program answers each tick with what to show: the explanation
grows with the search, arrows appear and settle as the engine goes deeper.
What a program keeps between ticks (its global variables) is its memory:
that is how it decides when a new line is steady enough to replace the one
shown.

Because the input is just the sequence of ticks, a sequence recorded in one
client can be replayed in another: that is how a wrong explanation becomes a
test, and how the interpreters are kept identical.

## The language

A BASIC: lines of statements, no types to declare, nothing clever. Keywords
and names are case-insensitive (`Material`, `MATERIAL` and `material` are one
name); texts keep their case.

### Lines and comments

One statement per line, or several separated by `:`. A `'` starts a comment
that runs to the end of the line, and so does `REM` at the start of a
statement.

```basic
' The share of a swing that material must account for.
CONST REALIZED_SHARE = 0.4
LET needed = MAX(MINIMUM, INT(drop * REALIZED_SHARE)) : NOTE "needed " + STR(needed)
```

### Values

- **Numbers**, always decimal (`3`, `-0.5`). `INT` cuts towards zero.
  `-0` is `0`: equal to it, in lists too.
- **Texts** in double quotes; a double quote inside is written twice
  (`"say ""hi"""`).
- **Lists**: `[1, 2, 3]`, `[]`. Indexes start at 0: `moves[0]`. A list is a
  value: `b = a` copies it, and changing `b[0]` leaves `a` alone.
- **`NOTHING`**: no value (no previous position, no evaluation yet).
- **`TRUE`** and **`FALSE`** are the numbers 1 and 0.
- **Objects** the client hands over — a position, an evaluation, a line —
  used only through the client's functions.

A condition must be a number (0 is false, anything else true) or `NOTHING`
(false); a text, a list or an object there is an error, so that a forgotten
comparison does not pass silently.

### Variables

`LET x = 1` or just `x = 1`; `list[2] = x` changes one element.
`CONST NAME = value` at the top level names a value that cannot change.

Variables assigned at the top level are **global** and live as long as the
program: they are its memory between ticks. Inside a `FUNCTION` a variable
assigned there is **local**, unless the function declares it
`SHARED name` (then it is the global one). A function may read a global
without declaring it. Reading a name nothing has assigned is an error.

### Statements

```basic
IF cond THEN
    ...
ELSEIF cond THEN
    ...
ELSE
    ...
END IF

IF cond THEN statement : statement ELSE statement   ' all on one line

FOR k = 1 TO 10 STEP 2     ' STEP is optional (1); it may be negative
    ...
    EXIT FOR
NEXT                       ' or NEXT k

WHILE cond
    ...
    EXIT WHILE
WEND

FUNCTION Gain(line, k, side)
    RETURN (MATERIAL(POSITION(line, k)) - base) * side
END FUNCTION
```

`RETURN` without a value returns `NOTHING`. A function is called in an
expression with parentheses, `Gain(line, 3, WHITE)`; as a statement its
arguments go without them, `Draw line, 0, 4`, or with `CALL`:
`CALL Draw(line, 0, 4)`. The clients' commands are called the same way:
`SAY "Checkmate."`. (A statement starting `Draw (a + b) * 2, c` passes the
two arguments `(a + b) * 2` and `c`.)

The grammar needs one token of lookahead and never goes back: a statement
starting with a name is an assignment if `=` or `[` follows the name, a call
otherwise. A call statement cannot start with a list literal, `Show [1, 2]`:
that reads as `Show[1, 2] = …`; write `CALL Show([1, 2])`.

Names do not tell case apart, so a program's function must not take the
name of one of the client's (`Arrow` would hide `ARROW`): the program's
function wins.

The top-level statements run once, when the program is loaded: they set the
constants and the memory. Then the client calls the program's entry
functions (`Tick`, `Judge`…) as often as it needs.

### Expressions

From the loosest to the tightest:

| | |
|---|---|
| `OR` | either (stops at the first true) |
| `AND` | both (stops at the first false) |
| `NOT` | |
| `=` `<>` `<` `>` `<=` `>=` | comparisons; `=` and `<>` take any values, the others numbers or texts |
| `+` `-` | `+` also joins texts (a text and a number: the number as `STR` writes it) and lists |
| `*` `/` `MOD` | `/` is exact (`7 / 2` is 3.5); `MOD` keeps the sign of the left side, as `INT` |
| `-x` | |
| `f(x)` `list[i]` | calls and indexes |

Comparisons and logic give 1 or 0.

### Built-in functions

| | |
|---|---|
| `LEN(x)` | elements of a list, characters of a text |
| `INT(x)` | the whole part, towards zero |
| `ABS(x)` | |
| `MIN(a, b, …)`, `MAX(a, b, …)` | |
| `STR(x)` | a number as text: whole numbers without decimals, others with at most 6 decimals, trailing zeros dropped |
| `FIXED(x, n)` | a number as text with exactly `n` decimals, halves rounded away from zero (`FIXED(2.25, 1)` is `"2.3"`, `FIXED(7, 2)` is `"7.00"`, `FIXED(-0.04, 1)` is `"0.0"`) |
| `TRIM(text)` | the text without the spaces at its ends |
| `REPEAT(value, n)` | a list of `n` copies |
| `SLICE(list, from, count)` | `count` elements from `from` (fewer at the end) |
| `CONTAINS(list, value)` | 1 or 0 |
| `INDEXOF(list, value)` | the first index, or -1 |

### Errors

A mistake in a program — a name nothing assigned, a list index out of
range, a condition that is not a number, a call with the wrong number of
arguments — stops it with the file's line number; the client shows nothing
for that tick and logs the error. A call that runs more than two million
statements is stopped too: a loop that never ends must not freeze a client.

## What the clients provide

The chess itself — positions, legal moves, material, the notation, the
engine's evaluations — and the commands that collect what a program shows
(arrows, texts) belong to the clients; `EXPLAIN.smart` and `TUTOR.smart` say
at their top which ones they use. Among the commands, `LOST square` rings a
piece that falls and `THREATENED square` rings, dashed, a piece attacked
that does not; `CAGE square` is a square of the region a losing king is
confined to, drawn as one fenced area. `ATTACKED(p, square, side)` says
whether `side` controls a square with the other side's king taken off the
board — whether that king could step there. `VIEWER()` says who asked — the side
the user plays, or sees from below —, so that a program can draw the other
side's plan as theirs rather than as a good idea. Texts a program says are English and
written whole (`"%1 mates in %2: %3."`): each client translates them with
the rest of its interface, `%1`, `%2`… replaced by the arguments.
