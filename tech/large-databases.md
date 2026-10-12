# Large databases

How Pragma Chess holds up as a database grows, what was done about it, and
what is still to do. The games are doubled step by step (a ladder of lichess
months, see Method) and every step is measured the same way, with
`scripts/stress-databases.sh`.

- **Where we are**: the summary below, kept up to date.
- **Targets** and **Plans**: what we want, and the optimizations still to
  make, by priority. Strike a plan out (and say where it was measured) when
  it is done; add new ideas there.
- **Measurements, by date**: the log. A change meant to make large
  databases faster is measured again on the same files and added at the
  bottom, never written over.

## Where we are

Release build, the machine of Method. "Before" is the first measurement of
2026-10-10, before any of this work.

| 1.26 million games (step 4) | Before | Now |
|---|---|---|
| Window frozen on opening | 33.6 s | 3.5 s |
| Memory of the whole client | 6.8 GB (peak 8.2) | 0.57 GB |
| Board ▸ Position and Variant ready | 96 s, at every opening | 18 s the first time, then 11 ms |
| Position after 1.e4 | 1.3 s | 0.53 s |
| Position after 5…a6 | 0.5 s | 0.21 s |
| Conversion from PGN | — | 199 s, 6 300 games a second |

| 2.32 million games (step 5) | Now |
|---|---|
| Conversion from PGN | 466 s, 5 000 games a second, `.pdb` 2.7 GB |
| Window frozen on opening | 5.8 s (reading 3.5 s, list 1.0 s, tree 1.3 s) |
| Memory of the whole client | 0.55 GB |
| Position index | 43 s the first time (peak 8 GB), then at once; its file 3.8 GB |
| Position at the start, after 1.e4, after 5…a6 | 2.3 s, 1.2 s, 0.34 s |

What made the difference, in order (each is a section of the log):

1. The games list filtered without taking rows out one range at a time,
   and headers no longer copied for every row (`invalidate()`, references).
2. A brief header per game in memory instead of the whole header: names,
   events and dates as indexes into tables where each text is once; the rest
   read from the file 256 games at a time when shown. The briefs are read
   from a covering index (`games_brief`), never from the games' rows.
3. The position index built on every core, 12 bytes an entry.
4. The position index saved in the cache folder and mapped: built once per
   version of the file, never held in the process's memory.

## Targets

For a database of 10 million games on a machine like the one of Method:

| | Target | Now, extrapolated to 10 million |
|---|---|---|
| Window frozen on opening | under 1 s | about 25 s |
| Memory of the whole client | under 1 GB | about 2.4 GB |
| Board ▸ Position and Variant, any position | under 200 ms | up to 10 s at the start |
| Position index, first time | under 2 minutes, peak under 4 GB | about 3 minutes, peak over 30 GB |
| Its file | under 8 GB | about 16 GB |
| Conversion from PGN | 20 000 games a second | 5 000 |

And on any database: the window never waits for the file — what takes
longer than a tenth of a second is done on a thread, and says so.

## Plans

By priority: what frees the most first. Each with what it should give and
what to watch out for.

### 1. The list takes the rows the index gives

A filter that keeps most of the games (the start position, 1.e4) builds a
`QSet` of millions of ids, then `QSortFilterProxyModel` asks for every row
whether it is in it, and sorts what is left: 2.3 s at the start for 2.32
million games.

- The index returns the game ids sorted (they are, within a key: `Entry`
  sorts by key, then game), turned into rows in one pass (`indexOfId` on
  sorted ids is a merge, not a search per id).
- The games list gets a proxy of its own (`QAbstractProxyModel`) that maps
  rows through a vector given whole — the filter's rows, in the sort's order
  — instead of `QSortFilterProxyModel`, which tests and sorts row by row.
  The sort by number is the rows' order already; other columns keep the
  keys of `GameListModel::sortKey`, computed once per column.
- Expected: under 200 ms whatever the number of games kept.

### 2. Nothing on the window's thread at opening

Opening still reads every brief (3.5 s at 2.32 million) and builds the tree
(1.3 s) before the window answers.

- The briefs saved beside the position index, in the cache folder, with the
  same stamp (`SqliteGameDatabase::fileStamp`), and mapped: a database opened
  again unchanged has them at once. The tables of texts (players, events,
  dates) go with them.
- Read the first time on a thread: the list shows the number of games at
  once (`SELECT count(*)`, 0.1 s) and fills when the briefs are there, the
  tree shows "…" meanwhile, as Position and Variant do.
- The tree's outline (`DatabaseOutline`) counted on the same thread, from
  the briefs' numbers directly rather than a `GameRecord` per game.
- Expected: under 0.5 s frozen at 10 million.

### 3. The position index: less memory, less disk, updated rather than rebuilt

- **The peak while building** (8 GB at 2.32 million, 30 GB extrapolated at
  10 million): every game's moves are read before indexing starts (a
  `QList<GameLine>` of all of them), and the sorted shares are merged in
  memory, twice their size. Read in chunks as the threads index them, and
  merge the shares into the file (each share sorted and written, then a
  k-way merge from the files): the peak becomes a share, not the whole.
- **Lines kept only while they are shared**: a line of moves stops being
  useful to the Variant filter at the ply where only one game still follows
  it. Keep a game's line entries up to that ply; a deeper line is answered
  by the last shared prefix and a check of that one game's moves. About half
  the entries go (lichess games part ways around the twelfth move and last
  seventy plies).
- **Updated, not rebuilt**: any write to the database makes the whole index
  be built again — even a source's sync state, which changes no game. Keep a
  small index of the games added since (built in a moment) beside the big
  one, merged into it in the background; and stamp the index with what the
  games are (their count, the greatest id, the last change of a game or a
  state) rather than with any write to the file.
- **Faster per game**: `PolyglotBook::key` is computed from the whole board
  at every ply; a key updated move by move (what Zobrist keys are for) and
  the moves read without building `QString`s would make indexing several
  times faster.
- **Smaller file**: ids delta-encoded in blocks, keys stored once per run of
  equal keys.

### 4. The file: a version of the schema for speed

The phone may need updating for these; for now speed comes first.

- **Moves stored once**: today a game keeps them twice, `moves_san` and
  `moves_uci`, a third of the file. Keep one, compact — two bytes a move, the
  index of the move among the legal ones, as ChessBase and SCID do, or UCI
  — and make SAN when a game is loaded. The file shrinks by a third to a
  half; reading the moves for the index gets as much faster.
- **Headers apart from moves**: a table of headers and a table of moves (as
  ChessBase keeps `.cbh` and `.cbg`), so that reading headers never crosses
  the moves; `games_brief` would no longer be needed.
- **Indexes for the tree's filters** (ECO, year, event, player) where they
  are answered from the briefs today, if the briefs alone stop being enough.

### 5. Conversion and import

- Reading and writing at the same time: today a batch is read on every core,
  then written on one, then the next is read. A pipeline (read batch n+1
  while batch n is written) and the moves written compact (plan 4) should
  give 20 000 games a second.
- The index of positions built during the conversion, from the moves just
  read, instead of from the file afterwards.
- More formats under Tools ▸ Convert: ChessBase (`.cbh`, `.2cbh`, read
  already for sources), SCID (`.si4`, `.si5`); and adding to an existing
  database, not only making a new one.

### 6. Large databases elsewhere in the application

- **Folder sync**: a `.pdb` of gigabytes goes whole through FTP, WebDAV or
  Git at every change. Git is unfit for it; the others need sending only
  what changed (SQLite pages, or games by uid). Until then, say so in Sync
  Settings when a synced database passes a size.
- **Search by text** (players, events) and the Opening Tree's Database
  column on millions of games: measure them in the ladder too.
- **The Android app** opens the same files: measure it on step 3.

### 7. The ladder goes on

Step 6, about 4.8 million games (2013-01 to 2014-02), then step 7, about
10 million (2013-01 to 2014-08): each measured after the plans above, against the
targets.

## Method

```bash
scripts/stress-databases.sh download 2013-01 2013-02 2013-03   # lichess months
scripts/stress-databases.sh join step2 2013-01 2013-02          # one PGN of them
scripts/stress-databases.sh convert step2                       # Tools ▸ Convert, timed
scripts/stress-databases.sh measure step2                       # opening, index, filters, memory
scripts/stress-databases.sh measure step2                       # again: the index from its file
```

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
  | 5 | 2013-01…10 | 2 321 149 | 1 704 MB |

- **Build**: Release (`-DCMAKE_BUILD_TYPE=Release`), in a build folder of its
  own. A Debug build is several times slower and says little.
- **Client**: an instance of its own with an empty home
  (`PRAGMA_CHESS_DIR`, `HOME`, `XDG_CONFIG_HOME` in a scratch folder),
  `QT_QPA_PLATFORM=offscreen`, `PRAGMA_DEV_API=1` on its own port. The home
  is emptied before opening a step, once the previous client has quit (it
  writes its session as it goes), or the session would open the previous
  database first and the memory would count both. The cache folder (the
  position indexes) stays, as a user's does: the first `measure` of a
  database builds its index, the next ones read it.
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

## Measurements, by date

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

## 2026-10-10 — step 5, 2.32 million games

| | |
|---|---|
| Conversion | 466 s, 4 976 games a second, 2 321 009 games (140 entries without moves left out), `.pdb` 2.7 GB |
| Window frozen on opening | 5.8 s (file 3.5 s, list 1.0 s, tree 1.3 s) |
| Position index, first opening | 43 s on its thread, peak 8.0 GB |
| Position index, then | at once; the file is 3.8 GB |
| Memory of the whole client | 0.55 GB |
| Position at the start (every game) | 2.3 s |
| Position after 1.e4 | 1.2 s |
| Position after 5…a6 | 0.34 s |

Next, by what is slowest now:

1. A filter that keeps most of the games (the start, 1.e4) builds a set of
   millions of ids and filters the list row by row: the index should give
   the rows themselves, in order, and the list take them at once.
2. The peak while the index is built (8 GB): every game's moves are read
   before indexing starts, and the shares are merged in memory. Reading in
   chunks as they are indexed, and merging into the file, would keep it low.
3. Opening still reads every brief (3.5 s) and builds the tree (1.3 s)
   before the window answers: both could be done on a thread.
4. The moves stored once instead of twice (SAN and UCI): a smaller file,
   faster to read.

