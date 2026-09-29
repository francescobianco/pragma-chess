package org.pragmachess.mobile.engine

import org.junit.Assert.assertEquals
import org.junit.Test

class OexEnginesTest {
    // As in the enginelist.xml of Stockfish Engines OEX.
    private val list = listOf(
        EngineListEntry("Stockfish 11", "libstockfish11.so", "armeabi-v7a|arm64-v8a|x86|x86_64"),
        EngineListEntry("Stockfish 16", "libstockfish16.so", "arm64-v8a"),
        EngineListEntry("Old", "libold.so", "armeabi"),
        EngineListEntry("", "libnameless.so", "arm64-v8a"),
    )

    @Test
    fun keepsTheEnginesThePhoneCanRun() {
        assertEquals(listOf("Stockfish 11", "Stockfish 16"),
            OexEngines.matching(list, listOf("arm64-v8a", "armeabi-v7a", "armeabi")).map { it.name }.take(2))
        assertEquals(listOf("Stockfish 11"), OexEngines.matching(list, listOf("armeabi-v7a")).map { it.name })
        assertEquals(emptyList<String>(), OexEngines.matching(list, listOf("mips")).map { it.name })
    }

    @Test
    fun acceptsAllTargetsAndCase() {
        val any = listOf(EngineListEntry("Any", "engine", "ALL"), EngineListEntry("Arm", "arm", " ARM64-V8A "))
        assertEquals(listOf("Any", "Arm"), OexEngines.matching(any, listOf("arm64-v8a")).map { it.name })
    }

    @Test
    fun findsTheBinaryWithOrWithoutLibPrefix() {
        assertEquals(listOf("libstockfish.so"), OexEngines.binaryCandidates("libstockfish.so"))
        assertEquals(listOf("stockfish", "libstockfish"), OexEngines.binaryCandidates("stockfish"))
        assertEquals(listOf("sf.so", "libsf.so"), OexEngines.binaryCandidates("engines/sf.so"))
    }
}
