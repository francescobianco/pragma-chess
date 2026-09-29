package org.pragmachess.mobile.chess

/** A played move with its notation and the position it leads to. */
data class PlayedMove(val move: Move, val san: String, val after: Position)

/**
 * The main line of a game: a starting position and the moves after it.
 * Immutable; [play] and [truncate] return new lines.
 */
class GameLine(val start: Position, val moves: List<PlayedMove> = emptyList()) {
    val plyCount: Int get() = moves.size

    /** Position after [ply] moves (0 = the start). */
    fun positionAt(ply: Int): Position = if (ply <= 0) start else moves[ply - 1].after

    val last: Position get() = positionAt(plyCount)

    /** The line cut after [ply] moves with [move] played there. */
    fun play(ply: Int, move: Move): GameLine {
        val before = positionAt(ply)
        val played = PlayedMove(move, before.san(move), before.play(move))
        return GameLine(start, moves.take(ply) + played)
    }

    val sanText: String get() = moves.joinToString(" ") { it.san }
    val uciText: String get() = moves.joinToString(" ") { it.move.uci }

    companion object {
        /**
         * Replays a stored game: the UCI moves when there are any (exact), the
         * SAN moves otherwise; the notation shown is regenerated, so checks and
         * mates are always marked. Stops at the first move it cannot play.
         */
        fun replay(startFen: String?, uci: String, san: String): GameLine {
            val start = startFen?.takeIf { it.isNotBlank() }?.let { Position.fromFen(it) } ?: Position.starting()
            val played = ArrayList<PlayedMove>()
            var position = start
            val uciMoves = uci.split(' ').filter { it.isNotBlank() }
            val sanMoves = san.split(' ').filter { it.isNotBlank() }
            val count = maxOf(uciMoves.size, sanMoves.size)
            for (i in 0 until count) {
                val move = uciMoves.getOrNull(i)?.let { position.parseUci(it) }
                    ?: sanMoves.getOrNull(i)?.let { position.parseSan(it) }
                    ?: break
                val next = position.play(move)
                played.add(PlayedMove(move, position.san(move), next))
                position = next
            }
            return GameLine(start, played)
        }
    }
}

/** Replaces the piece letters of a SAN move with figurines: "Nxe5+" → "♘xe5+", "e8=Q" → "e8=♕". */
fun figurineSan(san: String): String {
    fun figurine(c: Char): Char = when (c) {
        'K' -> '♔'
        'Q' -> '♕'
        'R' -> '♖'
        'B' -> '♗'
        'N' -> '♘'
        else -> c
    }
    if (san.isEmpty()) return san
    val chars = san.toCharArray()
    chars[0] = figurine(chars[0])
    val promotion = san.indexOf('=')
    if (promotion >= 0 && promotion + 1 < chars.size) chars[promotion + 1] = figurine(chars[promotion + 1])
    return String(chars)
}
