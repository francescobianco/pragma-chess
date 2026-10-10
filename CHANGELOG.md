# Changelog

All notable changes to Pragma Chess are listed here, newest version first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- The logo has a rich version, with chess pieces on its squares, shown in
  the welcome window; the application's icon keeps the plain
  board, which reads better when small.
- **Endgame Training** and **Tactics Training** hide the Elo, Result, Date,
  Site, Moves, ECO and Line columns, which a puzzle has nothing to put in;
  right-click a column title to show them again. Their Event column is
  called **Theme** and shows each puzzle's theme in your language: *Fork*,
  *Mate in 2*, *Rook Endgame*.
- In **Endgame Training** and **Tactics Training** the players have names:
  yours on the side you play, *Pragma Coach* on the other — stand-in names,
  never written in the database.
- A user who gave no name gets a champion's with three digits, such as
  *Spassky007*, different for everyone, so the lobby is not full of players
  called "Me"; Personal Settings shows it, to keep or change.
- The **Pawn Endgames** and **Rook Endgames** projects open **Endgame
  Training**, to try what they teach; a project finds its database again
  even when the file was moved or renamed.
- Projects (`.pch`) and databases (`.pdb`) open with Pragma Chess from
  your file manager — a database in the project you left open — and their
  icon is your system's document with a pawn on the page (a database with a
  pawn for databases).
- A **`.pragmaignore`** file in the synced folder on the server (FTP,
  WebDAV or Git) lists files that stay there only, written as in
  `.gitignore`: the README and LICENSE of your repository no longer land in
  your Pragma folder.
- The chess folder in your home (Chess, Scacchi…) and the Pragma folder in it have their own icon: the
  system's folder with a pawn on it, as Videos has a film and Pictures a
  picture, drawn in the colours of your icon theme.
- Two new projects, in English and Italian: **Pawn Endgames** (the square,
  the king in front of its pawn, the opposition, the rook’s pawn, the outside
  passed pawn, the breakthrough; every line checked with an engine) and
  **Fischer – Spassky 1972**, the sixth game of the Reykjavik match with
  comments.
- A **welcome window** at startup, over the main window: what Pragma Chess
  does, your projects and databases (read from their folders) to open with a
  click — a database opens in a new project — and New Project. "Don't show
  this window at startup" turns it off; Help ▸ Welcome… opens it again.
- The **Opening Tree** is shown at the first start, two thirds of its row
  beside the move list.
- A new database type, **Puzzles and Training** (Database Settings): its
  games list does not show the moves, so the solution stays hidden, and a
  game opened from it starts in Training Mode, the board turned to the side
  to move. Endgame Training and Tactics Training are of this type now.
- A move other than the game's next one, played in the middle of a game,
  asks whether to **insert it as a variation** or to **replace the main line**
  (or the variation) from there.
- **Time control** of a game: games imported from lichess.org and chess.com
  and online games keep theirs, Game Information lets you write it (3+2,
  90+30), and a new Time Control node of the database tree lists them —
  Blitz 3+2, Rapid 10+5, Classical 90+30 — to choose the games played at
  one. It is PGN's TimeControl tag, so it goes in and out with PGN.
- A database named in Database Settings is shown by its name, with the
  file in brackets — "My Games (games.pdb)" — in Switch Database and at the
  top of the tree, the name in bold; right-clicking it there opens Database
  Settings… too.
- Insert ▸ Game Break puts the new game right under the game you
  right-clicked in, always; a game with no moves yet has its own row in the
  move list ("1. …") and stays until deleted. Right-click a game for Delete
  Game (Delete Line when it does not start from move 1), and the number of
  its first move for Change Move Number…, so a line from a set-up position
  can start from move 12.
- Right-click the rule of a game break in the move list: **Delete Game
  Break** when nothing was entered after it, **Delete Following Game** when
  the game after it has moves. Titles and subtitles have Edit and Delete by
  their own name. A chapter that came by itself, from things put in a
  project without chapters, goes away again when they are deleted.
- The ChessBase files source reads the databases of **ChessBase 17 and
  later** too (`.2cbh`), with their variations and **annotations**:
  comments (in your language, when ChessBase has them in several), symbols
  such as `!` and `+/=`, coloured squares and arrows drawn on the board, and
  the engine's evaluation of each move. Choose the `.2cbh` file as you would
  a `.cbh`.
- **Null moves** (`--`): a game written by hand, in ChessBase or in PGN,
  where a side passes is read whole instead of stopping there; the move
  list and PGN show `--`, and the engine analyses the position after it.
- **Computing Power** for each engine (Engine ▸ Manage Engines…): how much
  of the processor it may use while it analyzes, from Minimum (3%) to
  Full (no limit), Medium (10%) by default, so the fans stay quiet and the
  rest of the computer stays free. The engine is just as strong, only
  slower: fewer threads, a lower priority and, on Linux and Windows, a hard
  cap on its share of the processor.
- **Where the line leads, and how**: hold the eye beside Stop Analysis and
  the board shows the position at the end of the engine's line, following
  the engine as it goes deeper. Violet arrows show the plans the line holds:
  a knight's route, a rook lift, the king marching in an endgame, a pawn
  running or breaking through, each arrow passing through the squares where
  the piece stopped.
- A new board style, **Classic Book**: the diagram of a printed chess book,
  one old paper with the dark squares hatched in diagonal ink lines, the
  Good Companion pieces, and a margin of paper around each piece standing
  on a dark square, without coordinates (Options ▸ Personal Settings…).
- A new source, **Lichess Study**: paste the address of a study on
  lichess.org and each of its chapters becomes a game of the database, with
  its comments and variations, the study and the chapter in its PGN tags
  (StudyName, ChapterName, ChapterURL). Chapters changed on lichess.org
  replace their game at the next sync. Public studies need no account; a
  private one needs signing in. Writing to the study is coming.
- Comments: the text between the moves of a PGN game or a lichess study is
  kept with the game and shown in italics in the move list; the commands
  programs put in comments (evaluations, clocks, arrows) are kept, not
  shown.
- Games keep the PGN tags that have no field of their own (TimeControl,
  Opening, Annotator…), and write them back to PGN.

- The sound of a piece set down on the board at every move: yours, the
  engine's in training and your opponent's online (when their piece
  lands). Options ▸ Graphics Settings… turns it off.

### Changed

- **Connect Mobile App** shows the app as the phone does, with its icon,
  and where to get it: the Android APK, and Google Play, F-Droid and the App
  Store marked as coming soon. The Android app's icon is the new logo.
- On Linux the application's icon is the rich logo, the pieces on the
  board, wherever it is shown large enough to read (GNOME's dock, the
  applications grid, Alt+Tab).
- The **Windows installer** asks less and looks like Pragma Chess: the
  language is taken from Windows, it installs for you without asking (an
  administrator can use `/ALLUSERS`), there is no license to accept and no
  summary to confirm; every page carries the welcome window's picture with
  the logo, the name and the motto, and the welcome and last pages speak in
  our words. The option to open projects and databases with Pragma Chess is
  named right.
- A project in one language **declares** its language in Project
  Information: its texts are written in it whatever the interface's, and
  declaring another relabels them; a multilingual project chooses there the
  language worked in. A new project takes the interface's language.
- Project Information has a **Description**, an **Author**, **Contacts**
  and an **Edition**, a line each, kept in the project file.
- File ▸ Project Settings… is now **Project Information…**: it opens
  locked, and **Edit**, with its padlock, unlocks the fields; what each one
  means is in a balloon behind its **?**.
- **Explain** plays a mate on the board only when you can use it: one
  against you within 4 moves, yours within 3, or a combination of checks
  and sacrifices. A long quiet mate, like queen against king, is technique:
  Explain says the mate and draws the plan of the side that mates instead
  of every move, with the cage the losing king is shut in.
- The evaluation bar reaches a pixel into the board's frame, top and bottom.
- In Italian the databases we distribute are called **Partite storiche**,
  **Allenati sui finali** and **Allenati sulla tattica**, and the project
  **Finali di torre**: copies already installed take the new names.
- The welcome window lists projects and databases in alphabetical order.
- Variations in the move list start where the moves above them start.
- The evaluation bar has a hairline edge, the board's own, so White's side
  stands out on a light window.
- Titles and subtitles in the move list have less room under them.
- The `.pdb` schema is now version 8 (the games' other PGN tags and their
  comments). A database opened by this version can no longer be opened by
  0.3.0, nor by the Android app before 0.3.1.

### Fixed

- On a dark theme the boxes of checkboxes and radio buttons, in dialogs
  and in menus (Engine ▸ Training Mode…), have a light edge: it was black on
  dark grey, hard to see.
- View ▸ Reset Panel Layout no longer leaves the move list's White column
  too wide, with Black's out of sight (it happened every other time).
- On GNOME's Wayland the File menu, opened a second time, no longer has its
  highlighted entry reaching past the menu, nor its submenus opening too far
  right.
- While playing online, the status under the clocks showed "â" in place
  of its dash.
- The moves of a variation in the move list could not be clicked.
- After a restart the move list could be built narrower than its header
  (its columns, paragraphs and titles some pixels short) until something
  redrew it.
- Players and tournaments of a ChessBase database converted by a recent
  ChessBase are no longer read four characters off ("ÿÿÿÿZukertort").
- The application could keep a processor core at 100% while idle: the move
  list rebuilt itself again and again when its scroll bar came and went.
- On Windows with a dark theme, Database ▸ Connect Source… showed a white
  window on which the texts could not be read; with a light theme some of
  them were white too. It is now drawn with the theme's colours like the
  other dialogs.
- On Windows the dialogs no longer show the application's icon in their
  title bar (the Connect Source one showed it out of place): only the main
  window does, as on GNOME.
- The application could crash at startup while it was resuming an online
  game left in progress.

## [0.3.0] - 2026-10-05

### Added

- Chapters: a project is now a collection of chapters, as a study or a
  chess book, and a chapter holds games one after the other. The move list
  shows the whole chapter, a light rule where each game begins and its
  numbering starting again. File ▸ New Chapter…, Switch Chapter and Manage
  Chapters… (reorder, rename, add, delete).
- Paragraphs between the moves: right-click the move list ▸ Insert
  Paragraph and write right there, set as in a book (its own face, each
  paragraph indented). Insert Game Break starts a new game in the chapter.
- New games, set-up positions, pastes, training and online games, and
  games opened from the list, are added to the chapter instead of
  replacing the game on the board.
- File ▸ Project Settings…: a name for the project, shown in the title bar
  (with the chapter, when there are several).
- Game ▸ Save Game to Another Database… saves the game without opening
  that database.
- Options ▸ Personal Settings…: your name, year of birth and FIDE ID. Your
  name goes on your side of new games, set-up positions and training games
  (a player marked as Me in the open database still wins). They are kept in
  `.pragma-chess.conf` in the Pragma folder, which Sync carries to your
  other computers.
- Board style, in Options ▸ Personal Settings…: Pragma Classic (the brown
  board and the pieces of chess books) or Lichess Alpha (lichess.org's
  green board and Alpha pieces).
- Game ▸ Set Up Position… draws a position on a board — pieces, side to
  move, castling, en passant, move number, or a FEN — and starts a game from
  it. Moves on the board not saved yet can be discarded, saved to the open
  database or saved to another one, which is not opened.
- Edit ▸ Paste is a menu: Paste FEN, Paste Line (a new game with the moves
  on the clipboard) and Paste Line from Current Position (Ctrl+Alt+V, plays
  them from the board: added at the end of the game, a variation elsewhere).
- A new source, PGN file: a `.pgn` file on this computer kept in step with
  the database, in the direction you choose — Read and write, Read only or
  Write only. The file can be created while connecting it. Games saved,
  changed or annotated reach the file a few seconds later; games added or
  edited in the file come into the database. Nothing is deleted on either
  side, and a game changed on both sides is kept twice.
- Options ▸ Folder Settings… chooses where this computer keeps the Pragma
  folder, or just the databases, projects, books or opening names. The new
  folders are used from the next start; files are not moved.
- Folder sync notices a file deleted by hand from the Pragma folder (it no
  longer comes back by itself) and asks: Delete Everywhere, Restore, or Ask
  Me Later. Each computer keeps what it last synced in a hidden
  `.pragma-chess.local` file of its Pragma folder, which is never uploaded.
- Manage Files…, in the Sync Settings, lists the files on the server and
  deletes the ones you no longer want from every synced device, after a
  confirmation.
- With a Git repository, a sync that only receives or finds nothing new no
  longer makes a commit, and each commit is named after the files it
  changes ("Update Databases/Games.pdb; add Projects/Study.pch").
- A database deleted in the mobile app is no longer sent back to it, and
  the computer asks whether to delete it there too or keep it. Delete
  Everywhere… warns first, then moves it to the trash and deletes it from
  the sync folder on the server and from the other computers that sync
  with it.
- In the games list the players you marked as "me" are in bold.
- **Play online.** Game ▸ Play Online… plays against a person on
  lichess.org: connect the platform from the dialog (it signs you in with
  your browser), choose clock, colour and rated or casual, and find an
  opponent. While looking and while playing, the engine, Explain, Training
  Mode and the Opening Tree are off: it is you against your opponent. The
  finished game is saved in the open database. Connections are yours on
  this computer, not the project's. The toolbar has a Play Online button
  after New Training; tick "Remember for this session" in the dialog and
  it looks for an opponent without asking.
- **Training plays the book.** While the position is in the opening book,
  the engine answers with a book move, each as often as its weight says —
  tune the weights in the Opening Tree to train against the lines you want.
  Out of the book it plays its own move; the tutor judges your moves as
  before.
- **Book weights.** Right-clicking a move of the Opening Tree offers Adjust
  Weight (+5% to +100%, the same downwards, and Zero Weight): the share
  of the move changes, the other moves give or take in proportion to what
  they have, the sum stays 100%, and the book in use is written. The move
  you changed glows for a moment and is marked with ↑, ↓ or = for where it
  went in the new order.
- **Variations.** Play a move that is not the next one of the game and it
  becomes a variation, shown in the Moves panel — still the classic table,
  one move a cell — in a row under the move it replaces;
  variations nest, are followed by clicking their moves, and are saved with
  the game — at once when it comes from the database — and written to and
  read from PGN. A game's moves are never overwritten any more: the old
  "the new line becomes a game of its own" is gone.
- **A guide.** *Help ▸ Pragma Chess Guide* (F1) explains the application
  topic by topic, in the language of the interface. Type in the search field
  and the list shows the topics that match, each with the words found.
- *About Pragma Chess* now credits the clubs that support the project —
  the first is ASD Circolo del Re, Castelvetrano Scacchi — and what the
  application is built with. Qt's notice is a button there, so the Help menu
  has a single About entry.
- **Annotations.** Right-click a move of the move list: *Annotations* lists
  the symbols (!!, !, !?, ?!, ?, ??, □ and the assessments of the position,
  from +− to −+), each with what it means. They are shown in the move list,
  saved with the game and written to PGN; pasted PGN keeps them.
- The same menu has *Copy ▸ Copy Move* and *Copy Line up to Here*.
- The games list ends with a *Line* column: the first moves of each game,
  cut with “…” where the column ends.
- Right-click a column title of the games list to hide the column or show a
  hidden one. The choice is stored in the database, so each database opens
  with its own columns.
- **A tutor in Training Mode.** When your move is an inaccuracy, a mistake,
  a blunder or lets a winning chance go, the engine does not answer: the Engine panel
  says so and offers to take the move back, to explain it on the board or to
  go on. It needs no extra analysis: it reads the jump in the evaluation.
- *New Training* has *Remember for this session*: once ticked, the toolbar
  button starts a training with that colour without asking, until the
  application is closed. *Game ▸ New Training…* always asks.
- The toolbar has an icon each for the opening book, the engine and the
  database, with a drop-down list to choose another; the tooltip says which
  one is in use.
- **ChessBase files as a source.** *Database ▸ Connect Source… ▸ ChessBase
  files* imports the games of a ChessBase database (`.cbh`) on this computer
  into the open database, main lines with players, event, date, result,
  ratings and ECO, and picks up games added to it later. When the file is not
  on the computer the sync says so and can ignore the source there.
- **Trash.** Right-click a game of the list and choose *Move Game to Trash*.
  The database tree ends with *Trash*, where a game can be restored or
  deleted, with *Recent* (trashed in the last seven days) and *Old* under
  it. A game in the trash is in no other list and is not searched.
- **Optimize Database.** Deleting is never immediate: *Database ▸ Database
  Settings… ▸ Optimize Database* removes the deleted games for good and
  compacts the file. Trashing, deleting and optimizing reach the other
  devices through the sync, so a deleted game does not come back.
- *Manage Engines* marks the engine in use, and *Use This Engine* switches
  to the one selected; the choice is saved with the project.
- The Android app hides the games a computer trashed or deleted, and asks to
  be updated when a database was made by a newer version of Pragma Chess.

### Changed

- File ▸ Sync… is now Options ▸ Sync Settings…, and File ▸ Sync Now is the
  toolbar's first button.
- Options ▸ Board Settings… is now Graphics Settings…, and chooses the
  appearance too: follow the system, or always light, or always dark. It is
  kept on each computer, so each one can match its own desktop.
- A little less space between the board and the Moves panel, the five
  buttons under the board are exactly under its middle, and the Moves
  panel's header lines up with the Opening Tree's to the pixel; the
  separators between panels are a light bar whose ends carry the grey of
  the panels' frames, so the frames' lines run on across them.
- Resizing or showing a panel marks the project as changed, so Save
  Project keeps the new proportions; resizing the window keeps the
  proportions of the panels, so the project file does not depend on the
  window's size.
- File ▸ New Project keeps the database, the engine and the panels as
  they are and starts a new, empty game seen from White's side, with
  Training Mode off.
- Opening a game from the games list turns Training Mode off: a stored
  game is for studying, and the engine must not play moves in it.
- Project files store the panels in clear: which are shown and their
  shares of the window in per cent, with two decimals (`workspace` in
  the `.pch`), so a
  project looks the same on another screen and can be read and edited by
  hand. Older projects are still read.
- View lists the panels as Moves, Opening Tree, Engine and, last, Games
  List.
- The title bar of the main window shows the Pragma Chess logo in its left
  corner.
- The toolbar has a Save Project button, the classic floppy, before New
  Game; the Save icons of the menus are floppies too.
- In online play the Opening Tree no longer disappears: the panel stays
  where you put it and lists no moves until the game ends, its first row
  saying why.
- **Android app (beta).** The companion app for phones is attached to the
  release as `PragmaChess-android.apk`: it pairs with the computer
  (Options ▸ Connect Mobile App…), keeps its databases offline and sends
  the games entered on the phone back. Signed for testing for now; it will
  come to the official app stores.
- The packages are about 80 MB smaller: the bundled Stockfish is now
  Stockfish 18 built by us with its small evaluation network only (about
  4 MB instead of 100). It is weaker than the official Stockfish and still
  far stronger than any human player; the official one, or any other UCI
  engine, can be added in *Engine ▸ Manage Engines…*.
- The Windows packages no longer carry the DirectX shader compiler nor Qt's
  separate translation files, which the application does not use.
- The `.pdb` schema is now version 7 (version 6 added the trash, 7 the
  variations) and is described by migrations: a database of an older
  version is upgraded step by step when it is opened. A database opened by
  this version can no longer be opened by 0.2.0 or by the Android app before
  this change: both say it was made by a newer version.
- In the games list, the Elo, Date, ECO and Moves columns are centred.
- The Opening Tree and the engine line draw the pieces with the same
  figurines as the move list.
- The Sync Now button of the toolbar shows two separate arrows, and its
  tooltip is *Sync Everything…*.
- On Wayland, menus, dialogs and the main window have rounded corners and a
  shadow that lifts them off what is behind, as in the other applications of
  the desktop, and a title bar of their own: the title with a close button
  for dialogs, with minimize, maximize and close for the main window.
- The toolbar buttons are larger, with more room around their icons.
- New toolbar icons: a board of four squares for *New Game* and a square
  face for *New Training*.
- Training Mode is remembered: closing the application while training and
  opening it again goes on with the training game.
- The title bar shows only the name of the open project, with an asterisk
  while it is not saved, and the application: “Untitled* - Pragma Chess”.

### Fixed

- The main window no longer grows at every start (by the height of its
  title bar and the width of its shadow) until it ran off the screen; a
  window never opens larger than the screen.
- The one-pixel edge of the main window is opaque, maximized too: it let
  the desktop show through, so it brightened over a white window and
  looked cut where a dark panel sat inside.
- Saving an untitled project (which opens Save Project As), and every
  other file dialog, no longer crashes the application on GNOME.
- The title bar no longer swallows a click now and then: moving the
  window starts only once the pointer is dragged, so double clicks and
  drags work every time.
- The title bar drops the asterisk as soon as the project is saved, not
  at the next click.
- When two copies of a database are merged (a file changed on two devices,
  or the phone link), the sources connected on the other copy and the games
  they already imported come along, so the next sync no longer imports those
  games again as duplicates; so do the player roles (Me, Friends, Opponents)
  set only on the other copy.
- On Windows the application did not start, saying that
  `libssl-3-x64.dll` and `libcrypto-3-x64.dll` were not found: they are now
  in the package, and every Windows package is started on the build machine
  with nothing but Windows around it before it is published.
- On GNOME (Wayland), opening a menu no longer makes the dock slide in over
  the window.
- Opening another database could crash the application: the games list was
  filtered with the database that had just been closed.
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

[Unreleased]: https://github.com/francescobianco/pragma-chess/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/francescobianco/pragma-chess/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/francescobianco/pragma-chess/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/francescobianco/pragma-chess/releases/tag/v0.1.0
