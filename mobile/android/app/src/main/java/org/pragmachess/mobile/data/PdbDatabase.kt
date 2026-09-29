package org.pragmachess.mobile.data

import android.content.ContentValues
import android.database.sqlite.SQLiteDatabase
import java.io.File
import java.time.Instant

/**
 * A Pragma .pdb database: SQLite with the desktop's schema (application_id
 * PRAG, user_version 4; see gui/qt/src/app/SqliteGameDatabase.cpp). The phone
 * reads any file of version 1 to 4 and writes games with their players,
 * events and sites, recording the phone as their source.
 */
class PdbDatabase private constructor(val file: File, private val db: SQLiteDatabase) : AutoCloseable {

    override fun close() = db.close()

    /** The database's own properties (table `properties`, version 4); empty for older files. */
    fun properties(): Map<String, String> = runCatching {
        db.rawQuery("SELECT key, value FROM properties", null).use { c ->
            buildMap { while (c.moveToNext()) put(c.getString(0), c.getString(1)) }
        }
    }.getOrDefault(emptyMap())

    /** Opening books (type "opening-book") hold named lines, not games to browse; missing type = games. */
    fun isOpeningBook(): Boolean = properties()[PROPERTY_TYPE] == TYPE_OPENING_BOOK

    fun gameCount(): Int = db.rawQuery("SELECT COUNT(*) FROM games", null).use { if (it.moveToFirst()) it.getInt(0) else 0 }

    /** Games whose players or event contain [filter] (all when blank), newest first. */
    fun games(filter: String = "", limit: Int = 5000): List<GameSummary> {
        val where = if (filter.isBlank()) "" else " WHERE w.name LIKE ?1 OR b.name LIKE ?1 OR e.name LIKE ?1"
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

    private fun sourceId(source: PhoneSource): Long {
        db.rawQuery("SELECT id FROM sources WHERE uuid = ?", arrayOf(source.uuid)).use { if (it.moveToFirst()) return it.getLong(0) }
        val values = ContentValues().apply {
            put("uuid", source.uuid)
            put("kind", "phone")
            put("account", source.name)
            put("created_at", Instant.now().toString())
        }
        return db.insertOrThrow("sources", null, values)
    }

    fun hasGame(source: PhoneSource, uuid: String): Boolean =
        db.rawQuery("SELECT 1 FROM game_sources gs JOIN sources s ON s.id = gs.source_id WHERE s.uuid = ? AND gs.external_id = ?",
            arrayOf(source.uuid, uuid)).use { it.moveToFirst() }

    /** Stores [game] (made on the phone, with a uuid) unless it is already there; returns its id or null. */
    fun insert(game: GameRecord, source: PhoneSource): Long? {
        val uuid = requireNotNull(game.uuid) { "phone games have a uuid" }
        db.beginTransaction()
        try {
            if (hasGame(source, uuid)) return null
            val h = game.headers
            val values = ContentValues().apply {
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
            }
            val id = db.insertOrThrow("games", null, values)
            db.execSQL("INSERT INTO game_sources (game_id, source_id, external_id) VALUES (?, ?, ?)",
                arrayOf<Any>(id, sourceId(source), uuid))
            db.setTransactionSuccessful()
            return id
        } finally {
            db.endTransaction()
        }
    }

    /** The games this phone made that are in this file, with their uuids. */
    fun phoneGames(source: PhoneSource): List<GameRecord> {
        val ids = db.rawQuery("SELECT gs.game_id, gs.external_id FROM game_sources gs JOIN sources s ON s.id = gs.source_id WHERE s.uuid = ?",
            arrayOf(source.uuid)).use { c -> buildList { while (c.moveToNext()) add(c.getLong(0) to c.getString(1)) } }
        return ids.mapNotNull { (id, uuid) -> game(id)?.copy(uuid = uuid) }
    }

    companion object {
        const val APPLICATION_ID = 0x50524147 // "PRAG"
        const val SCHEMA_VERSION = 4
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
                " moves_uci TEXT NOT NULL DEFAULT '')",
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
        )

        private fun openRaw(file: File, flags: Int): SQLiteDatabase =
            SQLiteDatabase.openDatabase(file.path, null, flags).also {
                // Plain rollback journal, like the desktop: a .pdb is always one file.
                it.disableWriteAheadLogging()
            }

        /** Creates a new, empty database at [file]. */
        fun create(file: File): PdbDatabase {
            file.parentFile?.mkdirs()
            require(!file.exists()) { "${file.name} already exists" }
            val db = openRaw(file, SQLiteDatabase.OPEN_READWRITE or SQLiteDatabase.CREATE_IF_NECESSARY)
            db.execSQL("PRAGMA application_id = $APPLICATION_ID")
            db.execSQL("PRAGMA user_version = $SCHEMA_VERSION")
            db.beginTransaction()
            try {
                SCHEMA.forEach { db.execSQL(it) }
                db.execSQL("INSERT INTO properties (key, value) VALUES (?, ?)", arrayOf(PROPERTY_TYPE, TYPE_GAMES))
                db.setTransactionSuccessful()
            } finally {
                db.endTransaction()
            }
            return PdbDatabase(file, db)
        }

        /**
         * Opens an existing database; writable ones get the tables of later
         * versions (sources, game_sources, properties) the phone needs to store
         * its games. Files of a newer version than [SCHEMA_VERSION] are refused.
         */
        fun open(file: File, writable: Boolean = false): PdbDatabase {
            val db = openRaw(file, if (writable) SQLiteDatabase.OPEN_READWRITE else SQLiteDatabase.OPEN_READONLY)
            val applicationId = db.rawQuery("PRAGMA application_id", null).use { if (it.moveToFirst()) it.getInt(0) else 0 }
            val version = db.rawQuery("PRAGMA user_version", null).use { if (it.moveToFirst()) it.getInt(0) else 0 }
            if (applicationId != APPLICATION_ID || version !in 1..SCHEMA_VERSION) {
                db.close()
                throw IllegalStateException("${file.name} is not a Pragma database this app can read")
            }
            if (writable && version < SCHEMA_VERSION) {
                db.beginTransaction()
                try {
                    SCHEMA.filter { it.contains("IF NOT EXISTS") }.forEach { db.execSQL(it) }
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

/** How the phone appears as a source in a database: kind "phone", uuid = its public key. */
data class PhoneSource(val uuid: String, val name: String)
