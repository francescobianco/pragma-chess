package org.pragmachess.mobile.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.pragmachess.mobile.engine.Analysis

/** The desktop's evaluation bar: White's share from the bottom (top when flipped), the score at the end of the side ahead. */
@Composable
fun EvaluationBar(analysis: Analysis?, flipped: Boolean, modifier: Modifier = Modifier) {
    val share by animateFloatAsState(analysis?.whiteShare ?: 0.5f, tween(250), label = "evaluation")
    val measurer = rememberTextMeasurer()
    Canvas(modifier.clip(RoundedCornerShape(3.dp))) {
        val whiteHeight = size.height * share
        if (flipped) {
            drawRect(EvaluationColors.black, Offset(0f, whiteHeight), Size(size.width, size.height - whiteHeight))
            drawRect(EvaluationColors.white, Offset.Zero, Size(size.width, whiteHeight))
        } else {
            drawRect(EvaluationColors.black, Offset.Zero, Size(size.width, size.height - whiteHeight))
            drawRect(EvaluationColors.white, Offset(0f, size.height - whiteHeight), Size(size.width, whiteHeight))
        }
        drawRect(EvaluationColors.midline, Offset(0f, size.height / 2 - 0.5f), Size(size.width, 1f))
        if (analysis == null) return@Canvas
        val atBottom = analysis.whiteAhead != flipped
        val text = measurer.measure(
            analysis.text,
            TextStyle(color = if (analysis.whiteAhead) EvaluationColors.black else EvaluationColors.white,
                fontSize = 8.sp, fontWeight = FontWeight.Bold),
        )
        val x = (size.width - text.size.width) / 2
        val y = if (atBottom) size.height - text.size.height - 4.dp.toPx() else 4.dp.toPx()
        drawText(text, topLeft = Offset(x, y))
    }
}
