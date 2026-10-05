package org.pragmachess.mobile.explain

import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Piece
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.figurineSan
import org.pragmachess.mobile.smart.SmartChess
import org.pragmachess.mobile.smart.SmartProgram
import org.pragmachess.mobile.smart.SmartPrograms
import org.pragmachess.mobile.smart.SmartValue

// Explain on the phone: the explanation is smart/EXPLAIN.smart's (and the
// verdict smart/TUTOR.smart's), the SMART programs the desktop and
// pragma-explain run too, so the phone explains a move as they do. This file
// only holds the types and feeds the programs; tuning happens in the
// programs (docs/tech/explain-tuning.md).

/** An arrow drawn on the board to explain a line. */
data class BoardArrow(
    val from: Int,
    val to: Int,
    val kind: Kind,
    /** Position in the line, starting at 1; 0 for arrows outside a sequence. */
    val step: Int = 0,
    /**
     * The piece the arrow moves ([Piece] code), drawn small and faint where it
     * goes: given for the better move, which starts from the position before
     * the one on the board, where that piece may no longer stand.
     */
    val piece: Int = Piece.NONE,
) {
    enum class Kind {
        /** A move of the side punishing a mistake: material is falling (red). */
        Refutation,
        /** A move of the side whose idea is being shown (green). */
        Idea,
        /** A reply of the other side within the line (blue). */
        Reply,
        /** The move that should have been played instead, from the previous position (dashed green). */
        Alternative,
        /** A capture threatened, not played: a piece left attacked (dashed red). */
        Threat,
    }
}

/** What Explain shows: arrows for the moves that justify the evaluation, the pieces that fall, and a summary. */
data class MoveExplanation(
    val verdict: Verdict = Verdict.None,
    val arrows: List<BoardArrow> = emptyList(),
    /** Squares of pieces lost along the explained line. */
    val lostPieces: List<Int> = emptyList(),
    /** "Blunder (+0.3 → −2.9). Black wins a knight: 14…Bxf2+ 15.Kxf2 Ng4+". */
    val summary: String = "",
    /** Moves (UCI) that demonstrate the explanation from the position shown: a forced mate, to the end. */
    val playback: List<String> = emptyList(),
    /** How the explanation was reached, when [ExplanationInput.trace] is set. */
    val trace: List<String> = emptyList(),
) {
    enum class Verdict { None, Best, Good, Inaccuracy, Mistake, Blunder }
}

data class ExplanationInput(
    /** Position before the last move and the move itself; null at the start of a game. */
    val before: Position? = null,
    val played: Move? = null,
    /** Evaluation of [before], when known. */
    val beforeEvaluation: EngineEvaluation? = null,
    /** Position on the board and its evaluation. */
    val after: Position,
    val afterEvaluation: EngineEvaluation,
    /** The ply of the line from which a shallow search agrees with the deep one, if a probe found it. */
    val concretePly: Int? = null,
    /** Why [afterEvaluation] is not the explanation's own search, for the trace. */
    val evaluationNote: String = "",
    /** Moves in the summary with figurines (♘f3) or letters (Nf3). */
    val figurines: Boolean = false,
    val trace: Boolean = false,
)


private fun signFor(side: Side) = if (side == Side.White) 1 else -1

private fun pieceValue(type: Int) = when (type) {
    Piece.PAWN -> 100
    Piece.KNIGHT, Piece.BISHOP -> 300
    Piece.ROOK -> 500
    Piece.QUEEN -> 900
    else -> 0
}

/** Material balance in centipawns, positive when White is ahead. */
fun Position.material(): Int {
    var total = 0
    for (square in 0 until 64) {
        val piece = pieceAt(square)
        if (piece != Piece.NONE) total += signFor(Piece.side(piece)) * pieceValue(Piece.type(piece))
    }
    return total
}

/** The piece [move] takes, en passant included, or [Piece.NONE]. */
fun Position.capturedPiece(move: Move): Int {
    val mover = pieceAt(move.from)
    if (Piece.type(mover) == Piece.PAWN && move.to == enPassant && pieceAt(move.to) == Piece.NONE)
        return Piece.of(Piece.PAWN, Piece.side(mover).opponent)
    return pieceAt(move.to)
}

/** "12.♘f3 ♞c6 13.♗b5": [uciMoves] from this position, at most [maxPlies] (all when negative). */
fun Position.lineText(uciMoves: List<String>, maxPlies: Int = -1, figurines: Boolean = false): String {
    val parts = ArrayList<String>()
    var position = this
    for ((i, uci) in uciMoves.withIndex()) {
        if (maxPlies in 0..i) break
        val move = position.parseUci(uci) ?: break
        val san = if (figurines) figurineSan(position.san(move)) else position.san(move)
        parts += if (i == 0 || position.sideToMove == Side.White) position.moveNumberText() + san else san
        position = position.play(move)
    }
    return parts.joinToString(" ")
}

/** What one tick of the engine made of the explanation ([explainTick]). */
data class ExplanationTick(
    /** Whether [explanation] is to be shown; otherwise what was shown stays. */
    val shown: Boolean,
    val explanation: MoveExplanation,
    /** The program's mistake, if it stopped on one. */
    val error: String = "",
)

private fun verdictNamed(name: String) = when (name) {
    "best" -> MoveExplanation.Verdict.Best
    "good" -> MoveExplanation.Verdict.Good
    "inaccuracy" -> MoveExplanation.Verdict.Inaccuracy
    "mistake" -> MoveExplanation.Verdict.Mistake
    "blunder" -> MoveExplanation.Verdict.Blunder
    else -> MoveExplanation.Verdict.None
}

/**
 * Classifies [played] by how much of its side's expected share of the game it
 * gave away, or by the material it hands over from [start]: TUTOR.smart's Classify.
 */
fun classifyMove(before: EngineEvaluation, after: EngineEvaluation, mover: Side, played: Move,
                 start: Position? = null): MoveExplanation.Verdict {
    val tutor = SmartPrograms.program("TUTOR.smart") ?: return MoveExplanation.Verdict.None
    val verdict = tutor.interpreter.call("Classify", listOf(SmartChess.evaluation(before), SmartChess.evaluation(after),
        SmartChess.side(mover), SmartValue.Text(played.uci), start?.let { SmartChess.position(it) } ?: SmartValue.None))
    return verdict.fold({ verdictNamed(it.toText()).takeIf { v -> v != MoveExplanation.Verdict.None } ?: MoveExplanation.Verdict.Good },
        { System.err.println("SMART TUTOR.smart Classify: ${it.message}"); MoveExplanation.Verdict.None })
}

private fun explainArguments(input: ExplanationInput): List<SmartValue> {
    // The move is given even when its position before has no evaluation:
    // it cannot be judged, but the explanation is still about it.
    val moved = input.before != null && input.played != null
    return listOf(
        if (moved) SmartChess.position(input.before!!) else SmartValue.None,
        if (moved) SmartValue.Text(input.played!!.uci) else SmartValue.None,
        if (moved && input.beforeEvaluation != null) SmartChess.evaluation(input.beforeEvaluation) else SmartValue.None,
        SmartChess.position(input.after), SmartChess.evaluation(input.afterEvaluation),
    )
}

private fun prepare(program: SmartProgram, input: ExplanationInput, text: ExplainText) {
    program.output.clear()
    program.output.figurines = input.figurines
    program.output.trace = input.trace
    program.output.text = text
}

/**
 * Explains the evaluation of a position by comparing it with the position
 * before the last move: EXPLAIN.smart's Explain, whose comments say how.
 */
fun explainPosition(input: ExplanationInput, text: ExplainText = ExplainText.English): MoveExplanation {
    val explain = SmartPrograms.program("EXPLAIN.smart")
        ?: return MoveExplanation(summary = "Explain cannot run: its program has a mistake.")
    prepare(explain, input, text)
    val args = explainArguments(input) + listOf(
        input.concretePly?.let { SmartValue.of(it) } ?: SmartValue.None, SmartValue.Text(input.evaluationNote))
    return explain.interpreter.call("Explain", args).fold({ explain.output.explanation() },
        { MoveExplanation(summary = "Explain stopped on a mistake of its program: ${it.message}") })
}

/**
 * Explaining reacts to the engine: [startExplanation] when Explain turns to a
 * move, then [explainTick] for every line the engine reports on
 * [ExplanationInput.after], with the deepest evaluation known of the position
 * before. EXPLAIN.smart's Start and Tick, as on the desktop.
 */
fun startExplanation() {
    SmartPrograms.program("EXPLAIN.smart")?.interpreter?.call("Start")
}

fun explainTick(input: ExplanationInput, text: ExplainText = ExplainText.English): ExplanationTick {
    val explain = SmartPrograms.program("EXPLAIN.smart")
        ?: return ExplanationTick(true, MoveExplanation(summary = "Explain cannot run: its program has a mistake."),
            "EXPLAIN.smart cannot be loaded")
    prepare(explain, input, text)
    return explain.interpreter.call("Tick", explainArguments(input)).fold(
        { shown -> ExplanationTick(shown is SmartValue.Number && shown.value != 0.0, explain.output.explanation()) },
        { ExplanationTick(true, MoveExplanation(summary = "Explain stopped on a mistake of its program: ${it.message}"),
            it.message.orEmpty()) })
}
