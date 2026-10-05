package org.pragmachess.mobile.smart

import org.pragmachess.mobile.chess.Piece
import org.pragmachess.mobile.chess.Position
import org.pragmachess.mobile.chess.Side
import org.pragmachess.mobile.chess.Square
import org.pragmachess.mobile.explain.BoardArrow
import org.pragmachess.mobile.explain.EngineEvaluation
import org.pragmachess.mobile.explain.ExplainText
import org.pragmachess.mobile.explain.MoveExplanation
import org.pragmachess.mobile.explain.args
import org.pragmachess.mobile.explain.capturedPiece
import org.pragmachess.mobile.explain.lineText
import org.pragmachess.mobile.explain.material
import org.pragmachess.mobile.smart.SmartInterpreter.Companion.expectArguments
import org.pragmachess.mobile.smart.SmartInterpreter.Companion.fail
import org.pragmachess.mobile.smart.SmartInterpreter.Companion.intArgument
import org.pragmachess.mobile.smart.SmartInterpreter.Companion.listArgument
import org.pragmachess.mobile.smart.SmartInterpreter.Companion.numberArgument
import org.pragmachess.mobile.smart.SmartInterpreter.Companion.textArgument

/**
 * The chess a SMART program is given by the app: the sides and pieces,
 * positions, the engine's evaluations, and the commands that collect what a
 * program shows — what smart/TUTOR.smart and smart/EXPLAIN.smart list at
 * their top, with the same results as the desktop's SmartChess.
 */
object SmartChess {
    class PositionObject(val position: Position) : SmartObject {
        override val typeName = "position"
    }

    class EvaluationObject(val evaluation: EngineEvaluation) : SmartObject {
        override val typeName = "evaluation"
    }

    /** What a program's commands collected, and how it wants moves and texts written. Cleared before each call. */
    class Output {
        var figurines = false
        /** Keep the NOTEs. */
        var trace = false
        var text: ExplainText = ExplainText.English
        var verdict = MoveExplanation.Verdict.None
        val arrows = ArrayList<BoardArrow>()
        val lostPieces = ArrayList<Int>()
        val summary = StringBuilder()
        var playback: List<String> = emptyList()
        val notes = ArrayList<String>()

        fun clear() {
            verdict = MoveExplanation.Verdict.None
            arrows.clear()
            lostPieces.clear()
            summary.setLength(0)
            playback = emptyList()
            notes.clear()
        }

        fun explanation() = MoveExplanation(verdict, arrows.toList(), lostPieces.toList(), summary.toString(), playback,
            notes.toList())
    }

    fun side(side: Side): SmartValue = SmartValue.of(if (side == Side.White) 1 else -1)
    fun position(position: Position): SmartValue = SmartValue.Object(PositionObject(position))
    fun evaluation(evaluation: EngineEvaluation): SmartValue = SmartValue.Object(EvaluationObject(evaluation))

    private fun evaluationArgument(name: String, args: List<SmartValue>, index: Int): EngineEvaluation =
        ((args.getOrNull(index) as? SmartValue.Object)?.value as? EvaluationObject)?.evaluation
            ?: fail("$name needs a evaluation as argument ${index + 1}, not ${args.getOrNull(index)?.typeName ?: "nothing"}")

    private fun positionArgument(name: String, args: List<SmartValue>, index: Int): Position =
        ((args.getOrNull(index) as? SmartValue.Object)?.value as? PositionObject)?.position
            ?: fail("$name needs a position as argument ${index + 1}, not ${args.getOrNull(index)?.typeName ?: "nothing"}")

    private fun sideArgument(name: String, args: List<SmartValue>, index: Int): Side {
        val value = numberArgument(name, args, index)
        if (value != 1.0 && value != -1.0) fail("$name needs a side (WHITE or BLACK) as argument ${index + 1}")
        return if (value > 0) Side.White else Side.Black
    }

    private fun squareArgument(name: String, args: List<SmartValue>, index: Int): Int {
        val square = intArgument(name, args, index)
        if (square < 0 || square > 63) fail("$name: $square is not a square (0 to 63)")
        return square
    }

    private fun moveArgument(name: String, args: List<SmartValue>, index: Int): String {
        val uci = textArgument(name, args, index)
        if (uci.length < 4 || Square.parse(uci.substring(0, 2)) < 0 || Square.parse(uci.substring(2, 4)) < 0)
            fail("$name: \"$uci\" is not a move")
        return uci
    }

    private fun movesArgument(name: String, args: List<SmartValue>, index: Int): List<String> =
        listArgument(name, args, index).map { (it as? SmartValue.Text)?.value ?: fail("$name: a list of moves holds a ${it.typeName}") }

    private fun arrowKind(kind: String) = when (kind) {
        "refutation" -> BoardArrow.Kind.Refutation
        "reply" -> BoardArrow.Kind.Reply
        "alternative" -> BoardArrow.Kind.Alternative
        "idea" -> BoardArrow.Kind.Idea
        else -> fail("ARROW: \"$kind\" is not a kind of arrow")
    }

    private fun verdictNamed(name: String) = when (name) {
        "best" -> MoveExplanation.Verdict.Best
        "good" -> MoveExplanation.Verdict.Good
        "inaccuracy" -> MoveExplanation.Verdict.Inaccuracy
        "mistake" -> MoveExplanation.Verdict.Mistake
        "blunder" -> MoveExplanation.Verdict.Blunder
        else -> MoveExplanation.Verdict.None
    }

    /** Defines the constants, the chess functions and the commands on [smart]; the commands write into [output]. */
    fun define(smart: SmartInterpreter, output: Output) {
        smart.defineConstant("WHITE", side(Side.White))
        smart.defineConstant("BLACK", side(Side.Black))
        smart.defineConstant("PAWN", SmartValue.of(Piece.PAWN))
        smart.defineConstant("KNIGHT", SmartValue.of(Piece.KNIGHT))
        smart.defineConstant("BISHOP", SmartValue.of(Piece.BISHOP))
        smart.defineConstant("ROOK", SmartValue.of(Piece.ROOK))
        smart.defineConstant("QUEEN", SmartValue.of(Piece.QUEEN))
        smart.defineConstant("KING", SmartValue.of(Piece.KING))

        // Evaluations.
        smart.define("SHARE") { args ->
            expectArguments("SHARE", args, 2)
            SmartValue.of(evaluationArgument("SHARE", args, 0).shareFor(sideArgument("SHARE", args, 1)))
        }
        smart.define("BESTMOVE") { args ->
            expectArguments("BESTMOVE", args, 1)
            SmartValue.Text(evaluationArgument("BESTMOVE", args, 0).pv.firstOrNull().orEmpty())
        }
        smart.define("ISMATE") { args ->
            expectArguments("ISMATE", args, 1)
            SmartValue.of(evaluationArgument("ISMATE", args, 0).isMate)
        }
        smart.define("MATEIN") { args ->
            expectArguments("MATEIN", args, 1)
            SmartValue.of(evaluationArgument("MATEIN", args, 0).mateIn)
        }
        smart.define("MATING") { args ->
            expectArguments("MATING", args, 1)
            side(evaluationArgument("MATING", args, 0).mating)
        }
        smart.define("CP") { args ->
            expectArguments("CP", args, 1)
            SmartValue.of(evaluationArgument("CP", args, 0).centipawns)
        }
        smart.define("CPFOR") { args ->
            expectArguments("CPFOR", args, 2)
            SmartValue.of(evaluationArgument("CPFOR", args, 0).centipawnsFor(sideArgument("CPFOR", args, 1)))
        }
        smart.define("PV") { args ->
            expectArguments("PV", args, 1)
            SmartValue.Items(evaluationArgument("PV", args, 0).pv.map { SmartValue.Text(it) })
        }
        smart.define("DEPTH") { args ->
            expectArguments("DEPTH", args, 1)
            SmartValue.of(evaluationArgument("DEPTH", args, 0).depth)
        }
        smart.define("EVALTEXT") { args ->
            expectArguments("EVALTEXT", args, 1)
            SmartValue.Text(evaluationArgument("EVALTEXT", args, 0).text)
        }

        // Positions.
        smart.define("PLAY") { args ->
            expectArguments("PLAY", args, 2)
            val position = positionArgument("PLAY", args, 0)
            val move = position.parseUci(moveArgument("PLAY", args, 1)) ?: return@define SmartValue.None
            position(position.play(move))
        }
        smart.define("SIDETOMOVE") { args ->
            expectArguments("SIDETOMOVE", args, 1)
            side(positionArgument("SIDETOMOVE", args, 0).sideToMove)
        }
        smart.define("MATERIAL") { args ->
            expectArguments("MATERIAL", args, 1)
            SmartValue.of(positionArgument("MATERIAL", args, 0).material())
        }
        smart.define("INCHECK") { args ->
            expectArguments("INCHECK", args, 1)
            SmartValue.of(positionArgument("INCHECK", args, 0).isCheck)
        }
        smart.define("CHECKMATE") { args ->
            expectArguments("CHECKMATE", args, 1)
            SmartValue.of(positionArgument("CHECKMATE", args, 0).isCheckmate)
        }
        smart.define("STALEMATE") { args ->
            expectArguments("STALEMATE", args, 1)
            SmartValue.of(positionArgument("STALEMATE", args, 0).isStalemate)
        }
        smart.define("CAPTURED") { args ->
            expectArguments("CAPTURED", args, 2)
            val position = positionArgument("CAPTURED", args, 0)
            val move = position.parseUci(moveArgument("CAPTURED", args, 1))
            SmartValue.of(if (move == null) 0 else Piece.type(position.capturedPiece(move)))
        }
        smart.define("PIECE") { args ->
            expectArguments("PIECE", args, 2)
            SmartValue.of(Piece.type(positionArgument("PIECE", args, 0).pieceAt(squareArgument("PIECE", args, 1))))
        }
        smart.define("COUNT") { args ->
            expectArguments("COUNT", args, 3)
            val position = positionArgument("COUNT", args, 0)
            val wanted = Piece.of(intArgument("COUNT", args, 2).coerceIn(0, 6), sideArgument("COUNT", args, 1))
            SmartValue.of((0 until 64).count { position.pieceAt(it) == wanted })
        }
        smart.define("LINETEXT") { args ->
            expectArguments("LINETEXT", args, 3)
            SmartValue.Text(positionArgument("LINETEXT", args, 0).lineText(movesArgument("LINETEXT", args, 1),
                intArgument("LINETEXT", args, 2), output.figurines))
        }

        // Moves.
        smart.define("FROMSQ") { args ->
            expectArguments("FROMSQ", args, 1)
            SmartValue.of(Square.parse(moveArgument("FROMSQ", args, 0).substring(0, 2)))
        }
        smart.define("TOSQ") { args ->
            expectArguments("TOSQ", args, 1)
            SmartValue.of(Square.parse(moveArgument("TOSQ", args, 0).substring(2, 4)))
        }
        smart.define("PROMOTION") { args ->
            expectArguments("PROMOTION", args, 1)
            val uci = moveArgument("PROMOTION", args, 0)
            SmartValue.of(if (uci.length > 4) "  nbrq".indexOf(uci[4].lowercaseChar()).coerceAtLeast(0) else 0)
        }

        // TUTOR.smart's judgement, so that Explain's verdict is the tutor's.
        smart.define("CLASSIFY") { args ->
            expectArguments("CLASSIFY", args, 5)
            val tutor = SmartPrograms.program("TUTOR.smart") ?: fail("CLASSIFY: TUTOR.smart cannot run")
            tutor.interpreter.call("Classify", args).getOrElse { fail("CLASSIFY: TUTOR.smart ${it.message}") }
        }

        // Texts and commands.
        smart.define("TEXT") { args ->
            val sentence = textArgument("TEXT", args, 0)
            SmartValue.Text(output.text.tr(sentence).args(*args.drop(1).map { it.toText() }.toTypedArray()))
        }
        smart.define("SAY") { args ->
            expectArguments("SAY", args, 1)
            output.summary.append(textArgument("SAY", args, 0))
            SmartValue.None
        }
        smart.define("NOTE") { args ->
            expectArguments("NOTE", args, 1)
            if (output.trace) output.notes += args[0].toText()
            SmartValue.None
        }
        smart.define("VERDICT") { args ->
            expectArguments("VERDICT", args, 1)
            output.verdict = verdictNamed(textArgument("VERDICT", args, 0))
            SmartValue.None
        }
        smart.define("ARROW") { args ->
            if (args.size != 6) expectArguments("ARROW", args, 4)
            val piece = if (args.size == 6)
                Piece.of(intArgument("ARROW", args, 4).coerceIn(0, 6), sideArgument("ARROW", args, 5)) else Piece.NONE
            output.arrows += BoardArrow(squareArgument("ARROW", args, 0), squareArgument("ARROW", args, 1),
                arrowKind(textArgument("ARROW", args, 2)), intArgument("ARROW", args, 3), piece)
            SmartValue.None
        }
        smart.define("LOST") { args ->
            expectArguments("LOST", args, 1)
            output.lostPieces += squareArgument("LOST", args, 0)
            SmartValue.None
        }
        smart.define("PLAYBACK") { args ->
            expectArguments("PLAYBACK", args, 1)
            output.playback = movesArgument("PLAYBACK", args, 0)
            SmartValue.None
        }
    }
}
