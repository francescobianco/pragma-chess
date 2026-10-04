package org.pragmachess.mobile.ui

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lightbulb
import androidx.compose.material.icons.outlined.Lightbulb
import androidx.compose.material3.Icon
import androidx.compose.material3.IconToggleButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.pragmachess.mobile.R

/** The bulb between the previous and next move, as on the desktop: Explain the move on the board. */
@Composable
fun ExplainButton(vm: AppViewModel) {
    val description = stringResource(R.string.explain_tooltip)
    IconToggleButton(
        checked = vm.explainer.enabled,
        onCheckedChange = { vm.toggleExplain() },
        modifier = Modifier.semantics { contentDescription = description },
    ) {
        Icon(if (vm.explainer.enabled) Icons.Filled.Lightbulb else Icons.Outlined.Lightbulb, stringResource(R.string.explain))
    }
}

/** The explanation's sentence under the engine's line, moves in figurines. */
@Composable
fun ExplanationText(summary: String) {
    if (summary.isEmpty()) return
    Surface(color = MaterialTheme.colorScheme.surfaceContainerHigh, modifier = Modifier.fillMaxWidth()) {
        Text(
            summary,
            fontFamily = FigurineFamily,
            fontSize = 15.sp,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 8.dp),
        )
    }
}
