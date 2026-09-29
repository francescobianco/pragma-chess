package org.pragmachess.mobile.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import org.json.JSONArray
import org.json.JSONObject

/** A paired computer. [pairSecret] is kept until the computer has accepted the phone once. */
data class Computer(
    val pubkey: String,
    val name: String,
    val relays: List<String>,
    val pairSecret: String?,
    val lastSync: Long,
)

/** One game waiting to be pushed to one computer. */
data class OutboxEntry(val gameId: String, val computer: String, val database: String, val game: GameRecord)

/** The app's own state: paired computers and the outbox of games to push. */
class AppStore(context: Context) : SQLiteOpenHelper(context, "pragma-mobile.db", null, 1) {
    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL("CREATE TABLE computers (pubkey TEXT PRIMARY KEY, name TEXT NOT NULL, relays TEXT NOT NULL," +
            " pair_secret TEXT, last_sync INTEGER NOT NULL DEFAULT 0)")
        db.execSQL("CREATE TABLE outbox (game_id TEXT NOT NULL, computer TEXT NOT NULL, database TEXT NOT NULL," +
            " game TEXT NOT NULL, created_at INTEGER NOT NULL, PRIMARY KEY (game_id, computer))")
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) = Unit

    fun computers(): List<Computer> = readableDatabase.rawQuery(
        "SELECT pubkey, name, relays, pair_secret, last_sync FROM computers ORDER BY name", null
    ).use { c ->
        buildList {
            while (c.moveToNext()) {
                val relays = JSONArray(c.getString(2))
                add(Computer(c.getString(0), c.getString(1), (0 until relays.length()).map { relays.getString(it) },
                    if (c.isNull(3)) null else c.getString(3), c.getLong(4)))
            }
        }
    }

    fun saveComputer(computer: Computer) {
        val values = ContentValues().apply {
            put("pubkey", computer.pubkey)
            put("name", computer.name)
            put("relays", JSONArray(computer.relays).toString())
            put("pair_secret", computer.pairSecret)
            put("last_sync", computer.lastSync)
        }
        writableDatabase.insertWithOnConflict("computers", null, values, SQLiteDatabase.CONFLICT_REPLACE)
    }

    fun removeComputer(pubkey: String) {
        writableDatabase.delete("computers", "pubkey = ?", arrayOf(pubkey))
        writableDatabase.delete("outbox", "computer = ?", arrayOf(pubkey))
    }

    fun enqueue(computer: String, database: String, game: GameRecord) {
        val values = ContentValues().apply {
            put("game_id", game.uuid)
            put("computer", computer)
            put("database", database)
            put("game", game.toJson().toString())
            put("created_at", System.currentTimeMillis())
        }
        writableDatabase.insertWithOnConflict("outbox", null, values, SQLiteDatabase.CONFLICT_REPLACE)
    }

    fun outbox(computer: String): List<OutboxEntry> = readableDatabase.rawQuery(
        "SELECT game_id, database, game FROM outbox WHERE computer = ? ORDER BY created_at", arrayOf(computer)
    ).use { c ->
        buildList {
            while (c.moveToNext()) add(OutboxEntry(c.getString(0), computer, c.getString(1), GameRecord.fromJson(JSONObject(c.getString(2)))))
        }
    }

    fun outboxCount(computer: String): Int = readableDatabase.rawQuery(
        "SELECT COUNT(*) FROM outbox WHERE computer = ?", arrayOf(computer)
    ).use { if (it.moveToFirst()) it.getInt(0) else 0 }

    fun sent(computer: String, gameIds: Collection<String>) {
        val db = writableDatabase
        db.beginTransaction()
        try {
            for (id in gameIds) db.delete("outbox", "game_id = ? AND computer = ?", arrayOf(id, computer))
            db.setTransactionSuccessful()
        } finally {
            db.endTransaction()
        }
    }
}
