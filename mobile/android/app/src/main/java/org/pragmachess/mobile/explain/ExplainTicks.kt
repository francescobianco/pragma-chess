package org.pragmachess.mobile.explain

import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.Square
import kotlin.math.abs

/**
 * The ticks of one move explained, as the desktop's ExplainTicks records
 * them (pragma-explain --record, PRAGMA_EXPLAIN_RECORD): what EXPLAIN.smart
 * was fed, line after line of the engine, and in `expect` lines what it must
 * end up showing. The records of smart/tests are replayed here as on the
 * desktop, so the two interpreters, and the chess they give the programs,
 * are held to the same results.
 */
data class ExplainTicks(
    val before: Position? = null,
    val played: Move? = null,
    val beforeEvaluation: EngineEvaluation? = null,
    val after: Position,
    /** Who asked (`viewer white`/`viewer black`), when known. */
    val viewer: Side? = null,
    val ticks: List<EngineEvaluation> = emptyList(),
    val expected: List<String> = emptyList(),
) {
    /** Feeds the ticks to EXPLAIN.smart as the app does: what each tick made. */
    fun replay(figurines: Boolean = false, text: ExplainText = ExplainText.English): List<ExplanationTick> {
        startExplanation()
        return ticks.map { tick ->
            explainTick(ExplanationInput(before = before, played = played, beforeEvaluation = beforeEvaluation,
                after = after, afterEvaluation = tick, viewer = viewer, figurines = figurines), text)
        }
    }

    companion object {
        /** "depth 20 cp 40 pv d2d4 g8f6", "depth 12 mate -3 pv …" (White's scores). */
        fun parseEvaluation(text: String): EngineEvaluation? {
            val words = text.trim().split(Regex("\\s+")).filter { it.isNotEmpty() }
            var evaluation = EngineEvaluation()
            var scored = false
            var i = 0
            while (i < words.size) {
                when (words[i]) {
                    "depth" -> evaluation = evaluation.copy(depth = words.getOrNull(++i)?.toIntOrNull() ?: return null)
                    "cp" -> {
                        evaluation = evaluation.copy(centipawns = words.getOrNull(++i)?.toIntOrNull() ?: return null)
                        scored = true
                    }
                    "mate" -> {
                        val moves = words.getOrNull(++i)?.toIntOrNull() ?: return null
                        evaluation = evaluation.copy(isMate = true, mateIn = abs(moves),
                            mating = if (moves < 0) Side.Black else Side.White)
                        scored = true
                    }
                    "pv" -> {
                        evaluation = evaluation.copy(pv = words.subList(i + 1, words.size).toList())
                        i = words.size
                    }
                    else -> return null
                }
                i++
            }
            return if (scored) evaluation else null
        }

        /** Every record of [text]; throws [IllegalArgumentException] ("line 3: …") on a mistake. */
        fun parse(text: String): List<ExplainTicks> {
            val records = ArrayList<ExplainTicks>()
            var open = false
            var before: Position? = null
            var after: Position? = null
            var played = ""
            var beforeEvaluation: EngineEvaluation? = null
            var viewer: Side? = null
            val ticks = ArrayList<EngineEvaluation>()
            val expected = ArrayList<String>()
            for ((n, raw) in text.split('\n').withIndex()) {
                val line = raw.trim()
                fun fail(message: String): Nothing = throw IllegalArgumentException("line ${n + 1}: $message")
                if (line.isEmpty() || line.startsWith("#")) continue
                val field = line.substringBefore(' ')
                val value = if (' ' in line) line.substringAfter(' ').trim() else ""
                if (field == "explain") {
                    if (open) fail("\"explain\" before the \"end\" of the record before")
                    open = true
                    before = null; after = null; played = ""; beforeEvaluation = null; viewer = null
                    ticks.clear(); expected.clear()
                    continue
                }
                if (!open) fail("\"$field\" outside a record (\"explain\" … \"end\")")
                when (field) {
                    "before" -> before = Position.fromFen(value) ?: fail("not a FEN: $value")
                    "after" -> after = Position.fromFen(value) ?: fail("not a FEN: $value")
                    "played" -> played = value
                    "viewer" -> viewer = when (value) {
                        "white" -> Side.White
                        "black" -> Side.Black
                        else -> fail("not a side: $value")
                    }
                    "before-eval" -> beforeEvaluation = parseEvaluation(value) ?: fail("not an evaluation: $value")
                    "tick" -> ticks += parseEvaluation(value) ?: fail("not an evaluation: $value")
                    "expect" -> expected += value
                    "end" -> {
                        val position = after ?: fail("a record without \"after\"")
                        var move: Move? = null
                        if (played.isNotEmpty()) {
                            val from = before ?: fail("\"played\" without \"before\"")
                            move = from.parseUci(played) ?: fail("$played is not a legal move before")
                        }
                        records += ExplainTicks(before, move, beforeEvaluation, position, viewer, ticks.toList(), expected.toList())
                        open = false
                    }
                    else -> fail("unknown field \"$field\"")
                }
            }
            if (open) throw IllegalArgumentException("the last record has no \"end\"")
            return records
        }

        /** The explanation shown last, if any tick showed one. */
        fun lastShown(ticks: List<ExplanationTick>): MoveExplanation? = ticks.lastOrNull { it.shown }?.explanation

        /** An explanation as the `expect` lines write it, as the desktop's outcome(). */
        fun outcome(explanation: MoveExplanation): List<String> {
            val verdicts = listOf("none", "best", "good", "inaccuracy", "mistake", "blunder")
            val kinds = listOf("refutation", "idea", "reply", "alternative", "threat", "plan")
            val arrows = explanation.arrows.joinToString(", ") { arrow ->
                Square.name(arrow.from) + Square.name(arrow.to) + " " + kinds[arrow.kind.ordinal] +
                    if (arrow.step > 0) " ${arrow.step}" else ""
            }
            val lines = arrayListOf("verdict " + verdicts[explanation.verdict.ordinal], "arrows " + arrows.ifEmpty { "-" })
            if (explanation.lostPieces.isNotEmpty()) lines += "lost " + explanation.lostPieces.joinToString(", ") { Square.name(it) }
            if (explanation.threatenedPieces.isNotEmpty())
                lines += "threatened " + explanation.threatenedPieces.joinToString(", ") { Square.name(it) }
            lines += "summary " + explanation.summary
            if (explanation.playback.isNotEmpty()) lines += "playback " + explanation.playback.joinToString(" ")
            return lines
        }
    }
}
