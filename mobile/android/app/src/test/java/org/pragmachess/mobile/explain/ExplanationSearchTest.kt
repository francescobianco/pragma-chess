package org.pragmachess.mobile.explain

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Test
import org.pragmachess.mobile.chess.Position
import java.io.File

/**
 * The whole search on a real UCI engine, run only with PRAGMA_EXPLAIN_ENGINE
 * set to its path (e.g. /usr/games/stockfish): it compares with
 * `pragma-explain --trace "1.e4 e5 2.Nf3 d6 3.Nxe5"` on the desktop.
 */
class ExplanationSearchTest {
    @Test
    fun explainsWithARealEngine() {
        val path = System.getenv("PRAGMA_EXPLAIN_ENGINE")
        assumeTrue(path != null && File(path).canExecute())
        var before = Position.starting()
        for (uci in listOf("e2e4", "e7e5", "g1f3", "d7d6")) before = before.play(before.parseUci(uci)!!)
        val played = before.parseUci("f3e5")!!
        ExplanationSearch().use { search ->
            val analysis = runBlocking { search.analyze(File(path!!), before, played, before.play(played)) }
            assertEquals(20, analysis.afterEvaluation!!.depth)
            assertTrue(analysis.probe.isNotEmpty())
            val explanation = explainPosition(analysis.input(trace = true))
            println(explanation.summary)
            explanation.trace.forEach(::println)
            assertTrue(explanation.summary, explanation.summary.contains("Black wins a knight for a pawn"))
        }
    }
}
