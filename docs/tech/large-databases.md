# Large databases: measurements

How Pragma Chess holds up as a database grows, measured by doubling the
number of games. Each step is logged here with its date and build: a change
meant to make large databases faster is measured again on the same files and
added below, never written over.

## Method

- **Corpus**: the lichess open database (database.lichess.org, CC0), rated
  standard games, one file a month. It is a test corpus only: it is kept in
  `~/.cache/pragma-chess-stress`, never in the repository, never
  distributed. The steps are months put together with `cat`:

  | Step | Months | Games | PGN |
  |---|---|---|---|
  | 1 | 2013-01 | 121 332 | 89 MB |
  | 2 | 2013-01…02 | 245 293 | 179 MB |
  | 3 | 2013-01…04 | 561 799 | 411 MB |
  | 4 | 2013-01…07 | 1 259 487 | 920 MB |

- **Build**: Release (`-DCMAKE_BUILD_TYPE=Release`), in a build folder of its
  own. A Debug build is several times slower and says little.
- **Client**: an instance of its own with an empty home
  (`PRAGMA_CHESS_DIR`, `HOME`, `XDG_CONFIG_HOME` in a scratch folder),
  `QT_QPA_PLATFORM=offscreen`, `PRAGMA_DEV_API=1` on its own port. The home
  is emptied before opening a step, or the session would open the previous
  database first and the memory would count both.
- **Driven by the development API**: `POST /api/convert` (Tools ▸ Convert ▸
  PGN to Pragma Database), `POST /api/database` (open), then the board's
  filters along a Najdorf (1.e4 c5 2.Nf3 d6 3.d4 cxd4 4.Nxd4 Nf6 5.Nc3 a6)
  with `POST /api/move` and `POST /api/category {"kind": "position"}` /
  `"variant"`, and `GET /api/profile` after each: `openFile` (reading the
  headers), `showDatabase` (list and tree), `buildIndex` (on its worker
  thread, reading the moves included; until the second section below,
  `readMoves` timed that reading on the window's thread), `countPosition`/`countVariant`,
  `findGames` (the index's ids), `filterList` (the games list filtered and
  sorted), and the process's memory (`VmRSS`, `VmHWM` its peak).
- **Machine**: 22 cores, 30 GB of RAM, NVMe disk, Ubuntu, Qt 6.4.

## 2026-10-10 — first measurements

### Conversion (Tools ▸ Convert ▸ PGN to Pragma Database)

| Step | Games | Time | Games a second | `.pdb` | Skipped |
|---|---|---|---|---|---|
| 1 | 121 332 | 17.8 s | 6 800 | 138 MB | 0 |
| 2 | 245 293 | 41.7 s | 5 900 | 279 MB | 0 |
| 3 | 561 799 | 86.2 s | 6 500 | 643 MB | 0 |
| 4 | 1 259 487 | 199.1 s | 6 300 | 1 438 MB | 0 |

Linear, and the window goes on meanwhile. The `.pdb` is about 1.5 times the
PGN: each game keeps its moves twice, `moves_san` and `moves_uci`.

### The games list filtered by the board (step 1, 121 332 games)

Found and fixed on the first step (commit "Tools ▸ Convert ▸ PGN to Pragma
Database, and large databases filter fast"):

| | Before | After |
|---|---|---|
| Position after 1.e4 (72 488 games) | 4 777 ms | 59 ms |
| Position after 5…a6 (271 games) | 130 ms | 6 ms |
| All Games | 667 ms | 75 ms |

- `QSortFilterProxyModel::invalidateFilter()` (Qt 6.4) takes the rows out
  range by range and the view pays for each range: `invalidate()` maps them
  again at once.
- `GameDatabase::header()` returned a copy of the whole `GameRecord`, and the
  filter and the model's `data()` (called for every comparison of the sort)
  copied it for every row: it returns a reference now.
- The index itself was never the problem: `findGames` stays under 10 ms,
  `countPosition` and `countVariant` under 1 ms at every step.

### Opening and memory

| Step | Games | Window frozen on opening | of which headers | Position index (background) | Memory | Peak |
|---|---|---|---|---|---|---|
| 1 | 121 332 | 3.7 s | 2.7 s | 8–10 s | 0.7 GB | 0.9 GB |
| 2 | 245 293 | 7.2 s | 5.5 s | 15.8 s | 1.5 GB | 1.8 GB |
| 3 | 561 799 | 15.3 s | 12.0 s | 39.7 s | 3.3 GB | 3.9 GB |
| 4 | 1 259 487 | 33.6 s | 28.1 s | 90.9 s | 6.8 GB | 8.2 GB |

A move on the board with Board ▸ Position selected, step 4: 1.3 s after
1.e4, 0.5 s deep in the Najdorf.

### Limits found

1. **Opening freezes the window**, about 27 ms per 1 000 games:
   `SqliteGameDatabase::loadHeaders` reads every header into memory on the
   window's thread (the code's own TODO: "page headers from SQL instead of
   caching them for very large databases"). A database of 5 million games
   would freeze it for over two minutes.
2. **Memory, about 5.4 KB per game**: some 2.2 KB is the position index
   (two pairs of 8 bytes per ply, 136 per game), the rest the cached
   headers — a `GameRecord` of some thirty `QString`s, the PGN tags parsed,
   the line preview. 5 million games would need about 27 GB.
3. **The position index takes 72 µs per game** on one thread, and a copy
   of every game's moves (`gameLines()`) is held while it is built: the
   peak.
4. **The list after a short line**: filtering and sorting the games of
   1.e4 among 1.26 million takes about a second.

### Next (as written then)

In order of what they free:

1. Headers paged from SQLite instead of cached (`GameListModel` asks for the
   rows on screen; the tree's counts and the filters become SQL queries
   with indexes), so opening is immediate and memory no longer grows with
   the games.
2. The position index built on several threads and kept on disk beside the
   database (built once, updated with the games), instead of being rebuilt
   at every opening.
3. Then step 5 (2013-08…2014-01, about 2.5 million games) and on.

## 2026-10-10 — headers no longer held in memory

`SqliteGameDatabase` keeps a brief header per game (a few dozen bytes:
indexes into tables where each name, event and date is once, numbers, the
state, the tags the tree reads), read from a covering index (`games_brief`)
without touching the games' rows; the rest of a header is read a page of 256
games at a time when it is shown. The moves for the position index are read
on its worker thread. Step 4, 1 259 487 games, Release:

| | Before | After |
|---|---|---|
| Window frozen on opening | 33.6 s | 3.5 s |
| of which reading the file | 28.1 s | 2.0 s |
| list | | 0.6 s |
| tree | | 0.9 s |
| Memory, index built | 6.8 GB | 3.7 GB |
| Peak | 8.2 GB | 5.1 GB |
| Position after 1.e4 | 1.3 s | 0.95 s |
| Position after 5…a6 | 0.5 s | 0.44 s |

The first opening of a database made before builds `games_brief`, once (5.6 s
for step 4); a converted database has it from the start.

The position index is now most of the memory (172 million pairs, 2.75 GB)
and takes 96 s to build at every opening: next, it is built on several
threads and kept in the database file. Then the moves stored once instead of
twice (SAN and UCI), which a version of the schema allows.

## 2026-10-10 — the position index on every core

`PositionIndex::build` shares the games among every core but one, each
sorting what it found, then merges the shares two by two on threads; an
entry is 12 bytes (the key, and the game's id with its result in its two low
bits) instead of 16 plus a hash of the results. Step 4:

| | Before | After |
|---|---|---|
| Building the index (reading the moves included) | 95.9 s | 18.2 s |
| Memory, index built | 3.7 GB | 2.7 GB |
| Peak | 5.1 GB | 4.7 GB |
| Position after 1.e4 | 0.95 s | 0.73 s |

## 2026-10-10 — the position index kept on disk

A built index is saved in the cache folder (`position-index/<sha1 of the
database's path>.pix`) with the stamp of the database file it was made from
(SQLite's file change counter and the size, `SqliteGameDatabase::fileStamp`),
and mapped rather than read: the system brings in the pages a search touches,
and the process holds no copy. A database opened again unchanged has its
index at once; any write to it makes the index be built again. Step 4:

| | Before | After |
|---|---|---|
| Board ▸ Position and Variant ready, second opening | 18.2 s | 11 ms |
| Memory of the whole client, second opening | 2.7 GB | 0.57 GB |
| Position after 1.e4 | 0.73 s | 0.53 s |
| Position after 5…a6 | 0.43 s | 0.21 s |

The file is 2.0 GB for step 4 (12 bytes an entry). Files of databases not
opened for a month are removed when another is saved.

Since the start of the day, step 4: window frozen on opening 33.6 s → 3.5 s,
memory 6.8 GB → 0.57 GB, the index 96 s at every opening → once.

