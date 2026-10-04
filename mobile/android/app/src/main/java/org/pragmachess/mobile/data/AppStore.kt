package org.pragmachess.mobile.data

import android.content.ContentValues
import android.content.Context
import android.database.sqlite.SQLiteDatabase
import android.database.sqlite.SQLiteOpenHelper
import org.json.JSONArray

/** A paired computer. [pairSecret] is kept until the computer has accepted the phone once. */
data class Computer(
    val pubkey: String,
    val name: String,
    val relays: List<String>,
    val pairSecret: String?,
    val lastSync: Long,
)

/**
 * The app's own state: paired computers, and where each database came from
 * (by lineage: the device that made it or the phone learnt it from).
 */
class AppStore(context: Context) : SQLiteOpenHelper(context, "pragma-mobile.db", null, 5) {
    override fun onCreate(db: SQLiteDatabase) {
        db.execSQL("CREATE TABLE computers (pubkey TEXT PRIMARY KEY, name TEXT NOT NULL, relays TEXT NOT NULL," +
            " pair_secret TEXT, last_sync INTEGER NOT NULL DEFAULT 0)")
        onUpgrade(db, 1, 5)
    }

    override fun onUpgrade(db: SQLiteDatabase, oldVersion: Int, newVersion: Int) {
        if (oldVersion < 2) {
            // Games no longer wait in an outbox: every sync reconciles the databases.
            db.execSQL("DROP TABLE IF EXISTS outbox")
            // provisional: the id was made up when a file older than version 5 was
            // upgraded here; the first sync adopts the computer's for the same name.
            db.execSQL("CREATE TABLE IF NOT EXISTS origins (lineage TEXT PRIMARY KEY, device TEXT NOT NULL," +
                " provisional INTEGER NOT NULL DEFAULT 0)")
        }
        if (oldVersion < 3) {
            // The two files of each database as they were when last reconciled with a computer.
            db.execSQL("CREATE TABLE IF NOT EXISTS reconciled (computer TEXT NOT NULL, lineage TEXT NOT NULL," +
                " remote_sha TEXT NOT NULL, local_sha TEXT NOT NULL, PRIMARY KEY (computer, lineage))")
        }
        if (oldVersion < 4) {
            db.execSQL("CREATE TABLE IF NOT EXISTS aliases (lineage TEXT PRIMARY KEY, merged_into TEXT NOT NULL)")
        }
        if (oldVersion < 5) {
            // Databases the user deleted here: never downloaded again, and each
            // computer that has one is told once (notified), so it can ask its user.
            db.execSQL("CREATE TABLE IF NOT EXISTS deleted (lineage TEXT PRIMARY KEY, name TEXT NOT NULL, deleted_at TEXT NOT NULL)")
            db.execSQL("CREATE TABLE IF NOT EXISTS deletion_notices (computer TEXT NOT NULL, lineage TEXT NOT NULL," +
                " PRIMARY KEY (computer, lineage))")
        }
    }

    /** Lineages merged into another one of the corpus (see Corpus.dedupe). */
    fun aliases(): Map<String, String> = readableDatabase.rawQuery("SELECT lineage, merged_into FROM aliases", null).use { c ->
        buildMap { while (c.moveToNext()) put(c.getString(0), c.getString(1)) }
    }

    fun alias(lineage: String, into: String) {
        writableDatabase.execSQL("INSERT OR REPLACE INTO aliases (lineage, merged_into) VALUES (?, ?)", arrayOf(lineage, into))
        // Anything that was merged into the removed lineage follows it.
        writableDatabase.execSQL("UPDATE aliases SET merged_into = ? WHERE merged_into = ?", arrayOf(into, lineage))
    }

    /** A database the user deleted on the phone: its file name and when (ISO 8601 UTC). */
    data class Deleted(val name: String, val deletedAt: String)

    /** The databases deleted here, by lineage. */
    fun deleted(): Map<String, Deleted> = readableDatabase.rawQuery("SELECT lineage, name, deleted_at FROM deleted", null).use { c ->
        buildMap { while (c.moveToNext()) put(c.getString(0), Deleted(c.getString(1), c.getString(2))) }
    }

    fun markDeleted(lineage: String, name: String, deletedAt: String) {
        writableDatabase.execSQL("INSERT OR REPLACE INTO deleted (lineage, name, deleted_at) VALUES (?, ?, ?)", arrayOf(lineage, name, deletedAt))
    }

    /** The lineages (as the computer lists them) [computer] was told were deleted here. */
    fun notified(computer: String): Set<String> = readableDatabase.rawQuery(
        "SELECT lineage FROM deletion_notices WHERE computer = ?", arrayOf(computer)
    ).use { c -> buildSet { while (c.moveToNext()) add(c.getString(0)) } }

    fun setNotified(computer: String, lineage: String) {
        writableDatabase.execSQL("INSERT OR IGNORE INTO deletion_notices (computer, lineage) VALUES (?, ?)", arrayOf(computer, lineage))
    }

    /** Remote and local hashes of each lineage at the last reconciliation with [computer]. */
    fun reconciled(computer: String): Map<String, Pair<String, String>> = readableDatabase.rawQuery(
        "SELECT lineage, remote_sha, local_sha FROM reconciled WHERE computer = ?", arrayOf(computer)
    ).use { c -> buildMap { while (c.moveToNext()) put(c.getString(0), c.getString(1) to c.getString(2)) } }

    fun setReconciled(computer: String, lineage: String, remoteSha: String, localSha: String) {
        writableDatabase.execSQL("INSERT OR REPLACE INTO reconciled (computer, lineage, remote_sha, local_sha) VALUES (?, ?, ?, ?)",
            arrayOf(computer, lineage, remoteSha, localSha))
    }

    fun origins(): Map<String, String> = readableDatabase.rawQuery("SELECT lineage, device FROM origins", null).use { c ->
        buildMap { while (c.moveToNext()) put(c.getString(0), c.getString(1)) }
    }

    /** Records where [lineage] came from, unless it is known already. */
    fun setOrigin(lineage: String, device: String, provisional: Boolean = false) {
        writableDatabase.execSQL("INSERT OR IGNORE INTO origins (lineage, device, provisional) VALUES (?, ?, ?)",
            arrayOf<Any>(lineage, device, if (provisional) 1 else 0))
    }

    fun isProvisional(lineage: String): Boolean =
        readableDatabase.rawQuery("SELECT 1 FROM origins WHERE lineage = ? AND provisional = 1", arrayOf(lineage)).use { it.moveToFirst() }

    /** A database with a provisional id took the computer's: the lineage becomes [adopted]. */
    fun adopt(provisional: String, adopted: String, device: String) {
        writableDatabase.delete("origins", "lineage = ?", arrayOf(provisional))
        writableDatabase.execSQL("INSERT OR REPLACE INTO origins (lineage, device, provisional) VALUES (?, ?, 0)", arrayOf(adopted, device))
    }

    fun confirm(lineage: String) {
        writableDatabase.execSQL("UPDATE origins SET provisional = 0 WHERE lineage = ?", arrayOf(lineage))
    }

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
        writableDatabase.delete("reconciled", "computer = ?", arrayOf(pubkey))
        writableDatabase.delete("deletion_notices", "computer = ?", arrayOf(pubkey))
    }
}
