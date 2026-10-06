package org.pragmachess.mobile.explain

import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Square
import org.pragmachess.mobile.smart.SmartChess
import org.pragmachess.mobile.smart.SmartPrograms
import org.pragmachess.mobile.smart.SmartValue

/**
 * The plans of a line of the engine (smart/INSIGHT.smart), as the desktop's
 * LineInsight: the trips of the pieces that make one, drawn as arrows over
 * the position at the end of the line. A reading of the line only.
 */
data class LineInsight(
    val arrows: List<BoardArrow> = emptyList(),
    val trace: List<String> = emptyList(),
    /** The program's mistake, if it stopped on one (nothing is drawn then). */
    val error: String = "",
)

/** The insight of [line] (UCI moves) played from [start]. */
fun lineInsight(start: Position, line: List<String>, trace: Boolean = false): LineInsight {
    val program = SmartPrograms.program("INSIGHT.smart") ?: return LineInsight(error = "INSIGHT.smart cannot be loaded")
    program.output.clear()
    program.output.trace = trace
    val args = listOf(SmartChess.position(start), SmartValue.Items(line.map { SmartValue.Text(it) }))
    return program.interpreter.call("Insight", args).fold(
        { LineInsight(program.output.arrows.toList(), program.output.notes.toList()) },
        { LineInsight(trace = program.output.notes.toList(), error = it.message.orEmpty()) })
}

/**
 * A line with the arrows INSIGHT.smart must draw for it, a case of
 * smart/tests/ *.insight as the desktop's InsightCase reads it.
 */
data class InsightCase(
    val name: String,
    val start: Position,
    val line: List<String>,
    val expected: List<String>,
) {
    companion object {
        /** Every case of [text]; throws [IllegalArgumentException] ("line 3: …") on a mistake. */
        fun parse(text: String): List<InsightCase> {
            val cases = ArrayList<InsightCase>()
            var name: String? = null
            var start = Position.starting()
            var line = emptyList<String>()
            val expected = ArrayList<String>()
            for ((n, raw) in text.split('\n').withIndex()) {
                val row = raw.trim()
                fun fail(message: String): Nothing = throw IllegalArgumentException("line ${n + 1}: $message")
                if (row.isEmpty() || row.startsWith("#")) continue
                val field = row.substringBefore(' ')
                val value = if (' ' in row) row.substringAfter(' ').trim() else ""
                if (field == "insight") {
                    if (name != null) fail("\"insight\" before the \"end\" of the case before")
                    name = value
                    start = Position.starting()
                    line = emptyList()
                    expected.clear()
                    continue
                }
                if (name == null) fail("\"$field\" outside a case (\"insight\" … \"end\")")
                when (field) {
                    "fen" -> start = Position.fromFen(value) ?: fail("not a FEN: $value")
                    "line" -> line = value.split(Regex("\\s+")).filter { it.isNotEmpty() }
                    "expect" -> expected += value
                    "end" -> {
                        var position = start
                        for (uci in line)
                            position = position.play(position.parseUci(uci) ?: fail("$uci is not a legal move of the line"))
                        if (expected.isEmpty()) fail("a case without \"expect\" (\"expect -\": no arrow)")
                        cases += InsightCase(name, start, line, expected.toList())
                        name = null
                    }
                    else -> fail("unknown field \"$field\"")
                }
            }
            if (name != null) throw IllegalArgumentException("the last case has no \"end\"")
            return cases
        }

        /** The arrows as the `expect` lines write them: "b1g3 via d2 f1", or "-". */
        fun outcome(arrows: List<BoardArrow>): List<String> =
            arrows.map { arrow ->
                Square.name(arrow.from) + Square.name(arrow.to) +
                    if (arrow.via.isEmpty()) "" else " via " + arrow.via.joinToString(" ") { Square.name(it) }
            }.ifEmpty { listOf("-") }
    }
}
