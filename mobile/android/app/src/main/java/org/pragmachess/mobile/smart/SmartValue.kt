package org.pragmachess.mobile.smart

import java.math.BigDecimal
import java.math.RoundingMode

// The interpreter of SMART (smart/README.md), the language the chess
// judgement of Pragma Chess is written in: a transcription of the desktop's
// (gui/qt/src/app/smart), value for value and message for message, so the
// same programs behave the same here. Change both together, with a test.

/** Something the client hands a program — a position, an evaluation —, used only through the client's functions. */
interface SmartObject {
    /** What it is, for error messages: "position", "evaluation"… */
    val typeName: String
}

/** A value of a SMART program: a number, a text, a list, NOTHING, or an object of the client. */
sealed class SmartValue {
    object None : SmartValue()
    data class Number(val value: Double) : SmartValue()
    data class Text(val value: String) : SmartValue()
    /** Lists are values: changing an element makes a new list. */
    data class Items(val items: List<SmartValue>) : SmartValue()
    class Object(val value: SmartObject) : SmartValue() {
        override fun equals(other: Any?) = other is Object && other.value === value
        override fun hashCode() = System.identityHashCode(value)
    }

    val typeName: String
        get() = when (this) {
            None -> "nothing"
            is Number -> "number"
            is Text -> "text"
            is Items -> "list"
            is Object -> value.typeName
        }

    /** The value as STR and SAY write it. */
    fun toText(): String = when (this) {
        None -> "NOTHING"
        is Number -> numberText(value)
        is Text -> value
        is Items -> items.joinToString(", ", "[", "]") { if (it is Text) "\"${it.value}\"" else it.toText() }
        is Object -> "<${value.typeName}>"
    }

    companion object {
        val TRUE = Number(1.0)
        val FALSE = Number(0.0)

        fun of(value: Int): SmartValue = Number(value.toDouble())
        fun of(value: Double): SmartValue = Number(value)
        fun of(value: Boolean): SmartValue = if (value) TRUE else FALSE
        fun of(value: String): SmartValue = Text(value)

        /** Whole numbers without decimals, others with at most 6, trailing zeros dropped. */
        fun numberText(number: Double): String {
            if (number.isFinite() && number == Math.floor(number) && Math.abs(number) < 1e15)
                return number.toLong().toString()
            var text = fixed(number, 6)
            text = text.trimEnd('0').trimEnd('.')
            return if (text == "-0") "0" else text
        }

        /** A number with exactly [decimals] decimals, rounded as the desktop does. */
        fun fixed(number: Double, decimals: Int): String {
            if (!number.isFinite()) return number.toString()
            val text = BigDecimal(number).setScale(decimals, RoundingMode.HALF_UP).toPlainString()
            return if (text.startsWith("-") && text.trimStart('-').all { it == '0' || it == '.' }) text.substring(1) else text
        }
    }
}
