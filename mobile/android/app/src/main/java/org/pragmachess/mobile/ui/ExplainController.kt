package org.pragmachess.mobile.ui

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.engine.Analysis
import org.pragmachess.mobile.explain.EngineEvaluation
import org.pragmachess.mobile.explain.ExplainText
import org.pragmachess.mobile.explain.ExplanationAnalysis
import org.pragmachess.mobile.explain.ExplanationSearch
import org.pragmachess.mobile.explain.MoveExplanation
import org.pragmachess.mobile.explain.explainPosition
import java.io.File

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
    )

    override fun tr(source: String): String = ids[source]?.let(context::getString) ?: source
}

/**
 * Explain on the phone, as the desktop's Explainer: the engine searches the
 * move on the board on a process of its own (positions before and after it at
 * fixed depth, then the line probe), finished analyses are kept by move, and
 * the explanation is drawn on the board. It applies to one move: the app turns
 * it off when the board moves on. While it searches, [onBusy] pauses the live
 * analysis, which would otherwise take the phone's cores from it.
 */
class ExplainController(
    private val scope: CoroutineScope,
    private val text: ExplainText,
    private val onBusy: (Boolean) -> Unit,
) {
    var enabled by mutableStateOf(false)
        private set
    /** The engine is searching for the explanation. */
    var thinking by mutableStateOf(false)
        private set
    var explanation by mutableStateOf<MoveExplanation?>(null)
        private set

    val border: BoardBorder
        get() = when {
            !enabled -> BoardBorder.Plain
            thinking -> BoardBorder.Thinking
            explanation.let { it != null && (it.summary.isNotEmpty() || it.arrows.isNotEmpty()) } -> BoardBorder.Explained
            else -> BoardBorder.Plain
        }

    private val search = ExplanationSearch()
    private val analyses = LinkedHashMap<String, ExplanationAnalysis>()
    private var job: Job? = null
    private var before: Position? = null
    private var played: Move? = null
    private var after: Position? = null
    private var hint: EngineEvaluation? = null
    private var usedHint: String = ""

    /** Explains [after], reached from [before] by [played] (both null at the start of a game). */
    fun start(executable: File, before: Position?, played: Move?, after: Position, analyzing: String, failed: (String) -> String) {
        this.before = before
        this.played = played
        this.after = after
        hint = null
        usedHint = ""
        enabled = true
        val known = analyses[key(before, played, after)]
        if (known != null) {
            show(known)
            return
        }
        explanation = MoveExplanation(summary = analyzing)
        thinking = true
        onBusy(true)
        job?.cancel()
        job = scope.launch {
            try {
                val analysis = search.analyze(executable, before, played, after)
                if (analyses.size >= MAX_REMEMBERED) analyses.clear()
                analyses[key(analysis.before, analysis.played, analysis.after)] = analysis
                val shown = this@ExplainController.after
                if (enabled && shown != null && key(analysis.before, analysis.played, analysis.after) ==
                    key(this@ExplainController.before, this@ExplainController.played, shown)) show(analysis)
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                if (enabled) explanation = MoveExplanation(summary = failed(e.message.orEmpty()))
            } finally {
                if (job == coroutineContext[Job]) {
                    thinking = false
                    onBusy(false)
                }
            }
        }
    }

    /** Off: the board moved on, or the user asked. */
    fun stop() {
        if (!enabled && job == null) return
        enabled = false
        explanation = null
        val running = job
        job = null
        if (running != null) {
            running.cancel()
            thinking = false
            onBusy(false)
        }
    }

    /**
     * The live analysis of the position on the board: a mate or a draw found
     * deeper than the fixed-depth search guides the explanation, as on the desktop.
     */
    fun liveAnalysis(analysis: Analysis?) {
        val position = after ?: return
        if (analysis == null || !enabled || thinking) return
        val evaluation = EngineEvaluation.of(analysis, position.sideToMove)
        hint = evaluation
        val known = analyses[key(before, played, position)] ?: return
        if (!known.acceptsHint(evaluation) || evaluation.text == usedHint) return
        show(known)
    }

    private fun show(analysis: ExplanationAnalysis) {
        val current = hint
        val hinted = current != null && analysis.acceptsHint(current)
        usedHint = if (hinted) current!!.text else ""
        explanation = explainPosition(analysis.input(figurines = true, hint = current), text)
    }

    fun close() {
        stop()
        search.close()
    }

    private fun key(before: Position?, played: Move?, after: Position) =
        "${before?.fen().orEmpty()}|${played?.uci.orEmpty()}|${after.fen()}"

    private companion object {
        const val MAX_REMEMBERED = 500
    }
}
