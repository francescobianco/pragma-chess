package org.pragmachess.mobile.data

import java.io.File

/** Where a database lives: made on the phone, or a copy of a computer's. */
sealed interface DatabaseLocation {
    data object Local : DatabaseLocation
    data class Computer(val pubkey: String) : DatabaseLocation
}

/** A database file of the library; [name] is relative to its folder, with "/" and the .pdb extension. */
data class DatabaseRef(val location: DatabaseLocation, val name: String) {
    val title: String get() = name.substringAfterLast('/').removeSuffix(".pdb")
}

/**
 * The databases on the phone, under the app's private files:
 * `databases/local/` for the ones made here, `databases/<computer pubkey>/`
 * for the copies of each paired computer's.
 */
class Library(private val root: File) {
    private val localDir get() = File(root, "local")

    fun dirFor(location: DatabaseLocation): File = when (location) {
        DatabaseLocation.Local -> localDir
        is DatabaseLocation.Computer -> File(root, location.pubkey)
    }

    fun file(ref: DatabaseRef): File = File(dirFor(ref.location), ref.name)

    fun list(location: DatabaseLocation): List<DatabaseRef> {
        val dir = dirFor(location)
        if (!dir.isDirectory) return emptyList()
        return dir.walkTopDown()
            .filter { it.isFile && it.name.endsWith(".pdb") }
            .map { DatabaseRef(location, it.relativeTo(dir).invariantSeparatorsPath) }
            .sortedBy { it.name.lowercase() }
            .toList()
    }

    fun localNames(): Set<String> = list(DatabaseLocation.Local).map { it.name }.toSet()

    /** Creates an empty local database called [title]; null if the name is taken or unusable. */
    fun createLocal(title: String): DatabaseRef? {
        val clean = title.trim().replace(Regex("[/\\\\:*?\"<>|]"), "").trim()
        if (clean.isEmpty()) return null
        val ref = DatabaseRef(DatabaseLocation.Local, "$clean.pdb")
        val file = file(ref)
        if (file.exists()) return null
        PdbDatabase.create(file).close()
        return ref
    }

    /** First launch: the one database a phone on its own starts with. */
    fun ensureDefault(title: String): DatabaseRef {
        list(DatabaseLocation.Local).firstOrNull()?.let { return it }
        return createLocal(title) ?: DatabaseRef(DatabaseLocation.Local, "$title.pdb")
    }

    /**
     * Where a file of a computer's list is kept: a database with the name of
     * a local one is that database (a phone database once pushed comes back
     * from the computer, with what was added there), anything else is a copy
     * in the computer's folder.
     */
    fun refForRemote(pubkey: String, name: String): DatabaseRef =
        if (name in localNames()) DatabaseRef(DatabaseLocation.Local, name)
        else DatabaseRef(DatabaseLocation.Computer(pubkey), name)

    fun delete(ref: DatabaseRef) {
        val file = file(ref)
        file.delete()
        File(file.path + "-journal").delete()
    }

    fun deleteComputer(pubkey: String) {
        dirFor(DatabaseLocation.Computer(pubkey)).deleteRecursively()
    }
}
