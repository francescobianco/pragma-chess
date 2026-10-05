package org.pragmachess.mobile.engine

import kotlin.math.abs
import kotlin.math.exp

/** An engine evaluation from White's point of view, as on the desktop. */
data class Analysis(
    val depth: Int,
    /** Centipawns for White when [mate] is null. */
    val centipawns: Int,
    /** Moves to mate, positive when White mates, negative when Black does. */
    val mate: Int?,
    /** Principal variation in UCI, from the analysed position. */
    val pv: List<String>,
    /** The position analysed, so a line that arrives late is not taken for the next one's. */
    val fen: String = "",
) {
    /** Share of the bar for White: the winning-chances curve the desktop uses. */
    val whiteShare: Float
        get() {
            if (mate != null) return if (mate > 0) 1f else if (mate < 0) 0f else 0.5f
            val chances = 2.0 / (1.0 + exp(-0.00368208 * centipawns)) - 1.0
            return (0.5 + 0.5 * chances).toFloat()
        }

    val whiteAhead: Boolean get() = if (mate != null) mate > 0 else centipawns >= 0

    /** "+1.3", "−0.4", "M3", "0.0", like the desktop's EngineEvaluation::text. */
    val text: String
        get() {
            if (mate != null) return if (mate == 0) "#" else "M${abs(mate)}"
            val pawns = centipawns / 100.0
            val number = if (abs(pawns) >= 10) "%.0f".format(java.util.Locale.ROOT, abs(pawns))
            else "%.1f".format(java.util.Locale.ROOT, abs(pawns))
            if (number == "0.0") return number
            return (if (pawns > 0) "+" else "−") + number
        }

    companion object {
        /**
         * Reads a UCI "info" line; [whiteToMove] turns the engine's
         * side-to-move score into White's. Null for lines without score or pv.
         */
        fun parseInfo(line: String, whiteToMove: Boolean): Analysis? {
            val tokens = line.trim().split(Regex("\\s+"))
            if (tokens.firstOrNull() != "info") return null
            var depth = 0
            var cp: Int? = null
            var mate: Int? = null
            var pv: List<String> = emptyList()
            var bound = false
            var multipv = 1
            var i = 1
            while (i < tokens.size) {
                when (tokens[i]) {
                    "depth" -> depth = tokens.getOrNull(++i)?.toIntOrNull() ?: 0
                    "multipv" -> multipv = tokens.getOrNull(++i)?.toIntOrNull() ?: 1
                    "score" -> {
                        when (tokens.getOrNull(++i)) {
                            "cp" -> cp = tokens.getOrNull(++i)?.toIntOrNull()
                            "mate" -> mate = tokens.getOrNull(++i)?.toIntOrNull()
                        }
                    }
                    "lowerbound", "upperbound" -> bound = true
                    "pv" -> {
                        pv = tokens.subList(i + 1, tokens.size)
                        i = tokens.size
                    }
                }
                i++
            }
            if (multipv != 1 || bound || pv.isEmpty() || (cp == null && mate == null)) return null
            val sign = if (whiteToMove) 1 else -1
            return Analysis(depth, (cp ?: 0) * sign, mate?.let { it * sign }, pv)
        }
    }
}
