package org.pragmachess.mobile.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import org.pragmachess.mobile.engine.Analysis

/**
 * A thin evaluation bar: White's share from the bottom (top when flipped).
 * No text: the score is in the engine line, where it can be read.
 */
@Composable
fun EvaluationBar(analysis: Analysis?, flipped: Boolean, modifier: Modifier = Modifier) {
    val share by animateFloatAsState(analysis?.whiteShare ?: 0.5f, tween(250), label = "evaluation")
    Canvas(modifier.clip(RoundedCornerShape(50))) {
        val whiteHeight = size.height * share
        if (flipped) {
            drawRect(EvaluationColors.black, Offset(0f, whiteHeight), Size(size.width, size.height - whiteHeight))
            drawRect(EvaluationColors.white, Offset.Zero, Size(size.width, whiteHeight))
        } else {
            drawRect(EvaluationColors.black, Offset.Zero, Size(size.width, size.height - whiteHeight))
            drawRect(EvaluationColors.white, Offset(0f, size.height - whiteHeight), Size(size.width, whiteHeight))
        }
        drawRect(EvaluationColors.midline, Offset(0f, size.height / 2 - 0.5f), Size(size.width, 1f))
    }
}
