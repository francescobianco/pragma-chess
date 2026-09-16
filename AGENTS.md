# AGENTS.md

Guidance for coding agents (Claude Code, Codex, Copilot, Cursor, Gemini, …)
working in this repository. Humans should start from [README.md](README.md).

## What this project is

**Pragma Chess** — an open-source chess database engine with a native
cross-platform desktop client (think ChessBase/SCID, with a modern, modular
architecture). The vision and architecture are in [DESIGN.md](DESIGN.md)
(Italian); read it before making structural decisions.

Layering, top to bottom — dependencies only point downwards:

```text
Desktop GUI (Qt 6 Widgets, C++20)   gui/qt
        │  GameDatabase interface (gui/qt/src/app)
Application core
        │
Chess DB engine (Rust)              core/, cli/
```

- The engine must never know Qt exists. It must be usable headless via the CLI.
- The interesting problem is indexing and searching millions of games and
  positions quickly, not drawing the board. The board is just one view.
- "Native desktop first": use Qt/OS widgets and behaviours (menus, docks, file
  pickers, shortcuts, drag & drop). No Electron, no web-style UI.

## Repository layout

```text
Cargo.toml             Rust workspace (members: core, cli)
core/                  chessdb-core: chess model, PGN, storage, index, search
  src/chess/           bitboards, types, FEN, move generation, positions
cli/                   chessdb-cli, binary `chessdb`
CMakeLists.txt         top-level CMake, only adds gui/qt
gui/qt/
  src/main.cpp         entry point (signal handling → clean quit, session save)
  src/MainWindow.*     main window, menus, docks, layouts, projects
  src/app/             non-widget logic: GameDatabase interface, SQLite .pdb
                       implementation, GameSession, ChessPosition (rules),
                       BoardState, Project (.pch), UciEngine, Explainer and
                       MoveExplanation, UserFolders, ClassicGames seed data
  src/models/          Qt item models (games list, move list)
  src/widgets/         board, evaluation bar, engine panel, game header, …
  src/dialogs/         dialogs
  src/platform/        GTK/GNOME desktop style, flat symbolic icons drawn in code
  resources/pieces/    SVG piece sets (Good Companion)
  data/                .desktop file
  tests/               Qt Test executables (added to CMake only if Qt Test is found)
scripts/dev-watch.sh   rebuild + restart loop used by `make start`
```

## Current state (keep in mind)

- The **Qt GUI is the working part**. It reads `.pdb` databases directly via
  `SqliteGameDatabase`; the Rust engine will later replace it behind the
  `GameDatabase` interface through a C API, without widgets changing.
- Chess rules in the GUI live in `ChessPosition` (legal moves, SAN, FEN,
  attacks): an interim implementation with the same status as
  `SqliteGameDatabase`, to be replaced by the Rust core. `BoardState` is
  display-only.
- The **Rust workspace is incomplete**: `core/src/lib.rs`, `cli/src/main.rs`
  and some modules declared in `core/src/chess/mod.rs` (`san`, `zobrist`) are
  not in the repository yet, so `cargo build` does not currently succeed.
  Don't assume Rust code is wired up; check before relying on it.
- Tests: `gui/qt/tests/tst_chessrules.cpp` (Qt Test, no display needed) covers
  `ChessPosition` with perft counts and the "Explain" logic with synthetic
  engine lines. There are no widget tests.

## Explain

"Explain" (button between previous/next move, key `E`) is a core feature: it
draws arrows justifying the evaluation, judged against the position before
the last move. It applies to the move on the board only: navigating or
playing a move turns it off, and the user asks again at the next move.

- `app/MoveExplanation.*` — pure logic, no Qt widgets or processes; unit-test
  every change here. It replays the engine's principal variation and finds
  where the evaluation becomes concrete: material won once exchanges, checks
  and recaptures are over (and stays won for a few plies), or a mate.
- `app/Explainer.*` — gathers the evaluations: the current position from the
  main analysis, the previous position from a second, depth-limited engine
  process, with a cache by position.
- `widgets/BoardWidget` only paints `BoardArrow`s and lost-piece rings.

## Build and run

GUI (Debian/Ubuntu):

```bash
make deps     # apt install Qt 6 (base, svg, sqlite driver), cmake, ninja, inotify-tools
make build    # configure (build/, Debug, Ninja if present) and build
make run      # build and launch build/gui/qt/pragma-chess
make test     # build and run the tests (ctest)
make start    # launch, rebuild and restart on every change (interactive, long-running)
make clean
```

`make start` never exits — don't run it from an agent unless it is backgrounded.
To verify a change compiles, use `make build` (or `cmake --build build`).
Compiler warnings are on (`-Wall -Wextra -Wpedantic`); don't introduce new ones.

Rust (once the workspace is complete):

```bash
cargo build
cargo test          # chessdb-core is built with opt-level 3 even in dev (perft)
cargo run -p chessdb-cli -- <args>
```

## Conventions

### C++ / Qt

- C++20, Qt 6.4+, Qt Widgets only (no QML).
- `QT_NO_KEYWORDS` is defined: use `Q_SIGNALS`, `Q_SLOTS`, `Q_EMIT`, never
  `signals`/`slots`/`emit`.
- **New source files must be added to `gui/qt/CMakeLists.txt`** (explicit list).
- Classes `PascalCase`, one class per `.h`/`.cpp` pair; methods `camelCase`;
  members `m_name`; constants `kName`; headers use `#pragma once`.
- Includes: own header / project headers first, then Qt, then std.
- Brief `///` doc comments on public API explaining *why/what*, not restating code.
- User-visible strings go through `tr()`. Code, comments and identifiers in English.
- Widgets hold no domain logic; put it in `src/app/` and keep the GUI talking to
  the database only through `GameDatabase`.
- Qt SVG is optional: code guarded by `PRAGMA_HAS_SVG` must still build without it
  (font glyph fallback).

### Rust

- Edition 2021, shared dependency versions in the workspace `Cargo.toml`.
- The chess model is on the hot path of import and search: avoid allocations,
  keep positions small `Copy` values backed by bitboards.
- Errors in the chess layer use `ChessError`; the CLI uses `anyhow`.

## File formats and user data

- **`.pdb` database**: SQLite with `PRAGMA application_id` = `PRAG` and schema
  version in `PRAGMA user_version`. Changing the schema means bumping the
  version and handling older files.
- **`.pch` project**: YAML (yaml-cpp, system package or fetched by CMake)
  capturing database, open game/ply (or the moves of a game not saved to the
  database), board orientation, engine, window layout.
  It is versioned; newer files are rejected with an error.
- Default user folder: `~/Chess/Pragma/{Databases,Projects}`, localized
  (e.g. `~/Scacchi/Pragma/…`); `PRAGMA_CHESS_DIR` overrides it. First launch
  seeds `Classic Games.pdb`. The last session is restored on startup.
- When testing, set `PRAGMA_CHESS_DIR` to a scratch directory rather than
  touching the user's real chess folder.

## Engines

Engines are generic **UCI** processes (`UciEngine`, `QProcess`). Stockfish is
only the default, found in `PATH` or common locations; never hard-wire
Stockfish-specific behaviour. Scores are normalized to White's point of view.

## Working agreements

- Keep changes focused; follow the style of the surrounding code.
- Update README.md / this file when build steps, formats or layout change.
- Commit messages: short imperative subject, blank line, then a bullet list of
  the user-visible changes (see `git log`).
- Don't commit build output (`build/`, `target/`) or IDE files (`.idea/`).
