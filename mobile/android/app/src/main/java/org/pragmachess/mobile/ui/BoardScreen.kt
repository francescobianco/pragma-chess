package org.pragmachess.mobile.ui

import android.content.res.Configuration
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.automirrored.filled.LastPage
import androidx.compose.material.icons.filled.FirstPage
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.SwapVert
import androidx.compose.material.icons.outlined.Insights
import androidx.compose.material3.BottomAppBar
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.IconToggleButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.text.font.FontWeight
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.GameLine
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.figurineSan
import org.pragmachess.mobile.engine.Analysis

/** The main screen: the board, the moves, the buttons to go through them, and the engine. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun BoardScreen(vm: AppViewModel, snackbar: SnackbarHostState, onMenu: () -> Unit) {
    var menu by remember { mutableStateOf(false) }
    var details by remember { mutableStateOf(false) }
    val line = vm.line
    val ply = vm.ply
    val position = vm.position

    Scaffold(
        topBar = {
            TopAppBar(
                navigationIcon = {
                    IconButton(onClick = onMenu) { Icon(Icons.Filled.Menu, stringResource(R.string.open_menu)) }
                },
                title = {
                    Column {
                        val h = vm.headers
                        val players = if (h.white.isBlank() && h.black.isBlank()) stringResource(R.string.new_game)
                        else "${h.white.ifBlank { "?" }} – ${h.black.ifBlank { "?" }}"
                        Text(players, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        val subtitle = listOfNotNull(
                            h.result.takeIf { it != "*" },
                            h.event.ifBlank { null },
                            h.date.ifBlank { null },
                            vm.gameDatabase?.title?.let { if (vm.dirty) stringResource(R.string.unsaved) else it },
                        ).joinToString(" · ")
                        if (subtitle.isNotEmpty()) {
                            Text(subtitle, style = MaterialTheme.typography.bodySmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        }
                    }
                },
                actions = {
                    IconToggleButton(checked = vm.engineOn, onCheckedChange = { vm.toggleEngine() }) {
                        Icon(Icons.Outlined.Insights, stringResource(R.string.engine))
                    }
                    IconButton(onClick = { vm.flipped = !vm.flipped }) {
                        Icon(Icons.Filled.SwapVert, stringResource(R.string.flip_board))
                    }
                    Box {
                        IconButton(onClick = { menu = true }) { Icon(Icons.Filled.MoreVert, stringResource(R.string.more)) }
                        DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                            DropdownMenuItem(text = { Text(stringResource(R.string.new_game)) }, onClick = {
                                menu = false
                                vm.newGame()
                            })
                            DropdownMenuItem(text = { Text(stringResource(R.string.save_game)) }, enabled = vm.canSave, onClick = {
                                menu = false
                                details = true
                            })
                        }
                    }
                },
            )
        },
        bottomBar = {
            BottomAppBar {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                    IconButton(onClick = { vm.goTo(0) }, enabled = ply > 0) {
                        Icon(Icons.Filled.FirstPage, stringResource(R.string.first_move))
                    }
                    IconButton(onClick = { vm.goTo(ply - 1) }, enabled = ply > 0) {
                        Icon(Icons.AutoMirrored.Filled.KeyboardArrowLeft, stringResource(R.string.previous_move))
                    }
                    IconButton(onClick = { vm.goTo(ply + 1) }, enabled = ply < line.plyCount) {
                        Icon(Icons.AutoMirrored.Filled.KeyboardArrowRight, stringResource(R.string.next_move))
                    }
                    IconButton(onClick = { vm.goTo(line.plyCount) }, enabled = ply < line.plyCount) {
                        Icon(Icons.AutoMirrored.Filled.LastPage, stringResource(R.string.last_move))
                    }
                }
            }
        },
        snackbarHost = { SnackbarHost(snackbar) },
    ) { padding ->
        val landscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
        val board: @Composable (Modifier) -> Unit = { modifier ->
            val withBar = vm.engineOn && vm.engineReady
            // The board is square: the bar and the turn column take what is left at its sides.
            BoxWithConstraints(modifier, contentAlignment = Alignment.TopCenter) {
                val extras = TURN_WIDTH + BOARD_GAP * 2 + if (withBar) BAR_WIDTH else 0.dp
                val side = minOf(maxWidth - extras, maxHeight)
                Row(Modifier.height(side), horizontalArrangement = Arrangement.spacedBy(BOARD_GAP)) {
                    if (withBar) EvaluationBar(vm.analysis, vm.flipped, Modifier.width(BAR_WIDTH).fillMaxHeight())
                    else Spacer(Modifier.width(0.dp))
                    ChessBoard(
                        position = position,
                        lastMove = line.moves.getOrNull(ply - 1)?.move,
                        flipped = vm.flipped,
                        onMove = vm::play,
                        modifier = Modifier.size(side),
                    )
                    TurnColumn(position.sideToMove, vm.flipped, Modifier.width(TURN_WIDTH).fillMaxHeight())
                }
            }
        }
        val panel: @Composable (Modifier) -> Unit = { modifier ->
            Column(modifier) {
                if (vm.engineOn) {
                    if (vm.engineReady) EngineLine(vm.analysis, position) else NoEngine(vm)
                }
                MoveList(line, ply, vm::goTo, Modifier.weight(1f).fillMaxWidth())
            }
        }
        if (landscape) {
            Row(Modifier.padding(padding).fillMaxSize().padding(8.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                board(Modifier.fillMaxHeight())
                panel(Modifier.weight(1f).fillMaxHeight())
            }
        } else {
            Column(Modifier.padding(padding).fillMaxSize()) {
                board(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 8.dp))
                panel(Modifier.weight(1f).fillMaxWidth())
            }
        }
    }

    if (details) {
        GameDetailsDialog(vm, onDismiss = { details = false })
    }
}

private val BAR_WIDTH = 4.dp
private val TURN_WIDTH = 10.dp
private val BOARD_GAP = 6.dp

/**
 * The engine's score, depth and best line in figurines, under the board. The
 * score stands out in a badge: the evaluation bar has no text.
 */
@Composable
private fun EngineLine(analysis: Analysis?, position: Position) {
    Surface(color = MaterialTheme.colorScheme.surfaceContainer, modifier = Modifier.fillMaxWidth()) {
        Row(Modifier.padding(horizontal = 12.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            if (analysis == null) {
                Text(stringResource(R.string.engine_thinking), fontSize = 15.sp)
                return@Row
            }
            val ahead = if (analysis.whiteAhead) EvaluationColors.white else EvaluationColors.black
            val ink = if (analysis.whiteAhead) EvaluationColors.black else EvaluationColors.white
            Surface(color = ahead, contentColor = ink, shape = RoundedCornerShape(6.dp),
                border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant)) {
                Text(analysis.text, fontSize = 17.sp, fontWeight = FontWeight.Bold,
                    modifier = Modifier.padding(horizontal = 8.dp, vertical = 2.dp))
            }
            // Replaying the line to write it is work: once per update, not per recomposition.
            val line = remember(analysis, position) { "d${analysis.depth}  ${pvText(position, analysis.pv, 10)}" }
            Text(
                line,
                fontFamily = FigurineFamily,
                fontSize = 15.sp,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.padding(start = 10.dp),
            )
        }
    }
}

/** Engine on, none installed: say so and offer one. */
@Composable
private fun NoEngine(vm: AppViewModel) {
    val context = androidx.compose.ui.platform.LocalContext.current
    Surface(color = MaterialTheme.colorScheme.surfaceContainer, modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(horizontal = 12.dp, vertical = 8.dp)) {
            Text(stringResource(R.string.engine_none), style = MaterialTheme.typography.bodyMedium)
            androidx.compose.material3.TextButton(onClick = { vm.installEngine(context) }) {
                Text(stringResource(R.string.engine_install))
            }
        }
    }
}

/** "12.♘f3 ♞c6 13.♗b5" — the principal variation from [position], at most [limit] moves. */
fun pvText(position: Position, pv: List<String>, limit: Int): String {
    val out = StringBuilder()
    var current = position
    for ((i, uci) in pv.take(limit).withIndex()) {
        val move = current.parseUci(uci) ?: break
        if (i == 0 || current.sideToMove == org.pragmachess.mobile.chess.Side.White) {
            if (out.isNotEmpty()) out.append(' ')
            out.append(current.moveNumberText())
        } else {
            out.append(' ')
        }
        out.append(figurineSan(current.san(move)))
        current = current.play(move)
    }
    return out.toString()
}
