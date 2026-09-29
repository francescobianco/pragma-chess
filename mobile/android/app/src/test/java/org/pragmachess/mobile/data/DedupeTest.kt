package org.pragmachess.mobile.data

import org.junit.Assert.assertEquals
import org.junit.Test

class DedupeTest {
    private fun c(lineage: String, title: String, origin: String, vararg uids: String) =
        Dedupe.Candidate(lineage, title, origin, uids.toSet())

    /** What version 0.1 left on a phone: each database from the computer twice. */
    @Test
    fun collapsesTheCopiesASyncMadeOfOneDatabase() {
        val groups = Dedupe.groups(listOf(
            c("b-2", "chess-com", "Intel5", "u1", "u2"),
            c("a-1", "chess-com (Intel5)", "Intel5", "u1", "u2"),
            c("c-3", "cava", "Intel5"),
            c("d-4", "cava (Intel5)", "Intel5"),
            c("e-5", "Classic Games", "Intel5", "g1"),
        ))
        // The smallest id is kept, so every device keeps the same one.
        assertEquals(listOf(Dedupe.Group("a-1", listOf("b-2")), Dedupe.Group("c-3", listOf("d-4"))), groups)
    }

    @Test
    fun sameNameFromTwoDevicesIsOneDatabaseOnlyWithTheSameGames() {
        // "Le mie partite" made on the phone, and the computer's file built from the phone's games.
        assertEquals(listOf(Dedupe.Group("a", listOf("b"))), Dedupe.groups(listOf(
            c("a", "Le mie partite", "Samsung SM-N960F", "u1", "u2"),
            c("b", "Le mie partite (Intel5)", "Intel5", "u1", "u2"),
        )))
        // Two different databases that happen to share a name stay two.
        assertEquals(emptyList<Dedupe.Group>(), Dedupe.groups(listOf(
            c("a", "Openings", "Laptop", "u1"),
            c("b", "Openings (Desktop)", "Desktop", "u9"),
        )))
        assertEquals(emptyList<Dedupe.Group>(), Dedupe.groups(listOf(
            c("a", "Empty", "Laptop"),
            c("b", "Empty (Desktop)", "Desktop"),
        )))
    }

    @Test
    fun theShownNameDropsOnlyTheOwnDevice() {
        assertEquals("chess-com", Dedupe.displayTitle("chess-com (Intel5)", "Intel5"))
        assertEquals("chess-com (Intel5)", Dedupe.displayTitle("chess-com (Intel5)", "Laptop"))
        assertEquals("x", Dedupe.displayTitle("x", ""))
    }
}
