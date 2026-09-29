package org.pragmachess.mobile.data

/**
 * How two copies of a database are merged, game by game, by uid
 * (docs/phone-link.md, "Merging two copies of a database"). Pure: the caller
 * applies the plan.
 */
object Reconciler {
    data class Plan(
        /** Incoming games the local copy lacks. */
        val insert: List<GameRecord>,
        /** Incoming games that replace the local version: they differ and are newer. */
        val update: List<GameRecord>,
        /** Local games the incoming copy lacks, or has in an older version: the other side needs them. */
        val send: List<GameRecord>,
        /** Incoming games already here as they are. */
        val same: Int,
        /** Uids whose two versions differ; the newer was kept. */
        val conflicts: List<String>,
    )

    /**
     * Games need a uid. A newer `modified` wins, compared as instants
     * whatever their offset; on a tie, or an incoming game with no date, the
     * local version stays.
     */
    fun plan(local: List<GameRecord>, incoming: List<GameRecord>): Plan {
        val mine = local.filter { it.uid != null }.associateBy { it.uid!! }
        val theirs = incoming.filter { it.uid != null }.associateBy { it.uid!! }
        val insert = ArrayList<GameRecord>()
        val update = ArrayList<GameRecord>()
        val send = ArrayList<GameRecord>()
        val conflicts = ArrayList<String>()
        var same = 0
        for ((uid, other) in theirs) {
            val game = mine[uid]
            when {
                game == null -> insert += other
                GameIdentity.sameContent(game, other) -> same++
                else -> {
                    conflicts += uid
                    if (newer(other.modified, game.modified)) update += other else send += game
                }
            }
        }
        for ((uid, game) in mine) if (uid !in theirs) send += game
        return Plan(insert, update, send, same, conflicts)
    }

    /** Whether [a] is later than [b]; an empty or unreadable time is the oldest. */
    fun newer(a: String, b: String): Boolean {
        val x = instant(a) ?: return false
        val y = instant(b) ?: return true
        return x > y
    }

    private fun instant(text: String): java.time.Instant? =
        if (text.isBlank()) null else runCatching { java.time.OffsetDateTime.parse(text.trim()).toInstant() }.getOrNull()
}
