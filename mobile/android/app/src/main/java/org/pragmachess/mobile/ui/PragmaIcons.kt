package org.pragmachess.mobile.ui

import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.graphics.vector.path
import androidx.compose.ui.unit.dp

/** Icons Material does not have, drawn like its outlined ones (24 dp, 2 dp strokes); `Icon` tints them. */
object PragmaIcons {
    /** The classic database cylinder. */
    val Database: ImageVector by lazy {
        ImageVector.Builder("Database", 24.dp, 24.dp, 24f, 24f).apply {
            path(stroke = SolidColor(Color.Black), strokeLineWidth = 2f,
                strokeLineCap = StrokeCap.Round, strokeLineJoin = StrokeJoin.Round) {
                // Top: a whole ellipse.
                moveTo(5f, 6f)
                arcTo(7f, 2.5f, 0f, false, true, 19f, 6f)
                arcTo(7f, 2.5f, 0f, false, true, 5f, 6f)
                // Sides and bottom.
                lineTo(5f, 18f)
                arcTo(7f, 2.5f, 0f, false, false, 19f, 18f)
                lineTo(19f, 6f)
                // The band in the middle.
                moveTo(5f, 12f)
                arcTo(7f, 2.5f, 0f, false, false, 19f, 12f)
            }
        }.build()
    }

    /** A 2×2 board in a frame: two dark squares, two light ones, a1 dark as on a real board. */
    val Board: ImageVector by lazy {
        ImageVector.Builder("Board", 24.dp, 24.dp, 24f, 24f).apply {
            path(stroke = SolidColor(Color.Black), strokeLineWidth = 2f, strokeLineJoin = StrokeJoin.Round) {
                moveTo(4f, 4f)
                lineTo(20f, 4f)
                lineTo(20f, 20f)
                lineTo(4f, 20f)
                close()
            }
            path(fill = SolidColor(Color.Black)) {
                // Bottom left and top right.
                moveTo(4f, 12f); lineTo(12f, 12f); lineTo(12f, 20f); lineTo(4f, 20f); close()
                moveTo(12f, 4f); lineTo(20f, 4f); lineTo(20f, 12f); lineTo(12f, 12f); close()
            }
        }.build()
    }
}
