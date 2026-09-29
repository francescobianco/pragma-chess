package org.pragmachess.mobile.data

import java.io.File

/** A database file of the library, by file name (with the .pdb extension). */
data class DatabaseRef(val name: String) {
    val title: String get() = name.substringAfterLast('/').removeSuffix(".pdb")
}

/**
 * The databases on the phone: one corpus, one folder (`databases/local/` in
 * the app's private files), whoever made each of them. A database is known by
 * its lineage (the `id` property), the file name is only how it is shown.
 */
class Library(private val root: File) {
    val dir: File get() = File(root, "local")

    fun file(ref: DatabaseRef): File = File(dir, ref.name)

    fun list(): List<DatabaseRef> {
        if (!dir.isDirectory) return emptyList()
        return dir.listFiles { f -> f.isFile && f.name.endsWith(".pdb") }.orEmpty()
            .map { DatabaseRef(it.name) }
            .sortedBy { it.name.lowercase() }
    }

    /** A file name for [title] not used yet: "Title.pdb", else "Title (other).pdb", numbered if needed. */
    fun freeName(title: String, other: String): String {
        val clean = clean(title).ifEmpty { "Database" }
        if (!File(dir, "$clean.pdb").exists()) return "$clean.pdb"
        val base = "$clean (${clean(other).ifEmpty { "2" }})"
        if (!File(dir, "$base.pdb").exists()) return "$base.pdb"
        return generateSequence(2) { it + 1 }.map { "$base $it.pdb" }.first { !File(dir, it).exists() }
    }

    /** Creates an empty database called [title] with a new lineage; null if the name is taken or unusable. */
    fun create(title: String): Pair<DatabaseRef, String>? {
        val clean = clean(title)
        if (clean.isEmpty()) return null
        val ref = DatabaseRef("$clean.pdb")
        val file = file(ref)
        if (file.exists()) return null
        val lineage = GameIdentity.newLineageId()
        PdbDatabase.create(file, lineage).close()
        return ref to lineage
    }

    /** Renames [ref] to [title] if that name is free. */
    fun rename(ref: DatabaseRef, title: String): DatabaseRef {
        val target = File(dir, "${clean(title)}.pdb")
        if (target.exists() || !file(ref).renameTo(target)) return ref
        return DatabaseRef(target.name)
    }

    fun delete(ref: DatabaseRef) {
        val file = file(ref)
        file.delete()
        File(file.path + "-journal").delete()
    }

    /**
     * Brings files of older versions into the one folder: the copies that
     * were kept per computer (`databases/<pubkey>/`) move in, renamed when
     * the name is taken. Returns the moved files with the computer they came from.
     */
    fun migrateComputerFolders(computerNames: Map<String, String>): List<Pair<DatabaseRef, String>> {
        val moved = ArrayList<Pair<DatabaseRef, String>>()
        dir.mkdirs()
        for (old in root.listFiles { f -> f.isDirectory && f.name != "local" }.orEmpty()) {
            for (file in old.walkTopDown().filter { it.isFile && it.name.endsWith(".pdb") }) {
                val name = freeName(file.name.removeSuffix(".pdb"), computerNames[old.name] ?: old.name.take(8))
                File(file.path + "-journal").delete()
                if (file.renameTo(File(dir, name))) moved += DatabaseRef(name) to old.name
            }
            old.deleteRecursively()
        }
        return moved
    }

    private fun clean(title: String) = title.trim().replace(Regex("[/\\\\:*?\"<>|]"), "").trim()
}
