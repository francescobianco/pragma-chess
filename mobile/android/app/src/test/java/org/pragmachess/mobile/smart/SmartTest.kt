package org.pragmachess.mobile.smart

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.pragmachess.mobile.explain.ExplainTicks
import org.pragmachess.mobile.explain.InsightCase
import org.pragmachess.mobile.explain.lineInsight
import java.io.File

/**
 * The SMART interpreter held to the desktop's: the same program and the same
 * expectations as tst_chessrules::runsSmartPrograms, and the records of
 * smart/tests replayed as tst_chessrules::replaysRecordedTicks does.
 */
class SmartTest {
    private val source = """
' Constants and memory: the top level runs once.
CONST LIMIT = 3
LET ticks = 0
said = []

FUNCTION Tick(depth)
    SHARED ticks
    ticks = ticks + 1
    local = depth * 2           ' a local: gone after the call
    IF depth >= LIMIT THEN
        Say "deep " + STR(depth)
    ELSEIF depth = 2 THEN
        Say "middle"
    ELSE
        Say "shallow"
    END IF
    RETURN ticks
END FUNCTION

FUNCTION Say(text)
    SHARED said
    said = said + [text]
END FUNCTION

FUNCTION Sum(list)
    total = 0
    FOR i = 0 TO LEN(list) - 1
        IF list[i] < 0 THEN EXIT FOR
        total = total + list[i]
    NEXT i
    RETURN total
END FUNCTION

FUNCTION Countdown(n)
    steps = []
    FOR k = n TO 1 STEP -1 : steps = steps + [k] : NEXT
    WHILE n > 0
        n = n - 1
        IF n = 1 THEN EXIT WHILE
    WEND
    RETURN [steps, n]
END FUNCTION

FUNCTION Copies()
    a = [1, 2, 3]
    b = a
    b[0] = 9            ' a list is a value
    RETURN a[0] * 10 + b[0]
END FUNCTION

FUNCTION Logic(x)
    IF x = NOTHING THEN RETURN "none"
    IF NOT (x > 1 AND x < 5) OR x = 10 THEN RETURN "out" ELSE RETURN "in"
END FUNCTION

FUNCTION Host()
    REM the client's functions are called like the program's
    Collect "a", 1
    Collect ("b"), 1 + 1
    CALL Collect("c", 3)
    RETURN Twice(21)
END FUNCTION
"""

    private fun number(value: Result<SmartValue>) = (value.getOrThrow() as SmartValue.Number).value

    /** The value of one expression, or its mistake, as the desktop test's `value`. */
    private fun value(expression: String): String {
        val script = try {
            SmartScript.parse("FUNCTION F()\nRETURN $expression\nEND FUNCTION")
        } catch (e: SmartError) {
            return "parse: line ${e.line}: ${e.message}"
        }
        val run = SmartInterpreter(script)
        run.load()
        return run.call("F").fold({ it.toText() }, { it.message.orEmpty() })
    }

    @Test
    fun runsPrograms() {
        val smart = SmartInterpreter(SmartScript.parse(source))
        val collected = ArrayList<String>()
        smart.define("collect") { args ->
            SmartInterpreter.expectArguments("COLLECT", args, 2)
            collected += (args[0] as SmartValue.Text).value + args[1].toText()
            SmartValue.None
        }
        smart.define("Twice") { args -> SmartValue.of(2 * SmartInterpreter.numberArgument("TWICE", args, 0)) }
        assertEquals(null, smart.load())

        assertEquals(1.0, number(smart.call("tick", listOf(SmartValue.of(1)))), 0.0)
        assertEquals(2.0, number(smart.call("Tick", listOf(SmartValue.of(2)))), 0.0)
        assertEquals(3.0, number(smart.call("TICK", listOf(SmartValue.of(5)))), 0.0)
        assertEquals("""["shallow", "middle", "deep 5"]""", smart.global("said").toText())
        assertEquals(SmartValue.None, smart.global("local"))

        assertEquals(3.0, number(smart.call("Sum", listOf(SmartValue.Items(listOf(1, 2, -1, 5).map { SmartValue.of(it) })))), 0.0)
        assertEquals("[[3, 2, 1], 1]", smart.call("Countdown", listOf(SmartValue.of(3))).getOrThrow().toText())
        assertEquals(19.0, number(smart.call("Copies")), 0.0)
        assertEquals("none", smart.call("Logic", listOf(SmartValue.None)).getOrThrow().toText())
        assertEquals("in", smart.call("Logic", listOf(SmartValue.of(3))).getOrThrow().toText())
        assertEquals("out", smart.call("Logic", listOf(SmartValue.of(10))).getOrThrow().toText())
        assertEquals(42.0, number(smart.call("Host")), 0.0)
        assertEquals(listOf("a1", "b2", "c3"), collected)

        assertEquals("3.5|0.333333|-2.5", value("STR(7 / 2) + \"|\" + STR(1 / 3) + \"|\" + STR(-2.50)"))
        assertEquals("[-2, 3, 2, 8, -1, 1]", value("[INT(-2.7), ABS(-3), MIN(4, 2, 8), MAX(4, 2, 8), -7 MOD 3, 7 MOD -3]"))
        assertEquals("[3, 2, 1, 1]", value("[LEN(\"abc\"), LEN([1, [2, 3]]), CONTAINS([1, 2], 2), INDEXOF([\"a\", \"b\"], \"b\")]"))
        assertEquals("[0, 1, 2]", value("SLICE(REPEAT(0, 3) + [1, 2], 2, 10)"))
        // -0 is 0, in lists too, as in C++ (Double.equals tells them apart).
        assertEquals("[1, 1, 1]", value("[-0 = 0, [-0, 1] = [0, 1], CONTAINS([-0], 0)]"))
        assertEquals("""["2.5", "7.00", "a b"]""", value("[FIXED(2.5, 1), FIXED(7, 2), TRIM(\"  a b \")]"))
        assertEquals("""["2.3", "-2.3", "0.0", "0.13", "3"]""",
            value("[FIXED(2.25, 1), FIXED(-2.25, 1), FIXED(-0.04, 1), FIXED(0.125, 2), FIXED(2.5, 0)]"))
        assertEquals("say \"hi\"2", value("\"say \"\"hi\"\"\" + 2"))
        assertEquals("1", value("1 + 2 * 3 - 4 / 2 = 5 AND \"a\" < \"b\""))

        assertEquals("line 2: MISSING has no value", value("missing + 1"))
        assertEquals("line 2: index 2 is outside a list of 2 element(s)", value("[1, 2][2]"))
        assertEquals("line 2: division by zero", value("1 / 0"))
        assertEquals("line 2: a condition must be a number, not a text", value("NOT \"text\""))
        assertEquals("line 2: there is no function NOWHERE", value("Nowhere(1)"))
        assertEquals("parse: line 2: comparisons cannot be chained: use AND", value("1 < 2 < 3"))
        fun parseError(text: String) = try {
            SmartScript.parse(text)
            ""
        } catch (e: SmartError) {
            "line ${e.line}: ${e.message}"
        }
        assertEquals("line 1: IF is never closed", parseError("IF 1 THEN\nx = 1\n"))
        assertEquals("line 1: a text is not closed", parseError("x = \"open\n"))
        assertEquals("line 5: EXIT outside a loop", parseError("CONST A = 1\nFUNCTION F()\nRETURN 1\nEND FUNCTION\nEXIT FOR"))

        val spinning = SmartInterpreter(SmartScript.parse(
            "CONST A = 1\nFUNCTION Change()\nSHARED A\nA = 2\nEND FUNCTION\nFUNCTION Spin()\nWHILE TRUE\nWEND\nEND FUNCTION"))
        assertEquals(null, spinning.load())
        assertEquals("line 4: A is a constant", spinning.call("Change").exceptionOrNull()?.message)
        spinning.stepLimit = 1000
        assertTrue(spinning.call("Spin").exceptionOrNull()?.message.orEmpty().startsWith("line 7: stopped after 1000 statements"))
    }

    @Test
    fun loadsTheProgramsOfTheRepository() {
        assertNotNull(SmartPrograms.program("TUTOR.smart"))
        assertTrue(SmartPrograms.program("EXPLAIN.smart")!!.interpreter.hasFunction("Tick"))
    }

    @Test
    fun replaysRecordedTicks() {
        val folder = File(System.getProperty("pragma.smart.dir")!!, "tests")
        val files = folder.listFiles { file -> file.name.endsWith(".ticks") }.orEmpty().sortedBy { it.name }
        assertTrue(files.isNotEmpty())
        var records = 0
        for (file in files) {
            for (record in ExplainTicks.parse(file.readText())) {
                records++
                val shown = ExplainTicks.lastShown(record.replay())
                assertNotNull(file.name, shown)
                assertEquals(file.name, record.expected, ExplainTicks.outcome(shown!!))
            }
        }
        assertTrue(records >= 6)
    }

    @Test
    fun drawsThePlansOfRecordedLines() {
        // smart/tests/*.insight: the arrows of the Engine panel's eye, as on the desktop.
        val folder = File(System.getProperty("pragma.smart.dir")!!, "tests")
        val files = folder.listFiles { file -> file.name.endsWith(".insight") }.orEmpty().sortedBy { it.name }
        assertTrue(files.isNotEmpty())
        var cases = 0
        for (file in files) {
            for (case in InsightCase.parse(file.readText())) {
                cases++
                val insight = lineInsight(case.start, case.line)
                assertEquals(case.name, "", insight.error)
                assertEquals(case.name, case.expected, InsightCase.outcome(insight.arrows))
            }
        }
        assertTrue(cases >= 8)
    }
}
