package org.pragmachess.mobile.explain

import kotlin.math.abs

/**
 * Finds where an advantage stops being a deep engine insight and shows on the
 * board (the desktop's AdvantageProbe). A +3 often needs several forced, good
 * moves before anything visible happens; the explanation points at that moment.
 *
 * - by depth: one position searched at increasing depths; the smallest depth
 *   from which the score stays with the deepest one tells how far ahead the
 *   advantage lies;
 * - along the line: every position of the principal variation searched at a
 *   small fixed depth; the first ply from which that shallow score agrees with
 *   the deep one is where the advantage becomes concrete.
 */
object AdvantageProbe {
    /** Winning-chance points (0–100) within which two evaluations agree. */
    const val AGREEMENT = 10.0

    fun agrees(a: EngineEvaluation, b: EngineEvaluation, agreement: Double = AGREEMENT): Boolean =
        100.0 * abs(a.whiteShare - b.whiteShare) <= agreement

    /** [byDepth] by increasing depth, the last being the reference: the depth from which all agree with it. */
    fun settledDepth(byDepth: List<EngineEvaluation>, agreement: Double = AGREEMENT): Int? {
        if (byDepth.isEmpty()) return null
        val reference = byDepth.last()
        var depth: Int? = null
        var i = byDepth.size - 1
        while (i >= 0 && agrees(byDepth[i], reference, agreement)) {
            depth = byDepth[i].depth
            i--
        }
        return depth
    }

    /**
     * [shallow]`[k]` is the shallow evaluation of the position after k moves of
     * the line whose deep evaluation is [deep]: the first ply from which all agree with it.
     */
    fun concretePly(shallow: List<EngineEvaluation>, deep: EngineEvaluation, agreement: Double = AGREEMENT): Int? {
        var ply: Int? = null
        var k = shallow.size - 1
        while (k >= 0 && agrees(shallow[k], deep, agreement)) {
            ply = k
            k--
        }
        return ply
    }
}
