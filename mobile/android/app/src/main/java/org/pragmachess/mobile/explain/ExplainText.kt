package org.pragmachess.mobile.explain

/**
 * The words of an explanation, looked up by their English text as the
 * desktop's tr() does, so the logic reads like MoveExplanation.cpp and the
 * translations are the desktop's. The app maps them to string resources
 * (ui/ExplainStrings); without a translation the English text is used.
 */
fun interface ExplainText {
    /** The template for [source], with %1, %2… where the arguments go. */
    fun tr(source: String): String

    companion object {
        val English = ExplainText { it }

        /** Every text an explanation may use (the keys of a translation). */
        val SOURCES = listOf(
            "White", "Black",
            "a pawn", "two pawns", "%1 pawns", "a knight", "%1 knights", "a bishop", "%1 bishops",
            "a rook", "%1 rooks", "the queen", "%1 queens", "%1 and %2",
            "wins the exchange", "wins material", "wins %1", "wins %1 for %2",
            "Best move", "Good move", "Inaccuracy", "Mistake", "Blunder",
            "%1 is winning.", "%1 is better.", "%1 is slightly better.", "The position is balanced.",
            "Checkmate.", "Stalemate.",
            " No material explains it: the assessment is positional, clear after %1.",
            "%1 (%2 → %3). ", "%1 (%2). ", " Better was %1.", "%1 mates in %2: %3.", "%1 %2: %3.",
            "Missed mate in %1: %2.", "Missed: %1 %2.", " Main line: %1.",
            "the pawn", "the knight", "the bishop", "the rook", "%1 attacks %2 on %3", "%1: %2 parries it.",
            "%1 leaves %2 on %3 attacked: %4.", "%1: %2 takes the attacker.",
            "%1: %2 moves it again, and %3 gains time.", "%1 attacks %2 on %3 and %4 on %5.",
            "%1 threatens %2, and the king cannot take back.", "%1 threatens %2.",
            "%1 attacks %2 on %3, in line with the king on %4.",
            " No material is lost: the evaluation is positional.",
        )
    }
}

/** Qt's QString::arg for several arguments: %1, %2… replaced in one pass. */
internal fun String.args(vararg values: Any): String =
    Regex("%(\\d)").replace(this) { match ->
        val index = match.groupValues[1].toInt() - 1
        values.getOrNull(index)?.toString() ?: match.value
    }
