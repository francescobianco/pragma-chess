#include "app/AdvantageProbe.h"
#include "app/ChessPosition.h"
#include "app/DatabaseOutline.h"
#include "app/ExplanationSearch.h"
#include "app/MoveExplanation.h"
#include "app/Pgn.h"
#include "app/SqliteGameDatabase.h"
#include "app/sources/ChessComFetch.h"
#include "app/sources/LichessFetch.h"
#include "app/sync/SyncManifest.h"

#include <QJsonDocument>
#include <QTemporaryDir>

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

        QCOMPARE(figurineSan(QStringLiteral("Nxe5+")), QStringLiteral("♘xe5+"));
        QCOMPARE(figurineSan(QStringLiteral("exd8=Q#")), QStringLiteral("exd8=♕#"));
        QCOMPARE(figurineSan(QStringLiteral("O-O")), QStringLiteral("O-O"));
        QCOMPARE(start.lineText({"e2e4", "e7e5", "g1f3"}, -1, SanStyle::Figurines), QStringLiteral("1.e4 e5 2.♘f3"));

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

    void capturedPieces()
    {
        const ChessPosition start = ChessPosition::startingPosition();
        const ChessPosition position = afterMoves(QString(), {"e2e4", "d7d5", "e4d5", "d8d5", "b1c3", "d5a5"});
        const PieceCounts captured = position.capturedSince(start);
        QCOMPARE(captured[int(Side::Black)][int(PieceType::Pawn)], 1);
        QCOMPARE(captured[int(Side::White)][int(PieceType::Pawn)], 1);
        QCOMPARE(captured[int(Side::White)][int(PieceType::Knight)], 0);

        // A promoted pawn is not a captured one; the queen it became is not "negative".
        const ChessPosition promoted = afterMoves(QStringLiteral("4k3/P7/8/8/8/8/8/4K3 w - - 0 1"), {"a7a8q"});
        const PieceCounts none = promoted.capturedSince(*ChessPosition::fromFen(QStringLiteral("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")));
        QCOMPARE(none[int(Side::White)][int(PieceType::Pawn)], 0);
        QCOMPARE(none[int(Side::White)][int(PieceType::Queen)], 0);
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

    void focusesLongRealizations()
    {
        // Evergreen game, 15…Qf5: the knight only falls eleven plies later (20…Nxe5 21.Rxe5).
        // Drawing the whole sequence says nothing; the decisive jump does.
        ExplanationInput input;
        QString error;
        const std::optional<Pgn::ParsedLine> game = Pgn::parseLine(
            QStringLiteral("1.e4 e5 2.Nf3 Nc6 3.Bc4 Bc5 4.b4 Bxb4 5.c3 Ba5 6.d4 exd4 7.O-O d3 8.Qb3 Qf6 9.e5 Qg6 "
                           "10.Re1 Nge7 11.Ba3 b5 12.Qxb5 Rb8 13.Qa4 Bb6 14.Nbd2 Bb7 15.Ne4"),
            QString(), &error);
        QVERIFY2(game, qPrintable(error));
        QStringList moves;
        for (const MoveRecord &move : game->moves)
            moves << move.uci;
        input = inputFor(moves, QStringLiteral("g6f5"), centipawns(140, {"d3d2", "e4d2"}),
                         centipawns(430, {"c4d3", "f5e6", "a1d1", "e8g8", "e4g5", "e6h6", "d3h7", "g8h8", "d1d7",
                                          "c6e5", "e1e5", "f7f6"}));
        const MoveExplanation explanation = explainPosition(input);
        QCOMPARE(explanation.verdict, MoveExplanation::Verdict::Mistake);
        QCOMPARE(explanation.arrows,
                 (QList<BoardArrow>{{BoardState::squareFromName(u"c6"), BoardState::squareFromName(u"e5"),
                                     BoardArrow::Kind::Reply, 1},
                                    {BoardState::squareFromName(u"e1"), BoardState::squareFromName(u"e5"),
                                     BoardArrow::Kind::Refutation, 2}}));
        QCOMPARE(explanation.lostPieces, QList<int>{BoardState::squareFromName(u"c6")});
        QVERIFY2(explanation.summary.contains(QStringLiteral("20.Rxd7 Nxe5 21.Rxe5.")), qPrintable(explanation.summary));
        QVERIFY(explanation.playback.isEmpty()); // Only mates are played.
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
        QCOMPARE(explanation.playback, QStringList{"d8h4"});
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

    void pgn()
    {
        GameRecord game;
        game.white = QStringLiteral("Anderssen, Adolf");
        game.black = QStringLiteral("Kieseritzky, Lionel");
        game.result = QStringLiteral("1-0");
        for (const char *uci : {"e2e4", "e7e5", "f2f4", "e5f4"})
            game.moves << MoveRecord{QString(), QString::fromLatin1(uci)};

        QCOMPARE(Pgn::moveText(game, 3), QStringLiteral("1.e4 e5 2.f4"));
        QCOMPARE(Pgn::game(game),
                 QStringLiteral("[Event \"?\"]\n[Site \"?\"]\n[Date \"????.??.??\"]\n[Round \"?\"]\n"
                                "[White \"Anderssen, Adolf\"]\n[Black \"Kieseritzky, Lionel\"]\n[Result \"1-0\"]\n\n"
                                "1.e4 e5 2.f4 exf4 1-0\n"));
        // Cut before the end: the result is unknown.
        QVERIFY(Pgn::game(game, 2).endsWith(QStringLiteral("\n1.e4 e5 *\n")));

        game.startFen = QStringLiteral("4k3/8/8/8/8/8/4P3/4K3 b - - 0 1");
        game.moves = {MoveRecord{QString(), QStringLiteral("e8d7")}};
        QVERIFY(Pgn::game(game).contains(QStringLiteral("[SetUp \"1\"]\n[FEN \"4k3/8/8/8/8/8/4P3/4K3 b - - 0 1\"]")));
        QCOMPARE(Pgn::moveText(game), QStringLiteral("1…Kd7"));
    }

    void parsesPastedLines()
    {
        QString error;
        const std::optional<Pgn::ParsedLine> pgn = Pgn::parseLine(
            QStringLiteral("[Event \"Casual\"]\n1. e4 e5 {open game} 2.Nf3 (2.f4!? exf4) d6?! 3.Nxe5?? $4 dxe5 0-1"),
            QString(), &error);
        QVERIFY2(pgn, qPrintable(error));
        QStringList uci;
        for (const MoveRecord &move : pgn->moves)
            uci << move.uci;
        QCOMPARE(uci, (QStringList{"e2e4", "e7e5", "g1f3", "d7d6", "f3e5", "d6e5"}));
        QCOMPARE(pgn->moves.at(4).san, QStringLiteral("Nxe5"));

        const std::optional<Pgn::ParsedLine> loose = Pgn::parseLine(
            QStringLiteral("1.e4 c5 2.Nf3 d6 3.d4 cxd4 4.Nxd4 Nf6 5.Nc3 a6 6.Bg5 e6 7.f4 Be7 8.Qf3 Qc7 9.0-0-0 Nbd7"),
            QString(), &error);
        QVERIFY2(loose, qPrintable(error));
        QCOMPARE(loose->moves.at(16).uci, QStringLiteral("e1c1"));

        const std::optional<Pgn::ParsedLine> fromFen = Pgn::parseLine(
            QStringLiteral("1... Kd7 2. e4"), QStringLiteral("4k3/8/8/8/8/8/4P3/4K3 b - - 0 1"), &error);
        QVERIFY2(fromFen && fromFen->moves.size() == 2, qPrintable(error));

        QVERIFY(!Pgn::parseLine(QStringLiteral("1.e4 e5 2.Ke3"), QString(), &error));
        QVERIFY(error.contains(QStringLiteral("Ke3")));

        const ChessPosition knights = *ChessPosition::fromFen(QStringLiteral("4k3/8/8/8/8/8/8/1N2KN2 w - - 0 1"));
        QVERIFY(!knights.moveFromSan(u"Nd2")); // Ambiguous.
        QCOMPARE(knights.moveFromSan(u"Nfd2")->from, BoardState::squareFromName(u"f1"));
        QCOMPARE(knights.moveFromSan(u"N1h2")->from, BoardState::squareFromName(u"f1")); // Extra disambiguation.
    }

    void probesFindWhereTheAdvantageShows()
    {
        const auto score = [](int cp, int depth = 0) {
            EngineEvaluation evaluation;
            evaluation.centipawns = cp;
            evaluation.depth = depth;
            return evaluation;
        };
        // The engine needs depth 7 to see the +3.
        QCOMPARE(AdvantageProbe::settledDepth({score(20, 1), score(40, 5), score(290, 7), score(310, 8), score(300, 9)}),
                 std::optional<int>(7));
        // Shallow searches along the line only agree after two forced moves.
        QCOMPARE(AdvantageProbe::concretePly({score(10), score(30), score(280), score(320)}, score(300)),
                 std::optional<int>(2));
        QCOMPARE(AdvantageProbe::concretePly({score(10), score(30)}, score(300)), std::nullopt);
    }

    void concretePlyGuidesQuietExplanations()
    {
        ExplanationInput input;
        input.after = ChessPosition::startingPosition();
        input.afterEvaluation = centipawns(20, {"e2e4", "e7e5", "g1f3", "b8c6"});
        QCOMPARE(explainPosition(input).arrows.size(), 2);

        input.concretePly = 3;
        input.trace = true;
        const MoveExplanation explanation = explainPosition(input);
        QCOMPARE(explanation.arrows.size(), 3);
        QVERIFY2(explanation.summary.contains(QStringLiteral("It becomes concrete after 1.e4 e5 2.Nf3.")),
                 qPrintable(explanation.summary));
        QVERIFY(!explanation.trace.isEmpty());
    }

    void usesLiveAnalysisHints()
    {
        // 14…Bxc3: depth 20 sees +13, the live analysis found a mate in 14 at depth 32.
        QString error;
        const std::optional<Pgn::ParsedLine> game = Pgn::parseLine(
            QStringLiteral("1.e4 e5 2.f4 exf4 3.Nf3 Nc6 4.Bc4 Nf6 5.Nc3 Bc5 6.d4 Bb6 7.Bxf4 O-O 8.O-O Re8 9.e5 Ng4 "
                           "10.Kh1 Kh8 11.Ng5 Nh6 12.Qd3 g6 13.Nge4 Bxd4 14.Bxh6 Bxc3"),
            QString(), &error);
        QVERIFY2(game, qPrintable(error));
        ExplanationAnalysis analysis;
        analysis.after = ChessPosition::startingPosition();
        for (const MoveRecord &move : game->moves)
            analysis.after.play(*analysis.after.moveFromUci(move.uci));
        analysis.afterByDepth = {centipawns(1315, {"d3c3", "e8e5"})};
        analysis.afterByDepth.first().depth = 20;

        EngineEvaluation mate;
        mate.isMate = true;
        mate.mateIn = 14;
        mate.mating = Side::White;
        mate.depth = 32;
        mate.pv = QString::fromLatin1("d3c3 d7d5 e5d6 f7f6 f1f6 e8e5 f6f7 c8e6 c4e6 d8g8 d6c7 a8c8 e4g5 c8e8 a1f1 b7b5 "
                                      "f7f8 e8f8 f1f8 g8f8 g5f7 f8f7 c7c8q c6d8 c3e5 h8g8 c8d8").split(QLatin1Char(' '));
        QVERIFY(analysis.acceptsHint(mate));

        const MoveExplanation explanation = explainPosition(analysis.input(SanStyle::Letters, true, mate));
        QCOMPARE(explanation.playback.size(), 27); // The whole mate, not only the first plies.
        QVERIFY(explanation.summary.contains(QStringLiteral("White mates in 14")));
        QVERIFY(explanation.trace.first().startsWith(QStringLiteral("hint from the live analysis")));

        EngineEvaluation shallow = mate;
        shallow.depth = 12;
        QVERIFY(!analysis.acceptsHint(shallow)); // Not deeper than the search.
        EngineEvaluation plusTwo = centipawns(200, mate.pv);
        plusTwo.depth = 40;
        QVERIFY(!analysis.acceptsHint(plusTwo)); // Only mates and draws guide the explanation.
        EngineEvaluation draw = centipawns(0, {"d3c3"});
        draw.depth = 40;
        QVERIFY(analysis.acceptsHint(draw));
        EngineEvaluation illegal = mate;
        illegal.pv = QStringList{"e2e4"};
        QVERIFY(!analysis.acceptsHint(illegal));
    }

    void plansFolderSync()
    {
        using Kind = SyncAction::Kind;
        const auto local = [](std::initializer_list<std::pair<const char *, const char *>> files) {
            QMap<QString, LocalFileState> map;
            for (const auto &[path, hash] : files)
                map.insert(QString::fromLatin1(path), LocalFileState{QString::fromLatin1(hash), 1, QDateTime()});
            return map;
        };
        const auto remote = [](std::initializer_list<std::pair<const char *, const char *>> files) {
            SyncManifest manifest;
            for (const auto &[path, hash] : files) {
                SyncFileState state;
                state.hash = QString::fromLatin1(hash);
                state.deleted = state.hash.isEmpty();
                manifest.files.insert(QString::fromLatin1(path), state);
            }
            return manifest;
        };
        const QMap<QString, QString> base{{"same.pdb", "a"}, {"edited-here.pdb", "a"}, {"edited-there.pdb", "a"},
                                          {"deleted-here.pdb", "a"}, {"deleted-there.pdb", "a"},
                                          {"edited-both.pdb", "a"}, {"deleted-here-edited-there.pdb", "a"},
                                          {"edited-here-deleted-there.pdb", "a"}, {"dropped-from-manifest.pch", "a"}};
        const QList<SyncAction> actions = planSync(
            local({{"same.pdb", "a"}, {"edited-here.pdb", "b"}, {"edited-there.pdb", "a"}, {"deleted-there.pdb", "a"},
                   {"edited-both.pdb", "b"}, {"edited-here-deleted-there.pdb", "b"}, {"new-here.pch", "n"},
                   {"new-both-same.pdb", "s"}, {"new-both-different.pdb", "x"}, {"dropped-from-manifest.pch", "a"}}),
            base,
            remote({{"same.pdb", "a"}, {"edited-here.pdb", "a"}, {"edited-there.pdb", "c"}, {"deleted-here.pdb", "a"},
                    {"deleted-there.pdb", ""}, {"edited-both.pdb", "c"}, {"deleted-here-edited-there.pdb", "c"},
                    {"edited-here-deleted-there.pdb", ""}, {"new-there.pdb", "t"}, {"new-both-same.pdb", "s"},
                    {"new-both-different.pdb", "y"}}));

        const QList<SyncAction> expected{
            {Kind::Download, "deleted-here-edited-there.pdb"},
            {Kind::DeleteRemote, "deleted-here.pdb"},
            {Kind::DeleteLocal, "deleted-there.pdb"},
            // Absent without a deletion record (a lost manifest update): put it back.
            {Kind::Upload, "dropped-from-manifest.pch"},
            {Kind::KeepBoth, "edited-both.pdb"},
            {Kind::Upload, "edited-here-deleted-there.pdb"},
            {Kind::Upload, "edited-here.pdb"},
            {Kind::Download, "edited-there.pdb"},
            {Kind::KeepBoth, "new-both-different.pdb"},
            {Kind::Record, "new-both-same.pdb"},
            {Kind::Upload, "new-here.pch"},
            {Kind::Download, "new-there.pdb"},
        };
        QCOMPARE(actions, expected);

        // A manifest survives a round trip, tombstones included.
        SyncManifest manifest = remote({{"Databases/Games.pdb", "abc"}, {"Projects/Old.pch", ""}});
        manifest.revision = 7;
        manifest.updatedBy = QStringLiteral("laptop");
        const std::optional<SyncManifest> read = SyncManifest::fromJson(manifest.toJson(), nullptr);
        QVERIFY(read);
        QCOMPARE(read->revision, 7);
        QCOMPARE(read->files.value("Databases/Games.pdb").hash, QStringLiteral("abc"));
        QVERIFY(read->files.value("Projects/Old.pch").deleted);

        QCOMPARE(conflictPath(QStringLiteral("Databases/Games.pdb"), QStringLiteral("laptop"),
                              QDateTime(QDate(2026, 9, 17), QTime(10, 30))),
                 QStringLiteral("Databases/Games (conflict, laptop, 2026-09-17 10.30).pdb"));
    }

    void outlinesDatabases()
    {
        const auto game = [](const char *eco, const char *event, const char *date) {
            GameRecord record;
            record.eco = QString::fromLatin1(eco);
            record.event = QString::fromLatin1(event);
            record.date = QString::fromLatin1(date);
            return record;
        };
        DatabaseOutline outline;
        for (const GameRecord &record : {game("E10", "Linares", "2001.02.03"), game("e11a", "Linares", "2001.??.??"),
                                         game("B90", "?", "????.??.??"), game("", "Casual", "1851.06.21")})
            outline.add(record);
        QCOMPARE(outline.eco.keys(), (QStringList{"B", "E"}));
        QCOMPARE(outline.eco.value("E").value("E11"), 1);
        QCOMPARE(outline.events.value("Linares"), 2);
        QVERIFY(!outline.events.contains("?"));
        QCOMPARE(outline.years.keys(), (QList<int>{1851, 2001}));
        QCOMPARE(DatabaseOutline::ecoCode(QStringLiteral("F10")), QString());
    }

    void parsesLichessGames()
    {
        // Built from a byte string: moc cannot read raw string literals holding braces.
        const QJsonObject game = QJsonDocument::fromJson(
            "{\"id\": \"q7ZvsdUF\", \"rated\": true, \"variant\": \"standard\", \"speed\": \"blitz\","
            " \"createdAt\": 1700000000000, \"status\": \"mate\", \"winner\": \"white\","
            " \"moves\": \"e4 e5 Bc4 Nc6 Qh5 Nf6 Qxf7#\","
            " \"players\": {\"white\": {\"user\": {\"name\": \"Alice\"}, \"rating\": 1850},"
            " \"black\": {\"user\": {\"name\": \"Bob\"}, \"rating\": 1790}},"
            " \"opening\": {\"eco\": \"C23\", \"name\": \"Bishop's Opening\"}}").object();
        const std::optional<ImportedGame> imported = LichessFetch::parseGame(game);
        QVERIFY(imported);
        QCOMPARE(imported->externalId, QStringLiteral("q7ZvsdUF"));
        QCOMPARE(imported->game.white, QStringLiteral("Alice"));
        QCOMPARE(imported->game.blackElo, 1790);
        QCOMPARE(imported->game.result, QStringLiteral("1-0"));
        QCOMPARE(imported->game.date, QStringLiteral("2023.11.14"));
        QCOMPARE(imported->game.eco, QStringLiteral("C23"));
        QCOMPARE(imported->game.moves.size(), 7);
        QCOMPARE(imported->game.site, QStringLiteral("https://lichess.org/q7ZvsdUF"));

        QJsonObject ongoing = game;
        ongoing.insert(QStringLiteral("status"), QStringLiteral("started"));
        QVERIFY(!LichessFetch::parseGame(ongoing));
        QJsonObject chess960 = game;
        chess960.insert(QStringLiteral("variant"), QStringLiteral("chess960"));
        QVERIFY(!LichessFetch::parseGame(chess960));
    }

    void parsesChessComGames()
    {
        QJsonObject game;
        game.insert(QStringLiteral("url"), QStringLiteral("https://www.chess.com/game/live/692667823"));
        game.insert(QStringLiteral("uuid"), QStringLiteral("82282996-91e2-11de-8000-000000010001"));
        game.insert(QStringLiteral("rules"), QStringLiteral("chess"));
        game.insert(QStringLiteral("end_time"), 1389052479);
        game.insert(QStringLiteral("pgn"), QStringLiteral(
            "[Event \"Live Chess\"]\n[Site \"Chess.com\"]\n[Date \"2014.01.06\"]\n[Result \"1-0\"]\n[ECO \"C25\"]\n\n"
            "1. e4 {[%clk 0:03:00]} 1... e5 {[%clk 0:03:00]} 2. Nc3 {[%clk 0:02:57.6]} 1-0"));
        game.insert(QStringLiteral("white"), QJsonObject{{"username", "Hikaru"}, {"rating", 2354}});
        game.insert(QStringLiteral("black"), QJsonObject{{"username", "Godswill"}, {"rating", 2167}});
        const std::optional<ImportedGame> imported = ChessComFetch::parseGame(game);
        QVERIFY(imported);
        QCOMPARE(imported->externalId, QStringLiteral("82282996-91e2-11de-8000-000000010001"));
        QCOMPARE(imported->game.white, QStringLiteral("Hikaru"));
        QCOMPARE(imported->game.whiteElo, 2354);
        QCOMPARE(imported->game.date, QStringLiteral("2014.01.06"));
        QCOMPARE(imported->game.eco, QStringLiteral("C25"));
        QCOMPARE(imported->game.moves.size(), 3);

        game.insert(QStringLiteral("rules"), QStringLiteral("crazyhouse"));
        QVERIFY(!ChessComFetch::parseGame(game));
    }

    void storesSourcesAndSkipsKnownGames()
    {
        QTemporaryDir dir;
        QString error;
        const std::unique_ptr<SqliteGameDatabase> database =
            SqliteGameDatabase::create(dir.filePath(QStringLiteral("test.pdb")), {}, &error);
        QVERIFY2(database, qPrintable(error));

        GameSource source;
        source.kind = QStringLiteral("chesscom");
        source.account = QStringLiteral("hikaru");
        source.settings.insert(QLatin1String(SourceSettings::ratedOnly), true);
        QVERIFY2(database->addSource(source, &error), qPrintable(error));
        QVERIFY(source.id > 0);
        QVERIFY(!source.uuid.isEmpty());

        ImportedGame game;
        game.externalId = QStringLiteral("a");
        game.game.white = QStringLiteral("Hikaru");
        game.game.moves = {MoveRecord{QStringLiteral("e4"), QStringLiteral("e2e4")}};
        ImportedGame other = game;
        other.externalId = QStringLiteral("b");
        QCOMPARE(database->importGames(source.id, {game, other}, &error), 2);
        QCOMPARE(database->importGames(source.id, {game}, &error), 0); // Already imported.
        QCOMPARE(database->gameCount(), 2);

        source.state.insert(QStringLiteral("month"), QStringLiteral("2024/05"));
        source.lastError = QStringLiteral("offline");
        QVERIFY(database->updateSource(source, &error));
        const QList<GameSource> stored = database->sources();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.first().importedGames, 2);
        QCOMPARE(stored.first().state.value(QStringLiteral("month")).toString(), QStringLiteral("2024/05"));
        QCOMPARE(stored.first().lastError, QStringLiteral("offline"));
        QCOMPARE(database->sourceGameIds(source.id).size(), 2);

        QVERIFY(database->removeSource(source.id, &error));
        QVERIFY(database->sources().isEmpty());
        QCOMPARE(database->gameCount(), 2); // Imported games stay.
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
