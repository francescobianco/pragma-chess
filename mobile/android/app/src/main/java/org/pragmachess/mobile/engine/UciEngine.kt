package org.pragmachess.mobile.engine

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.BufferedWriter
import java.io.File

/**
 * A UCI engine process. The binary belongs to another app (see [OexEngines]):
 * Android only lets an app execute files from a native library folder, its own
 * or, readable by everyone, another app's.
 */
class UciEngine : AutoCloseable {
    private var binary: File? = null
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var process: Process? = null
    private var input: BufferedWriter? = null
    private var reader: Job? = null
    /** The position being searched, or null. */
    private class Search(val fen: String, val whiteToMove: Boolean, val limited: Boolean)
    @Volatile private var search: Search? = null
    /**
     * Searches stopped whose "bestmove" has not come yet: until it does, the
     * engine may still print lines of the old position, which must not be
     * taken for the new one's.
     */
    private val stale = java.util.concurrent.atomic.AtomicInteger(0)

    private val _analysis = MutableStateFlow<Analysis?>(null)
    val analysis: StateFlow<Analysis?> = _analysis

    private val _finished = kotlinx.coroutines.flow.MutableSharedFlow<String>(extraBufferCapacity = 4)
    /** A search limited by depth ended: its position's FEN. */
    val finished: kotlinx.coroutines.flow.SharedFlow<String> = _finished

    private fun start(executable: File): Boolean {
        if (process?.isAlive == true && executable == binary) return true
        close()
        if (!executable.canExecute()) return false
        return try {
            val started = ProcessBuilder(executable.path).redirectErrorStream(true).start()
            process = started
            binary = executable
            input = started.outputStream.bufferedWriter()
            // Few threads: the phone must stay responsive to the finger while
            // the engine thinks, and an eighth of the cores is plenty to
            // follow a game.
            val threads = (Runtime.getRuntime().availableProcessors() / 4).coerceIn(1, 2)
            send("uci")
            send("setoption name Threads value $threads")
            send("setoption name Hash value 64")
            send("isready")
            reader = scope.launch {
                started.inputStream.bufferedReader().useLines { lines ->
                    for (line in lines) {
                        if (!isActive) break
                        if (line.startsWith("bestmove")) {
                            if (stale.get() > 0) {
                                stale.updateAndGet { if (it > 0) it - 1 else 0 }
                            } else {
                                // The end of the search asked for, not of one stopped.
                                val current = search
                                if (current != null && current.limited) {
                                    search = null
                                    _finished.tryEmit(current.fen)
                                }
                            }
                        } else if (line.startsWith("info ") && stale.get() == 0) {
                            val current = search
                            if (current != null) Analysis.parseInfo(line, current.whiteToMove)?.let {
                                if (current === search && stale.get() == 0) _analysis.value = it.copy(fen = current.fen)
                            }
                        }
                    }
                }
            }
            true
        } catch (e: Exception) {
            process = null
            false
        }
    }

    @Synchronized
    private fun send(command: String) {
        val writer = input ?: return
        try {
            writer.write(command)
            writer.newLine()
            writer.flush()
        } catch (e: Exception) {
            // The process died; the next analyse() starts it again.
            process = null
        }
    }

    /**
     * Starts an analysis of [fen] with [executable], infinite or to [depth]
     * (its end comes in [finished]); false if it cannot run.
     */
    fun analyse(executable: File, fen: String, depth: Int? = null): Boolean {
        // A diagram without both kings is no position an engine can search.
        if (org.pragmachess.mobile.chess.Position.fromFen(fen)?.hasKings != true) return false
        if (!start(executable)) return false
        stopSearch()
        search = Search(fen, fen.split(' ').getOrNull(1) != "b", depth != null)
        send("position fen $fen")
        send(if (depth != null) "go depth $depth" else "go infinite")
        return true
    }

    fun stop() {
        stopSearch()
    }

    private fun stopSearch() {
        if (search != null) {
            stale.incrementAndGet()
            search = null
            send("stop")
        }
        _analysis.value = null
    }

    override fun close() {
        search = null
        stale.set(0)
        send("quit")
        reader?.cancel()
        process?.destroy()
        process = null
        input = null
        binary = null
    }
}
