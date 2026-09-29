package org.pragmachess.mobile.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.UUID

class GameIdentityTest {
    @Test
    fun uuidV5MatchesTheReference() {
        // RFC 4122 namespace DNS, as Python's uuid.uuid5 computes it.
        val dns = UUID.fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
        assertEquals("2ed6657d-e927-568b-95e1-2665a8aea6a2", GameIdentity.uuidV5(dns, "www.example.com".toByteArray()))
    }

    /** The cross-check value shared with the desktop's GameIdentity (tst_chessrules). */
    @Test
    fun gameUidMatchesTheDesktop() {
        val game = GameRecord(GameHeaders(white = "A", black = "B", result = ""), movesUci = "e2e4")
        assertEquals("A\nB\n\n\n\n\n\n\ne2e4", GameIdentity.content(game))
        assertEquals("539093dc-2b01-519f-8380-24f807f5973a", GameIdentity.uid(game))
        assertEquals("7cc1d196-b1c4-5ed6-9d79-66c8d2eb7f30", GameIdentity.uid(game, 2))
    }

    @Test
    fun contentIsTrimmedAndMovesNormalized() {
        val a = GameRecord(GameHeaders(white = " A ", black = "B", result = ""), movesUci = " e2e4  e7e5 ")
        val b = GameRecord(GameHeaders(white = "A", black = "B", result = ""), movesUci = "e2e4 e7e5")
        assertEquals(GameIdentity.uid(a), GameIdentity.uid(b))
        assertNotEquals(GameIdentity.uid(b), GameIdentity.uid(b.copy(movesUci = "e2e4")))
    }

    @Test
    fun sameContentLooksAtEcoAndRatingsToo() {
        val game = GameRecord(GameHeaders(white = "A", black = "B", eco = "C20", whiteElo = 1800), movesUci = "e2e4")
        assertTrue(GameIdentity.sameContent(game, game.copy(movesSan = "e4", modified = "x")))
        assertFalse(GameIdentity.sameContent(game, game.copy(headers = game.headers.copy(eco = "B00"))))
        assertFalse(GameIdentity.sameContent(game, game.copy(headers = game.headers.copy(whiteElo = 1900))))
    }

    @Test
    fun timestampsSortAsTime() {
        val now = GameIdentity.now()
        assertTrue(Regex("""\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z""").matches(now))
        assertTrue("" < now)
    }
}
