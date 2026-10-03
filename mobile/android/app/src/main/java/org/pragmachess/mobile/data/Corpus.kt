package org.pragmachess.mobile.data

/** A database of the corpus as the side menu and the sync see it. */
data class CorpusEntry(
    val ref: DatabaseRef,
    val lineage: String,
    val games: Int,
    val openingBook: Boolean,
    /** The device the database was made on or learnt from. */
    val origin: String,
)

/**
 * Every database of the phone as one corpus (docs/phone-link.md, "One corpus"):
 * each file has a lineage and an origin.
 */
class Corpus(private val library: Library, private val store: AppStore) {

    /**
     * Makes every file current (schema version 7, a lineage) and records
     * where it came from. Files that had no lineage get a provisional one,
     * which the first sync replaces with the computer's for the same name;
     * the databases Pragma Chess ships get their fixed ids at once.
     */
    fun prepare(phoneName: String, computerNames: Map<String, String>) {
        val known = store.origins()
        val moved = library.migrateComputerFolders(computerNames).toMap()
        for (ref in library.list()) {
            val file = library.file(ref)
            val existing = runCatching { PdbDatabase.open(file).use { it.lineage() } }.getOrNull()
            val lineage = existing ?: runCatching { PdbDatabase.open(file, writable = true).use { it.lineage() } }.getOrNull() ?: continue
            if (lineage in known) continue
            val origin = moved[ref]?.let { computerNames[it] } ?: phoneName
            store.setOrigin(lineage, origin, provisional = existing == null && lineage !in GameIdentity.SHIPPED.values)
        }
    }

    /**
     * Collapses the databases that are one database under two lineages (see
     * [Dedupe]): the games of the others are merged into the keeper by uid,
     * their files removed, and their lineages remembered as aliases, so a
     * computer that still lists them merges into the keeper. Returns how many
     * files were merged away.
     */
    fun dedupe(): Int {
        val entries = entries()
        val uids = entries.associate { e ->
            e.lineage to runCatching { PdbDatabase.open(library.file(e.ref)).use { db -> db.allGames().mapNotNull { it.uid }.toSet() } }
                .getOrDefault(emptySet())
        }
        val groups = Dedupe.groups(entries.map { Dedupe.Candidate(it.lineage, it.ref.title, it.origin, uids.getValue(it.lineage)) })
        var removed = 0
        for (group in groups) {
            val keeper = entries.first { it.lineage == group.keeper }
            PdbDatabase.open(library.file(keeper.ref), writable = true).use { db ->
                for (other in group.others) {
                    val entry = entries.first { it.lineage == other }
                    val (games, states) = PdbDatabase.open(library.file(entry.ref)).use { it.allGames() to it.states() }
                    db.mergeStates(states)
                    db.merge(Reconciler.plan(db.allGames(), games))
                    library.delete(entry.ref)
                    store.alias(other, group.keeper)
                    removed++
                }
            }
            // The keeper takes the plain name if a removed file had it.
            val plain = Dedupe.displayTitle(keeper.ref.title, keeper.origin)
            if (plain != keeper.ref.title) library.rename(keeper.ref, plain)
        }
        return removed
    }

    fun entries(): List<CorpusEntry> {
        val origins = store.origins()
        return library.list().mapNotNull { ref ->
            runCatching {
                PdbDatabase.open(library.file(ref)).use { db ->
                    val lineage = db.lineage() ?: return@use null
                    CorpusEntry(ref, lineage, db.gameCount(), db.isOpeningBook(), origins[lineage].orEmpty())
                }
            }.getOrNull()
        }
    }

    fun byLineage(): Map<String, DatabaseRef> = entries().associate { it.lineage to it.ref }

    /** A lineage as the corpus knows it: another one it was merged into, or itself. */
    fun resolve(lineage: String): String = store.aliases()[lineage] ?: lineage
}
