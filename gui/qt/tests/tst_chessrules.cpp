#include "app/ChessPosition.h"
#include "app/MoveExplanation.h"

#include <QTest>

namespace {

qint64 perft(const ChessPosition &position, int depth)
{
    if (depth == 0)
        return 1;
    const QList<ChessMove> moves = position.legalMoves();
    if (depth == 1)
        return moves.size();
    qint64 nodes = 0;
    for (const ChessMove &move : moves) {
        ChessPosition next = position;
        next.play(move);
        nodes += perft(next, depth - 1);
    }
    return nodes;
}

ChessPosition afterMoves(const QString &fen, const QStringList &uciMoves)
{
    ChessPosition position = fen.isEmpty() ? ChessPosition::startingPosition() : *ChessPosition::fromFen(fen);
    for (const QString &uci : uciMoves)
        position.play(*position.moveFromUci(uci));
    return position;
}

EngineEvaluation centipawns(int value, const QStringList &pv)
{
    EngineEvaluation evaluation;
    evaluation.centipawns = value;
    evaluation.depth = 20;
    evaluation.pv = pv;
    return evaluation;
}

ExplanationInput inputFor(const QStringList &movesBefore, const QString &played,
                          const EngineEvaluation &beforeEvaluation, const EngineEvaluation &afterEvaluation)
{
    ExplanationInput input;
    input.before = afterMoves(QString(), movesBefore);
    input.played = input.before->moveFromUci(played);
    input.beforeEvaluation = beforeEvaluation;
    input.after = *input.before;
    input.after.play(*input.played);
    input.afterEvaluation = afterEvaluation;
    return input;
}

} // namespace

class TestChessRules : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void perftCounts_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::addColumn<int>("depth");
        QTest::addColumn<qint64>("nodes");
        QTest::newRow("start") << QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") << 4 << qint64(197281);
        QTest::newRow("kiwipete") << QStringLiteral("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1") << 3 << qint64(97862);
        QTest::newRow("endgame") << QStringLiteral("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1") << 5 << qint64(674624);
        QTest::newRow("promotions") << QStringLiteral("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1") << 3 << qint64(9467);
        QTest::newRow("castling-checks") << QStringLiteral("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8") << 3 << qint64(62379);
    }

    void perftCounts()
    {
        QFETCH(QString, fen);
        QFETCH(int, depth);
        QFETCH(qint64, nodes);
        const std::optional<ChessPosition> position = ChessPosition::fromFen(fen);
        QVERIFY(position);
        QCOMPARE(position->fen(), fen);
        QCOMPARE(perft(*position, depth), nodes);
    }

    void san()
    {
        const ChessPosition start = ChessPosition::startingPosition();
        QCOMPARE(start.san(*start.moveFromUci(u"g1f3")), QStringLiteral("Nf3"));
        QCOMPARE(start.lineText({"e2e4", "e7e5", "g1f3"}), QStringLiteral("1.e4 e5 2.Nf3"));

        const ChessPosition knights = *ChessPosition::fromFen(QStringLiteral("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1"));
        QCOMPARE(knights.san(*knights.moveFromUci(u"b1d2")), QStringLiteral("Nbd2"));

        const ChessPosition promotion = *ChessPosition::fromFen(QStringLiteral("8/P3k3/8/8/8/8/8/4K3 w - - 0 1"));
        QCOMPARE(promotion.san(*promotion.moveFromUci(u"a7a8q")), QStringLiteral("a8=Q"));

        const ChessPosition castle = *ChessPosition::fromFen(QStringLiteral("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"));
        QCOMPARE(castle.san(*castle.moveFromUci(u"e8c8")), QStringLiteral("O-O-O"));
        QCOMPARE(castle.lineText({"e8g8"}), QStringLiteral("1…O-O"));

        const ChessPosition fool = afterMoves(QString(), {"f2f3", "e7e5", "g2g4"});
        QCOMPARE(fool.san(*fool.moveFromUci(u"d8h4")), QStringLiteral("Qh4#"));

        const ChessPosition enPassant = afterMoves(QString(), {"e2e4", "a7a6", "e4e5", "d7d5"});
        QCOMPARE(enPassant.fen(), QStringLiteral("rnbqkbnr/1pp1pppp/p7/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3"));
        QCOMPARE(enPassant.san(*enPassant.moveFromUci(u"e5d6")), QStringLiteral("exd6"));
    }

    void rejectsInvalidFen()
    {
        QVERIFY(!ChessPosition::fromFen(QStringLiteral("8/8/8/8/8/8/8/8 w - - 0 1")));
        // The side that just moved cannot be in check.
        QVERIFY(!ChessPosition::fromFen(QStringLiteral("4k3/8/8/8/8/8/8/4R1K1 w - - 0 1")));
    }

    void attackers()
    {
        const ChessPosition position = afterMoves(QString(), {"e2e4", "e7e5", "g1f3"});
        QCOMPARE(position.attackers(BoardState::squareFromName(u"e5"), Side::White),
                 QList<int>{BoardState::squareFromName(u"f3")});
    }

    void explainsHangingPiece()
    {
        // 3.Nxe5?? grabs a pawn but the knight is defended: 3…dxe5 leaves Black a piece for a pawn up.
        const ExplanationInput input = inputFor({"e2e4", "e7e5", "g1f3", "d7d6"}, QStringLiteral("f3e5"),
                                                centipawns(40, {"d2d4"}),
                                                centipawns(-190, {"d6e5", "b1c3", "g8f6"}));
        const MoveExplanation explanation = explainPosition(input);
        QCOMPARE(explanation.verdict, MoveExplanation::Verdict::Mistake);
        QCOMPARE(explanation.arrows.size(), 2);
        QCOMPARE(explanation.arrows.at(0).kind, BoardArrow::Kind::Alternative);
        QCOMPARE(explanation.arrows.at(1), (BoardArrow{BoardState::squareFromName(u"d6"), BoardState::squareFromName(u"e5"),
                                            BoardArrow::Kind::Refutation, 1}));
        QCOMPARE(explanation.lostPieces, QList<int>{BoardState::squareFromName(u"e5")});
        QVERIFY2(explanation.summary.contains(QStringLiteral("Black wins a knight for a pawn: 3…dxe5. Better was 3.d4.")),
                 qPrintable(explanation.summary));
    }

    void waitsForIntermediateMoves()
    {
        // After 4…exd4 White inserts 5.Bxf7+: the material is counted only once
        // the capture, the check and the recapture are over.
        const ExplanationInput input = inputFor({"e2e4", "e7e5", "g1f3", "d7d6", "f1c4", "h7h6"}, QStringLiteral("f3d4"),
                                                centipawns(60, {"d2d4"}),
                                                centipawns(-150, {"e5d4", "c4f7", "e8f7", "d1h5", "g7g6", "h5d5"}));
        const MoveExplanation explanation = explainPosition(input);
        QVERIFY2(isErrorVerdict(explanation.verdict), qPrintable(explanation.summary));
        QVERIFY2(explanation.summary.contains(QStringLiteral("Black wins a bishop and a knight for a pawn: 4…exd4 5.Bxf7+ Kxf7.")),
                 qPrintable(explanation.summary));
        QCOMPARE(explanation.arrows.size(), 4); // The better move and the three moves of the sequence.
    }

    void explainsAllowedMate()
    {
        EngineEvaluation mate;
        mate.isMate = true;
        mate.mateIn = 1;
        mate.mating = Side::Black;
        mate.pv = QStringList{"d8h4"};
        const ExplanationInput input = inputFor({"f2f3", "e7e5"}, QStringLiteral("g2g4"), centipawns(-60, {"e1f2"}), mate);
        const MoveExplanation explanation = explainPosition(input);
        QCOMPARE(explanation.verdict, MoveExplanation::Verdict::Blunder);
        QVERIFY(explanation.arrows.contains(BoardArrow{BoardState::squareFromName(u"d8"), BoardState::squareFromName(u"h4"),
                                                       BoardArrow::Kind::Refutation, 1}));
        QVERIFY2(explanation.summary.contains(QStringLiteral("Black mates in 1: 2…Qh4#")), qPrintable(explanation.summary));
    }

    void explainsWinningCapture()
    {
        const ExplanationInput input = inputFor({"e2e4", "e7e5", "g1f3", "d7d6", "f3d4"}, QStringLiteral("e5d4"),
                                                centipawns(-190, {"e5d4", "c2c3"}),
                                                centipawns(-190, {"c2c3", "d4c3"}));
        const MoveExplanation explanation = explainPosition(input);
        QCOMPARE(explanation.verdict, MoveExplanation::Verdict::Best);
        QVERIFY2(explanation.summary.contains(QStringLiteral("Best move (−1.9). Black wins a knight: 3…exd4.")),
                 qPrintable(explanation.summary));
    }

private:
    static bool isErrorVerdict(MoveExplanation::Verdict verdict)
    {
        return verdict == MoveExplanation::Verdict::Inaccuracy || verdict == MoveExplanation::Verdict::Mistake
            || verdict == MoveExplanation::Verdict::Blunder;
    }
};

QTEST_GUILESS_MAIN(TestChessRules)
#include "tst_chessrules.moc"
