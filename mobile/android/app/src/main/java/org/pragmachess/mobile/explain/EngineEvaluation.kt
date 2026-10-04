package org.pragmachess.mobile.explain

import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.engine.Analysis
import kotlin.math.abs
import kotlin.math.exp

/**
 * An evaluation reported by an engine, always from White's point of view: the
 * desktop's EngineEvaluation, which Explain is written against.
 */
data class EngineEvaluation(
    val isMate: Boolean = false,
    /** Centipawns, positive when White is better (when not [isMate]). */
    val centipawns: Int = 0,
    /** Moves to mate (when [isMate]); 0 means the position is already checkmate. */
    val mateIn: Int = 0,
    /** Side delivering mate (when [isMate]). */
    val mating: Side = Side.White,
    val depth: Int = 0,
    /** Principal variation in UCI notation. */
    val pv: List<String> = emptyList(),
) {
    /** Expected share of the game for White in [0, 1]: lichess's winning-chances curve. */
    val whiteShare: Double
        get() {
            if (isMate) return if (mating == Side.White) 1.0 else 0.0
            val winningChances = 2.0 / (1.0 + exp(-0.00368208 * centipawns)) - 1.0
            return 0.5 + 0.5 * winningChances
        }

    fun shareFor(side: Side): Double = if (side == Side.White) whiteShare else 1.0 - whiteShare

    /** Centipawns from [side]'s point of view; mates count as a large, bounded score. */
    fun centipawnsFor(side: Side): Int {
        // A closer mate is worth more; any mate outweighs every material balance.
        val white = if (isMate) (if (mating == Side.White) 1 else -1) * (10000 - 10 * mateIn) else centipawns
        return if (side == Side.White) white else -white
    }

    /** "+1.3", "−0.4", "M3", "#". */
    val text: String
        get() {
            if (isMate) return if (mateIn == 0) "#" else "M$mateIn"
            val pawns = centipawns / 100.0
            val number = if (abs(pawns) >= 10) "%.0f".format(java.util.Locale.ROOT, abs(pawns))
            else "%.1f".format(java.util.Locale.ROOT, abs(pawns))
            if (number == "0.0") return number
            return (if (pawns > 0) "+" else "−") + number
        }

    companion object {
        /**
         * Reads a UCI "info" line as the desktop's UciEngine does: the score
         * of the first line, turned to White's view, with or without a pv.
         * Null for lines without a score, other lines of a multipv search and
         * provisional scores (lowerbound, upperbound).
         */
        fun parseInfo(line: String, whiteToMove: Boolean): EngineEvaluation? {
            val tokens = line.trim().split(Regex("\\s+"))
            if (tokens.firstOrNull() != "info") return null
            var evaluation = EngineEvaluation()
            var hasScore = false
            var i = 1
            while (i < tokens.size) {
                when (tokens[i]) {
                    "depth" -> evaluation = evaluation.copy(depth = tokens.getOrNull(++i)?.toIntOrNull() ?: 0)
                    "multipv" -> if ((tokens.getOrNull(++i)?.toIntOrNull() ?: 1) != 1) return null
                    "score" -> {
                        val kind = tokens.getOrNull(++i)
                        val value = tokens.getOrNull(++i)?.toIntOrNull() ?: 0
                        if (kind == "cp") {
                            evaluation = evaluation.copy(centipawns = if (whiteToMove) value else -value)
                            hasScore = true
                        } else if (kind == "mate") {
                            // "mate 0": the side to move is checkmated.
                            val sideToMoveMates = value > 0
                            evaluation = evaluation.copy(isMate = true, mateIn = abs(value),
                                mating = if (sideToMoveMates == whiteToMove) Side.White else Side.Black)
                            hasScore = true
                        }
                    }
                    "lowerbound", "upperbound" -> return null
                    "pv" -> {
                        evaluation = evaluation.copy(pv = tokens.subList(i + 1, tokens.size).toList())
                        i = tokens.size
                    }
                    "string" -> return null
                }
                i++
            }
            return if (hasScore) evaluation else null
        }

        /** The live analysis of the app as an evaluation of a position with [sideToMove] to move. */
        fun of(analysis: Analysis, sideToMove: Side): EngineEvaluation {
            val mate = analysis.mate ?: return EngineEvaluation(centipawns = analysis.centipawns, depth = analysis.depth, pv = analysis.pv)
            val mating = when {
                mate > 0 -> Side.White
                mate < 0 -> Side.Black
                else -> sideToMove.opponent
            }
            return EngineEvaluation(isMate = true, mateIn = abs(mate), mating = mating, depth = analysis.depth, pv = analysis.pv)
        }
    }
}
