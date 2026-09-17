# pragma-chess

An open-source chess database engine with a native cross-platform desktop client.
See [DESIGN.md](DESIGN.md) for the vision and architecture.

## Desktop client (Qt 6 Widgets)

The GUI lives in `gui/qt`. It talks to the database through the `GameDatabase`
interface (`gui/qt/src/app/GameDatabase.h`).

Databases are `.pdb` files — SQLite databases tagged with `PRAGMA application_id`
(`PRAG`) and a schema version in `PRAGMA user_version`. By default they live in
a localized chess folder in the home directory, next to Desktop and Documents:

```text
~/Chess/Pragma/Databases/      (English)
~/Scacchi/Pragma/Databases/    (Italian)
```

Projects are `.pch` files (YAML) saved from the File menu. A project captures
the whole working environment — database, open game and move, board
orientation, engine, window layout — and is stored by default in
`~/Chess/Pragma/Projects/` (localized like the databases folder). The last
session is restored automatically on startup, attached to its project file.

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
a mate it allows or misses. It starts the engine if needed.

### Me, friends and opponents

Right-click a player in the games list and choose *Who Is This?* ▸ *It's Me*,
*A Friend* or *An Opponent*. The database remembers it: the tree lists your
games, your friends and your opponents, and a game you played opens with your
pieces at the bottom of the board.

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
database, *Opening Names*, where each game is a named line (Event is the name):
open it to add or rename variations, or choose another one (for instance in
another language) in *Book ▸ Opening Names*. Pragma Chess ships *Pragma Openings*, built from the public domain
[lichess chess-openings](https://github.com/lichess-org/chess-openings)
collection; `pragma-book` builds and probes books on the command line.

### Language

*Options ▸ Language* chooses the language of the interface for your user
account; it applies the next time Pragma Chess starts.

### Sync between computers

*File ▸ Sync…* connects the Pragma folder to a folder on an FTP or WebDAV
server (Nextcloud, a NAS…) or to a Git repository. With Git the real files
never become a repository: each sync copies them into a clone kept by Pragma
Chess, commits and pushes (databases are binary, so the history grows with
every change). Every computer set up with the same server folder
keeps the same databases and projects: changes are sent and received every few
minutes, a `.pragma-chess.sync` file on the server tracks what changed where,
edits made on two computers at once keep both files, and deleted files go to
the trash.

### Build

Debian/Ubuntu dependencies:

```bash
make deps    # sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libqt6sql6-sqlite qt6-tools-dev qt6-l10n-tools inotify-tools
```

```bash
make start   # build, launch, and rebuild + restart on every change under gui/qt
make run     # build and launch once
make build   # build only
make test    # build and run the tests
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
