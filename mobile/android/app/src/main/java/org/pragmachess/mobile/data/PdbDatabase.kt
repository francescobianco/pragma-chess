package org.pragmachess.mobile.data

import android.content.ContentValues
import android.database.sqlite.SQLiteDatabase
import java.io.File

/**
 * A database made by a newer version of Pragma Chess than this app: its
 * schema is later than the last one the app knows. Nothing is wrong with the
 * file; the user is asked to update the app to open it.
 */
class NewerSchemaException(name: String) : IllegalStateException("$name was made by a newer version of Pragma Chess")

/** Where a game is, by uid, and when that was set (table `game_states`, version 6; see the desktop's GameState). */
data class GameStateRecord(val uid: String, val state: String, val modified: String)

/**
 * A Pragma .pdb database: SQLite with the desktop's schema (application_id
 * PRAG, user_version 7; see gui/qt/src/app/DatabaseMigrations.cpp). The phone
 * reads any file of version 1 to 6 and upgrades the ones it opens for
 * writing; a file of a later version is refused with [NewerSchemaException].
 */
class PdbDatabase private constructor(val file: File, private val db: SQLiteDatabase) : AutoCloseable {

    override fun close() = db.close()

    /** Files older than version 6 opened read-only have no `game_states` yet: all their games are live. */
    private val hasStates: Boolean by lazy {
        db.rawQuery("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'game_states'", null).use { it.moveToFirst() }
    }

    /** Condition on `games g`: the game is in the lists, not in the trash, deleted or purged. */
    private val live: String
        get() = if (hasStates) "NOT EXISTS (SELECT 1 FROM game_states st WHERE st.uid = g.uid AND st.state <> 'live')" else "1"

    /** The database's own properties (table `properties`, version 4); empty for older files. */
    fun properties(): Map<String, String> = runCatching {
        val hasTable = db.rawQuery("SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = 'properties'", null)
            .use { it.moveToFirst() }
        if (!hasTable) return@runCatching emptyMap()
        db.rawQuery("SELECT key, value FROM properties", null).use { c ->
            buildMap { while (c.moveToNext()) put(c.getString(0), c.getString(1)) }
        }
    }.getOrDefault(emptyMap())

    /** Opening books (type "opening-book") hold named lines, not games to browse; missing type = games. */
    fun isOpeningBook(): Boolean = properties().let { p ->
        // The Opening Names we ship is one even in copies made before databases had a type.
        p[PROPERTY_TYPE]?.let { it == TYPE_OPENING_BOOK } ?: (p[PROPERTY_ID] == GameIdentity.OPENING_NAMES_LINEAGE)
    }

    /** The games in the lists: those in the trash or deleted on a computer do not count. */
    fun gameCount(): Int =
        db.rawQuery("SELECT COUNT(*) FROM games g WHERE $live", null).use { if (it.moveToFirst()) it.getInt(0) else 0 }

    /** Games whose players or event contain [filter] (all when blank), newest first; never those in the trash. */
    fun games(filter: String = "", limit: Int = 5000): List<GameSummary> {
        val where = if (filter.isBlank()) " WHERE $live"
        else " WHERE $live AND (w.name LIKE ?1 OR b.name LIKE ?1 OR e.name LIKE ?1)"
        val args = if (filter.isBlank()) null else arrayOf("%${filter.trim()}%")
        val sql = "SELECT g.id, w.name, b.name, g.result, g.date, e.name, g.ply_count FROM games g" +
            " LEFT JOIN players w ON w.id = g.white_id LEFT JOIN players b ON b.id = g.black_id" +
            " LEFT JOIN events e ON e.id = g.event_id$where ORDER BY g.date DESC, g.id DESC LIMIT $limit"
        return db.rawQuery(sql, args).use { c ->
            buildList {
                while (c.moveToNext()) {
                    add(GameSummary(c.getLong(0), c.getString(1).orEmpty(), c.getString(2).orEmpty(),
                        c.getString(3).orEmpty(), c.getString(4).orEmpty(), c.getString(5).orEmpty(), c.getInt(6)))
                }
            }
        }
    }

    fun game(id: Long): GameRecord? {
        val sql = "SELECT w.name, b.name, e.name, s.name, g.date, g.round, g.result, g.white_elo, g.black_elo," +
            " g.eco, g.start_fen, g.moves_san, g.moves_uci FROM games g" +
            " LEFT JOIN players w ON w.id = g.white_id LEFT JOIN players b ON b.id = g.black_id" +
            " LEFT JOIN events e ON e.id = g.event_id LEFT JOIN sites s ON s.id = g.site_id WHERE g.id = ?"
        return db.rawQuery(sql, arrayOf(id.toString())).use { c ->
            if (!c.moveToFirst()) return null
            fun text(i: Int) = if (c.isNull(i)) "" else c.getString(i)
            GameRecord(
                GameHeaders(text(0), text(1), text(2), text(3), text(4), text(5), text(6).ifEmpty { "*" },
                    if (c.isNull(7)) 0 else c.getInt(7), if (c.isNull(8)) 0 else c.getInt(8), text(9)),
                startFen = text(10), movesSan = text(11), movesUci = text(12),
            )
        }
    }

    private fun nameId(table: String, name: String): Long? {
        if (name.isBlank()) return null
        db.execSQL("INSERT OR IGNORE INTO $table (name) VALUES (?)", arrayOf(name))
        return db.rawQuery("SELECT id FROM $table WHERE name = ?", arrayOf(name)).use { if (it.moveToFirst()) it.getLong(0) else null }
    }

    /** The universal id of this database (properties row `id`), or null for files older than version 5. */
    fun lineage(): String? = properties()[PROPERTY_ID]?.ifBlank { null }

    fun setProperty(key: String, value: String) {
        db.execSQL("INSERT OR REPLACE INTO properties (key, value) VALUES (?, ?)", arrayOf(key, value))
    }

    private fun gameValues(game: GameRecord) = ContentValues().apply {
        val h = game.headers
        put("white_id", nameId("players", h.white))
        put("black_id", nameId("players", h.black))
        put("event_id", nameId("events", h.event))
        put("site_id", nameId("sites", h.site))
        put("date", h.date.ifBlank { null })
        put("round", h.round.ifBlank { null })
        put("result", h.result.ifBlank { null })
        put("white_elo", h.whiteElo.takeIf { it > 0 })
        put("black_elo", h.blackElo.takeIf { it > 0 })
        put("eco", h.eco.ifBlank { null })
        put("ply_count", game.plyCount)
        put("start_fen", game.startFen.ifBlank { null })
        put("moves_san", game.movesSan)
        put("moves_uci", game.movesUci)
        put("modified", game.modified)
    }

    private fun uidTaken(uid: String): Boolean =
        db.rawQuery("SELECT 1 FROM games WHERE uid = ?", arrayOf(uid)).use { it.moveToFirst() }

    /**
     * Stores [game]. One with a uid keeps it (null if the database has it
     * already); one without gets the uid of its content, told apart from an
     * identical game already here by its occurrence, like the desktop does.
     * Returns the stored game.
     */
    fun insert(game: GameRecord): GameRecord? {
        db.beginTransaction()
        try {
            val uid = game.uid ?: generateSequence(1) { it + 1 }.map { GameIdentity.uid(game, it) }.first { !uidTaken(it) }
            if (uidTaken(uid)) return null
            val stored = game.copy(uid = uid)
            db.insertOrThrow("games", null, gameValues(stored).apply { put("uid", uid) })
            // A purged game stored again is a game again, as on the desktop.
            if (hasStates) {
                db.execSQL("UPDATE game_states SET state = 'live', modified = ? WHERE uid = ? AND state = 'purged'",
                    arrayOf(stored.modified, uid))
            }
            db.setTransactionSuccessful()
            return stored
        } finally {
            db.endTransaction()
        }
    }

    /** Replaces the game with [game]'s uid by that version (a conflict the other side won). */
    fun replace(game: GameRecord) {
        db.update("games", gameValues(game), "uid = ?", arrayOf(requireNotNull(game.uid)))
    }

    /**
     * Applies the incoming side of a merge in one transaction and returns how
     * many games it added. A game purged here ([mergeStates] comes first) is
     * not taken back from a copy that still has it.
     */
    fun merge(plan: Reconciler.Plan): Int {
        val purged = states().filter { it.state == STATE_PURGED }.map { it.uid }.toSet()
        var added = 0
        db.beginTransaction()
        try {
            plan.insert.forEach { game ->
                val uid = game.uid!!
                if (uid !in purged && !uidTaken(uid)) {
                    db.insertOrThrow("games", null, gameValues(game).apply { put("uid", uid) })
                    added++
                }
            }
            plan.update.forEach(::replace)
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
        return added
    }

    /** The state of every game that was ever trashed, deleted or purged; empty before version 6. */
    fun states(): List<GameStateRecord> {
        if (!hasStates) return emptyList()
        return db.rawQuery("SELECT uid, state, modified FROM game_states", null).use { c ->
            buildList { while (c.moveToNext()) add(GameStateRecord(c.getString(0), c.getString(1), c.getString(2))) }
        }
    }

    /**
     * Takes the states of another copy that are newer than ours (a tie keeps
     * ours), as the desktop does: a game trashed or deleted on a computer
     * leaves the lists here too, and one it purged goes for good.
     */
    fun mergeStates(incoming: List<GameStateRecord>) {
        if (incoming.isEmpty() || !hasStates) return
        val mine = states().associateBy { it.uid }.toMutableMap()
        db.beginTransaction()
        try {
            for (state in incoming) {
                val current = mine[state.uid]
                if (current != null && !Reconciler.newer(state.modified, current.modified)) continue
                db.execSQL("INSERT OR REPLACE INTO game_states (uid, state, modified) VALUES (?, ?, ?)",
                    arrayOf(state.uid, state.state, state.modified))
                mine[state.uid] = state
            }
            val purged = "SELECT g.id FROM games g JOIN game_states st ON st.uid = g.uid WHERE st.state = '$STATE_PURGED'"
            // What a source imported stays known by its external id; it no longer points to a game.
            db.execSQL("UPDATE game_sources SET game_id = 0 WHERE game_id IN ($purged)")
            db.execSQL("DELETE FROM games WHERE id IN ($purged)")
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }

    /**
     * Upgrade to version 5: the uid of every game from its content, as if it
     * were created now, in id order so identical games get the same
     * occurrences on every copy.
     */
    private fun fillUids() {
        val occurrences = HashMap<String, Int>()
        val ids = db.rawQuery("SELECT id FROM games ORDER BY id", null).use { c -> buildList { while (c.moveToNext()) add(c.getLong(0)) } }
        for ((id, game) in ids.zip(allGames())) {
            val first = GameIdentity.uid(game)
            val occurrence = occurrences.merge(first, 1, Int::plus)!!
            db.execSQL("UPDATE games SET uid = ? WHERE id = ?",
                arrayOf(if (occurrence == 1) first else GameIdentity.uid(game, occurrence), id))
        }
    }

    /** Every game, whole, with uid and revision: what a merge compares. */
    fun allGames(): List<GameRecord> {
        val sql = "SELECT w.name, b.name, e.name, s.name, g.date, g.round, g.result, g.white_elo, g.black_elo," +
            " g.eco, g.start_fen, g.moves_san, g.moves_uci, g.uid, g.modified FROM games g" +
            " LEFT JOIN players w ON w.id = g.white_id LEFT JOIN players b ON b.id = g.black_id" +
            " LEFT JOIN events e ON e.id = g.event_id LEFT JOIN sites s ON s.id = g.site_id ORDER BY g.id"
        return db.rawQuery(sql, null).use { c ->
            fun text(i: Int) = if (c.isNull(i)) "" else c.getString(i)
            buildList {
                while (c.moveToNext()) {
                    add(GameRecord(
                        GameHeaders(text(0), text(1), text(2), text(3), text(4), text(5), text(6),
                            if (c.isNull(7)) 0 else c.getInt(7), if (c.isNull(8)) 0 else c.getInt(8), text(9)),
                        startFen = text(10), movesSan = text(11), movesUci = text(12),
                        uid = text(13).ifEmpty { null }, modified = text(14),
                    ))
                }
            }
        }
    }

    companion object {
        const val APPLICATION_ID = 0x50524147 // "PRAG"
        /** The last schema this app knows: the desktop's DatabaseMigrations::latestVersion(). */
        const val SCHEMA_VERSION = 7
        const val STATE_PURGED = "purged"
        const val PROPERTY_ID = "id"
        const val PROPERTY_TYPE = "type"
        const val TYPE_GAMES = "games"
        const val TYPE_OPENING_BOOK = "opening-book"

        private val SCHEMA = listOf(
            "CREATE TABLE players (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
            "CREATE TABLE events (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
            "CREATE TABLE sites (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)",
            "CREATE TABLE games (" +
                " id INTEGER PRIMARY KEY," +
                " white_id INTEGER REFERENCES players(id)," +
                " black_id INTEGER REFERENCES players(id)," +
                " event_id INTEGER REFERENCES events(id)," +
                " site_id INTEGER REFERENCES sites(id)," +
                " date TEXT, round TEXT, result TEXT," +
                " white_elo INTEGER, black_elo INTEGER, eco TEXT," +
                " ply_count INTEGER NOT NULL DEFAULT 0," +
                " start_fen TEXT," +
                " moves_san TEXT NOT NULL DEFAULT ''," +
                " moves_uci TEXT NOT NULL DEFAULT ''," +
                // Version 7: the variations, as the desktop writes them; the phone shows the main line.
                " variations TEXT NOT NULL DEFAULT ''," +
                " uid TEXT, modified TEXT)",
            "CREATE INDEX games_white ON games(white_id)",
            "CREATE INDEX games_black ON games(black_id)",
            "CREATE TABLE IF NOT EXISTS sources (" +
                " id INTEGER PRIMARY KEY," +
                " uuid TEXT NOT NULL UNIQUE," +
                " kind TEXT NOT NULL," +
                " account TEXT NOT NULL," +
                " settings TEXT NOT NULL DEFAULT '{}'," +
                " state TEXT NOT NULL DEFAULT '{}'," +
                " enabled INTEGER NOT NULL DEFAULT 1," +
                " created_at TEXT NOT NULL," +
                " last_sync_at TEXT," +
                " last_error TEXT)",
            "CREATE TABLE IF NOT EXISTS game_sources (" +
                " game_id INTEGER NOT NULL REFERENCES games(id)," +
                " source_id INTEGER NOT NULL REFERENCES sources(id)," +
                " external_id TEXT NOT NULL," +
                " UNIQUE (source_id, external_id))",
            "CREATE TABLE IF NOT EXISTS player_roles (" +
                " player_id INTEGER PRIMARY KEY REFERENCES players(id)," +
                " role TEXT NOT NULL)",
            "CREATE TABLE IF NOT EXISTS properties (key TEXT PRIMARY KEY, value TEXT NOT NULL)",
            // Version 6: the trash. A game with no row is live; a purged game leaves only its row.
            "CREATE TABLE IF NOT EXISTS game_states (uid TEXT PRIMARY KEY, state TEXT NOT NULL, modified TEXT NOT NULL)",
            UID_INDEX,
        )
        private const val UID_INDEX = "CREATE UNIQUE INDEX IF NOT EXISTS games_uid ON games(uid)"

        private fun openRaw(file: File, flags: Int): SQLiteDatabase =
            SQLiteDatabase.openDatabase(file.path, null, flags).also {
                // Plain rollback journal, like the desktop: a .pdb is always one file.
                it.disableWriteAheadLogging()
            }

        /** Creates a new, empty database at [file], born with its universal id. */
        fun create(file: File, lineage: String = GameIdentity.newLineageId()): PdbDatabase {
            file.parentFile?.mkdirs()
            require(!file.exists()) { "${file.name} already exists" }
            val db = openRaw(file, SQLiteDatabase.OPEN_READWRITE or SQLiteDatabase.CREATE_IF_NECESSARY)
            db.execSQL("PRAGMA application_id = $APPLICATION_ID")
            db.execSQL("PRAGMA user_version = $SCHEMA_VERSION")
            db.beginTransaction()
            try {
                SCHEMA.forEach { db.execSQL(it) }
                db.execSQL("INSERT INTO properties (key, value) VALUES (?, ?)", arrayOf(PROPERTY_TYPE, TYPE_GAMES))
                db.execSQL("INSERT INTO properties (key, value) VALUES (?, ?)", arrayOf(PROPERTY_ID, lineage))
                db.setTransactionSuccessful()
            } finally {
                db.endTransaction()
            }
            return PdbDatabase(file, db)
        }

        /**
         * Opens an existing database; writable ones are upgraded to
         * [SCHEMA_VERSION] (missing tables, game uids from their content, a
         * universal id: fixed for the databases Pragma Chess ships, new
         * otherwise, the table of game states). A file of a newer version is
         * left as it is and refused with [NewerSchemaException]: the app has
         * to be updated to open it.
         */
        fun open(file: File, writable: Boolean = false): PdbDatabase {
            val db = openRaw(file, if (writable) SQLiteDatabase.OPEN_READWRITE else SQLiteDatabase.OPEN_READONLY)
            val applicationId = db.rawQuery("PRAGMA application_id", null).use { if (it.moveToFirst()) it.getInt(0) else 0 }
            val version = db.rawQuery("PRAGMA user_version", null).use { if (it.moveToFirst()) it.getInt(0) else 0 }
            if (applicationId == APPLICATION_ID && version > SCHEMA_VERSION) {
                db.close()
                throw NewerSchemaException(file.name)
            }
            if (applicationId != APPLICATION_ID || version !in 1..SCHEMA_VERSION) {
                db.close()
                throw IllegalStateException("${file.name} is not a Pragma database this app can read")
            }
            if (writable && version < SCHEMA_VERSION) {
                db.beginTransaction()
                try {
                    SCHEMA.filter { it.contains("IF NOT EXISTS") && it != UID_INDEX }.forEach { db.execSQL(it) }
                    if (version < 5) {
                        db.execSQL("ALTER TABLE games ADD COLUMN uid TEXT")
                        db.execSQL("ALTER TABLE games ADD COLUMN modified TEXT")
                        PdbDatabase(file, db).fillUids()
                    }
                    db.execSQL(UID_INDEX)
                    if (version < 7) {
                        db.execSQL("ALTER TABLE games ADD COLUMN variations TEXT NOT NULL DEFAULT ''")
                    }
                    val hasId = db.rawQuery("SELECT 1 FROM properties WHERE key = ? AND value <> ''", arrayOf(PROPERTY_ID))
                        .use { it.moveToFirst() }
                    if (!hasId) {
                        val id = GameIdentity.SHIPPED[file.name] ?: GameIdentity.newLineageId()
                        db.execSQL("INSERT OR REPLACE INTO properties (key, value) VALUES (?, ?)", arrayOf(PROPERTY_ID, id))
                    }
                    db.execSQL("PRAGMA user_version = $SCHEMA_VERSION")
                    db.setTransactionSuccessful()
                } finally {
                    db.endTransaction()
                }
            }
            return PdbDatabase(file, db)
        }
    }
}
