#include "SmartChess.h"

#include "SmartInterpreter.h"
#include "SmartPrograms.h"

#include <QCoreApplication>
#include <QRegularExpression>

namespace SmartChess {

namespace {

// Names are plain C strings: a QString temporary would make GCC fear for the
// references these return.
QString fn(const char *name)
{
    return QLatin1String(name);
}

void expect(const char *name, const std::vector<SmartValue> &args, int count)
{
    SmartInterpreter::expectArguments(fn(name), args, count);
}

template <typename T>
const T &objectArgument(const char *name, const char *kind, const std::vector<SmartValue> &args, int index)
{
    const std::shared_ptr<T> object = index < int(args.size()) ? args.at(index).as<T>() : nullptr;
    if (!object)
        SmartInterpreter::fail(QStringLiteral("%1 needs a %2 as argument %3, not %4")
                                   .arg(fn(name), fn(kind)).arg(index + 1)
                                   .arg(index < int(args.size()) ? args.at(index).typeName() : QStringLiteral("nothing")));
    return *object;
}

const EngineEvaluation &evaluationArgument(const char *name, const std::vector<SmartValue> &args, int index)
{
    return objectArgument<EvaluationObject>(name, "evaluation", args, index).evaluation;
}

const ChessPosition &positionArgument(const char *name, const std::vector<SmartValue> &args, int index)
{
    return objectArgument<PositionObject>(name, "position", args, index).position;
}

Side sideArgument(const char *name, const std::vector<SmartValue> &args, int index)
{
    const double value = SmartInterpreter::numberArgument(fn(name), args, index);
    if (value != 1 && value != -1)
        SmartInterpreter::fail(QStringLiteral("%1 needs a side (WHITE or BLACK) as argument %2").arg(fn(name)).arg(index + 1));
    return value > 0 ? Side::White : Side::Black;
}

int squareArgument(const char *name, const std::vector<SmartValue> &args, int index)
{
    const int square = SmartInterpreter::intArgument(fn(name), args, index);
    if (square < 0 || square > 63)
        SmartInterpreter::fail(QStringLiteral("%1: %2 is not a square (0 to 63)").arg(fn(name)).arg(square));
    return square;
}

/// A move in UCI ("e2e4", "e7e8q"): its squares, or fails.
QString moveArgument(const char *name, const std::vector<SmartValue> &args, int index)
{
    const QString uci = SmartInterpreter::textArgument(fn(name), args, index);
    if (uci.size() < 4 || BoardState::squareFromName(QStringView(uci).left(2)) < 0
        || BoardState::squareFromName(QStringView(uci).mid(2, 2)) < 0)
        SmartInterpreter::fail(QStringLiteral("%1: \"%2\" is not a move").arg(fn(name), uci));
    return uci;
}

QStringList movesArgument(const char *name, const std::vector<SmartValue> &args, int index)
{
    QStringList moves;
    const QString function = fn(name);
    for (const SmartValue &move : SmartInterpreter::listArgument(function, args, index)) {
        if (!move.isText())
            SmartInterpreter::fail(QStringLiteral("%1: a list of moves holds a %2").arg(fn(name), move.typeName()));
        moves << move.text();
    }
    return moves;
}

SmartValue texts(const QStringList &list)
{
    std::vector<SmartValue> items;
    for (const QString &text : list)
        items.emplace_back(text);
    return SmartValue(std::move(items));
}

BoardArrow::Kind arrowKind(const QString &kind)
{
    if (kind == QLatin1String("refutation"))
        return BoardArrow::Kind::Refutation;
    if (kind == QLatin1String("reply"))
        return BoardArrow::Kind::Reply;
    if (kind == QLatin1String("alternative"))
        return BoardArrow::Kind::Alternative;
    if (kind != QLatin1String("idea"))
        SmartInterpreter::fail(QStringLiteral("ARROW: \"%1\" is not a kind of arrow").arg(kind));
    return BoardArrow::Kind::Idea;
}

MoveExplanation::Verdict verdictNamed(const QString &name)
{
    if (name == QLatin1String("best"))
        return MoveExplanation::Verdict::Best;
    if (name == QLatin1String("good"))
        return MoveExplanation::Verdict::Good;
    if (name == QLatin1String("inaccuracy"))
        return MoveExplanation::Verdict::Inaccuracy;
    if (name == QLatin1String("mistake"))
        return MoveExplanation::Verdict::Mistake;
    if (name == QLatin1String("blunder"))
        return MoveExplanation::Verdict::Blunder;
    return MoveExplanation::Verdict::None;
}

} // namespace

SmartValue side(Side side)
{
    return SmartValue(side == Side::White ? 1 : -1);
}

SmartValue position(const ChessPosition &position)
{
    return SmartValue(std::make_shared<PositionObject>(position));
}

SmartValue evaluation(const EngineEvaluation &evaluation)
{
    return SmartValue(std::make_shared<EvaluationObject>(evaluation));
}

QString text(const QString &sentence, const QStringList &args)
{
    const QString translated = QCoreApplication::translate("MoveExplanation", sentence.toUtf8().constData());
    static const QRegularExpression placeholder(QStringLiteral("%(\\d)"));
    QString result;
    qsizetype done = 0;
    for (const QRegularExpressionMatch &match : placeholder.globalMatch(translated)) {
        const int index = match.captured(1).toInt() - 1;
        result += translated.mid(done, match.capturedStart() - done);
        result += index >= 0 && index < args.size() ? args.at(index) : match.captured();
        done = match.capturedEnd();
    }
    return result + translated.mid(done);
}

void define(SmartInterpreter &smart, Output &output)
{
    smart.defineConstant(QStringLiteral("WHITE"), side(Side::White));
    smart.defineConstant(QStringLiteral("BLACK"), side(Side::Black));
    smart.defineConstant(QStringLiteral("PAWN"), int(PieceType::Pawn));
    smart.defineConstant(QStringLiteral("KNIGHT"), int(PieceType::Knight));
    smart.defineConstant(QStringLiteral("BISHOP"), int(PieceType::Bishop));
    smart.defineConstant(QStringLiteral("ROOK"), int(PieceType::Rook));
    smart.defineConstant(QStringLiteral("QUEEN"), int(PieceType::Queen));
    smart.defineConstant(QStringLiteral("KING"), int(PieceType::King));

    // Evaluations.
    smart.define(QStringLiteral("SHARE"), [](const std::vector<SmartValue> &args) {
        expect("SHARE", args, 2);
        return SmartValue(evaluationArgument("SHARE", args, 0).shareFor(sideArgument("SHARE", args, 1)));
    });
    smart.define(QStringLiteral("BESTMOVE"), [](const std::vector<SmartValue> &args) {
        expect("BESTMOVE", args, 1);
        const EngineEvaluation &evaluation = evaluationArgument("BESTMOVE", args, 0);
        return SmartValue(evaluation.pv.isEmpty() ? QString() : evaluation.pv.first());
    });
    smart.define(QStringLiteral("ISMATE"), [](const std::vector<SmartValue> &args) {
        expect("ISMATE", args, 1);
        return SmartValue(evaluationArgument("ISMATE", args, 0).isMate);
    });
    smart.define(QStringLiteral("MATEIN"), [](const std::vector<SmartValue> &args) {
        expect("MATEIN", args, 1);
        return SmartValue(evaluationArgument("MATEIN", args, 0).mateIn);
    });
    smart.define(QStringLiteral("MATING"), [](const std::vector<SmartValue> &args) {
        expect("MATING", args, 1);
        return side(evaluationArgument("MATING", args, 0).mating);
    });
    smart.define(QStringLiteral("CP"), [](const std::vector<SmartValue> &args) {
        expect("CP", args, 1);
        return SmartValue(evaluationArgument("CP", args, 0).centipawns);
    });
    smart.define(QStringLiteral("CPFOR"), [](const std::vector<SmartValue> &args) {
        expect("CPFOR", args, 2);
        return SmartValue(evaluationArgument("CPFOR", args, 0).centipawnsFor(sideArgument("CPFOR", args, 1)));
    });
    smart.define(QStringLiteral("PV"), [](const std::vector<SmartValue> &args) {
        expect("PV", args, 1);
        return texts(evaluationArgument("PV", args, 0).pv);
    });
    smart.define(QStringLiteral("DEPTH"), [](const std::vector<SmartValue> &args) {
        expect("DEPTH", args, 1);
        return SmartValue(evaluationArgument("DEPTH", args, 0).depth);
    });
    smart.define(QStringLiteral("EVALTEXT"), [](const std::vector<SmartValue> &args) {
        expect("EVALTEXT", args, 1);
        return SmartValue(evaluationArgument("EVALTEXT", args, 0).text());
    });

    // Positions.
    smart.define(QStringLiteral("PLAY"), [](const std::vector<SmartValue> &args) {
        expect("PLAY", args, 2);
        ChessPosition position = positionArgument("PLAY", args, 0);
        const std::optional<ChessMove> move = position.moveFromUci(moveArgument("PLAY", args, 1));
        if (!move)
            return SmartValue();
        position.play(*move);
        return SmartChess::position(position);
    });
    smart.define(QStringLiteral("SIDETOMOVE"), [](const std::vector<SmartValue> &args) {
        expect("SIDETOMOVE", args, 1);
        return side(positionArgument("SIDETOMOVE", args, 0).sideToMove());
    });
    smart.define(QStringLiteral("MATERIAL"), [](const std::vector<SmartValue> &args) {
        expect("MATERIAL", args, 1);
        return SmartValue(positionArgument("MATERIAL", args, 0).material());
    });
    smart.define(QStringLiteral("INCHECK"), [](const std::vector<SmartValue> &args) {
        expect("INCHECK", args, 1);
        return SmartValue(positionArgument("INCHECK", args, 0).inCheck());
    });
    smart.define(QStringLiteral("CHECKMATE"), [](const std::vector<SmartValue> &args) {
        expect("CHECKMATE", args, 1);
        return SmartValue(positionArgument("CHECKMATE", args, 0).isCheckmate());
    });
    smart.define(QStringLiteral("STALEMATE"), [](const std::vector<SmartValue> &args) {
        expect("STALEMATE", args, 1);
        return SmartValue(positionArgument("STALEMATE", args, 0).isStalemate());
    });
    smart.define(QStringLiteral("CAPTURED"), [](const std::vector<SmartValue> &args) {
        expect("CAPTURED", args, 2);
        const ChessPosition &position = positionArgument("CAPTURED", args, 0);
        const std::optional<ChessMove> move = position.moveFromUci(moveArgument("CAPTURED", args, 1));
        return SmartValue(move ? int(position.capturedPiece(*move).type) : 0);
    });
    smart.define(QStringLiteral("PIECE"), [](const std::vector<SmartValue> &args) {
        expect("PIECE", args, 2);
        return SmartValue(int(positionArgument("PIECE", args, 0).at(squareArgument("PIECE", args, 1)).type));
    });
    smart.define(QStringLiteral("COUNT"), [](const std::vector<SmartValue> &args) {
        expect("COUNT", args, 3);
        const ChessPosition &position = positionArgument("COUNT", args, 0);
        const Piece wanted{PieceType(qBound(0, SmartInterpreter::intArgument(fn("COUNT"), args, 2), 6)),
                           sideArgument("COUNT", args, 1)};
        int count = 0;
        for (int square = 0; square < 64; ++square)
            count += position.at(square) == wanted ? 1 : 0;
        return SmartValue(count);
    });
    smart.define(QStringLiteral("LINETEXT"), [&output](const std::vector<SmartValue> &args) {
        expect("LINETEXT", args, 3);
        return SmartValue(positionArgument("LINETEXT", args, 0)
                              .lineText(movesArgument("LINETEXT", args, 1),
                                        SmartInterpreter::intArgument(fn("LINETEXT"), args, 2), output.sanStyle));
    });

    // Moves.
    smart.define(QStringLiteral("FROMSQ"), [](const std::vector<SmartValue> &args) {
        expect("FROMSQ", args, 1);
        return SmartValue(BoardState::squareFromName(QStringView(moveArgument("FROMSQ", args, 0)).left(2)));
    });
    smart.define(QStringLiteral("TOSQ"), [](const std::vector<SmartValue> &args) {
        expect("TOSQ", args, 1);
        return SmartValue(BoardState::squareFromName(QStringView(moveArgument("TOSQ", args, 0)).mid(2, 2)));
    });
    smart.define(QStringLiteral("PROMOTION"), [](const std::vector<SmartValue> &args) {
        expect("PROMOTION", args, 1);
        const QString uci = moveArgument("PROMOTION", args, 0);
        static const QString pieces = QStringLiteral("  nbrq");
        return SmartValue(uci.size() > 4 ? int(qMax(qsizetype(0), pieces.indexOf(uci.at(4).toLower()))) : 0);
    });

    // TUTOR.smart's judgement, so that Explain's verdict is the tutor's.
    smart.define(QStringLiteral("CLASSIFY"), [](const std::vector<SmartValue> &args) {
        expect("CLASSIFY", args, 5);
        SmartProgram *tutor = SmartPrograms::program(QStringLiteral("TUTOR.smart"));
        if (!tutor)
            SmartInterpreter::fail(QStringLiteral("CLASSIFY: TUTOR.smart cannot run"));
        QString error;
        const std::optional<SmartValue> verdict = tutor->interpreter.call(QStringLiteral("Classify"), args, &error);
        if (!verdict)
            SmartInterpreter::fail(QStringLiteral("CLASSIFY: TUTOR.smart %1").arg(error));
        return *verdict;
    });

    // Texts and commands.
    smart.define(QStringLiteral("TEXT"), [](const std::vector<SmartValue> &args) {
        const QString sentence = SmartInterpreter::textArgument(fn("TEXT"), args, 0);
        QStringList values;
        for (size_t i = 1; i < args.size(); ++i)
            values << args.at(i).toText();
        return SmartValue(text(sentence, values));
    });
    smart.define(QStringLiteral("SAY"), [&output](const std::vector<SmartValue> &args) {
        expect("SAY", args, 1);
        output.explanation.summary += SmartInterpreter::textArgument(fn("SAY"), args, 0);
        return SmartValue();
    });
    smart.define(QStringLiteral("NOTE"), [&output](const std::vector<SmartValue> &args) {
        expect("NOTE", args, 1);
        if (output.trace)
            output.explanation.trace << args.at(0).toText();
        return SmartValue();
    });
    smart.define(QStringLiteral("VERDICT"), [&output](const std::vector<SmartValue> &args) {
        expect("VERDICT", args, 1);
        output.explanation.verdict = verdictNamed(SmartInterpreter::textArgument(fn("VERDICT"), args, 0));
        return SmartValue();
    });
    smart.define(QStringLiteral("ARROW"), [&output](const std::vector<SmartValue> &args) {
        if (args.size() != 6)
            expect("ARROW", args, 4);
        Piece piece;
        if (args.size() == 6)
            piece = {PieceType(qBound(0, SmartInterpreter::intArgument(fn("ARROW"), args, 4), 6)),
                     sideArgument("ARROW", args, 5)};
        output.explanation.arrows << BoardArrow{squareArgument("ARROW", args, 0), squareArgument("ARROW", args, 1),
                                                arrowKind(SmartInterpreter::textArgument(fn("ARROW"), args, 2)),
                                                SmartInterpreter::intArgument(fn("ARROW"), args, 3), piece};
        return SmartValue();
    });
    smart.define(QStringLiteral("LOST"), [&output](const std::vector<SmartValue> &args) {
        expect("LOST", args, 1);
        output.explanation.lostPieces << squareArgument("LOST", args, 0);
        return SmartValue();
    });
    smart.define(QStringLiteral("PLAYBACK"), [&output](const std::vector<SmartValue> &args) {
        expect("PLAYBACK", args, 1);
        output.explanation.playback = movesArgument("PLAYBACK", args, 0);
        return SmartValue();
    });
}

} // namespace SmartChess
