package org.pragmachess.mobile.data

/**
 * Finds the databases of the corpus that are one database under two
 * lineages, and says which lineage survives. Pure and unit-tested.
 *
 * It happens with files from before lineages existed: the phone gave its
 * copy one id, the computer gave its file another, and a sync brought each
 * side the other's as a new database. Two databases are the same when they
 * have the same name (as shown, without the "(device)" a sync added) and
 * either come from the same device (one device never holds two files of
 * one name) or hold exactly the same games.
 */
object Dedupe {
    data class Candidate(
        val lineage: String,
        /** The file name without .pdb. */
        val title: String,
        val origin: String,
        val uids: Set<String>,
    )

    /** One database kept under [keeper]; the files of [others] are merged into it and removed. */
    data class Group(val keeper: String, val others: List<String>)

    /** The name shown: "Name (device)", made by a sync to tell two files apart, shows as "Name". */
    fun displayTitle(title: String, origin: String): String {
        val suffix = " ($origin)"
        return if (origin.isNotBlank() && title.endsWith(suffix)) title.removeSuffix(suffix) else title
    }

    fun groups(candidates: List<Candidate>): List<Group> {
        val result = ArrayList<Group>()
        val byName = candidates.groupBy { displayTitle(it.title, it.origin).lowercase() }
        for (named in byName.values) {
            val pending = named.sortedBy { it.lineage }.toMutableList()
            while (pending.isNotEmpty()) {
                val first = pending.removeAt(0)
                val same = pending.filter { it.origin == first.origin || (it.uids.isNotEmpty() && it.uids == first.uids) }
                pending.removeAll(same)
                // The smallest id wins: any device applying this rule keeps the same one.
                if (same.isNotEmpty()) result += Group(first.lineage, same.map { it.lineage })
            }
        }
        return result
    }
}
