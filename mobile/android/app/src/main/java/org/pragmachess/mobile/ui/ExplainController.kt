package org.pragmachess.mobile.ui

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.engine.Analysis
import org.pragmachess.mobile.explain.EngineEvaluation
import org.pragmachess.mobile.explain.ExplainText
import org.pragmachess.mobile.explain.ExplanationInput
import org.pragmachess.mobile.explain.MoveExplanation
import org.pragmachess.mobile.explain.explainTick
import org.pragmachess.mobile.explain.startExplanation

/** The texts of an explanation in the app's language: the desktop's, as string resources. */
class ExplainStrings(private val context: Context) : ExplainText {
    private val ids = mapOf(
        "White" to R.string.white, "Black" to R.string.black,
        "a pawn" to R.string.explain_a_pawn, "two pawns" to R.string.explain_two_pawns, "%1 pawns" to R.string.explain_n_pawns,
        "a knight" to R.string.explain_a_knight, "%1 knights" to R.string.explain_n_knights,
        "a bishop" to R.string.explain_a_bishop, "%1 bishops" to R.string.explain_n_bishops,
        "a rook" to R.string.explain_a_rook, "%1 rooks" to R.string.explain_n_rooks,
        "the queen" to R.string.explain_the_queen, "%1 queens" to R.string.explain_n_queens,
        "%1 and %2" to R.string.explain_and, "wins the exchange" to R.string.explain_wins_exchange,
        "wins material" to R.string.explain_wins_material, "wins %1" to R.string.explain_wins,
        "wins %1 for %2" to R.string.explain_wins_for,
        "Best move" to R.string.explain_best, "Good move" to R.string.explain_good,
        "Inaccuracy" to R.string.explain_inaccuracy, "Mistake" to R.string.explain_mistake, "Blunder" to R.string.explain_blunder,
        "%1 is winning." to R.string.explain_winning, "%1 is better." to R.string.explain_better,
        "%1 is slightly better." to R.string.explain_slightly_better, "The position is balanced." to R.string.explain_balanced,
        "Checkmate." to R.string.explain_checkmate, "Stalemate." to R.string.explain_stalemate,
        " No material explains it: the assessment is positional, clear after %1." to R.string.explain_positional,
        "%1 (%2 → %3). " to R.string.explain_error_prefix, "%1 (%2). " to R.string.explain_verdict_prefix,
        " Better was %1." to R.string.explain_better_was, "%1 mates in %2: %3." to R.string.explain_mates,
        "%1 %2: %3." to R.string.explain_side_wins, "Missed mate in %1: %2." to R.string.explain_missed_mate,
        "Missed: %1 %2." to R.string.explain_missed, " Main line: %1." to R.string.explain_main_line,
        "the pawn" to R.string.explain_the_pawn, "the knight" to R.string.explain_the_knight,
        "the bishop" to R.string.explain_the_bishop, "the rook" to R.string.explain_the_rook,
        "%1 attacks %2 on %3" to R.string.explain_attacks, "%1: %2 parries it." to R.string.explain_parries,
        "%1 leaves %2 on %3 attacked: %4." to R.string.explain_leaves_attacked,
    )

    override fun tr(source: String): String = ids[source]?.let(context::getString) ?: source
}

/**
 * Explain on the phone, as the desktop's Explainer: it runs no engine of its
 * own. Every line of the live analysis is a tick for smart/EXPLAIN.smart,
 * which answers with the explanation, so it grows and settles as the engine
 * goes deeper. The deepest evaluation seen of each position is kept: the
 * position before the move is judged with it, and a position analysed
 * before is explained at once. It applies to one move: the app turns it off
 * when the board moves on.
 */
class ExplainController(private val text: ExplainText) {
    var enabled by mutableStateOf(false)
        private set
    var explanation by mutableStateOf<MoveExplanation?>(null)
        private set
    /** Waiting for the engine to be deep enough for a first answer. */
    var thinking by mutableStateOf(false)
        private set

    val border: BoardBorder
        get() = when {
            !enabled -> BoardBorder.Plain
            thinking -> BoardBorder.Thinking
            explanation.let { it != null && (it.summary.isNotEmpty() || it.arrows.isNotEmpty()) } -> BoardBorder.Explained
            else -> BoardBorder.Plain
        }

    private val evaluations = HashMap<String, EngineEvaluation>()
    private var before: Position? = null
    private var played: Move? = null
    private var after: Position? = null

    /** Explains [after], reached from [before] by [played] (both null at the start of a game). */
    fun start(before: Position?, played: Move?, after: Position, analyzing: String) {
        this.before = before
        this.played = played
        this.after = after
        enabled = true
        startExplanation()
        explanation = MoveExplanation(summary = analyzing)
        thinking = true
        tick() // A position analysed before is explained at once.
    }

    /** Off: the board moved on, or the user asked. */
    fun stop() {
        enabled = false
        explanation = null
        thinking = false
    }

    /** A line of the live analysis, of whatever position is on the board. */
    fun liveAnalysis(analysis: Analysis?) {
        if (analysis == null || analysis.fen.isEmpty()) return
        val key = key(analysis.fen)
        val sideToMove = if (analysis.fen.split(' ').getOrNull(1) == "b") Side.Black else Side.White
        val evaluation = EngineEvaluation.of(analysis, sideToMove)
        // Only a search at least as deep as the one known adds anything.
        if ((evaluations[key]?.depth ?: -1) > evaluation.depth) return
        if (evaluations.size >= MAX_REMEMBERED) evaluations.clear()
        evaluations[key] = evaluation
        if (enabled && after?.let { key(it.fen()) } == key) tick()
    }

    private fun tick() {
        val position = after ?: return
        val evaluation = evaluations[key(position.fen())] ?: return
        val previous = before
        val tick = explainTick(ExplanationInput(before = previous, played = played,
            beforeEvaluation = previous?.let { evaluations[key(it.fen())] }, after = position,
            afterEvaluation = evaluation, figurines = true), text)
        if (!tick.shown) return
        thinking = false
        if (tick.explanation != explanation) explanation = tick.explanation
    }

    fun close() = stop()

    /** A position by what makes it one: pieces, side to move, castling, en passant. */
    private fun key(fen: String) = fen.split(' ').take(4).joinToString(" ")

    private companion object {
        const val MAX_REMEMBERED = 5000
    }
}
