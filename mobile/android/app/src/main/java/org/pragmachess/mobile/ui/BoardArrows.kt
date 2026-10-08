package org.pragmachess.mobile.ui

import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextMeasurer
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.TextUnitType
import org.pragmachess.mobile.explain.BoardArrow
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt

/** Where Explain is, said by the colour of the board's frame (the desktop's BoardBorder). */
enum class BoardBorder {
    Plain,
    /** The engine is searching: the frame breathes between plain and blue. */
    Thinking,
    /** An explanation is shown: the blue of the reply arrows. */
    Explained,
}

/** The desktop's arrow colours: a colour is a claim, red means material is falling. */
object ArrowColors {
    val refutation = Color(0xD0D43F32)
    val idea = Color(0xD02F8F44)
    val reply = Color(0xC03A6EB5)
    val alternative = Color(0xA82F8F44)
    val threat = Color(0xA8D43F32)
    /**
     * The plans, by rank: each route its own colour, so that two routes
     * through one square can be told apart. Not Explain's red, green and
     * blue, which are claims about material.
     */
    val plans = listOf(Color(0xC88E4FB5), Color(0xC8E07B1F), Color(0xC8179A94))

    fun plan(rank: Int): Color = plans[(rank - 1).coerceAtLeast(0) % plans.size]
    /** The frame of an explained board. */
    val explainFrame = Color(0xFF3A6EB5)

    fun of(kind: BoardArrow.Kind): Color = when (kind) {
        BoardArrow.Kind.Refutation -> refutation
        BoardArrow.Kind.Idea -> idea
        BoardArrow.Kind.Reply -> reply
        BoardArrow.Kind.Alternative -> alternative
        BoardArrow.Kind.Threat -> threat
        BoardArrow.Kind.Plan -> plan(1)
    }
}

/**
 * Explain on the board, drawn as the desktop's BoardWidget does: a red ring
 * on each piece lost along the line, then the arrows, knight moves bent along
 * their long leg, the better move dashed, each step numbered where it starts,
 * and the piece the better move moves small and faint where it goes.
 */
fun DrawScope.drawExplanation(
    arrows: List<BoardArrow>,
    lostPieces: List<Int>,
    squareSize: Float,
    topLeft: (Int) -> Offset,
    measurer: TextMeasurer,
    pieces: Map<Int, ImageBitmap> = emptyMap(),
    threatenedPieces: List<Int> = emptyList(),
) {
    fun center(square: Int) = topLeft(square) + Offset(squareSize / 2, squareSize / 2)
    for (square in lostPieces) {
        val inset = squareSize * 0.07f
        drawOval(ArrowColors.refutation, topLeft(square) + Offset(inset, inset),
            Size(squareSize - 2 * inset, squareSize - 2 * inset), style = Stroke(max(2f, squareSize * 0.06f)))
    }
    // Attacked, not falling: the same ring, dashed.
    for (square in threatenedPieces) {
        val inset = squareSize * 0.07f
        val width = max(2f, squareSize * 0.05f)
        drawOval(ArrowColors.refutation, topLeft(square) + Offset(inset, inset),
            Size(squareSize - 2 * inset, squareSize - 2 * inset),
            style = Stroke(width, pathEffect = androidx.compose.ui.graphics.PathEffect.dashPathEffect(floatArrayOf(width * 1.3f, width))))
    }
    // The strongest plan is drawn last, on top.
    for (arrow in arrows.sortedByDescending { if (it.kind == BoardArrow.Kind.Plan) it.step else 0 }) {
        if (arrow.from !in 0..63 || arrow.to !in 0..63 || arrow.from == arrow.to) continue
        val color = if (arrow.kind == BoardArrow.Kind.Plan) ArrowColors.plan(arrow.step) else ArrowColors.of(arrow.kind)
        // A route passes through its squares; knight moves bend along their long leg.
        val squares = listOf(arrow.from) + arrow.via + arrow.to
        val points = arrayListOf(center(arrow.from))
        for (i in 1 until squares.size) {
            val from = squares[i - 1]
            val to = squares[i]
            val fileDistance = abs(to % 8 - from % 8)
            val rankDistance = abs(to / 8 - from / 8)
            if (fileDistance + rankDistance == 3 && fileDistance > 0 && rankDistance > 0) {
                val corner = if (rankDistance == 2) (to / 8) * 8 + from % 8 else (from / 8) * 8 + to % 8
                points += center(corner)
            }
            points += center(to)
        }

        fun unit(from: Offset, to: Offset): Offset {
            val delta = to - from
            val length = hypot(delta.x, delta.y)
            return if (length > 0) delta / length else Offset.Zero
        }
        val lastFrom = points[points.size - 2]
        val lastUnit = unit(lastFrom, points.last())
        val lastLength = (points.last() - lastFrom).getDistance()
        val tip = points.last() - lastUnit * (squareSize * 0.1f)
        // Arrows to a neighbouring square get a shorter head, leaving room for the step number.
        val headBase = tip - lastUnit * min(squareSize * 0.42f, lastLength * 0.3f)
        val start = points.first() + unit(points[0], points[1]) * (squareSize * 0.2f)

        val shaft = Path().apply {
            moveTo(start.x, start.y)
            for (i in 1 until points.size - 1) lineTo(points[i].x, points[i].y)
            lineTo(headBase.x, headBase.y)
        }
        val width = squareSize * 0.15f
        drawPath(shaft, color, style = Stroke(
            width = width, cap = StrokeCap.Butt, join = if (arrow.via.isEmpty()) StrokeJoin.Miter else StrokeJoin.Round,
            pathEffect = if (arrow.kind == BoardArrow.Kind.Alternative || arrow.kind == BoardArrow.Kind.Threat)
                PathEffect.dashPathEffect(floatArrayOf(0.9f * width, 0.6f * width)) else null,
        ))
        val normal = Offset(-lastUnit.y, lastUnit.x)
        val halfHead = squareSize * 0.22f
        val head = Path().apply {
            moveTo(tip.x, tip.y)
            (headBase + normal * halfHead).let { lineTo(it.x, it.y) }
            (headBase - normal * halfHead).let { lineTo(it.x, it.y) }
            close()
        }
        drawPath(head, color)

        if (arrow.step <= 0 || arrow.kind == BoardArrow.Kind.Plan) continue // A plan's rank is its colour.
        // The step number where the arrow leaves its square, readable when several arrows end on one square.
        val radius = squareSize * 0.15f
        val badge = points.first() + unit(points[0], points[1]) * (squareSize * 0.4f)
        val badgeColor = color.copy(alpha = 1f).let { Color(it.red * 0.87f, it.green * 0.87f, it.blue * 0.87f) }
        drawCircle(badgeColor, radius, badge)
        drawCircle(Color(1f, 1f, 1f, 0.86f), radius, badge, style = Stroke(max(1f, squareSize * 0.02f)))
        val label = measurer.measure(arrow.step.toString(), TextStyle(
            color = Color.White, fontWeight = FontWeight.Bold,
            fontSize = TextUnit(max(8f, radius * 1.3f) / density / fontScale, TextUnitType.Sp),
        ))
        drawText(label, topLeft = badge - Offset(label.size.width / 2f, label.size.height / 2f))
    }
    // The piece the better move would have moved, small and faint where it
    // goes: the square the arrow leaves is often empty on this board.
    for (arrow in arrows) {
        val image = pieces[arrow.piece] ?: continue
        if (arrow.to !in 0..63) continue
        val ghost = squareSize * 0.55f
        val corner = center(arrow.to) - Offset(ghost / 2, ghost / 2)
        drawImage(image, srcSize = IntSize(image.width, image.height),
            dstOffset = IntOffset(corner.x.roundToInt(), corner.y.roundToInt()),
            dstSize = IntSize(ghost.roundToInt(), ghost.roundToInt()), alpha = 0.6f)
    }
}
