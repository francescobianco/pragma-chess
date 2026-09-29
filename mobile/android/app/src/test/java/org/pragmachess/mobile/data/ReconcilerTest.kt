package org.pragmachess.mobile.data

import org.junit.Assert.assertEquals
import org.junit.Test

class ReconcilerTest {
    private fun game(uid: String, moves: String, modified: String = "") =
        GameRecord(GameHeaders(white = "W", black = "B"), movesUci = moves, uid = uid, modified = modified)

    @Test
    fun insertsWhatIsMissingAndSendsWhatTheOtherLacks() {
        val plan = Reconciler.plan(
            local = listOf(game("a", "e2e4"), game("b", "d2d4")),
            incoming = listOf(game("a", "e2e4"), game("c", "c2c4")),
        )
        assertEquals(listOf("c"), plan.insert.map { it.uid })
        assertEquals(listOf("b"), plan.send.map { it.uid })
        assertEquals(1, plan.same)
        assertEquals(emptyList<String>(), plan.conflicts)
        assertEquals(emptyList<GameRecord>(), plan.update)
    }

    @Test
    fun theNewerVersionWinsAConflict() {
        val older = game("a", "e2e4", "2026-09-29T10:00:00.000Z")
        val newer = game("a", "e2e4 e7e5", "2026-09-29T11:00:00.000Z")
        val incomingWins = Reconciler.plan(listOf(older), listOf(newer))
        assertEquals(listOf(newer), incomingWins.update)
        assertEquals(listOf("a"), incomingWins.conflicts)
        assertEquals(emptyList<GameRecord>(), incomingWins.send)

        val localWins = Reconciler.plan(listOf(newer), listOf(older))
        assertEquals(emptyList<GameRecord>(), localWins.update)
        assertEquals(listOf(newer), localWins.send)
        assertEquals(listOf("a"), localWins.conflicts)
    }

    @Test
    fun onATieOrNoDatesTheLocalVersionStays() {
        val plan = Reconciler.plan(listOf(game("a", "e2e4")), listOf(game("a", "d2d4")))
        assertEquals(emptyList<GameRecord>(), plan.update)
        assertEquals(listOf("a"), plan.conflicts)
    }

    @Test
    fun timesCompareAsInstants() {
        assertEquals(true, Reconciler.newer("2026-09-29T12:00:00.000+02:00", "2026-09-29T09:30:00Z"))
        assertEquals(false, Reconciler.newer("2026-09-29T11:00:00+02:00", "2026-09-29T09:30:00.000Z"))
        assertEquals(false, Reconciler.newer("", "2026-09-29T09:30:00Z"))
        assertEquals(true, Reconciler.newer("2026-09-29T09:30:00Z", ""))
    }

    @Test
    fun anEmptySideGetsEverything() {
        val games = listOf(game("a", "e2e4"), game("b", "d2d4"))
        assertEquals(games, Reconciler.plan(emptyList(), games).insert)
        assertEquals(games, Reconciler.plan(games, emptyList()).send)
    }
}
