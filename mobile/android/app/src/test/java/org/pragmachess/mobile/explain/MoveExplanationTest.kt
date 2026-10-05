package org.pragmachess.mobile.explain

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.Square

/** The desktop's Explain tests (gui/qt/tests/tst_chessrules.cpp), with the same synthetic engine lines. */
class MoveExplanationTest {
    private fun sq(name: String) = Square.parse(name)

    private fun afterMoves(uci: List<String>, start: Position = Position.starting()): Position =
        uci.fold(start) { position, move -> position.play(position.parseUci(move)!!) }

    /** Plays SAN text ("1.e4 e5 2.Nf3") and returns the moves in UCI. */
    private fun uciOf(san: String): List<String> {
        var position = Position.starting()
        val moves = ArrayList<String>()
        for (token in san.split(' ').filter { it.isNotBlank() }) {
            val word = token.substringAfterLast('.').substringAfterLast('…')
            if (word.isEmpty()) continue
            val move = position.parseSan(word) ?: error("illegal $word")
            moves += move.uci
            position = position.play(move)
        }
        return moves
    }

    private fun centipawns(value: Int, pv: List<String>) = EngineEvaluation(centipawns = value, depth = 20, pv = pv)

    private fun inputFor(movesBefore: List<String>, played: String, before: EngineEvaluation, after: EngineEvaluation): ExplanationInput {
        val position = afterMoves(movesBefore)
        val move = position.parseUci(played)!!
        return ExplanationInput(before = position, played = move, beforeEvaluation = before,
            after = position.play(move), afterEvaluation = after)
    }

    private fun isError(verdict: MoveExplanation.Verdict) = verdict == MoveExplanation.Verdict.Inaccuracy ||
        verdict == MoveExplanation.Verdict.Mistake || verdict == MoveExplanation.Verdict.Blunder

    @Test
    fun explainsHangingPiece() {
        // 3.Nxe5?? grabs a pawn but the knight is defended: 3…dxe5 leaves Black a piece for a pawn up.
        val explanation = explainPosition(inputFor(listOf("e2e4", "e7e5", "g1f3", "d7d6"), "f3e5",
            centipawns(40, listOf("d2d4")), centipawns(-190, listOf("d6e5", "b1c3", "g8f6"))))
        assertEquals(MoveExplanation.Verdict.Mistake, explanation.verdict)
        assertEquals(2, explanation.arrows.size)
        assertEquals(BoardArrow.Kind.Alternative, explanation.arrows[0].kind)
        assertEquals(BoardArrow(sq("d6"), sq("e5"), BoardArrow.Kind.Refutation, 1), explanation.arrows[1])
        assertEquals(listOf(sq("e5")), explanation.lostPieces)
        assertTrue(explanation.summary, explanation.summary.contains("Black wins a knight for a pawn: 3…dxe5. Better was 3.d4."))
    }

    @Test
    fun waitsForIntermediateMoves() {
        // After 4…exd4 White inserts 5.Bxf7+: the material is counted once the capture, check and recapture are over.
        val explanation = explainPosition(inputFor(listOf("e2e4", "e7e5", "g1f3", "d7d6", "f1c4", "h7h6"), "f3d4",
            centipawns(60, listOf("d2d4")), centipawns(-150, listOf("e5d4", "c4f7", "e8f7", "d1h5", "g7g6", "h5d5"))))
        assertTrue(explanation.summary, isError(explanation.verdict))
        assertTrue(explanation.summary, explanation.summary.contains("Black wins a bishop and a knight for a pawn: 4…exd4 5.Bxf7+ Kxf7."))
        assertEquals(4, explanation.arrows.size)
    }

    @Test
    fun focusesLongRealizations() {
        // Evergreen game, 15…Qf5: the knight only falls eleven plies later; the decisive jump is drawn.
        val moves = uciOf("1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5 4.b4 Bxb4 5.c3 Ba5 6.d4 exd4 7.O-O d3 8.Qb3 Qf6 9.e5 Qg6 " +
            "10.Re1 Nge7 11.Ba3 b5 12.Qxb5 Rb8 13.Qa4 Bb6 14.Nbd2 Bb7 15.Ne4")
        val explanation = explainPosition(inputFor(moves, "g6f5", centipawns(140, listOf("d3d2", "e4d2")),
            centipawns(430, listOf("c4d3", "f5e6", "a1d1", "e8g8", "e4g5", "e6h6", "d3h7", "g8h8", "d1d7", "c6e5", "e1e5", "f7f6"))))
        assertEquals(MoveExplanation.Verdict.Mistake, explanation.verdict)
        assertEquals(listOf(BoardArrow(sq("c6"), sq("e5"), BoardArrow.Kind.Reply, 1),
            BoardArrow(sq("e1"), sq("e5"), BoardArrow.Kind.Refutation, 2)), explanation.arrows)
        assertEquals(listOf(sq("c6")), explanation.lostPieces)
        assertTrue(explanation.summary, explanation.summary.contains("20.Rxd7 Nxe5 21.Rxe5."))
        assertTrue(explanation.playback.isEmpty()) // Only mates are played.
    }

    @Test
    fun explainsAllowedMate() {
        val mate = EngineEvaluation(isMate = true, mateIn = 1, mating = Side.Black, pv = listOf("d8h4"))
        val explanation = explainPosition(inputFor(listOf("f2f3", "e7e5"), "g2g4", centipawns(-60, listOf("e1f2")), mate))
        assertEquals(MoveExplanation.Verdict.Blunder, explanation.verdict)
        assertTrue(BoardArrow(sq("d8"), sq("h4"), BoardArrow.Kind.Refutation, 1) in explanation.arrows)
        assertTrue(explanation.summary, explanation.summary.contains("Black mates in 1: 2…Qh4#"))
        assertEquals(listOf("d8h4"), explanation.playback)
    }

    @Test
    fun explainsWinningCapture() {
        val explanation = explainPosition(inputFor(listOf("e2e4", "e7e5", "g1f3", "d7d6", "f3d4"), "e5d4",
            centipawns(-190, listOf("e5d4", "c2c3")), centipawns(-190, listOf("c2c3", "d4c3"))))
        assertEquals(MoveExplanation.Verdict.Best, explanation.verdict)
        assertTrue(explanation.summary, explanation.summary.contains("Best move (−1.9). Black wins a knight: 3…exd4."))
    }

    @Test
    fun concretePlyGuidesQuietExplanations() {
        val input = ExplanationInput(after = Position.starting(), afterEvaluation = centipawns(20, listOf("e2e4", "e7e5", "g1f3", "b8c6")))
        assertEquals(2, explainPosition(input).arrows.size)

        val explanation = explainPosition(input.copy(concretePly = 3, trace = true))
        assertEquals(3, explanation.arrows.size)
        assertTrue(explanation.summary, explanation.summary.contains("No material explains it: the assessment is positional, clear after 1.e4 e5 2.Nf3."))
        assertFalse(explanation.trace.isEmpty())
    }

    /** 1.e4 e5 2.Qg4 Nf6 3.Qf5: the evaluation drops but no material is lost, so nothing is red. */
    @Test
    fun positionalDropIsNotAMaterialLoss() {
        // Stockfish 16, depth 20, from pragma-explain --trace.
        val input = inputFor(listOf("e2e4", "e7e5", "d1g4", "g8f6"), "g4f5",
            centipawns(-90, listOf("g4g3", "f8e7", "f1c4", "e8g8", "d2d3", "c7c6", "b1c3", "d7d5")),
            centipawns(-220, listOf("b8c6", "f5f3", "d7d5", "e4d5", "c8g4", "f3g3", "d8d5", "b1c3")))
        val explanation = explainPosition(input.copy(concretePly = 1))
        assertEquals(MoveExplanation.Verdict.Inaccuracy, explanation.verdict)
        assertTrue(explanation.playback.isEmpty())
        assertTrue(explanation.lostPieces.isEmpty())
        for (arrow in explanation.arrows) assertTrue(explanation.summary, arrow.kind != BoardArrow.Kind.Refutation)
        assertTrue(explanation.summary, explanation.summary.contains("No material explains it"))
        assertFalse(explanation.summary, explanation.summary.contains("becomes concrete"))
    }

    @Test
    fun playsTheWholeMate() {
        // 14…Bxc3: a mate in 14 for White, played on the board to the end.
        val after = afterMoves(uciOf("1.e4 e5 2.f4 exf4 3.Nf3 Nc6 4.Bc4 Nf6 5.Nc3 Bc5 6.d4 Bb6 7.Bxf4 O-O 8.O-O Re8 9.e5 Ng4 " +
            "10.Kh1 Kh8 11.Ng5 Nh6 12.Qd3 g6 13.Nge4 Bxd4 14.Bxh6 Bxc3"))
        val mate = EngineEvaluation(isMate = true, mateIn = 14, mating = Side.White, depth = 32,
            pv = ("d3c3 d7d5 e5d6 f7f6 f1f6 e8e5 f6f7 c8e6 c4e6 d8g8 d6c7 a8c8 e4g5 c8e8 a1f1 b7b5 " +
                "f7f8 e8f8 f1f8 g8f8 g5f7 f8f7 c7c8q c6d8 c3e5 h8g8 c8d8").split(' '))
        val explanation = explainPosition(ExplanationInput(after = after, afterEvaluation = mate))
        assertEquals(27, explanation.playback.size) // The whole mate, not only the first plies.
        assertTrue(explanation.summary.contains("White mates in 14"))
    }

    @Test
    fun translatesTheSummary() {
        val italian = ExplainText { source ->
            mapOf("Black" to "Nero", "Mistake" to "Errore", "a knight" to "un cavallo", "a pawn" to "un pedone",
                "wins %1 for %2" to "vince %1 in cambio di %2", "%1 %2: %3." to "Il %1 %2: %3.",
                " Better was %1." to " Era meglio %1.")[source] ?: source
        }
        val explanation = explainPosition(inputFor(listOf("e2e4", "e7e5", "g1f3", "d7d6"), "f3e5",
            centipawns(40, listOf("d2d4")), centipawns(-190, listOf("d6e5", "b1c3", "g8f6"))), italian)
        assertTrue(explanation.summary, explanation.summary.contains("Il Nero vince un cavallo in cambio di un pedone: 3…dxe5. Era meglio 3.d4."))
    }

    @Test
    fun readsEngineLinesAsTheDesktop() {
        val black = EngineEvaluation.parseInfo("info depth 12 seldepth 18 multipv 1 score cp 35 nodes 1 pv e7e5 g1f3", whiteToMove = false)!!
        assertEquals(-35, black.centipawns)
        assertEquals(12, black.depth)
        assertEquals(listOf("e7e5", "g1f3"), black.pv)
        val mated = EngineEvaluation.parseInfo("info depth 1 score mate -2 pv e1f2", whiteToMove = true)!!
        assertTrue(mated.isMate)
        assertEquals(2, mated.mateIn)
        assertEquals(Side.Black, mated.mating)
        assertNull(EngineEvaluation.parseInfo("info depth 9 multipv 2 score cp 10 pv e2e4", true))
        assertNull(EngineEvaluation.parseInfo("info depth 9 score cp 10 lowerbound pv e2e4", true))
        assertEquals(emptyList<String>(), EngineEvaluation.parseInfo("info depth 9 score cp 10", true)!!.pv)
    }
}
