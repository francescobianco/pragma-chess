package org.pragmachess.mobile.smart

/** A SMART program ready to run in the app: its interpreter, with the chess of [SmartChess], and what its commands collect. */
class SmartProgram(script: SmartScript) {
    val interpreter = SmartInterpreter(script)
    val output = SmartChess.Output()
}

/**
 * The SMART programs of the repository's smart/ folder: the app carries them
 * as assets (the build copies them, see app/build.gradle.kts), the JVM tests
 * read them from the folder (system property `pragma.smart.dir`).
 */
object SmartPrograms {
    /** Reads a program's text by file name ("TUTOR.smart"); null if there is none. */
    @Volatile
    var reader: (String) -> String? = { name ->
        System.getProperty("pragma.smart.dir")?.let { java.io.File(it, name) }?.takeIf { it.isFile }?.readText()
    }

    // Interpreters keep their memory and are not shared between threads.
    private val loaded = ThreadLocal.withInitial { HashMap<String, SmartProgram?>() }

    /** The program [fileName] loaded, one per thread; null if it cannot be read or loaded (logged once). */
    fun program(fileName: String): SmartProgram? {
        val programs = loaded.get()!!
        if (fileName in programs) return programs[fileName]
        var error = ""
        val program = reader(fileName)?.let { text ->
            SmartScript.parse(text) { error = it }?.let { script ->
                SmartProgram(script).also { program ->
                    SmartChess.define(program.interpreter, program.output)
                    program.interpreter.load()?.let { error = it }
                }
            }
        }?.takeIf { error.isEmpty() }
        if (program == null) System.err.println("SMART $fileName: ${error.ifEmpty { "cannot be read" }}")
        programs[fileName] = program // A program that failed is not tried again.
        return program
    }

    /** Forgets the programs loaded on this thread (after [reader] changed). */
    fun reset() = loaded.get()!!.clear()
}
