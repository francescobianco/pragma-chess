package org.pragmachess.mobile.chess

/** A side of the board. */
enum class Side {
    White, Black;

    val opponent: Side get() = if (this == White) Black else White
}

/**
 * Pieces are small ints: the type (1..6) plus [BLACK] for Black's, 0 for an
 * empty square. Squares are 0..63 from a1, as in the desktop client.
 */
object Piece {
    const val NONE = 0
    const val PAWN = 1
    const val KNIGHT = 2
    const val BISHOP = 3
    const val ROOK = 4
    const val QUEEN = 5
    const val KING = 6
    const val BLACK = 8

    fun type(piece: Int) = piece and 7
    fun side(piece: Int) = if (piece and BLACK != 0) Side.Black else Side.White
    fun of(type: Int, side: Side) = if (side == Side.White) type else type or BLACK

    /** SAN letter of a piece type ("" for pawns). */
    fun letter(type: Int): String = when (type) {
        KNIGHT -> "N"
        BISHOP -> "B"
        ROOK -> "R"
        QUEEN -> "Q"
        KING -> "K"
        else -> ""
    }

    fun typeFromLetter(letter: Char): Int = when (letter.uppercaseChar()) {
        'P' -> PAWN
        'N' -> KNIGHT
        'B' -> BISHOP
        'R' -> ROOK
        'Q' -> QUEEN
        'K' -> KING
        else -> NONE
    }
}

object Square {
    fun file(square: Int) = square and 7
    fun rank(square: Int) = square shr 3
    fun of(file: Int, rank: Int) = rank * 8 + file
    fun name(square: Int) = "${'a' + file(square)}${'1' + rank(square)}"
    fun parse(text: String): Int {
        if (text.length != 2) return -1
        val file = text[0] - 'a'
        val rank = text[1] - '1'
        return if (file in 0..7 && rank in 0..7) of(file, rank) else -1
    }
}

/** A move; [promotion] is a piece type, 0 when none. Castling is the king's move of two squares. */
data class Move(val from: Int, val to: Int, val promotion: Int = Piece.NONE) {
    val uci: String
        get() = Square.name(from) + Square.name(to) +
            if (promotion != Piece.NONE) Piece.letter(promotion).lowercase() else ""
}

/** An immutable chess position with the rules: legal moves, SAN, FEN. */
class Position private constructor(
    private val board: IntArray,
    val sideToMove: Side,
    /** Castling rights: bit 0 white king side, 1 white queen side, 2 black king side, 3 black queen side. */
    val castling: Int,
    /** Square a pawn can capture en passant onto, or -1. */
    val enPassant: Int,
    val halfmoveClock: Int,
    val fullmoveNumber: Int,
) {
    fun pieceAt(square: Int): Int = board[square]

    fun kingSquare(side: Side): Int {
        val king = Piece.of(Piece.KING, side)
        for (square in 0 until 64) if (board[square] == king) return square
        return -1
    }

    fun isAttacked(square: Int, by: Side): Boolean {
        val file = Square.file(square)
        val rank = Square.rank(square)
        // Pawns attack diagonally forward, so look backwards from the square.
        val pawnRank = if (by == Side.White) rank - 1 else rank + 1
        if (pawnRank in 0..7) {
            for (df in intArrayOf(-1, 1)) {
                val f = file + df
                if (f in 0..7 && board[Square.of(f, pawnRank)] == Piece.of(Piece.PAWN, by)) return true
            }
        }
        for ((df, dr) in KNIGHT_STEPS) {
            val f = file + df
            val r = rank + dr
            if (f in 0..7 && r in 0..7 && board[Square.of(f, r)] == Piece.of(Piece.KNIGHT, by)) return true
        }
        for ((df, dr) in KING_STEPS) {
            val f = file + df
            val r = rank + dr
            if (f in 0..7 && r in 0..7 && board[Square.of(f, r)] == Piece.of(Piece.KING, by)) return true
        }
        for ((df, dr) in ROOK_DIRECTIONS) {
            if (slidingAttack(file, rank, df, dr, by, Piece.ROOK)) return true
        }
        for ((df, dr) in BISHOP_DIRECTIONS) {
            if (slidingAttack(file, rank, df, dr, by, Piece.BISHOP)) return true
        }
        return false
    }

    private fun slidingAttack(file: Int, rank: Int, df: Int, dr: Int, by: Side, slider: Int): Boolean {
        var f = file + df
        var r = rank + dr
        while (f in 0..7 && r in 0..7) {
            val piece = board[Square.of(f, r)]
            if (piece != Piece.NONE) {
                if (Piece.side(piece) != by) return false
                val type = Piece.type(piece)
                return type == slider || type == Piece.QUEEN
            }
            f += df
            r += dr
        }
        return false
    }

    val isCheck: Boolean get() = kingSquare(sideToMove).let { it >= 0 && isAttacked(it, sideToMove.opponent) }

    val isCheckmate: Boolean get() = isCheck && legalMoves().isEmpty()

    val isStalemate: Boolean get() = !isCheck && legalMoves().isEmpty()

    /** Pseudo-legal moves: follow the piece rules, may leave the own king in check. */
    private fun pseudoLegalMoves(): List<Move> {
        val moves = ArrayList<Move>(48)
        val us = sideToMove
        for (from in 0 until 64) {
            val piece = board[from]
            if (piece == Piece.NONE || Piece.side(piece) != us) continue
            val file = Square.file(from)
            val rank = Square.rank(from)
            when (Piece.type(piece)) {
                Piece.PAWN -> pawnMoves(from, file, rank, moves)
                Piece.KNIGHT -> stepMoves(from, file, rank, KNIGHT_STEPS, moves)
                Piece.BISHOP -> slideMoves(from, file, rank, BISHOP_DIRECTIONS, moves)
                Piece.ROOK -> slideMoves(from, file, rank, ROOK_DIRECTIONS, moves)
                Piece.QUEEN -> {
                    slideMoves(from, file, rank, ROOK_DIRECTIONS, moves)
                    slideMoves(from, file, rank, BISHOP_DIRECTIONS, moves)
                }
                Piece.KING -> {
                    stepMoves(from, file, rank, KING_STEPS, moves)
                    castlingMoves(from, moves)
                }
            }
        }
        return moves
    }

    private fun pawnMoves(from: Int, file: Int, rank: Int, moves: MutableList<Move>) {
        val forward = if (sideToMove == Side.White) 1 else -1
        val startRank = if (sideToMove == Side.White) 1 else 6
        val lastRank = if (sideToMove == Side.White) 7 else 0
        fun add(to: Int) {
            if (Square.rank(to) == lastRank) {
                for (promotion in PROMOTIONS) moves.add(Move(from, to, promotion))
            } else {
                moves.add(Move(from, to))
            }
        }
        val oneRank = rank + forward
        if (oneRank !in 0..7) return
        val one = Square.of(file, oneRank)
        if (board[one] == Piece.NONE) {
            add(one)
            if (rank == startRank) {
                val two = Square.of(file, rank + 2 * forward)
                if (board[two] == Piece.NONE) moves.add(Move(from, two))
            }
        }
        for (df in intArrayOf(-1, 1)) {
            val f = file + df
            if (f !in 0..7) continue
            val to = Square.of(f, oneRank)
            val target = board[to]
            if (target != Piece.NONE && Piece.side(target) != sideToMove) add(to)
            else if (to == enPassant) moves.add(Move(from, to))
        }
    }

    private fun stepMoves(from: Int, file: Int, rank: Int, steps: Array<IntArray>, moves: MutableList<Move>) {
        for ((df, dr) in steps) {
            val f = file + df
            val r = rank + dr
            if (f !in 0..7 || r !in 0..7) continue
            val to = Square.of(f, r)
            val target = board[to]
            if (target == Piece.NONE || Piece.side(target) != sideToMove) moves.add(Move(from, to))
        }
    }

    private fun slideMoves(from: Int, file: Int, rank: Int, directions: Array<IntArray>, moves: MutableList<Move>) {
        for ((df, dr) in directions) {
            var f = file + df
            var r = rank + dr
            while (f in 0..7 && r in 0..7) {
                val to = Square.of(f, r)
                val target = board[to]
                if (target == Piece.NONE) {
                    moves.add(Move(from, to))
                } else {
                    if (Piece.side(target) != sideToMove) moves.add(Move(from, to))
                    break
                }
                f += df
                r += dr
            }
        }
    }

    private fun castlingMoves(from: Int, moves: MutableList<Move>) {
        val white = sideToMove == Side.White
        val home = if (white) 4 else 60
        if (from != home) return
        val them = sideToMove.opponent
        val kingSide = if (white) 1 else 4
        val queenSide = if (white) 2 else 8
        val rook = Piece.of(Piece.ROOK, sideToMove)
        if (castling and kingSide != 0 && board[home + 3] == rook &&
            board[home + 1] == Piece.NONE && board[home + 2] == Piece.NONE &&
            !isAttacked(home, them) && !isAttacked(home + 1, them) && !isAttacked(home + 2, them)
        ) moves.add(Move(home, home + 2))
        if (castling and queenSide != 0 && board[home - 4] == rook &&
            board[home - 1] == Piece.NONE && board[home - 2] == Piece.NONE && board[home - 3] == Piece.NONE &&
            !isAttacked(home, them) && !isAttacked(home - 1, them) && !isAttacked(home - 2, them)
        ) moves.add(Move(home, home - 2))
    }

    private var legalCache: List<Move>? = null

    fun legalMoves(): List<Move> {
        legalCache?.let { return it }
        val legal = pseudoLegalMoves().filter { move ->
            val next = play(move)
            val king = next.kingSquare(sideToMove)
            king < 0 || !next.isAttacked(king, sideToMove.opponent)
        }
        legalCache = legal
        return legal
    }

    fun isLegal(move: Move): Boolean = move in legalMoves()

    /** The position after [move], which must at least be pseudo-legal. */
    fun play(move: Move): Position {
        val next = board.copyOf()
        val piece = next[move.from]
        val type = Piece.type(piece)
        val captured = next[move.to]
        next[move.to] = if (move.promotion != Piece.NONE) Piece.of(move.promotion, sideToMove) else piece
        next[move.from] = Piece.NONE
        var nextEnPassant = -1
        if (type == Piece.PAWN) {
            if (move.to == enPassant && captured == Piece.NONE && Square.file(move.from) != Square.file(move.to)) {
                // The captured pawn stands beside the moving one.
                next[Square.of(Square.file(move.to), Square.rank(move.from))] = Piece.NONE
            }
            if (kotlin.math.abs(move.to - move.from) == 16) nextEnPassant = (move.from + move.to) / 2
        }
        if (type == Piece.KING && kotlin.math.abs(move.to - move.from) == 2) {
            val kingSide = move.to > move.from
            val rookFrom = if (kingSide) move.from + 3 else move.from - 4
            val rookTo = if (kingSide) move.from + 1 else move.from - 1
            next[rookTo] = next[rookFrom]
            next[rookFrom] = Piece.NONE
        }
        var rights = castling
        for (square in intArrayOf(move.from, move.to)) {
            rights = rights and when (square) {
                0 -> 2.inv()
                4 -> 3.inv()
                7 -> 1.inv()
                56 -> 8.inv()
                60 -> 12.inv()
                63 -> 4.inv()
                else -> 15
            }
        }
        val resetClock = type == Piece.PAWN || captured != Piece.NONE
        return Position(
            next,
            sideToMove.opponent,
            rights,
            nextEnPassant,
            if (resetClock) 0 else halfmoveClock + 1,
            if (sideToMove == Side.Black) fullmoveNumber + 1 else fullmoveNumber,
        )
    }

    /** Standard Algebraic Notation of a legal [move], with + or #. */
    fun san(move: Move): String {
        val piece = board[move.from]
        val type = Piece.type(piece)
        val text = StringBuilder()
        if (type == Piece.KING && kotlin.math.abs(move.to - move.from) == 2) {
            text.append(if (move.to > move.from) "O-O" else "O-O-O")
        } else {
            val capture = board[move.to] != Piece.NONE ||
                (type == Piece.PAWN && Square.file(move.from) != Square.file(move.to))
            if (type == Piece.PAWN) {
                if (capture) text.append('a' + Square.file(move.from))
            } else {
                text.append(Piece.letter(type))
                val rivals = legalMoves().filter {
                    it.to == move.to && it.from != move.from && board[it.from] == piece
                }
                if (rivals.isNotEmpty()) {
                    val sameFile = rivals.any { Square.file(it.from) == Square.file(move.from) }
                    val sameRank = rivals.any { Square.rank(it.from) == Square.rank(move.from) }
                    when {
                        !sameFile -> text.append('a' + Square.file(move.from))
                        !sameRank -> text.append('1' + Square.rank(move.from))
                        else -> text.append(Square.name(move.from))
                    }
                }
            }
            if (capture) text.append('x')
            text.append(Square.name(move.to))
            if (move.promotion != Piece.NONE) text.append('=').append(Piece.letter(move.promotion))
        }
        val next = play(move)
        if (next.isCheck) text.append(if (next.legalMoves().isEmpty()) '#' else '+')
        return text.toString()
    }

    /** The legal move written as [san] (tolerant of missing or extra +, #, x, !?), or null. */
    fun parseSan(san: String): Move? {
        val clean = san.trim().trimEnd('+', '#', '!', '?').replace("0", "O")
        return legalMoves().firstOrNull { stripSan(san(it)) == stripSan(clean) }
    }

    private fun stripSan(text: String) = text.trimEnd('+', '#').replace("x", "").replace("=", "")

    /** The legal move written in UCI ("e2e4", "e7e8q", castling as the king's two-square move), or null. */
    fun parseUci(uci: String): Move? {
        if (uci.length !in 4..5) return null
        val from = Square.parse(uci.substring(0, 2))
        val to = Square.parse(uci.substring(2, 4))
        if (from < 0 || to < 0) return null
        val promotion = if (uci.length == 5) Piece.typeFromLetter(uci[4]) else Piece.NONE
        var move = Move(from, to, promotion)
        // Castling written as king takes rook (e1h1), as some tools do.
        if (Piece.type(board[from]) == Piece.KING && board[to] == Piece.of(Piece.ROOK, sideToMove)) {
            move = Move(from, if (to > from) from + 2 else from - 2)
        }
        return move.takeIf { isLegal(it) }
    }

    fun fen(): String {
        val text = StringBuilder()
        for (rank in 7 downTo 0) {
            var empty = 0
            for (file in 0..7) {
                val piece = board[Square.of(file, rank)]
                if (piece == Piece.NONE) {
                    empty++
                    continue
                }
                if (empty > 0) text.append(empty).also { empty = 0 }
                val letter = Piece.letter(Piece.type(piece)).ifEmpty { "P" }
                text.append(if (Piece.side(piece) == Side.White) letter else letter.lowercase())
            }
            if (empty > 0) text.append(empty)
            if (rank > 0) text.append('/')
        }
        text.append(if (sideToMove == Side.White) " w " else " b ")
        val rights = buildString {
            if (castling and 1 != 0) append('K')
            if (castling and 2 != 0) append('Q')
            if (castling and 4 != 0) append('k')
            if (castling and 8 != 0) append('q')
        }
        text.append(rights.ifEmpty { "-" })
        text.append(' ').append(if (enPassant >= 0) Square.name(enPassant) else "-")
        text.append(' ').append(halfmoveClock).append(' ').append(fullmoveNumber)
        return text.toString()
    }

    /** "12." for White to move, "12…" for Black. */
    fun moveNumberText(): String = if (sideToMove == Side.White) "$fullmoveNumber." else "$fullmoveNumber…"

    override fun equals(other: Any?): Boolean =
        other is Position && board.contentEquals(other.board) && sideToMove == other.sideToMove &&
            castling == other.castling && enPassant == other.enPassant

    override fun hashCode(): Int = board.contentHashCode() * 31 + sideToMove.hashCode()

    companion object {
        const val STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

        private val KNIGHT_STEPS = arrayOf(
            intArrayOf(1, 2), intArrayOf(2, 1), intArrayOf(2, -1), intArrayOf(1, -2),
            intArrayOf(-1, -2), intArrayOf(-2, -1), intArrayOf(-2, 1), intArrayOf(-1, 2),
        )
        private val KING_STEPS = arrayOf(
            intArrayOf(1, 0), intArrayOf(1, 1), intArrayOf(0, 1), intArrayOf(-1, 1),
            intArrayOf(-1, 0), intArrayOf(-1, -1), intArrayOf(0, -1), intArrayOf(1, -1),
        )
        private val ROOK_DIRECTIONS = arrayOf(intArrayOf(1, 0), intArrayOf(-1, 0), intArrayOf(0, 1), intArrayOf(0, -1))
        private val BISHOP_DIRECTIONS = arrayOf(intArrayOf(1, 1), intArrayOf(1, -1), intArrayOf(-1, 1), intArrayOf(-1, -1))
        private val PROMOTIONS = intArrayOf(Piece.QUEEN, Piece.ROOK, Piece.BISHOP, Piece.KNIGHT)

        fun starting(): Position = fromFen(STARTING_FEN)!!

        /** The position described by [fen], or null when it cannot be read. */
        fun fromFen(fen: String): Position? {
            val fields = fen.trim().split(Regex("\\s+"))
            if (fields.size < 4) return null
            val board = IntArray(64)
            val ranks = fields[0].split('/')
            if (ranks.size != 8) return null
            for ((index, row) in ranks.withIndex()) {
                val rank = 7 - index
                var file = 0
                for (c in row) {
                    if (c.isDigit()) {
                        file += c - '0'
                    } else {
                        val type = Piece.typeFromLetter(c)
                        if (type == Piece.NONE || file > 7) return null
                        board[Square.of(file, rank)] = Piece.of(type, if (c.isUpperCase()) Side.White else Side.Black)
                        file++
                    }
                }
                if (file != 8) return null
            }
            val side = when (fields[1]) {
                "w" -> Side.White
                "b" -> Side.Black
                else -> return null
            }
            var rights = 0
            for (c in fields[2]) {
                rights = rights or when (c) {
                    'K' -> 1
                    'Q' -> 2
                    'k' -> 4
                    'q' -> 8
                    else -> 0
                }
            }
            val enPassant = if (fields[3] == "-") -1 else Square.parse(fields[3])
            val halfmove = fields.getOrNull(4)?.toIntOrNull() ?: 0
            val fullmove = fields.getOrNull(5)?.toIntOrNull() ?: 1
            return Position(board, side, rights, enPassant, halfmove, fullmove)
        }
    }
}
