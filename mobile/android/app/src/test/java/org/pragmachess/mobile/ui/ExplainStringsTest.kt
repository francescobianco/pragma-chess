package org.pragmachess.mobile.ui

import org.junit.Assert.assertTrue
import org.junit.Test
import org.pragmachess.mobile.explain.ExplainText
import java.io.File

/**
 * A sentence EXPLAIN.smart says is translated on the phone only when it is
 * in [ExplainText.SOURCES] and has its string resource: a new sentence that
 * misses either would stay in English, as tst_chessrules checks for the
 * desktop's list.
 */
class ExplainStringsTest {
    @Test
    fun everySentenceOfTheProgramIsTranslated() {
        val program = File(System.getProperty("pragma.smart.dir")!!, "EXPLAIN.smart").readText()
        val sentences = Regex("TEXT\\(\"((?:[^\"\\\\]|\\\\.)*)\"").findAll(program).map { it.groupValues[1] }.toSet()
        assertTrue(sentences.isNotEmpty())
        val unknown = sentences - ExplainText.SOURCES.toSet()
        assertTrue("not in ExplainText.SOURCES: $unknown", unknown.isEmpty())
        val untranslated = ExplainText.SOURCES.toSet() - ExplainStrings.IDS.keys
        assertTrue("no string resource in ExplainStrings: $untranslated", untranslated.isEmpty())
    }
}
