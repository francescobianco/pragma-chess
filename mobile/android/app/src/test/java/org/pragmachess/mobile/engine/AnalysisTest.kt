package org.pragmachess.mobile.engine

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class AnalysisTest {
    @Test
    fun scoresAreFromWhitesPointOfView() {
        val line = "info depth 18 seldepth 25 multipv 1 score cp 34 nodes 1 nps 1 pv e2e4 e7e5 g1f3"
        val white = Analysis.parseInfo(line, whiteToMove = true)!!
        assertEquals(34, white.centipawns)
        assertEquals(listOf("e2e4", "e7e5", "g1f3"), white.pv)
        assertEquals("+0.3", white.text)
        val black = Analysis.parseInfo(line, whiteToMove = false)!!
        assertEquals(-34, black.centipawns)
        assertEquals("−0.3", black.text)
    }

    @Test
    fun matesAndBounds() {
        val mate = Analysis.parseInfo("info depth 5 score mate 2 pv d1h5", whiteToMove = false)!!
        assertEquals(-2, mate.mate)
        assertEquals("M2", mate.text)
        assertEquals(0f, mate.whiteShare)
        assertNull(Analysis.parseInfo("info depth 9 score cp 20 lowerbound pv e2e4", true))
        assertNull(Analysis.parseInfo("info depth 9 multipv 2 score cp 20 pv e2e4", true))
        assertNull(Analysis.parseInfo("info string NNUE evaluation enabled", true))
        assertEquals(0.5f, Analysis(10, 0, null, listOf("e2e4")).whiteShare, 1e-6f)
        assertEquals("12", Analysis(10, -1234, null, listOf("e2e4")).text.drop(1))
    }
}
