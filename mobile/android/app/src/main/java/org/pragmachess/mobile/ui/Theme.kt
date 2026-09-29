package org.pragmachess.mobile.ui

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import org.pragmachess.mobile.R

/** The colours of the desktop board (gui/qt/src/widgets/BoardWidget.cpp). */
object BoardColors {
    val light = Color(0xFFF0D9B5)
    val dark = Color(0xFFB58863)
    val lastMove = Color(0xB0CDD26A)
    val selected = Color(0xA0649F5A)
    val moveHint = Color(0x4814330F)
    val check = Color(0xFFD43F32)
    val replyBlue = Color(0xFF3A6EB5)
}

/** The evaluation bar of the desktop (EvaluationBar.cpp). */
object EvaluationColors {
    val white = Color(0xFFF2F2F2)
    val black = Color(0xFF404040)
    val midline = Color(0x90808080)
}

/** SkakNew figurines for SAN, as in the desktop move list; other characters fall back to the system font. */
val FigurineFamily = FontFamily(Font(R.font.pragma_figurine))

private val LightColors = lightColorScheme(
    primary = Color(0xFF3A6EB5),
    secondary = Color(0xFF2F8F44),
)
private val DarkColors = darkColorScheme(
    primary = Color(0xFF9DBBE6),
    secondary = Color(0xFF8FD19E),
)

@Composable
fun PragmaTheme(content: @Composable () -> Unit) {
    val dark = isSystemInDarkTheme()
    val context = LocalContext.current
    val colors = when {
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.S -> if (dark) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        dark -> DarkColors
        else -> LightColors
    }
    MaterialTheme(colorScheme = colors, content = content)
}
