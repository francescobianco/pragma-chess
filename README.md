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

### Build

Debian/Ubuntu dependencies:

```bash
make deps    # sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libqt6sql6-sqlite inotify-tools
```

```bash
make start   # build, launch, and rebuild + restart on every change under gui/qt
make run     # build and launch once
make build   # build only
```

`make start` keeps the running window if a build fails, so you can fix the
error and save again. It uses `inotifywait` when available and falls back to
polling otherwise.
