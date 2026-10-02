# Changelog

All notable changes to Pragma Chess are listed here, newest version first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **Trash.** Right-click a game of the list and choose *Move Game to Trash*.
  The database tree ends with *Trash*, where a game can be restored or
  deleted. A game in the trash is in no other list and is not searched.
- **Optimize Database.** Deleting is never immediate: *Database ▸ Database
  Settings… ▸ Optimize Database* removes the deleted games for good and
  compacts the file. Trashing, deleting and optimizing reach the other
  devices through the sync, so a deleted game does not come back.
- The Android app hides the games a computer trashed or deleted, and asks to
  be updated when a database was made by a newer version of Pragma Chess.

### Changed

- The `.pdb` schema is now version 6 and is described by migrations: a
  database of an older version is upgraded step by step when it is opened.
  A database opened by this version can no longer be opened by 0.2.0 or by
  the Android app before this change: both say it was made by a newer
  version.
- In the games list, the Elo, Date, ECO and Moves columns are centred.

### Fixed

- Dialog buttons and the other texts that come from Qt (OK, Cancel, Save,
  Discard…) follow the interface language in every package: on Windows and
  macOS they stayed in English.
- On macOS the open and save panels and the application menu follow the
  interface language.
- The Linux menu entry and the description in software centres are
  translated into Italian.

## [0.2.0] - 2026-10-02

### Added

- **Android app (beta).** A board with navigation buttons, a native side
  menu, a local "My Games" database, games entered on the phone and analysis
  with any chess engine app installed on the phone (Open Exchange, as in
  DroidFish). The APK is linked from the README.
- **Phone Link.** *Options ▸ Connect Mobile App…* shows a pairing QR code and
  the connected phones. Phone and computer find each other over Nostr relays
  and exchange databases over a WebRTC data channel.
- **One corpus across devices.** Every database and every game has a
  universal id, so copies on the phone and on several computers are merged
  game by game (the newer version wins a conflict) and the phone carries
  databases from one computer to another.
- **Duplicate databases are merged.** Files that are the same database are
  merged into one without losing games, and the merge propagates to the
  other devices through the folder sync.
- **Manage Engines.** *Engine ▸ Manage Engines…* creates and edits engines
  (executable, threads, hash); *Detect Engines* adds the UCI engines
  installed on the computer. *Engine ▸ Use Engine* chooses the engine for
  analysis, saved in the project.
- **Stockfish is bundled.** The installers ship Stockfish 19 as the default
  engine, so a new installation analyses right away; its source is attached
  to every release.
- **Database Settings.** *Database ▸ Database Settings…* sets what a database
  is (Game Collection or Opening Book), its name and a description, stored in
  the file so they travel with it.
- **Filter games by the board.** The database tree has a *Board* branch:
  *Position* lists the games in which the position on the board occurs,
  whatever the move order, and *Variant* the games that begin with exactly
  the moves played so far. Both follow the board, with live counts.
- **Repertoire.** Right-click a move of the Opening Tree to add it to your
  repertoire or remove it. Repertoire moves are listed first, in bold. The
  mark is stored in the Polyglot book itself, which still works in engines
  and other programs.
- **Database results in the Opening Tree.** A *Database* column shows how
  many games of the open database reach the position after each book move,
  and how many White won, drew and Black won.
- **Italian translation** of the desktop interface.
- **Opening names in English and Italian**, chosen in *Options ▸ Opening
  Names*; until one is chosen they follow the interface language.
- *Book ▸ New Book…* creates a copy of the shipped book under a new name.
- *Options ▸ Board Settings…* places the captured pieces beside or below the
  board and shows whose turn it is with a dot beside the board.
- Permanent download links: every release also carries each package under a
  name without the version, so the README always links the latest installer.

### Changed

- The move list draws the pieces with the figurines of chess books (SkakNew,
  bundled as the "Pragma Figurine" font).
- The captured pieces moved from the game controls to a column beside the
  board, next to the player who took them, greyed out so they do not draw
  the eye.
- The menus are now Game, Book, Engine, Database.
- The opening names databases live in `Books/Opening Names` instead of the
  Databases folder: they are reference data, not games. Copies seeded by the
  previous version are moved there at startup.
- The `.pdb` schema is now version 5 (database properties, universal ids for
  databases and games). Older files are upgraded when opened.

### Fixed

- The status bar text keeps clear of the window edges (rounded corners on
  macOS).

## [0.1.0] - 2026-09-28

First public release.

### Added

- **Desktop client.** A native Qt 6 Widgets application: board, move list,
  engine panel, opening tree and games list, with native menus, dialogs and
  shortcuts, flat icons that follow the theme and the Good Companion piece
  set. Installers for Windows, macOS (Apple Silicon), Debian/Ubuntu and
  Fedora.
- **Databases.** Games are kept in `.pdb` files (SQLite) in a localized chess
  folder (`~/Chess/Pragma/Databases`), managed from the Database menu. The
  first launch seeds *Classic Games*.
- **Database tree.** Next to the games list, the open database is browsed by
  ECO (letter, then code), tournament, year and source, with counts;
  selecting a node filters the list.
- **Who Is This?** Right-click a player in the games list to say it is you, a
  friend or an opponent. The tree lists Me, Friends and Opponents, and
  opening a game where you play turns the board to your side.
- **Game header.** Players, year and tournament above the board; clicking it
  edits players, Elo, event, site, date, round, result and ECO.
- **Entering games.** *Game ▸ New Game* enters a game move by move by
  clicking or dragging pieces, with promotion choice; *Save Game to Database*
  stores it. Playing a different move in a stored game starts an unsaved
  game instead of changing it.
- **Projects.** `.pch` files capture the whole workspace: database, game and
  move, board orientation, engine and panel layout. The last session is
  restored on startup, and *View ▸ Reset Panel Layout* puts the panels back.
- **Engine analysis.** Any UCI engine (Stockfish is found automatically),
  with an evaluation bar beside the board and the score, depth and best line
  in the Engine panel.
- **Explain** (the light bulb between previous and next move, key `E`).
  Arrows on the board justify the evaluation of the last move: the engine's
  main line is replayed until the advantage becomes concrete (material won
  once exchanges are over, or a mate), showing refutations, replies, the
  better move and the pieces lost, with a summary in the Engine panel. A
  forced mate is played on the board with a red frame, and the board border
  shows when Explain is searching and when the answer is on the board. A
  purely positional drop is said to be positional rather than drawn as lost
  material.
- **`pragma-explain`.** The same explanation on the command line, on lines
  pasted as PGN, SAN or UCI, with a trace of how it is reached.
- **Training.** *Game ▸ New Training…* starts a game against the engine with
  the colour you choose. The engine's best line is hidden while you think,
  its move crosses the board slowly so it cannot be missed, and the game is
  saved to the database when it ends.
- **Game sources.** *Database ▸ Connect Source…* connects a lichess.org
  account (OAuth sign-in), a chess.com account or a torneionline.com player
  (by FIDE or FSI ID) to the open database. Sources are synced in the
  background, and games already imported are skipped.
- **Opening books.** The Book menu chooses a Polyglot book (`.bin`); the
  first launch seeds *Pragma Openings*, built from the public domain lichess
  chess-openings collection. The Opening Tree lists the book moves of the
  position with their weight and the opening each leads to; clicking one
  plays it.
- **Opening names.** The Engine panel shows the opening the game is in. The
  names come from an ordinary database that can be edited or replaced.
- **`pragma-book`.** Builds and probes Polyglot books on the command line.
- **Folder sync.** *File ▸ Sync…* keeps the databases and projects the same
  on several computers through an FTP/FTPS or WebDAV folder, or a Git
  repository. The sync reconciles and never deletes: every computer ends up
  with the union of what all of them have, and conflicting edits keep both
  versions.
- **Sync Now** (`Ctrl+Y`). One toolbar button syncs the connected sources,
  saves the project and the session, then sends the folder to the server.
  *Sync before closing* does the same when the window is closed.
- **Copy.** *Edit ▸ Copy* copies the moves, the PGN (whole or up to the
  current move), the FEN, the engine line or the explanation.
- **Board details.** Figurine notation in the move list, engine line and
  Explain summary; captured pieces shown next to the game controls; a king in
  check or mated is marked with a glow; captures and castling are animated
  as they happen on a real board.
- **Language.** *Options ▸ Language* chooses the interface language.

[Unreleased]: https://github.com/francescobianco/pragma-chess/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/francescobianco/pragma-chess/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/francescobianco/pragma-chess/releases/tag/v0.1.0
