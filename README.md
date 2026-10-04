# Pragma Chess

An open source chess database for studying, training and playing: light,
simple, with what really matters. For Windows, macOS and Linux.

**Web site: <https://yafb.net/pragma-chess/>** (English and Italian, with
screenshots and downloads). See [DESIGN.md](DESIGN.md) for the vision and
architecture, [CHANGELOG.md](CHANGELOG.md) for what changed, and
[DISTRIBUTING.md](DISTRIBUTING.md) for how we make it known.

## Download

The latest version, always at these links:

| System | Download |
|---|---|
| Windows 10/11 (64-bit) | [Installer](https://github.com/francescobianco/pragma-chess/releases/latest/download/PragmaChess-windows-x64-setup.exe) · [Portable zip](https://github.com/francescobianco/pragma-chess/releases/latest/download/PragmaChess-windows-x64-portable.zip) |
| macOS 12+ (Apple Silicon) | [Disk image](https://github.com/francescobianco/pragma-chess/releases/latest/download/PragmaChess-macos-arm64.dmg) |
| Ubuntu 24.04+ / Debian 13+ | [.deb](https://github.com/francescobianco/pragma-chess/releases/latest/download/pragma-chess_amd64.deb) |
| Fedora | [.rpm](https://github.com/francescobianco/pragma-chess/releases/latest/download/pragma-chess.x86_64.rpm) |

Every version, with files named after it, is on the
[releases page](https://github.com/francescobianco/pragma-chess/releases).
The packages are built by GitHub Actions from [packaging/](packaging/README.md).

### Try the Android app

The mobile app is in testing and we'd love your feedback. It reads your
databases offline, records the games you play on the phone and analyses them
with an engine app such as Stockfish (any Open Exchange engine, as in
DroidFish; the app offers to install one); pair it with the desktop client in
*Options ▸ Connect Mobile App…* to keep both in sync.

**[Download the APK](https://github.com/francescobianco/pragma-chess/releases/download/v0.1.0/pragma-chess-0.1.0-android.apk)**
(Android 8+, about 9 MB). Open the link on
the phone and allow installing from that source when Android asks. Tell us
what works and what doesn't in the
[issues](https://github.com/francescobianco/pragma-chess/issues): the phone
model and the Android version help.

## Desktop client (Qt 6 Widgets)

The GUI lives in `gui/qt`. It talks to the database through the `GameDatabase`
interface (`gui/qt/src/app/GameDatabase.h`).

Databases are `.pdb` files — SQLite databases tagged with `PRAGMA application_id`
(`PRAG`) and a schema version in `PRAGMA user_version`, upgraded by migrations
when an older file is opened. By default they live in
a localized chess folder in the home directory, next to Desktop and Documents:

```text
~/Chess/Pragma/Databases/      (English)
~/Scacchi/Pragma/Databases/    (Italian)
```

Projects are `.pch` files (YAML) saved from the File menu. A project *is* the
workspace: it captures the whole working environment — database, open game and
move, board orientation, engine, which panels are visible and how much room
each one takes — and is stored by default in `~/Chess/Pragma/Projects/`
(localized like the databases folder). The last session is restored
automatically on startup, attached to its project file. The *View* menu shows
and hides the panels, and *View ▸ Reset Panel Layout* puts them back.

An existing chess folder is reused if the language changes; `PRAGMA_CHESS_DIR`
overrides it. On first launch the folder is seeded with `Classic Games.pdb`.

### Entering and explaining games

*Game ▸ New Game* starts a game that is entered move by move on the board
(click or drag the pieces) and analyzed as it grows; *Game ▸ Save Game to
Database* adds it to the open `.pdb`. Playing a different move in a stored
game never changes it: the new line becomes an unsaved game.

**Explain** (the light bulb between the previous and next move buttons, or
`E`) shows why the evaluation is what it is compared with the position
before the last move: the refutation of a mistake, the material a move wins,
a mate it allows or misses. It starts the engine if needed: the border of the
board breathes while the engine is looking and turns blue when the answer is
on the board.

### Training against the engine

*Game ▸ New Training…* asks which colour you want and starts a game in which
the engine answers as the other one. It is a plain game with *Engine ▸
Training Mode* switched on: while it is your move the engine's best line is
hidden (the score is not), and you can only move your own pieces. The
checkmate or stalemate that ends the game is stored in the open database with
the result filled in. *Game ▸ New Game* switches training off again.

The engine's answer crosses the board slowly, growing and wearing a halo, so
a move you did not make is impossible to miss.

### Me, friends and opponents

Right-click a player in the games list and choose *Who Is This?* ▸ *It's Me*,
*A Friend* or *An Opponent*. The database remembers it: the tree lists your
games, your friends and your opponents, and a game you played opens with your
pieces at the bottom of the board.

### Games with the position on the board

In the tree next to the games list, *Board* under the database has two views
that follow the board. *Position* lists the games in which the position on the
board occurs, reached in any move order; *Variant* lists the games that begin
with exactly the moves played to get there. Their counts and the list change as
you move through the game or play new moves.

### Sources

*Database ▸ Connect Source…* connects the games of a lichess.org or chess.com
account to the open database. They are imported and kept up to date in the
background while the database is open; *Database ▸ Manage Sources…* syncs,
edits, signs in again or removes sources. lichess.org needs signing in, which
happens in the browser.

torneionline.com (the Italian federation's rating site) is found by FIDE or
FSI ID: every game of the player's tournaments is added with players, Elo,
tournament, round and result but no moves, since the site has none, ready for
the moves to be entered by hand.

### Opening books

The *Book* menu chooses a Polyglot opening book (`.bin`) from the Books folder
(`~/Chess/Pragma/Books`) or anywhere else. The Opening Tree panel shows the
book moves of the position on the board with their weight and the name of the
opening each move leads to; click one to play it. Names come from an ordinary
database of type *Opening Book*, where each game is a named line (Event is the
name). Two are installed ready made in `Books/Opening Names`, *English* and
*Italian*; the one of the interface language is used unless you choose another
in *Options ▸ Opening Names*, which lists each database by its own name in your
language (set in *Database ▸ Database Settings*) and also any opening book
among your databases. Open one to add or rename variations. Pragma Chess ships *Pragma Openings*, built from the public domain
[lichess chess-openings](https://github.com/lichess-org/chess-openings)
collection; `pragma-book` builds and probes books on the command line.

### Language

*Options ▸ Switch Language* chooses the language of the interface for your user
account; it applies the next time Pragma Chess starts.

### Sync between computers

The toolbar holds three buttons: *Sync Now*, *New Game* and *New Training*.
The first of them (Ctrl+Y) syncs everything in one
go and in the order that keeps the pieces consistent: the games of the
connected sources come down into the database, the project file and the
session are written, and only then does the folder go to the server. What is
pushed is always what you are looking at. *File ▸ Sync Now* is the same
button; *Options ▸ Sync Settings…* holds it too, with a *Sync before
closing* option, which is remembered and also offered by the dialog that
asks to save a modified project on quit.

Syncing reconciles, it deletes nothing on its own. Every computer ends up
with the union of what all of them have: a file that appears anywhere is
added everywhere, and two computers editing the same file keep both
versions. A file deleted by hand from the Pragma folder is noticed (each
computer keeps what it last synced in a hidden `.pragma-chess.local` there)
and you are asked whether to delete it everywhere or restore it; *Manage
Files…* in the Sync Settings deletes files from the server and every
computer, after a confirmation. Nothing else is ever removed.

*Options ▸ Sync Settings…* connects the Pragma folder to a folder on an FTP
or WebDAV server (Nextcloud, a NAS…) or to a Git repository. With Git the
real files never become a repository: each sync copies them into a clone
kept by Pragma Chess, commits and pushes, one commit per sync that changes
files, named after them (databases are binary, so the history grows with
every change). Every computer set up with the same server folder
keeps the same databases and projects: changes are sent and received every few
minutes and a `.pragma-chess.sync` file on the server tracks what changed
where.

### Build

Debian/Ubuntu dependencies:

```bash
make deps    # sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libqt6sql6-sqlite qt6-tools-dev qt6-l10n-tools libssl-dev inotify-tools
```

```bash
make start   # build, launch, and rebuild + restart on every change under gui/qt
make run     # build and launch once
make build   # build only
make test    # build and run the tests
make stockfish  # build the bundled Stockfish into the build
make install # install for the current user in ~/.local (PREFIX=/usr/local for everyone)
```

`make install` adds the menu entry and the icon, so the desktop shows the
Pragma Chess logo in the launcher, dock and window list.

`make start` and `make run` register the development build the same way for
the current user (`make desktop-dev`), because on Wayland the dock only shows
the icon of an installed menu entry.

`make start` keeps the running window if a build fails, so you can fix the
error and save again. It uses `inotifywait` when available and falls back to
polling otherwise.
