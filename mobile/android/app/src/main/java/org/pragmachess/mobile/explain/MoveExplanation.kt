package org.pragmachess.mobile.explain

import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Piece
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.figurineSan
import kotlin.math.abs
import kotlin.math.max
import kotlin.math.min

// A port of the desktop's MoveExplanation (gui/qt/src/app/MoveExplanation.cpp):
// the same rules, thresholds and texts, so the phone explains a move as the
// desktop and pragma-explain do. Tuning happens on the desktop
// (docs/tech/explain-tuning.md); bring its changes here.

/** An arrow drawn on the board to explain a line. */
data class BoardArrow(
    val from: Int,
    val to: Int,
    val kind: Kind,
    /** Position in the line, starting at 1; 0 for arrows outside a sequence. */
    val step: Int = 0,
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
    /** From [AdvantageProbe]: the ply of the line from which a shallow search agrees with the deep one. */
    val concretePly: Int? = null,
    /** Why [afterEvaluation] is not the explanation's own search, for the trace. */
    val evaluationNote: String = "",
    /** Moves in the summary with figurines (♘f3) or letters (Nf3). */
    val figurines: Boolean = false,
    val trace: Boolean = false,
)

private const val MAX_SEARCH_PLIES = 16
private const val HOLD_PLIES = 4
private const val MAX_ARROWS = 8
private const val MAX_MATE_PLIES = 100
private const val FOCUS_ABOVE = 3
private const val FOCUS_PLIES = 2
private const val MINIMUM_MATERIAL = 80
private const val REALIZED_SHARE = 0.4
private const val MAX_SWING = 1500

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

/** A principal variation replayed: positions[k] is the position before moves[k], the last follows the last move. */
private class Line(val moves: List<Move>, val positions: List<Position>) {
    val plies: Int get() = moves.size
}

private fun replay(start: Position, uciMoves: List<String>, maxPlies: Int): Line {
    val moves = ArrayList<Move>()
    val positions = arrayListOf(start)
    for (uci in uciMoves) {
        if (moves.size >= maxPlies) break
        val position = positions.last()
        val move = position.parseUci(uci) ?: break
        moves += move
        positions += position.play(move)
    }
    return Line(moves, positions)
}

/** Whether the exchanges are over at ply [k]: no check to answer and no capture or promotion coming next. */
private fun isSettled(line: Line, k: Int): Boolean {
    val position = line.positions[k]
    if (position.isCheck) return position.legalMoves().isEmpty()
    if (k < line.plies) {
        val next = line.moves[k]
        if (position.capturedPiece(next) != Piece.NONE || next.promotion != Piece.NONE) return false
    }
    return true
}

private class Explaining(private val text: ExplainText, val input: ExplanationInput) {
    val trace: MutableList<String>? = if (input.trace) ArrayList() else null
    val arrows = ArrayList<BoardArrow>()
    val lostPieces = ArrayList<Int>()

    fun tr(source: String) = text.tr(source)
    fun note(line: String) {
        trace?.add(line)
    }

    fun sideName(side: Side) = if (side == Side.White) tr("White") else tr("Black")

    /**
     * First ply from [firstPly] at which [beneficiary] is up at least [needed]
     * centipawns of material compared with [baseline], the exchanges are over,
     * and the gain holds for the next few plies of the line.
     */
    fun findRealization(line: Line, firstPly: Int, beneficiary: Side, baseline: Int, needed: Int): Int? {
        fun gainAt(k: Int) = (line.positions[k].material() - baseline) * signFor(beneficiary)
        note("  material for ${sideName(beneficiary)}, needs +$needed cp from ply $firstPly (settled and held $HOLD_PLIES plies):")
        var found: Int? = null
        var k = firstPly
        while (k <= line.plies && found == null) {
            val settled = isSettled(line, k)
            var holds = gainAt(k) >= needed && settled
            var later = k + 1
            while (holds && later <= min(line.plies, k + HOLD_PLIES)) {
                holds = gainAt(later) >= needed
                later++
            }
            if (trace != null) {
                val move = line.positions[k - 1].moveNumberText() + line.positions[k - 1].san(line.moves[k - 1])
                val gain = gainAt(k)
                trace += "    ply ${"%2d".format(k)} ${move.padEnd(10)} gain ${if (gain >= 0) "+" else ""}$gain" +
                    if (holds) "  <- realized" else if (settled) "" else "  (not settled)"
            }
            if (holds) found = k
            k++
        }
        if (found == null) note("    not realized within ${line.plies} plies")
        return found
    }

    /**
     * Adds arrows for moves [from, to) of [line], or only for [focusFrom, focusTo)
     * when given. Moves of [ideaSide] get [ideaKind], the others are replies.
     * Pieces taken by [ideaSide] in the drawn moves are traced back to where
     * they stand on the board at ply [from].
     */
    fun addArrows(line: Line, from: Int, to: Int, ideaSide: Side, ideaKind: BoardArrow.Kind, focus: Pair<Int, Int>? = null) {
        val (focusFrom, focusTo) = focus ?: (from to to)
        val origin = IntArray(64) { square -> if (line.positions[from].pieceAt(square) == Piece.NONE) -1 else square }
        var step = 1
        var k = from
        while (k < min(to, line.plies) && step <= MAX_ARROWS) {
            val position = line.positions[k]
            val move = line.moves[k]
            val side = position.sideToMove
            val drawn = k in focusFrom until focusTo
            if (drawn) arrows += BoardArrow(move.from, move.to, if (side == ideaSide) ideaKind else BoardArrow.Kind.Reply, step++)

            if (position.capturedPiece(move) != Piece.NONE) {
                val square = if (position.pieceAt(move.to) == Piece.NONE) (move.from / 8) * 8 + move.to % 8 else move.to
                if (drawn && side == ideaSide && origin[square] >= 0 && origin[square] !in lostPieces) lostPieces += origin[square]
                origin[square] = -1
            }
            val fileDelta = move.to % 8 - move.from % 8
            if (Piece.type(position.pieceAt(move.from)) == Piece.KING && abs(fileDelta) == 2) {
                val rank = move.from / 8
                val rookFrom = rank * 8 + if (fileDelta > 0) 7 else 0
                val rookTo = rank * 8 + if (fileDelta > 0) 5 else 3
                origin[rookTo] = origin[rookFrom]
                origin[rookFrom] = -1
            }
            origin[move.to] = origin[move.from]
            origin[move.from] = -1
            k++
        }
    }

    /**
     * Moves to draw for a realization [from, to): all of them when short,
     * otherwise the move with the biggest material jump for [beneficiary] (the
     * latest on ties), preceded by the move before it only when that move is
     * part of the tactic.
     */
    fun focusWindow(line: Line, from: Int, to: Int, beneficiary: Side): Pair<Int, Int> {
        if (to - from <= FOCUS_ABOVE) return from to to
        var decisive = from
        var biggest = Int.MIN_VALUE
        for (k in from until to) {
            val jump = (line.positions[k + 1].material() - line.positions[k].material()) * signFor(beneficiary)
            if (jump >= biggest) {
                biggest = jump
                decisive = k
            }
        }
        var focusFrom = decisive
        if (decisive > from) {
            val previousPosition = line.positions[decisive - 1]
            val previous = line.moves[decisive - 1]
            val capture = line.moves[decisive]
            val related = previousPosition.capturedPiece(previous) != Piece.NONE ||
                line.positions[decisive].isCheck || previous.to == capture.to ||
                isBetween(previous.from, capture.from, capture.to)
            if (related) focusFrom = max(from, decisive + 1 - FOCUS_PLIES)
        }
        note("  focus: ${to - from} plies are too many arrows; biggest jump +$biggest at " +
            line.positions[decisive].moveNumberText() + line.positions[decisive].san(line.moves[decisive]) +
            ", drawing plies ${focusFrom + 1}-${decisive + 1}")
        return focusFrom to decisive + 1
    }

    fun countedPiece(type: Int, count: Int): String = when (type) {
        Piece.PAWN -> if (count == 1) tr("a pawn") else if (count == 2) tr("two pawns") else tr("%1 pawns").args(count)
        Piece.KNIGHT -> if (count == 1) tr("a knight") else tr("%1 knights").args(count)
        Piece.BISHOP -> if (count == 1) tr("a bishop") else tr("%1 bishops").args(count)
        Piece.ROOK -> if (count == 1) tr("a rook") else tr("%1 rooks").args(count)
        Piece.QUEEN -> if (count == 1) tr("the queen") else tr("%1 queens").args(count)
        else -> ""
    }

    fun joinPieces(counts: IntArray): String {
        val parts = ArrayList<String>()
        for (type in intArrayOf(Piece.QUEEN, Piece.ROOK, Piece.BISHOP, Piece.KNIGHT, Piece.PAWN)) {
            if (counts[type] > 0) parts += countedPiece(type, counts[type])
        }
        if (parts.size <= 1) return parts.firstOrNull().orEmpty()
        val last = parts.removeAt(parts.size - 1)
        return tr("%1 and %2").args(parts.joinToString(", "), last)
    }

    /** "wins a knight", "wins the exchange", "wins a rook for a pawn". */
    fun materialPhrase(from: Position, to: Position, beneficiary: Side): String {
        fun count(position: Position, side: Side, type: Int) = (0 until 64).count { position.pieceAt(it) == Piece.of(type, side) }
        val won = IntArray(7)
        val given = IntArray(7)
        val loser = beneficiary.opponent
        for (type in intArrayOf(Piece.PAWN, Piece.KNIGHT, Piece.BISHOP, Piece.ROOK, Piece.QUEEN)) {
            val lostByLoser = count(from, loser, type) - count(to, loser, type)
            val lostByBeneficiary = count(from, beneficiary, type) - count(to, beneficiary, type)
            val net = lostByLoser - lostByBeneficiary
            (if (net > 0) won else given)[type] = abs(net)
        }
        val minorsGiven = given[Piece.KNIGHT] + given[Piece.BISHOP]
        val onlyExchange = won.contentEquals(intArrayOf(0, 0, 0, 0, 1, 0, 0)) && minorsGiven == 1 &&
            given[Piece.PAWN] == 0 && given[Piece.ROOK] == 0 && given[Piece.QUEEN] == 0
        if (onlyExchange) return tr("wins the exchange")
        val gains = joinPieces(won)
        if (gains.isEmpty()) return tr("wins material")
        val losses = joinPieces(given)
        return if (losses.isEmpty()) tr("wins %1").args(gains) else tr("wins %1 for %2").args(gains, losses)
    }

    fun verdictName(verdict: MoveExplanation.Verdict): String = when (verdict) {
        MoveExplanation.Verdict.Best -> tr("Best move")
        MoveExplanation.Verdict.Good -> tr("Good move")
        MoveExplanation.Verdict.Inaccuracy -> tr("Inaccuracy")
        MoveExplanation.Verdict.Mistake -> tr("Mistake")
        MoveExplanation.Verdict.Blunder -> tr("Blunder")
        MoveExplanation.Verdict.None -> ""
    }

    fun assessment(evaluation: EngineEvaluation): String {
        val score = abs(evaluation.centipawns)
        val side = sideName(if (evaluation.centipawns >= 0) Side.White else Side.Black)
        return when {
            score >= 300 -> tr("%1 is winning.").args(side)
            score >= 150 -> tr("%1 is better.").args(side)
            score >= 50 -> tr("%1 is slightly better.").args(side)
            else -> tr("The position is balanced.")
        }
    }

    fun line(position: Position, moves: List<String>, plies: Int) = position.lineText(moves, plies, input.figurines)
}

/** Whether [square] lies strictly between [from] and [to] on a line or diagonal. */
private fun isBetween(square: Int, from: Int, to: Int): Boolean {
    val fileStep = Integer.signum(to % 8 - from % 8)
    val rankStep = Integer.signum(to / 8 - from / 8)
    val aligned = from % 8 == to % 8 || from / 8 == to / 8 || abs(to % 8 - from % 8) == abs(to / 8 - from / 8)
    if (!aligned || from == to) return false
    var s = from + fileStep + 8 * rankStep
    while (s != to) {
        if (s == square) return true
        s += fileStep + 8 * rankStep
    }
    return false
}

/** The moves of [line] when they end in checkmate, to be played on the board. */
private fun mateToPlay(line: Line): List<String> =
    if (line.plies == 0 || !line.positions.last().isCheckmate) emptyList() else line.moves.map { it.uci }

private fun isError(verdict: MoveExplanation.Verdict) =
    verdict == MoveExplanation.Verdict.Inaccuracy || verdict == MoveExplanation.Verdict.Mistake || verdict == MoveExplanation.Verdict.Blunder

/** Classifies [played] by how much of its side's expected share of the game it gave away (lichess's scale). */
fun classifyMove(before: EngineEvaluation, after: EngineEvaluation, mover: Side, played: Move): MoveExplanation.Verdict {
    if (before.pv.isNotEmpty() && before.pv.first() == played.uci) return MoveExplanation.Verdict.Best
    val drop = 100.0 * (before.shareFor(mover) - after.shareFor(mover))
    return when {
        drop >= 30 -> MoveExplanation.Verdict.Blunder
        drop >= 20 -> MoveExplanation.Verdict.Mistake
        drop >= 10 -> MoveExplanation.Verdict.Inaccuracy
        else -> MoveExplanation.Verdict.Good
    }
}

/**
 * Explains the evaluation of a position by comparing it with the position
 * before the last move. The engine's principal variation is replayed until the
 * evaluation turns into something concrete on the board — material won after
 * the exchanges settle, or a mate — and the moves up to that point become the
 * arrows. When the last move threw away the advantage without losing
 * material, the better move's line is shown.
 */
fun explainPosition(input: ExplanationInput, text: ExplainText = ExplainText.English): MoveExplanation {
    val e = Explaining(text, input)
    val after = input.after
    val afterEvaluation = input.afterEvaluation

    fun result(verdict: MoveExplanation.Verdict, summary: String, playback: List<String> = emptyList()) =
        MoveExplanation(verdict, e.arrows.toList(), e.lostPieces.toList(), summary, playback, e.trace.orEmpty())

    if (after.isCheckmate) return result(MoveExplanation.Verdict.None, e.tr("Checkmate."))
    if (after.isStalemate) return result(MoveExplanation.Verdict.None, e.tr("Stalemate."))

    val before = input.before
    val played = input.played
    val beforeEvaluation = input.beforeEvaluation
    val comparable = before != null && played != null && beforeEvaluation != null
    val toMove = after.sideToMove
    val mover = toMove.opponent
    if (input.evaluationNote.isNotEmpty()) e.note(input.evaluationNote)
    var verdict = MoveExplanation.Verdict.None
    if (comparable) {
        verdict = classifyMove(beforeEvaluation!!, afterEvaluation, mover, played!!)
        e.note("verdict ${e.verdictName(verdict)}: ${e.sideName(mover)}'s winning chances " +
            "%.1f -> %.1f (drop %.1f)".format(java.util.Locale.ROOT, 100 * beforeEvaluation.shareFor(mover),
                100 * afterEvaluation.shareFor(mover), 100 * (beforeEvaluation.shareFor(mover) - afterEvaluation.shareFor(mover))))
    } else {
        e.note("no previous evaluation: explaining the position alone")
    }
    // Moves of the principal variation to show when nothing more concrete is found.
    val concretePly = input.concretePly ?: 0
    val fallbackPlies = if (concretePly > 0) min(concretePly, MAX_ARROWS) else 2
    // Only the branches that found neither material nor a mate say this, so it
    // must not promise a win that never comes.
    val concreteText = if (concretePly > 0)
        e.tr(" No material explains it: the assessment is positional, clear after %1.").args(e.line(after, afterEvaluation.pv, concretePly))
    else ""
    input.concretePly?.let { e.note("line probe: concrete at ply $it") }

    val prefix = when {
        isError(verdict) -> e.tr("%1 (%2 → %3). ").args(e.verdictName(verdict), beforeEvaluation!!.text, afterEvaluation.text)
        verdict != MoveExplanation.Verdict.None -> e.tr("%1 (%2). ").args(e.verdictName(verdict), afterEvaluation.text)
        else -> afterEvaluation.text + ". "
    }

    val current = replay(after, afterEvaluation.pv, MAX_SEARCH_PLIES)

    if (isError(verdict)) {
        before!!
        beforeEvaluation!!
        played!!
        val best = replay(before, beforeEvaluation.pv, MAX_SEARCH_PLIES)
        val betterExists = best.plies > 0 && best.moves.first() != played
        val better = if (betterExists) e.tr(" Better was %1.").args(e.line(before, beforeEvaluation.pv, 1)) else ""
        fun addAlternative() {
            if (betterExists) e.arrows += BoardArrow(best.moves.first().from, best.moves.first().to, BoardArrow.Kind.Alternative, 0)
        }

        // The move allows a mate.
        if (afterEvaluation.isMate && afterEvaluation.mating == toMove && afterEvaluation.mateIn > 0) {
            e.note("branch: the move allows mate")
            addAlternative()
            e.addArrows(current, 0, current.plies, toMove, BoardArrow.Kind.Refutation)
            return result(verdict, prefix +
                e.tr("%1 mates in %2: %3.").args(e.sideName(toMove), afterEvaluation.mateIn, e.line(after, afterEvaluation.pv, MAX_ARROWS)) +
                better, mateToPlay(replay(after, afterEvaluation.pv, MAX_MATE_PLIES)))
        }

        // The move loses material: replayed together with the refutation, so
        // that a capture made by the move itself counts against what follows.
        val drop = min(beforeEvaluation.centipawnsFor(mover) - afterEvaluation.centipawnsFor(mover), MAX_SWING)
        e.note("refutation: evaluation drop $drop cp (minimum $MINIMUM_MATERIAL)")
        if (drop >= MINIMUM_MATERIAL) {
            val refutation = listOf(played.uci) + afterEvaluation.pv
            val line = replay(before, refutation, MAX_SEARCH_PLIES + 1)
            val needed = max(MINIMUM_MATERIAL, (drop * REALIZED_SHARE).toInt())
            val ply = e.findRealization(line, 2, toMove, before.material(), needed)
            if (ply != null) {
                e.note("branch: the move loses material")
                val focus = e.focusWindow(line, 1, ply, toMove)
                // A focused refutation is clearer without the better move on top.
                if (focus.first == 1) addAlternative()
                e.addArrows(line, 1, ply, toMove, BoardArrow.Kind.Refutation, focus)
                return result(verdict, prefix +
                    e.tr("%1 %2: %3.").args(e.sideName(toMove), e.materialPhrase(before, line.positions[ply], toMove),
                        e.line(after, afterEvaluation.pv, ply - 1)) + better)
            }
        }

        // The move misses a mate or a win of material.
        if (betterExists && beforeEvaluation.isMate && beforeEvaluation.mating == mover) {
            e.note("branch: the move misses a mate")
            addAlternative()
            return result(verdict, prefix +
                e.tr("Missed mate in %1: %2.").args(beforeEvaluation.mateIn, e.line(before, beforeEvaluation.pv, MAX_ARROWS)))
        }
        val missed = min(beforeEvaluation.centipawnsFor(mover) - before.material() * signFor(mover), MAX_SWING)
        e.note("missed win: $missed cp above the material before the move")
        if (betterExists && missed >= MINIMUM_MATERIAL) {
            val needed = max(MINIMUM_MATERIAL, (missed * REALIZED_SHARE).toInt())
            val ply = e.findRealization(best, 1, mover, before.material(), needed)
            if (ply != null) {
                e.note("branch: the move misses a win of material")
                addAlternative()
                return result(verdict, prefix +
                    e.tr("Missed: %1 %2.").args(e.line(before, beforeEvaluation.pv, ply), e.materialPhrase(before, best.positions[ply], mover)))
            }
        }

        // Nothing concrete within reach: show how the opponent takes over.
        e.note("branch: no material or mate, showing $fallbackPlies plies of the line")
        addAlternative()
        // Not a refutation: nothing is won here, so the opponent's continuation
        // is drawn as a reply. Red is the colour of material falling.
        e.addArrows(current, 0, fallbackPlies, toMove, BoardArrow.Kind.Reply)
        var summary = prefix + better.trim()
        if (concreteText.isNotEmpty()) summary += concreteText
        else if (current.plies > 0) summary += e.tr(" Main line: %1.").args(e.line(after, afterEvaluation.pv, 4))
        return result(verdict, summary.trim())
    }

    // Not a mistake: explain why the evaluation is what it is.
    val favored = if (afterEvaluation.isMate) afterEvaluation.mating
    else if (afterEvaluation.centipawns >= 0) Side.White else Side.Black
    // A line won by the opponent of the side that just moved reads as a threat.
    val favoredKind = if (comparable && favored != mover) BoardArrow.Kind.Refutation else BoardArrow.Kind.Idea
    if (afterEvaluation.isMate && afterEvaluation.mateIn > 0) {
        e.note("branch: mate on the board")
        e.addArrows(current, 0, current.plies, favored, favoredKind)
        return result(verdict, prefix +
            e.tr("%1 mates in %2: %3.").args(e.sideName(favored), afterEvaluation.mateIn, e.line(after, afterEvaluation.pv, MAX_ARROWS)),
            mateToPlay(replay(after, afterEvaluation.pv, MAX_MATE_PLIES)))
    }

    val score = min(afterEvaluation.centipawnsFor(favored), MAX_SWING)

    // The last move itself won (or starts winning) material.
    if (comparable && favored == mover) {
        before!!
        val extra = score - before.material() * signFor(favored)
        e.note("won by the move: $extra cp above the material before the move")
        if (extra >= MINIMUM_MATERIAL) {
            val moves = listOf(played!!.uci) + afterEvaluation.pv
            val line = replay(before, moves, MAX_SEARCH_PLIES + 1)
            val needed = max(MINIMUM_MATERIAL, (extra * REALIZED_SHARE).toInt())
            val ply = e.findRealization(line, 1, favored, before.material(), needed)
            if (ply != null) {
                e.note("branch: the move wins material")
                val focus = e.focusWindow(line, 1, ply, favored)
                e.addArrows(line, 1, ply, favored, BoardArrow.Kind.Idea, focus)
                return result(verdict, prefix +
                    e.tr("%1 %2: %3.").args(e.sideName(favored), e.materialPhrase(before, line.positions[ply], favored), e.line(before, moves, ply)))
            }
        }
    }

    // Material still to be won in the principal variation.
    val extra = score - after.material() * signFor(favored)
    e.note("still to win: $extra cp above the material on the board")
    if (extra >= MINIMUM_MATERIAL) {
        val needed = max(MINIMUM_MATERIAL, (extra * REALIZED_SHARE).toInt())
        val ply = e.findRealization(current, 1, favored, after.material(), needed)
        if (ply != null) {
            e.note("branch: material won in the line")
            val focus = e.focusWindow(current, 0, ply, favored)
            e.addArrows(current, 0, ply, favored, favoredKind, focus)
            return result(verdict, prefix +
                e.tr("%1 %2: %3.").args(e.sideName(favored), e.materialPhrase(after, current.positions[ply], favored),
                    e.line(after, afterEvaluation.pv, ply)))
        }
    }

    e.note("branch: no material or mate, showing $fallbackPlies plies of the line")
    e.addArrows(current, 0, fallbackPlies, toMove, BoardArrow.Kind.Idea)
    var summary = prefix + e.assessment(afterEvaluation)
    if (concreteText.isNotEmpty()) summary += concreteText
    else if (current.plies > 0) summary += e.tr(" Main line: %1.").args(e.line(after, afterEvaluation.pv, 4))
    return result(verdict, summary)
}
