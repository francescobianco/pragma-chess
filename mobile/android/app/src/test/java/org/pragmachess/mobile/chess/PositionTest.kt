package org.pragmachess.mobile.chess

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class PositionTest {
    private fun perft(position: Position, depth: Int): Long {
        if (depth == 0) return 1
        val moves = position.legalMoves()
        if (depth == 1) return moves.size.toLong()
        return moves.sumOf { perft(position.play(it), depth - 1) }
    }

    private fun perftOf(fen: String, depth: Int) = perft(Position.fromFen(fen)!!, depth)

    @Test
    fun perftStartingPosition() {
        assertEquals(20, perftOf(Position.STARTING_FEN, 1))
        assertEquals(400, perftOf(Position.STARTING_FEN, 2))
        assertEquals(8902, perftOf(Position.STARTING_FEN, 3))
        assertEquals(197281, perftOf(Position.STARTING_FEN, 4))
    }

    @Test
    fun perftKiwipete() {
        val fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
        assertEquals(48, perftOf(fen, 1))
        assertEquals(2039, perftOf(fen, 2))
        assertEquals(97862, perftOf(fen, 3))
    }

    @Test
    fun perftEndgameWithEnPassant() {
        val fen = "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1"
        assertEquals(14, perftOf(fen, 1))
        assertEquals(191, perftOf(fen, 2))
        assertEquals(2812, perftOf(fen, 3))
        assertEquals(43238, perftOf(fen, 4))
    }

    @Test
    fun perftPromotionsAndCastling() {
        val fen = "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1"
        assertEquals(6, perftOf(fen, 1))
        assertEquals(264, perftOf(fen, 2))
        assertEquals(9467, perftOf(fen, 3))
    }

    @Test
    fun perftPosition5() {
        val fen = "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
        assertEquals(44, perftOf(fen, 1))
        assertEquals(1486, perftOf(fen, 2))
        assertEquals(62379, perftOf(fen, 3))
    }

    @Test
    fun fenRoundTrip() {
        val fen = "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"
        assertEquals(fen, Position.fromFen(fen)!!.fen())
        assertEquals(Position.STARTING_FEN, Position.starting().fen())
    }

    @Test
    fun sanDisambiguationChecksAndMate() {
        val position = Position.fromFen("4k3/8/8/8/8/8/8/R3K2R w KQ - 0 1")!!
        assertEquals("O-O", position.san(Move(4, 6)))
        assertEquals("O-O-O", position.san(Move(4, 2)))
        assertEquals("Rb1", position.san(Move(0, 1)))
        assertEquals("Ra8+", position.san(Move(0, 56)))
        val knights = Position.fromFen("4k3/8/8/8/8/5N2/8/1N2K1N1 w - - 0 1")!!
        assertEquals("Nbd2", knights.san(Move(Square.parse("b1"), Square.parse("d2"))))
        val three = Position.fromFen("4k3/8/8/8/8/6N1/8/2N1K1N1 w - - 0 1")!!
        assertEquals("Ng1e2", three.san(Move(Square.parse("g1"), Square.parse("e2"))))
        assertEquals("N3e2", three.san(Move(Square.parse("g3"), Square.parse("e2"))))
        assertEquals("Nce2", three.san(Move(Square.parse("c1"), Square.parse("e2"))))
        val mate = GameLine.replay(null, "", "f3 e5 g4 Qh4")
        assertEquals("Qh4#", mate.moves.last().san)
        assertTrue(mate.last.isCheckmate)
        assertFalse(mate.last.isStalemate)
    }

    @Test
    fun parsesSanAndUci() {
        val start = Position.starting()
        assertEquals(Move(12, 28), start.parseSan("e4"))
        assertEquals(Move(6, 21), start.parseSan("Nf3!"))
        assertNull(start.parseSan("Nf4"))
        assertEquals(Move(12, 28), start.parseUci("e2e4"))
        assertNull(start.parseUci("e2e5"))
        val promotion = Position.fromFen("8/4P3/8/8/8/k7/8/4K3 w - - 0 1")!!
        assertEquals(Move(52, 60, Piece.QUEEN), promotion.parseUci("e7e8q"))
        assertEquals("e8=N", promotion.san(Move(52, 60, Piece.KNIGHT)))
    }

    @Test
    fun replaysNullMoves() {
        // A game written by hand passes ("--", UCI "0000"): the other side moves again.
        val line = GameLine.replay(null, "e2e4 0000 d2d4", "")
        assertEquals(3, line.plyCount)
        assertEquals("e4 -- d4", line.sanText)
        assertEquals("e2e4 0000 d2d4", line.uciText)
        assertEquals(Side.White, line.positionAt(2).sideToMove)
        assertEquals("rnbqkbnr/pppppppp/8/8/3PP3/8/PPP2PPP/RNBQKBNR b KQkq d3 0 2", line.last.fen())
        // Never in check: the line stops there.
        val checked = GameLine.replay(null, "e2e4 f7f6 d1h5 0000", "")
        assertEquals(3, checked.plyCount)
    }

    @Test
    fun replaysUciWithCastlingEnPassantAndPromotion() {
        // Castling both sides, en passant (exd6) and a promotion with capture (gxh8=Q).
        val uci = "e2e4 d7d5 e4e5 f7f5 e5f6 g8f6 g1f3 e7e6 f1d3 f8e7 e1g1 e8g8 " +
            "b1c3 b8c6 h2h3 d8e8 d1e2 e8h5 a2a3 a8b8"
        val line = GameLine.replay(null, uci, "")
        assertEquals(20, line.plyCount)
        assertEquals("exf6", line.moves[4].san)
        assertEquals("O-O", line.moves[10].san)
        assertEquals(Piece.ROOK, Piece.type(line.last.pieceAt(Square.parse("f1"))))
        assertEquals(Piece.KING, Piece.type(line.last.pieceAt(Square.parse("g8"))))
        assertEquals(Piece.NONE, line.last.pieceAt(Square.parse("f5")))

        val promo = GameLine.replay("7r/6P1/8/8/8/8/k7/4K3 w - - 0 1", "g7h8q", "")
        assertEquals("gxh8=Q", promo.moves.single().san)
        assertEquals(Piece.QUEEN, promo.last.pieceAt(Square.parse("h8")))

        val queenSide = GameLine.replay("r3k3/8/8/8/8/8/8/4K3 b q - 0 1", "e8c8", "")
        assertEquals("O-O-O", queenSide.moves.single().san)
        assertEquals(Piece.of(Piece.ROOK, Side.Black), queenSide.last.pieceAt(Square.parse("d8")))
    }

    @Test
    fun replayFallsBackToSanAndStopsAtBadMoves() {
        val line = GameLine.replay(null, "", "e4 e5 Nf3 Nc6 Bb5 a6 Zz9 d6")
        assertEquals(6, line.plyCount)
        assertNotNull(line.last)
        assertEquals("♘xe5+", figurineSan("Nxe5+"))
        assertEquals("e8=♕", figurineSan("e8=Q"))
    }
}
