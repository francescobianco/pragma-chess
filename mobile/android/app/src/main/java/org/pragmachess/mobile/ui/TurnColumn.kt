package org.pragmachess.mobile.ui

import androidx.compose.foundation.Canvas
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.res.stringResource
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.Side

/**
 * The column at the right of the board, as on the desktop: a dot in the
 * colour of the player to move, level with the edge of the board on their side.
 */
@Composable
fun TurnColumn(sideToMove: Side, flipped: Boolean, modifier: Modifier = Modifier) {
    val outline = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.45f)
    val label = stringResource(if (sideToMove == Side.White) R.string.white_to_move else R.string.black_to_move)
    Canvas(modifier.semantics { contentDescription = label }) {
        val radius = size.width / 2
        val atTop = (sideToMove == Side.White) == flipped
        val center = Offset(radius, if (atTop) radius else size.height - radius)
        drawCircle(if (sideToMove == Side.White) TurnColors.white else TurnColors.black, radius - 0.5f, center)
        drawCircle(outline, radius - 0.5f, center, style = Stroke(1f))
    }
}

private object TurnColors {
    val white = Color(0xFFFAFAF7)
    val black = Color(0xFF1F1F1F)
}
