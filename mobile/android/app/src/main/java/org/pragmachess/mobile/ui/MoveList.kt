package org.pragmachess.mobile.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.GameLine
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.figurineSan

/**
 * The moves as the desktop shows them: a table with the move number and
 * White's and Black's columns, SAN in figurines, the current move selected.
 */
@Composable
fun MoveList(line: GameLine, ply: Int, onPly: (Int) -> Unit, modifier: Modifier = Modifier) {
    // A game starting with Black to move leaves White's first cell empty.
    val offset = if (line.start.sideToMove == Side.Black) 1 else 0
    val rows = (line.plyCount + offset + 1) / 2
    val firstNumber = line.start.fullmoveNumber
    val state = rememberLazyListState()
    LaunchedEffect(ply, rows) {
        if (ply > 0) state.animateScrollToItem(((ply - 1 + offset) / 2 + 1).coerceAtMost(rows))
    }
    LazyColumn(modifier, state = state) {
        item {
            Row(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
                Text("", Modifier.width(44.dp))
                for (title in listOf(R.string.white, R.string.black)) {
                    Text(stringResource(title), Modifier.weight(1f), style = MaterialTheme.typography.labelMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant, textAlign = TextAlign.Start)
                }
            }
            HorizontalDivider()
        }
        items(rows) { row ->
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                val current = (ply - 1 + offset) / 2 == row && ply > 0
                Text("${firstNumber + row}.", Modifier.width(44.dp).padding(start = 8.dp),
                    style = MaterialTheme.typography.bodyMedium,
                    fontWeight = if (current) FontWeight.Bold else FontWeight.Normal,
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                for (column in 0..1) {
                    val cellPly = row * 2 + column - offset + 1
                    val move = line.moves.getOrNull(cellPly - 1)
                    val selected = cellPly == ply
                    Box(
                        Modifier
                            .weight(1f)
                            .padding(horizontal = 2.dp, vertical = 1.dp)
                            .background(if (selected) MaterialTheme.colorScheme.primaryContainer else Color.Transparent, RoundedCornerShape(4.dp))
                            .clickable(enabled = move != null) { onPly(cellPly) }
                            .padding(horizontal = 8.dp, vertical = 6.dp),
                    ) {
                        Text(
                            when {
                                move != null -> figurineSan(move.san)
                                cellPly < 1 -> "…"
                                else -> ""
                            },
                            fontFamily = FigurineFamily,
                            fontSize = 16.sp,
                            color = if (selected) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurface,
                        )
                    }
                }
            }
        }
    }
}
