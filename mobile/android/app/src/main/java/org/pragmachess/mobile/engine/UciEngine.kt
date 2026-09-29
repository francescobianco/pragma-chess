package org.pragmachess.mobile.engine

import android.content.Context
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
 * A UCI engine process: the Stockfish shipped as libstockfish.so in the
 * native library folder (the one place Android lets an app execute a file
 * from). Nothing here is Stockfish-specific but the file name.
 */
class UciEngine(context: Context) : AutoCloseable {
    private val binary = File(context.applicationInfo.nativeLibraryDir, "libstockfish.so")
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var process: Process? = null
    private var input: BufferedWriter? = null
    private var reader: Job? = null
    @Volatile private var whiteToMove = true
    @Volatile private var generation = 0

    private val _analysis = MutableStateFlow<Analysis?>(null)
    val analysis: StateFlow<Analysis?> = _analysis

    val isAvailable: Boolean get() = binary.canExecute()

    private fun start(): Boolean {
        if (process?.isAlive == true) return true
        if (!isAvailable) return false
        return try {
            val started = ProcessBuilder(binary.path).redirectErrorStream(true).start()
            process = started
            input = started.outputStream.bufferedWriter()
            val threads = (Runtime.getRuntime().availableProcessors() - 1).coerceIn(1, 4)
            send("uci")
            send("setoption name Threads value $threads")
            send("setoption name Hash value 64")
            send("isready")
            reader = scope.launch {
                started.inputStream.bufferedReader().useLines { lines ->
                    for (line in lines) {
                        if (!isActive) break
                        val current = generation
                        if (line.startsWith("info ")) {
                            Analysis.parseInfo(line, whiteToMove)?.let { if (current == generation) _analysis.value = it }
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

    /** Starts an infinite analysis of [fen]; false if the engine cannot run. */
    fun analyse(fen: String): Boolean {
        if (!start()) return false
        send("stop")
        generation++
        _analysis.value = null
        whiteToMove = fen.split(' ').getOrNull(1) != "b"
        send("position fen $fen")
        send("go infinite")
        return true
    }

    fun stop() {
        generation++
        send("stop")
        _analysis.value = null
    }

    override fun close() {
        send("quit")
        reader?.cancel()
        process?.destroy()
        process = null
    }
}
