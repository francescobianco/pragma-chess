package org.pragmachess.mobile.explain

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.job
import kotlinx.coroutines.withContext
import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Position
import java.io.BufferedReader
import java.io.BufferedWriter
import java.io.File

/**
 * How the engine is searched for an explanation: the desktop's defaults, so
 * the phone explains a move the same way as the desktop and pragma-explain.
 */
data class ExplainSettings(
    /** Depth of the searches of the positions before and after the move. */
    val depth: Int = 20,
    /** Depth and length of the line probe (AdvantageProbe); depth 0 skips it. */
    val probeDepth: Int = 2,
    val probePlies: Int = 12,
    /** A single thread and a cleared hash make fixed-depth searches reproducible. */
    val threads: Int = 1,
    val hashMb: Int = 64,
)

/** Everything the engine said about a move to explain. */
data class ExplanationAnalysis(
    val before: Position?,
    val played: Move?,
    val after: Position,
    /** Evaluations by increasing depth; the last one is the result. */
    val beforeByDepth: List<EngineEvaluation> = emptyList(),
    val afterByDepth: List<EngineEvaluation> = emptyList(),
    /** Shallow evaluations of the positions along the principal variation of [after] (index 0 = [after]). */
    val probe: List<EngineEvaluation> = emptyList(),
) {
    val beforeEvaluation: EngineEvaluation? get() = beforeByDepth.lastOrNull()
    val afterEvaluation: EngineEvaluation? get() = afterByDepth.lastOrNull()

    /**
     * Whether an evaluation of [after] from elsewhere (the live analysis)
     * should be trusted over this analysis: only a mate or a 0.00 the search
     * did not see, from a search at least as deep, with a line that can be played.
     */
    fun acceptsHint(hint: EngineEvaluation): Boolean {
        val searched = afterEvaluation
        if (hint.pv.isEmpty() || (searched != null && hint.depth < searched.depth)) return false
        val mate = hint.isMate && hint.mateIn > 0
        val draw = !hint.isMate && hint.centipawns == 0
        // Only what a long analysis finds and a fixed-depth search may miss.
        val rejected = if (mate) searched != null && searched.isMate
        else !draw || (searched != null && !searched.isMate && searched.centipawns == 0)
        if (rejected) return false
        var position = after
        for (uci in hint.pv) {
            val move = position.parseUci(uci) ?: return false
            position = position.play(move)
        }
        return true
    }

    /** Input for [explainPosition], with the concrete ply from the probe. An accepted [hint] replaces the evaluation of [after]. */
    fun input(figurines: Boolean = false, trace: Boolean = false, hint: EngineEvaluation? = null): ExplanationInput {
        val evaluation = afterEvaluation
        var input = ExplanationInput(
            before = before,
            played = played,
            beforeEvaluation = beforeEvaluation,
            after = after,
            afterEvaluation = evaluation ?: EngineEvaluation(),
            concretePly = if (evaluation != null && probe.isNotEmpty()) AdvantageProbe.concretePly(probe, evaluation) else null,
            figurines = figurines,
            trace = trace,
        )
        if (hint != null && acceptsHint(hint)) {
            val replaced = if (evaluation == null) "no evaluation" else "${evaluation.text} at depth ${evaluation.depth}"
            input = input.copy(
                evaluationNote = "hint from the live analysis: ${hint.text} at depth ${hint.depth} replaces $replaced",
                afterEvaluation = hint,
                concretePly = null, // The probe followed the other line.
            )
        }
        return input
    }
}

/**
 * Runs the searches for an explanation on an engine process of its own, apart
 * from the live analysis: the position before the move, the position after
 * it, then the line probe — as the desktop's ExplanationSearch.
 */
class ExplanationSearch(private val settings: ExplainSettings = ExplainSettings()) : AutoCloseable {
    private var process: Process? = null
    private var binary: File? = null
    private var input: BufferedWriter? = null
    private var output: BufferedReader? = null

    /** Analyzes a move (or, without [before], a position). Cancelling the coroutine ends the process. */
    suspend fun analyze(executable: File, before: Position?, played: Move?, after: Position): ExplanationAnalysis =
        withContext(Dispatchers.IO) {
            // A blocking read cannot be interrupted: cancelling ends the process, which ends the read.
            val cancel = currentCoroutineContext().job.invokeOnCompletion { cause -> if (cause != null) close() }
            try {
                start(executable)
                var analysis = ExplanationAnalysis(before, played, after)
                if (before != null) analysis = analysis.copy(beforeByDepth = search(before, settings.depth))
                analysis = analysis.copy(afterByDepth = search(after, settings.depth))
                val deep = analysis.afterEvaluation
                if (settings.probeDepth > 0 && deep != null && deep.pv.isNotEmpty()) {
                    val probe = ArrayList<EngineEvaluation>()
                    var position = after
                    while (true) {
                        currentCoroutineContext().ensureActive()
                        val current = search(position, settings.probeDepth)
                        if (current.isEmpty()) break
                        probe += current.last()
                        // Moves played to reach the position just searched.
                        val ply = probe.size - 1
                        if (ply >= deep.pv.size || ply >= settings.probePlies) break
                        val next = position.parseUci(deep.pv[ply]) ?: break
                        position = position.play(next)
                    }
                    analysis = analysis.copy(probe = probe)
                }
                analysis
            } finally {
                cancel.dispose()
            }
        }

    private fun start(executable: File) {
        if (process?.isAlive == true && executable == binary) return
        close()
        if (!executable.canExecute()) throw IllegalStateException("cannot run ${executable.name}")
        val started = ProcessBuilder(executable.path).redirectErrorStream(true).start()
        process = started
        binary = executable
        input = started.outputStream.bufferedWriter()
        output = started.inputStream.bufferedReader()
        send("uci")
        readUntil("uciok")
        send("setoption name Threads value ${settings.threads.coerceAtLeast(1)}")
        send("setoption name Hash value ${settings.hashMb.coerceAtLeast(1)}")
    }

    /** One fixed-depth search from a cleared hash: the last evaluation of each depth. */
    private suspend fun search(position: Position, depth: Int): List<EngineEvaluation> {
        currentCoroutineContext().ensureActive()
        val whiteToMove = position.sideToMove == org.pragmachess.mobile.chess.Side.White
        send("ucinewgame")
        send("isready")
        readUntil("readyok")
        send("position fen ${position.fen()}")
        send("go depth $depth")
        val byDepth = ArrayList<EngineEvaluation>()
        val reader = output ?: throw IllegalStateException("the engine stopped")
        while (true) {
            val line = reader.readLine() ?: throw IllegalStateException("the engine stopped")
            if (line.startsWith("bestmove")) break
            val evaluation = EngineEvaluation.parseInfo(line, whiteToMove) ?: continue
            // Engines report several lines per depth; keep the last one of each.
            if (byDepth.isNotEmpty() && byDepth.last().depth >= evaluation.depth) byDepth[byDepth.size - 1] = evaluation
            else byDepth += evaluation
        }
        return byDepth
    }

    private fun readUntil(token: String) {
        val reader = output ?: throw IllegalStateException("the engine stopped")
        while (true) {
            val line = reader.readLine() ?: throw IllegalStateException("the engine stopped")
            if (line.trim() == token) return
        }
    }

    @Synchronized
    private fun send(command: String) {
        val writer = input ?: throw IllegalStateException("the engine stopped")
        writer.write(command)
        writer.newLine()
        writer.flush()
    }

    @Synchronized
    override fun close() {
        runCatching { input?.apply { write("quit"); newLine(); flush() } }
        process?.destroy()
        process = null
        input = null
        output = null
        binary = null
    }
}
