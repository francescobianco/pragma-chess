package org.pragmachess.mobile.ui

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalViewConfiguration
import androidx.compose.ui.res.imageResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.pragmachess.mobile.R
import org.pragmachess.mobile.chess.Move
import org.pragmachess.mobile.chess.Piece
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.Square
import org.pragmachess.mobile.explain.BoardArrow
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.roundToInt

/** One breath of the frame while Explain waits for the engine (the desktop's). */
private const val PULSE_MS = 1100

/** The Good Companion pieces of the desktop, as bitmaps rendered from its SVGs. */
@Composable
fun rememberPieceImages(): Map<Int, ImageBitmap> {
    val ids = mapOf(
        Piece.of(Piece.KING, Side.White) to R.drawable.piece_white_king,
        Piece.of(Piece.QUEEN, Side.White) to R.drawable.piece_white_queen,
        Piece.of(Piece.ROOK, Side.White) to R.drawable.piece_white_rook,
        Piece.of(Piece.BISHOP, Side.White) to R.drawable.piece_white_bishop,
        Piece.of(Piece.KNIGHT, Side.White) to R.drawable.piece_white_knight,
        Piece.of(Piece.PAWN, Side.White) to R.drawable.piece_white_pawn,
        Piece.of(Piece.KING, Side.Black) to R.drawable.piece_black_king,
        Piece.of(Piece.QUEEN, Side.Black) to R.drawable.piece_black_queen,
        Piece.of(Piece.ROOK, Side.Black) to R.drawable.piece_black_rook,
        Piece.of(Piece.BISHOP, Side.Black) to R.drawable.piece_black_bishop,
        Piece.of(Piece.KNIGHT, Side.Black) to R.drawable.piece_black_knight,
        Piece.of(Piece.PAWN, Side.Black) to R.drawable.piece_black_pawn,
    )
    return ids.mapValues { (_, id) -> ImageBitmap.imageResource(id) }
}

/**
 * The board, drawn like the desktop's: its colours, coordinates in the
 * corners, the last move highlighted, a two-pixel frame. Moves are played by
 * tapping the piece and then the square, or by dragging it.
 */
@Composable
fun ChessBoard(
    position: Position,
    lastMove: Move?,
    flipped: Boolean,
    onMove: (Move) -> Unit,
    modifier: Modifier = Modifier,
    /** Explain's arrows and the pieces lost along its line (ringed in red). */
    arrows: List<BoardArrow> = emptyList(),
    lostPieces: List<Int> = emptyList(),
    border: BoardBorder = BoardBorder.Plain,
) {
    val pieces = rememberPieceImages()
    val measurer = rememberTextMeasurer()
    val plainFrame = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.28f)
    // Only the colour of the frame changes, never its width.
    val frameColor = when (border) {
        BoardBorder.Plain -> plainFrame
        BoardBorder.Explained -> ArrowColors.explainFrame
        BoardBorder.Thinking -> {
            val pulse by rememberInfiniteTransition(label = "explain").animateFloat(
                0f, 1f, infiniteRepeatable(tween(PULSE_MS, easing = LinearEasing)), label = "breath")
            // cos() turns the looping 0 → 1 into a breath with no seam.
            lerp(plainFrame, ArrowColors.explainFrame, 0.5f - 0.5f * cos(2 * PI.toFloat() * pulse))
        }
    }
    var selected by remember(position) { mutableStateOf(-1) }
    var dragFrom by remember(position) { mutableStateOf(-1) }
    var dragAt by remember { mutableStateOf(Offset.Zero) }
    var promotion by remember(position) { mutableStateOf<List<Move>?>(null) }
    val touchSlop = LocalViewConfiguration.current.touchSlop

    fun squareAt(offset: Offset, side: Float): Int {
        val size = side / 8
        var file = (offset.x / size).toInt()
        var rank = 7 - (offset.y / size).toInt()
        if (file !in 0..7 || rank !in 0..7) return -1
        if (flipped) {
            file = 7 - file
            rank = 7 - rank
        }
        return Square.of(file, rank)
    }

    fun movable(square: Int) = square >= 0 && position.pieceAt(square) != Piece.NONE &&
        Piece.side(position.pieceAt(square)) == position.sideToMove && position.legalMoves().any { it.from == square }

    /** Plays from → to if legal; asks for the piece when it is a promotion. Returns whether it was a move. */
    fun attempt(from: Int, to: Int): Boolean {
        val candidates = position.legalMoves().filter { it.from == from && it.to == to }
        when {
            candidates.isEmpty() -> return false
            candidates.size == 1 -> onMove(candidates.single())
            else -> promotion = candidates
        }
        selected = -1
        return true
    }

    // What the draw phase needs, worked out once per change instead of once per frame.
    val targets = remember(position, selected) {
        if (selected < 0) emptyList() else position.legalMoves().filter { it.from == selected }.distinctBy { it.to }
    }
    val checkedKing = remember(position) { if (position.isCheck) position.kingSquare(position.sideToMove) else -1 }

    BoxWithConstraints(modifier.aspectRatio(1f)) {
        val sidePx = constraints.maxWidth.toFloat()
        val coordinates = remember(sidePx, flipped, measurer) { measureCoordinates(measurer, sidePx / 8, flipped) }
        Canvas(
            Modifier
                .aspectRatio(1f)
                .fillMaxWidth()
                // Rendered once into a texture and reused: while a piece is
                // dragged only its own small layer moves over it.
                .graphicsLayer { compositingStrategy = CompositingStrategy.Offscreen }
                .pointerInput(position, flipped) {
                    awaitEachGesture {
                        val down = awaitFirstDown()
                        val side = size.width.toFloat()
                        val from = squareAt(down.position, side)
                        var dragging = false
                        var pointer = down.position
                        while (true) {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull { it.id == down.id } ?: break
                            pointer = change.position
                            if (!dragging && movable(from) && (pointer - down.position).getDistance() > touchSlop) {
                                dragging = true
                                dragFrom = from
                                selected = from
                            }
                            if (dragging) {
                                dragAt = pointer
                                change.consume()
                            }
                            if (!change.pressed) break
                            if (change.positionChange() != Offset.Zero && dragging) change.consume()
                        }
                        val to = squareAt(pointer, side)
                        if (dragging) {
                            dragFrom = -1
                            if (to != from) attempt(from, to) else selected = from
                        } else if (selected >= 0 && selected != from && attempt(selected, from)) {
                            // Played by tapping the target square.
                        } else {
                            selected = if (from == selected || !movable(from)) -1 else from
                        }
                    }
                },
        ) {
            val size = this.size.width / 8
            val corner = 4.dp.toPx()
            val rounded = Path().apply {
                addRoundRect(androidx.compose.ui.geometry.RoundRect(0f, 0f, this@Canvas.size.width, this@Canvas.size.height, CornerRadius(corner)))
            }
            fun topLeft(square: Int): Offset {
                var file = Square.file(square)
                var rank = Square.rank(square)
                if (flipped) {
                    file = 7 - file
                    rank = 7 - rank
                }
                return Offset(file * size, (7 - rank) * size)
            }
            clipPath(rounded) {
                for (square in 0 until 64) {
                    val light = (Square.rank(square) + Square.file(square)) % 2 == 1
                    drawRect(if (light) BoardColors.light else BoardColors.dark, topLeft(square), Size(size, size))
                    if (lastMove != null && (square == lastMove.from || square == lastMove.to)) {
                        drawRect(BoardColors.lastMove, topLeft(square), Size(size, size))
                    }
                }
                for ((text, at) in coordinates) drawText(text, topLeft = at)
                if (selected >= 0) drawRect(BoardColors.selected, topLeft(selected), Size(size, size))
            }
            drawRoundRect(frameColor, topLeft = Offset(-1.dp.toPx(), -1.dp.toPx()),
                size = Size(this.size.width + 2.dp.toPx(), this.size.height + 2.dp.toPx()),
                cornerRadius = CornerRadius(corner + 1.dp.toPx()), style = Stroke(2.dp.toPx()))

            // A king in check glows red under the piece, as on the desktop.
            if (checkedKing >= 0) {
                val center = topLeft(checkedKing) + Offset(size / 2, size / 2)
                drawCircle(Brush.radialGradient(
                    listOf(BoardColors.check.copy(alpha = 0.85f), BoardColors.check.copy(alpha = 0.5f), Color.Transparent),
                    center, size * 0.6f), size * 0.6f, center)
            }
            for (square in 0 until 64) {
                val piece = position.pieceAt(square)
                if (piece == Piece.NONE || square == dragFrom) continue
                drawPiece(pieces.getValue(piece), topLeft(square), size)
            }
            if (selected >= 0) {
                for (move in targets) {
                    val center = topLeft(move.to) + Offset(size / 2, size / 2)
                    if (position.pieceAt(move.to) != Piece.NONE) {
                        drawCircle(BoardColors.moveHint, size * 0.46f, center, style = Stroke(size * 0.08f))
                    } else {
                        drawCircle(BoardColors.moveHint, size * 0.16f, center)
                    }
                }
            }
            drawExplanation(arrows, lostPieces, size, ::topLeft, measurer)
        }
        if (dragFrom >= 0) {
            // The dragged piece is a layer of its own that only moves: the
            // board under it is not drawn again while the finger slides.
            val image = pieces.getValue(position.pieceAt(dragFrom))
            val lifted = sidePx / 8 * 1.25f
            val liftedDp = with(androidx.compose.ui.platform.LocalDensity.current) { lifted.toDp() }
            Canvas(
                Modifier
                    .size(liftedDp)
                    // Lifted a little above the finger, so it stays visible.
                    .offset { IntOffset((dragAt.x - lifted / 2).roundToInt(), (dragAt.y - lifted * 0.9f).roundToInt()) },
            ) {
                drawPiece(image, Offset.Zero, this.size.width)
            }
        }
    }

    promotion?.let { choices ->
        PromotionDialog(position.sideToMove, pieces, onPick = { type ->
            choices.firstOrNull { it.promotion == type }?.let(onMove)
            promotion = null
        }, onDismiss = { promotion = null })
    }
}

private fun DrawScope.drawPiece(image: ImageBitmap, topLeft: Offset, size: Float) {
    drawImage(
        image,
        srcSize = IntSize(image.width, image.height),
        dstOffset = IntOffset(topLeft.x.roundToInt(), topLeft.y.roundToInt()),
        dstSize = IntSize(size.roundToInt(), size.roundToInt()),
    )
}

/** The file letters along the bottom and the rank numbers along the left, laid out once per size. */
private fun measureCoordinates(
    measurer: androidx.compose.ui.text.TextMeasurer,
    size: Float,
    flipped: Boolean,
): List<Pair<androidx.compose.ui.text.TextLayoutResult, Offset>> {
    val pad = size * 0.05f
    val fontPx = (size * 0.16f).coerceAtLeast(16f)
    // Pixels as sp at density 1: the layout is drawn in the canvas's pixels.
    val density = androidx.compose.ui.unit.Density(1f)
    val result = ArrayList<Pair<androidx.compose.ui.text.TextLayoutResult, Offset>>(16)
    for (i in 0..7) {
        val bottomSquare = if (flipped) 63 - i else i
        val leftSquare = if (flipped) 63 - i * 8 else i * 8
        val bottomLight = (Square.rank(bottomSquare) + Square.file(bottomSquare)) % 2 == 1
        val leftLight = (Square.rank(leftSquare) + Square.file(leftSquare)) % 2 == 1
        val fileText = measurer.measure(
            ('a' + Square.file(bottomSquare)).toString(),
            TextStyle(color = if (bottomLight) BoardColors.dark else BoardColors.light, fontSize = fontPx.sp, fontWeight = FontWeight.Bold),
            density = density,
        )
        result += fileText to Offset((i + 1) * size - pad - fileText.size.width, 8 * size - pad - fileText.size.height)
        val rankText = measurer.measure(
            ('1' + Square.rank(leftSquare)).toString(),
            TextStyle(color = if (leftLight) BoardColors.dark else BoardColors.light, fontSize = fontPx.sp, fontWeight = FontWeight.Bold),
            density = density,
        )
        result += rankText to Offset(pad, (7 - i) * size + pad)
    }
    return result
}

@Composable
private fun PromotionDialog(side: Side, pieces: Map<Int, ImageBitmap>, onPick: (Int) -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.promote_to)) },
        text = {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
                for (type in listOf(Piece.QUEEN, Piece.ROOK, Piece.BISHOP, Piece.KNIGHT)) {
                    Box(Modifier.size(64.dp).clickable { onPick(type) }.padding(4.dp)) {
                        Image(pieces.getValue(Piece.of(type, side)), contentDescription = Piece.letter(type))
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(android.R.string.cancel)) } },
    )
}
