#include "app/AdvantageProbe.h"
#include "app/ChessPosition.h"
#include "app/DatabaseDedupe.h"
#include "app/DatabaseMerge.h"
#include "app/DatabaseMigrations.h"
#include "app/DatabaseOutline.h"
#include "app/EngineCatalog.h"
#include "app/EngineDetector.h"
#include "app/ExplanationSearch.h"
#include "app/GameIdentity.h"
#include "app/GameState.h"
#include "app/HelpGuide.h"
#include "app/MoveAnnotation.h"
#include "app/MoveExplanation.h"
#include "app/OpeningNames.h"
#include "app/Pgn.h"
#include "app/PolyglotBook.h"
#include "app/PositionIndex.h"
#include "app/Reconcile.h"
#include "app/ShippedOpeningNames.h"
#include "app/SqliteGameDatabase.h"
#include "app/TrainingTutor.h"
#include "app/sources/ChessComFetch.h"
#include "app/sources/TorneiOnlineFetch.h"
#include "app/sources/LichessFetch.h"
#include "app/sync/FolderSync.h"
#include "app/sync/GitStore.h"
#include "app/sync/SyncManifest.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
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

using namespace Qt::StringLiterals;

class TestChessRules : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void keepsTheBundledEngine();
    void savesAndResolvesEngines();
    void addsDetectedEnginesOnce();
    void recognizesEngineFiles();
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

    void readsAndSearchesTheGuide()
    {
        const HelpGuide guide = HelpGuide::fromMarkdown(QStringLiteral(
            "# Getting started {#start}\n\nOpen a **database** and double-click a game.\n\n"
            "# The trash {#trash}\n\n- Right-click a game: *Move Game to Trash*.\n"
            "- [Optimize](https://example.org) removes it for good, perché no.\n\n## Details\n\nMore.\n"));
        QCOMPARE(guide.topics().size(), 2);
        QCOMPARE(guide.topics().at(0).id, QStringLiteral("start"));
        QCOMPARE(guide.topics().at(0).title, QStringLiteral("Getting started"));
        QVERIFY(guide.topics().at(0).markdown.startsWith(QStringLiteral("# Getting started\n\nOpen a **database**")));
        QCOMPARE(guide.topics().at(0).text, QStringLiteral("Open a database and double-click a game."));
        QVERIFY(guide.topics().at(1).markdown.contains(QStringLiteral("## Details"))); // A subheading is not a topic.
        QVERIFY(guide.topics().at(1).text.contains(QStringLiteral("Optimize removes it")));
        QCOMPARE(guide.indexOf(QStringLiteral("trash")), 1);
        QCOMPARE(guide.indexOf(QStringLiteral("nowhere")), -1);

        // Every word must be there, whatever the case and the accents; the snippet says where.
        QVERIFY(guide.search(QString()).isEmpty());
        QVERIFY(guide.search(QStringLiteral("zugzwang")).isEmpty());
        QList<HelpGuide::Match> found = guide.search(QStringLiteral("OPTIMIZE perche"));
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().topic, 1);
        QVERIFY2(found.first().snippet.contains(QStringLiteral("Optimize removes")), qPrintable(found.first().snippet));
        // Both topics speak of a game: the one that says it in its title comes first... here neither, so in order.
        found = guide.search(QStringLiteral("game"));
        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).topic, 0);
        found = guide.search(QStringLiteral("trash"));
        QCOMPARE(found.first().topic, 1);
        // A word of the title alone is told by how the topic begins.
        found = guide.search(QStringLiteral("started"));
        QCOMPARE(found.size(), 1);
        QVERIFY(found.first().snippet.startsWith(QStringLiteral("Open a database")));

        // The guides we ship: every language has the same topics as English, none empty.
        const auto shipped = [](const char *code) {
            QFile file(QStringLiteral(PRAGMA_HELP_DIR "/guide_%1.md").arg(QLatin1String(code)));
            return file.open(QIODevice::ReadOnly) ? HelpGuide::fromMarkdown(QString::fromUtf8(file.readAll()))
                                                  : HelpGuide();
        };
        const HelpGuide english = shipped("en");
        QVERIFY(english.topics().size() >= 10);
        for (const char *code : {"it"}) {
            const HelpGuide translated = shipped(code);
            QCOMPARE(translated.topics().size(), english.topics().size());
            for (int index = 0; index < english.topics().size(); ++index) {
                QCOMPARE(translated.topics().at(index).id, english.topics().at(index).id);
                QVERIFY2(translated.topics().at(index).text.size() > 40, qPrintable(translated.topics().at(index).id));
                QVERIFY2(english.topics().at(index).text.size() > 40, qPrintable(english.topics().at(index).id));
            }
        }
        QCOMPARE(shipped("it").search(QStringLiteral("cestino")).first().topic, english.indexOf(QStringLiteral("trash")));
    }

    void tutorJudgesTrainingMoves()
    {
        // Evaluations are White's; the user plays White here. No engine line: the move is not "the best".
        const auto eval = [](int centipawns) {
            EngineEvaluation evaluation;
            evaluation.centipawns = centipawns;
            evaluation.depth = 12;
            return evaluation;
        };
        const ChessMove played = *ChessPosition::startingPosition().moveFromUci(QStringLiteral("e2e4"));
        using TrainingTutor::Alert;
        QCOMPARE(TrainingTutor::judge(eval(30), eval(10), Side::White, played), Alert::None);
        QCOMPARE(TrainingTutor::judge(eval(30), eval(-60), Side::White, played), Alert::None); // An inaccuracy passes.
        QCOMPARE(TrainingTutor::judge(eval(50), eval(-200), Side::White, played), Alert::Mistake);
        QCOMPARE(TrainingTutor::judge(eval(30), eval(-450), Side::White, played), Alert::Blunder);
        // Winning before, only equal after: nothing was lost, a chance was.
        QCOMPARE(TrainingTutor::judge(eval(400), eval(10), Side::White, played), Alert::MissedChance);
        QCOMPARE(TrainingTutor::judge(eval(400), eval(-300), Side::White, played), Alert::Blunder);
        // Still winning: giving back part of a big advantage is no alarm.
        QCOMPARE(TrainingTutor::judge(eval(900), eval(600), Side::White, played), Alert::None);
        // From Black's side the signs turn.
        QCOMPARE(TrainingTutor::judge(eval(-30), eval(450), Side::Black, played), Alert::Blunder);
        QCOMPARE(TrainingTutor::judge(eval(-400), eval(0), Side::Black, played), Alert::MissedChance);
        // A mate thrown away, and walking into one.
        EngineEvaluation mate;
        mate.isMate = true;
        mate.mateIn = 2;
        mate.mating = Side::White;
        QCOMPARE(TrainingTutor::judge(mate, eval(20), Side::White, played), Alert::MissedChance);
        mate.mating = Side::Black;
        QCOMPARE(TrainingTutor::judge(eval(20), mate, Side::White, played), Alert::Blunder);
        // The move the engine expected is never an error, whatever the next search says.
        EngineEvaluation expected = eval(50);
        expected.pv = {QStringLiteral("e2e4")};
        QCOMPARE(TrainingTutor::judge(expected, eval(-300), Side::White, played), Alert::None);
    }

    void annotatesMoves()
    {
        // One glyph for the move and one for the position, the move's first.
        QCOMPARE(MoveAnnotation::toggled({}, 1), QList<int>{1});
        QCOMPARE(MoveAnnotation::toggled({1}, 16), (QList<int>{1, 16}));
        QCOMPARE(MoveAnnotation::toggled({1, 16}, 4), (QList<int>{4, 16})); // "??" takes the place of "!".
        QCOMPARE(MoveAnnotation::toggled({4, 16}, 4), QList<int>{16});      // Chosen again, it goes.
        QCOMPARE(MoveAnnotation::toggled({16}, 3), (QList<int>{3, 16}));
        QCOMPARE(MoveAnnotation::normalized({16, 999, 1, 2}), (QList<int>{2, 16}));
        QCOMPARE(MoveAnnotation::symbols({5, 14}), QStringLiteral("!? ⩲"));
        QCOMPARE(MoveAnnotation::symbols({18}), QStringLiteral(" +−"));
        for (const MoveAnnotation::Glyph &glyph : MoveAnnotation::glyphs())
            QVERIFY2(!MoveAnnotation::meaning(glyph.nag).isEmpty(), qPrintable(glyph.symbol));

        // Glued to the SAN where a move is one word, PGN where it is text.
        QCOMPARE(MoveAnnotation::storedSuffix({1, 16}), QStringLiteral("!$16"));
        QCOMPARE(MoveAnnotation::pgnSuffix({1, 16}), QStringLiteral("! $16"));
        QCOMPARE(MoveAnnotation::pgnSuffix({7}), QStringLiteral(" $7"));
        QList<int> nags;
        QCOMPARE(MoveAnnotation::split(QStringLiteral("Nxe5+!$16"), &nags), QStringLiteral("Nxe5+"));
        QCOMPARE(nags, (QList<int>{1, 16}));
        nags.clear();
        QCOMPARE(MoveAnnotation::split(QStringLiteral("e8=Q#"), &nags), QStringLiteral("e8=Q#"));
        QVERIFY(nags.isEmpty());

        // Copied as PGN and pasted back.
        GameRecord game;
        for (const char *uci : {"e2e4", "e7e5", "g1f3", "d7d6"})
            game.moves << MoveRecord{QString(), QString::fromLatin1(uci), {}};
        game.moves[2].nags = {1, 14};
        game.moves[3].nags = {6};
        QCOMPARE(Pgn::moveText(game), QStringLiteral("1.e4 e5 2.Nf3! $14 d6?!"));
        QString error;
        const std::optional<Pgn::ParsedLine> pasted = Pgn::parseLine(Pgn::game(game), QString(), &error);
        QVERIFY2(pasted, qPrintable(error));
        QCOMPARE(pasted->moves.at(2).nags, (QList<int>{1, 14}));
        QCOMPARE(pasted->moves.at(3).nags, QList<int>{6});

        // How a game begins, for lists: cut games end in "…", Black may move first.
        QCOMPARE(Pgn::preview(QString(), {QStringLiteral("e4"), QStringLiteral("e5"), QStringLiteral("Nf3!$16")}, 3),
                 QStringLiteral("1.e4 e5 2.Nf3! ±"));
        QCOMPARE(Pgn::preview(QString(), {QStringLiteral("e4"), QStringLiteral("e5")}, 40), QStringLiteral("1.e4 e5…"));
        QCOMPARE(Pgn::preview(QStringLiteral("4k3/8/8/8/8/8/4P3/4K3 b - - 0 7"),
                              {QStringLiteral("Kd7"), QStringLiteral("e4"), QStringLiteral("Kd6")}, 3),
                 QStringLiteral("7…Kd7 8.e4 Kd6"));
        QCOMPARE(Pgn::preview(QString(), {}, 0), QString());
        QCOMPARE(figurineLine(QStringLiteral("1.Nf3 d5 2.O-O e8=Q+")), QStringLiteral("1.♘f3 d5 2.O-O e8=♕+"));

        // Stored with the game, without the SAN showing them.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("annotated.pdb"));
        {
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(path, {GameRecord()}, &error);
            QVERIFY2(database, qPrintable(error));
            GameRecord stored = *database->loadGame(0);
            stored.moves = pasted->moves;
            stored.modified.clear();
            QVERIFY2(database->replaceGame(0, stored, &error), qPrintable(error));
            stored.uid.clear(); // A game of its own.
            QVERIFY2(database->addGame(stored, &error) >= 0, qPrintable(error));
        }
        const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
        QVERIFY2(database, qPrintable(error));
        for (const qint64 index : {0, 1}) {
            // The games list shows the line, annotations included, without loading the game.
            QCOMPARE(database->header(index).linePreview, QStringLiteral("1.e4 e5 2.Nf3! ⩲ d6?!"));
            const GameRecord loaded = *database->loadGame(index);
            QCOMPARE(loaded.moves.size(), 4);
            QCOMPARE(loaded.moves.at(2).san, QStringLiteral("Nf3"));
            QCOMPARE(loaded.moves.at(2).nags, (QList<int>{1, 14}));
            QCOMPARE(loaded.moves.at(3).nags, QList<int>{6});
            QVERIFY(loaded.moves.at(0).nags.isEmpty());
        }
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
        // The glyphs stay with their moves; those of the variation go with it.
        QCOMPARE(pgn->moves.at(2).nags, QList<int>());
        QCOMPARE(pgn->moves.at(3).nags, QList<int>{6});
        QCOMPARE(pgn->moves.at(4).nags, QList<int>{4});

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
        QVERIFY2(explanation.summary.contains(
                     QStringLiteral("No material explains it: the assessment is positional, clear after 1.e4 e5 2.Nf3.")),
                 qPrintable(explanation.summary));
        QVERIFY(!explanation.trace.isEmpty());
    }

    /// 1.e4 e5 2.Qg4 Nf6 3.Qf5: the evaluation drops but no material is lost.
    /// The explanation used to draw a red refutation arrow and say the
    /// advantage "becomes concrete", which reads as a piece falling.
    void positionalDropIsNotAMaterialLoss()
    {
        // Stockfish 16, depth 20, from pragma-explain --trace.
        const ExplanationInput input = inputFor(
            {"e2e4", "e7e5", "d1g4", "g8f6"}, QStringLiteral("g4f5"),
            centipawns(-90, {"g4g3", "f8e7", "f1c4", "e8g8", "d2d3", "c7c6", "b1c3", "d7d5"}),
            centipawns(-220, {"b8c6", "f5f3", "d7d5", "e4d5", "c8g4", "f3g3", "d8d5", "b1c3"}));
        ExplanationInput probed = input;
        probed.concretePly = 1; // Where the shallow probe agrees with depth 20.

        const MoveExplanation explanation = explainPosition(probed);
        QCOMPARE(explanation.verdict, MoveExplanation::Verdict::Inaccuracy);
        QVERIFY(explanation.playback.isEmpty());
        // Nothing falls, so nothing is ringed and no arrow shouts "refutation".
        QVERIFY(explanation.lostPieces.isEmpty());
        for (const BoardArrow &arrow : explanation.arrows) {
            QVERIFY2(arrow.kind != BoardArrow::Kind::Refutation, qPrintable(explanation.summary));
        }
        QVERIFY2(explanation.summary.contains(QStringLiteral("No material explains it")),
                 qPrintable(explanation.summary));
        QVERIFY2(!explanation.summary.contains(QStringLiteral("becomes concrete")),
                 qPrintable(explanation.summary));
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
                manifest.files.insert(QString::fromLatin1(path), state);
            }
            return manifest;
        };
        const QMap<QString, QString> base{{"same.pdb", "a"}, {"edited-here.pdb", "a"}, {"edited-there.pdb", "a"},
                                          {"deleted-here.pdb", "a"}, {"deleted-there.pdb", "a"},
                                          {"edited-both.pdb", "a"}, {"deleted-here-edited-there.pdb", "a"},
                                          {"edited-here-deleted-there.pdb", "a"}, {"gone-from-both.pdb", "a"},
                                          {"dropped-from-manifest.pch", "a"}};
        const QList<SyncAction> actions = planSync(
            local({{"same.pdb", "a"}, {"edited-here.pdb", "b"}, {"edited-there.pdb", "a"}, {"deleted-there.pdb", "a"},
                   {"edited-both.pdb", "b"}, {"edited-here-deleted-there.pdb", "b"}, {"new-here.pch", "n"},
                   {"new-both-same.pdb", "s"}, {"new-both-different.pdb", "x"}, {"dropped-from-manifest.pch", "a"}}),
            base,
            remote({{"same.pdb", "a"}, {"edited-here.pdb", "a"}, {"edited-there.pdb", "c"}, {"deleted-here.pdb", "a"},
                    {"edited-both.pdb", "c"}, {"deleted-here-edited-there.pdb", "c"}, {"new-there.pdb", "t"},
                    {"new-both-same.pdb", "s"}, {"new-both-different.pdb", "y"}}));

        // Nothing is ever deleted: a file missing on one side comes back from
        // the other, whether it is new or was deleted by hand.
        const QList<SyncAction> expected{
            {Kind::Download, "deleted-here-edited-there.pdb"},
            {Kind::Download, "deleted-here.pdb"},
            {Kind::Upload, "deleted-there.pdb"},
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
        // A file gone from both sides is gone; the base alone never revives it.
        for (const SyncAction &action : actions)
            QVERIFY(action.path != QLatin1String("gone-from-both.pdb"));

        // A manifest survives a round trip, and drops the tombstones of older versions.
        SyncManifest manifest = remote({{"Databases/Games.pdb", "abc"}, {"Projects/Old.pch", ""}});
        manifest.revision = 7;
        manifest.updatedBy = QStringLiteral("laptop");
        const std::optional<SyncManifest> read = SyncManifest::fromJson(manifest.toJson(), nullptr);
        QVERIFY(read);
        QCOMPARE(read->revision, 7);
        QCOMPARE(read->files.value("Databases/Games.pdb").hash, QStringLiteral("abc"));
        QVERIFY(!read->files.contains("Projects/Old.pch"));

        QCOMPARE(conflictPath(QStringLiteral("Databases/Games.pdb"), QStringLiteral("laptop"),
                              QDateTime(QDate(2026, 9, 17), QTime(10, 30))),
                 QStringLiteral("Databases/Games (conflict, laptop, 2026-09-17 10.30).pdb"));
    }


    /// Two devices sharing a bare Git repository. The sync reconciles, it does
    /// not mirror: a database deleted by hand comes back, and nothing ever
    /// leaves the repository.
    void reconcilesGitFoldersWithoutDeleting()
    {
        const QString git = QStandardPaths::findExecutable(QStringLiteral("git"));
        if (git.isEmpty())
            QSKIP("git is not installed");

        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QDir base(root.path());
        const QString origin = base.filePath(QStringLiteral("origin.git"));
        QProcess init;
        init.start(git, {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral("--initial-branch=main"),
                         origin});
        QVERIFY(init.waitForFinished(30000));
        QCOMPARE(init.exitCode(), 0);

        const auto write = [](const QString &path, const QByteArray &data) {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(data);
        };

        struct Device {
            std::unique_ptr<GitStore> store;
            std::unique_ptr<FolderSync> sync;
            QString folder;
        };
        const auto makeDevice = [&](const QString &name) {
            Device device;
            device.folder = base.filePath(name + QStringLiteral("/Pragma"));
            QDir().mkpath(device.folder);
            device.store = std::make_unique<GitStore>(origin, QStringLiteral("main"), QString(), QString(),
                                                      base.filePath(name + QStringLiteral("/clone")), name);
            device.sync = std::make_unique<FolderSync>(device.folder, base.filePath(name + QStringLiteral("/state.json")),
                                                       name);
            device.sync->setStore(device.store.get());
            return device;
        };
        const auto syncOnce = [](Device &device) {
            QSignalSpy finished(device.sync.get(), &FolderSync::finished);
            device.sync->sync();
            QVERIFY(finished.wait(60000));
            QCOMPARE(finished.constFirst().at(0).toString(), QString()); // No error.
        };

        Device laptop = makeDevice(QStringLiteral("laptop"));
        const QString laptopGames = laptop.folder + QStringLiteral("/Databases/Games.pdb");
        write(laptopGames, "one");
        syncOnce(laptop);

        // The other device starts empty and receives the database.
        Device desktop = makeDevice(QStringLiteral("desktop"));
        const QString desktopGames = desktop.folder + QStringLiteral("/Databases/Games.pdb");
        syncOnce(desktop);
        QVERIFY(QFile::exists(desktopGames));

        // A file added on one device appears on the other; nothing is removed.
        write(desktop.folder + QStringLiteral("/Projects/Study.pch"), "study");
        syncOnce(desktop);
        syncOnce(laptop);
        QVERIFY(QFile::exists(laptop.folder + QStringLiteral("/Projects/Study.pch")));

        // The database is deleted by hand: the next sync brings it back
        // instead of deleting it everywhere.
        QVERIFY(QFile::remove(laptopGames));
        syncOnce(laptop);
        QVERIFY2(QFile::exists(laptopGames), "a deleted database must come back, not disappear");
        syncOnce(desktop);
        QVERIFY2(QFile::exists(desktopGames), "the other device must keep its copy");

        // And the repository still holds it.
        QProcess tree;
        tree.setWorkingDirectory(origin);
        tree.start(git, {QStringLiteral("ls-tree"), QStringLiteral("-r"), QStringLiteral("--name-only"),
                         QStringLiteral("main")});
        QVERIFY(tree.waitForFinished(30000));
        const QString listing = QString::fromUtf8(tree.readAll());
        QVERIFY2(listing.contains(QLatin1String("Databases/Games.pdb")), qPrintable(listing));
        QVERIFY2(listing.contains(QLatin1String("Projects/Study.pch")), qPrintable(listing));
    }

    void findsDuplicateDatabases()
    {
        using DatabaseDedupe::Candidate;
        using DatabaseDedupe::Group;
        QCOMPARE(DatabaseDedupe::baseTitle(QStringLiteral("chess-com (Samsung SM-N960F)")), QStringLiteral("chess-com"));
        QCOMPARE(DatabaseDedupe::baseTitle(QStringLiteral("Opening Names (old 2)")), QStringLiteral("Opening Names"));
        QCOMPARE(DatabaseDedupe::baseTitle(QStringLiteral("Games (conflict, laptop, 2026-09-17 10.30)")),
                 QStringLiteral("Games"));
        QCOMPARE(DatabaseDedupe::baseTitle(QStringLiteral("(untitled)")), QStringLiteral("(untitled)"));

        const QSet<QString> games{QStringLiteral("u1"), QStringLiteral("u2")};
        const QList<Candidate> candidates{
            // A duplicate made by a phone sync: same games, another lineage.
            {QStringLiteral("Databases/chess-com.pdb"), QStringLiteral("b-lineage"), games},
            {QStringLiteral("Databases/chess-com (Samsung SM-N960F).pdb"), QStringLiteral("a-lineage"), games},
            // Same name, different games: two databases, both kept.
            {QStringLiteral("Databases/Study.pdb"), QStringLiteral("c1"), {QStringLiteral("x")}},
            {QStringLiteral("Databases/Study (laptop).pdb"), QStringLiteral("c2"), {QStringLiteral("y")}},
            // Two empty databases of one name.
            {QStringLiteral("Databases/prova.pdb"), QStringLiteral("p2"), {}},
            {QStringLiteral("Databases/prova (Samsung SM-N960F).pdb"), QStringLiteral("p1"), {}},
            // A database we ship, copies kept aside anywhere, whatever their games.
            {QStringLiteral("Databases/Opening Names.pdb"), QStringLiteral("shipped"), {QStringLiteral("n")}},
            {QStringLiteral("Books/Opening Names/Old/Opening Names.pdb"), QStringLiteral("shipped"), {}},
            {QStringLiteral("Books/Opening Names/English.pdb"), QStringLiteral("shipped"), {QStringLiteral("n")}, true},
            {QStringLiteral("Books/Opening Names/Opening Names (old).pdb"), QStringLiteral("shipped"), {}},
            {QStringLiteral("Databases/Alone.pdb"), QStringLiteral("z"), {}},
        };
        const QList<Group> expected{
            {QStringLiteral("Books/Opening Names/English.pdb"), QStringLiteral("shipped"),
             {QStringLiteral("Books/Opening Names/Old/Opening Names.pdb"),
              QStringLiteral("Books/Opening Names/Opening Names (old).pdb"), QStringLiteral("Databases/Opening Names.pdb")}},
            // The plain name keeps the file, the smallest lineage (the phone's choice) the id.
            {QStringLiteral("Databases/chess-com.pdb"), QStringLiteral("a-lineage"),
             {QStringLiteral("Databases/chess-com (Samsung SM-N960F).pdb")}},
            {QStringLiteral("Databases/prova.pdb"), QStringLiteral("p1"),
             {QStringLiteral("Databases/prova (Samsung SM-N960F).pdb")}},
        };
        QCOMPARE(DatabaseDedupe::groups(candidates), expected);

        // A merged file is merged into its database, or forgotten remotely; never synced again.
        using Kind = SyncAction::Kind;
        SyncManifest remote;
        remote.files.insert(QStringLiteral("Databases/Dup.pdb"), SyncFileState{QStringLiteral("h"), 1, {}, {}});
        remote.files.insert(QStringLiteral("Databases/Gone.pdb"), SyncFileState{QStringLiteral("g"), 1, {}, {}});
        for (const char *path : {"Databases/Dup.pdb", "Databases/Gone.pdb", "Databases/Nowhere.pdb"}) {
            remote.merged.insert(QString::fromLatin1(path),
                                 SyncMergeRecord{QString::fromLatin1(path), QStringLiteral("l"), QStringLiteral("k"),
                                                 QStringLiteral("Databases/Games.pdb"), {}, QStringLiteral("laptop")});
        }
        QMap<QString, LocalFileState> local;
        local.insert(QStringLiteral("Databases/Dup.pdb"), LocalFileState{QStringLiteral("changed"), 1, {}});
        const QList<SyncAction> actions = planSync(local, {{QStringLiteral("Databases/Dup.pdb"), QStringLiteral("h")}}, remote);
        QCOMPARE(actions, (QList<SyncAction>{{Kind::Merge, QStringLiteral("Databases/Dup.pdb")},
                                             {Kind::Forget, QStringLiteral("Databases/Gone.pdb")}}));
        const std::optional<SyncManifest> read = SyncManifest::fromJson(remote.toJson(), nullptr);
        QVERIFY(read);
        QCOMPARE(read->merged, remote.merged);
    }

    /// A duplicate merged on one device is merged on every device: its games
    /// (even ones only another device had) end in the database kept, and the
    /// file goes, locally and from the repository.
    void mergesDuplicatesAcrossGitDevices()
    {
        const QString git = QStandardPaths::findExecutable(QStringLiteral("git"));
        if (git.isEmpty())
            QSKIP("git is not installed");
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QDir base(root.path());
        const QString origin = base.filePath(QStringLiteral("origin.git"));
        QProcess init;
        init.start(git, {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral("--initial-branch=main"), origin});
        QVERIFY(init.waitForFinished(30000));
        QCOMPARE(init.exitCode(), 0);

        struct Device {
            std::unique_ptr<GitStore> store;
            std::unique_ptr<FolderSync> sync;
            QString folder;
        };
        const auto makeDevice = [&](const QString &name) {
            Device device;
            device.folder = base.filePath(name + QStringLiteral("/Pragma"));
            QDir().mkpath(device.folder + QStringLiteral("/Databases"));
            device.store = std::make_unique<GitStore>(origin, QStringLiteral("main"), QString(), QString(),
                                                      base.filePath(name + QStringLiteral("/clone")), name);
            device.sync = std::make_unique<FolderSync>(device.folder, base.filePath(name + QStringLiteral("/state.json")), name);
            device.sync->setStore(device.store.get());
            return device;
        };
        // Until the folder is quiet: a merge makes the sync run again to send the database it changed.
        const auto syncOnce = [](Device &device) {
            QSignalSpy finished(device.sync.get(), &FolderSync::finished);
            device.sync->sync();
            do {
                QVERIFY(finished.wait(60000));
                QCOMPARE(finished.constLast().at(0).toString(), QString());
            } while (device.sync->isRunning() || finished.count() == 0);
        };
        const auto game = [](const char *white) {
            GameRecord record;
            record.white = QString::fromLatin1(white);
            record.black = QStringLiteral("Black");
            record.result = QStringLiteral("1-0");
            record.moves = {MoveRecord{QStringLiteral("e4"), QStringLiteral("e2e4")}};
            return record;
        };
        const auto makeDatabase = [](const QString &path, const QList<GameRecord> &games, const QString &lineage) {
            QString error;
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(path, games, &error);
            QVERIFY2(database, qPrintable(error));
            DatabaseProperties properties = database->properties();
            properties.id = lineage;
            QVERIFY(database->setProperties(properties, &error));
        };
        const auto whites = [](const QString &path) {
            QStringList result;
            QString error;
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
            if (!database)
                return result;
            for (qint64 i = 0; i < database->gameCount(); ++i)
                result << database->header(i).white;
            result.sort();
            return result;
        };
        const QString keptLineage = QStringLiteral("00000000-0000-4000-8000-000000000001");
        const QString otherLineage = QStringLiteral("ffffffff-0000-4000-8000-000000000002");

        // The laptop has a database and, from a phone sync, its duplicate.
        Device laptop = makeDevice(QStringLiteral("laptop"));
        makeDatabase(laptop.folder + QStringLiteral("/Databases/Games.pdb"), {game("A"), game("B")}, otherLineage);
        makeDatabase(laptop.folder + QStringLiteral("/Databases/Games (Phone).pdb"), {game("A"), game("B")}, keptLineage);
        syncOnce(laptop); // No database hooks yet: both go up as they are.

        // The desktop receives both, then adds a game to its copy of the duplicate.
        Device desktop = makeDevice(QStringLiteral("desktop"));
        syncOnce(desktop);
        const QString desktopDuplicate = desktop.folder + QStringLiteral("/Databases/Games (Phone).pdb");
        QVERIFY(QFile::exists(desktopDuplicate));
        {
            QString error;
            const std::unique_ptr<SqliteGameDatabase> copy = SqliteGameDatabase::open(desktopDuplicate, &error);
            QVERIFY2(copy, qPrintable(error));
            QVERIFY(copy->addGame(game("OnlyOnDesktop"), &error) >= 0);
        }

        // The laptop merges the duplicate into the plain name, which takes the smaller lineage.
        laptop.sync->setDatabaseHooks(DatabaseMerge::hooks());
        syncOnce(laptop);
        QVERIFY(!QFile::exists(laptop.folder + QStringLiteral("/Databases/Games (Phone).pdb")));
        QCOMPARE(SqliteGameDatabase::readProperties(laptop.folder + QStringLiteral("/Databases/Games.pdb")).id, keptLineage);

        // The desktop merges its copy, extra game included, and the file goes.
        desktop.sync->setDatabaseHooks(DatabaseMerge::hooks());
        syncOnce(desktop);
        QVERIFY2(!QFile::exists(desktopDuplicate), "a merged database must go on every device");
        QCOMPARE(whites(desktop.folder + QStringLiteral("/Databases/Games.pdb")),
                 (QStringList{"A", "B", "OnlyOnDesktop"}));

        // And the laptop gets the game back through the database kept.
        syncOnce(laptop);
        QCOMPARE(whites(laptop.folder + QStringLiteral("/Databases/Games.pdb")), (QStringList{"A", "B", "OnlyOnDesktop"}));
        QVERIFY(!QFile::exists(laptop.folder + QStringLiteral("/Databases/Games (Phone).pdb")));

        QProcess tree;
        tree.setWorkingDirectory(origin);
        tree.start(git, {QStringLiteral("ls-tree"), QStringLiteral("-r"), QStringLiteral("--name-only"), QStringLiteral("main")});
        QVERIFY(tree.waitForFinished(30000));
        const QString listing = QString::fromUtf8(tree.readAll());
        QVERIFY2(listing.contains(QLatin1String("Databases/Games.pdb")), qPrintable(listing));
        QVERIFY2(!listing.contains(QLatin1String("Games (Phone)")), qPrintable(listing));
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

    void countsResultsByPosition()
    {
        // Four games reach the same position, one of them by another move order.
        const QList<GameLine> games{
            {1, QString(), QStringLiteral("e2e4 e7e5 g1f3 b8c6"), QStringLiteral("1-0")},
            {2, QString(), QStringLiteral("g1f3 b8c6 e2e4 e7e5"), QStringLiteral("1/2-1/2")},
            {3, QString(), QStringLiteral("e2e4 e7e5 g1f3 b8c6 f1b5"), QStringLiteral("0-1")},
            {4, QString(), QStringLiteral("e2e4 e7e5 g1f3 b8c6"), QStringLiteral("*")},
            {5, QString(), QStringLiteral("d2d4"), QStringLiteral("1-0")},
        };
        const PositionIndex index = PositionIndex::build(games);
        ChessPosition position = ChessPosition::startingPosition();
        for (const QString &uci : {u"e2e4"_s, u"e7e5"_s, u"g1f3"_s, u"b8c6"_s})
            position.play(*position.moveFromUci(uci));
        const PositionIndex::Stats stats = index.statsWithPosition(position);
        QCOMPARE(stats.games, 4);
        QCOMPARE(stats.whiteWins, 1);
        QCOMPARE(stats.draws, 1);
        QCOMPARE(stats.blackWins, 1); // Game 4 has no result: counted, not scored.
        QCOMPARE(index.statsWithPosition(ChessPosition::startingPosition()).games, 5);
        position.play(*position.moveFromUci(u"f1b5"));
        QCOMPARE(index.statsWithPosition(position), (PositionIndex::Stats{1, 0, 0, 1}));
        position.play(*position.moveFromUci(u"a7a6"));
        QCOMPARE(index.statsWithPosition(position), PositionIndex::Stats());
    }

    void indexesPositionsAndLines()
    {
        const QString endgame = QStringLiteral("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
        const QList<GameLine> games{
            {1, QString(), QStringLiteral("e2e4 e7e5 g1f3 b8c6 f1b5")},
            {2, QString(), QStringLiteral("g1f3 b8c6 e2e4 e7e5 f1c4")},
            {3, QString(), QStringLiteral("e2e4 e7e6")},
            {4, QString(), QStringLiteral("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6")},
            {5, endgame, QStringLiteral("e2e4 e8e7")},
            {6, QString(), QStringLiteral("d2d4 xx e7e5")}, // Cut at the illegal move.
        };
        const PositionIndex index = PositionIndex::build(games);
        QCOMPARE(index.gameCount(), 6);
        const auto ids = [](std::initializer_list<qint64> list) { return QSet<qint64>(list); };
        const auto moves = [](const ChessPosition &start, const QStringList &uci) {
            QList<ChessMove> line;
            ChessPosition position = start;
            for (const QString &move : uci) {
                line << *position.moveFromUci(move);
                position.play(line.last());
            }
            return line;
        };
        const ChessPosition start = ChessPosition::startingPosition();

        // The start position is in every standard game, not in the endgame.
        QCOMPARE(index.gamesWithPosition(start), ids({1, 2, 3, 4, 6}));
        QCOMPARE(index.gamesWithLine(start, {}), ids({1, 2, 3, 4, 6}));
        QCOMPARE(index.countWithPosition(start), 5);

        // 1.e4 e5 2.Nf3 Nc6 and 1.Nf3 Nc6 2.e4 e5 reach the same position…
        const QStringList open{"e2e4", "e7e5", "g1f3", "b8c6"};
        QCOMPARE(index.gamesWithPosition(afterMoves(QString(), open)), ids({1, 2, 4}));
        // … but only those that played these moves in this order share the line.
        QCOMPARE(index.gamesWithLine(start, moves(start, open)), ids({1, 4}));
        QCOMPARE(index.countWithLine(start, moves(start, open)), 2);
        QCOMPARE(index.gamesWithLine(start, moves(start, {"g1f3", "b8c6", "e2e4", "e7e5"})), ids({2}));

        // Whole moves: the line after 1.e4 includes 1…e5 and 1…e6 games.
        QCOMPARE(index.gamesWithLine(start, moves(start, {"e2e4"})), ids({1, 3, 4}));
        QCOMPARE(index.gamesWithLine(start, moves(start, {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6"})), ids({4}));
        QVERIFY(index.gamesWithLine(start, moves(start, {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a5"})).isEmpty());

        // A game from a set-up position: its own start, its own lines.
        const ChessPosition custom = *ChessPosition::fromFen(endgame);
        QCOMPARE(index.gamesWithPosition(custom), ids({5}));
        QCOMPARE(index.gamesWithPosition(afterMoves(endgame, {"e2e4", "e8e7"})), ids({5}));
        QCOMPARE(index.gamesWithLine(custom, moves(custom, {"e2e4"})), ids({5}));
        QVERIFY(!index.gamesWithLine(start, moves(start, {"e2e4"})).contains(5));

        // Moves after an illegal one are not indexed.
        QCOMPARE(index.gamesWithPosition(afterMoves(QString(), {"d2d4"})), ids({6}));
        QCOMPARE(index.countWithPosition(afterMoves(QString(), {"d2d4", "e7e5"})), 0);

        // The lines of a database file, read at once.
        QTemporaryDir dir;
        QString error;
        GameRecord record;
        record.startFen = endgame;
        record.moves = {{QStringLiteral("e4"), QStringLiteral("e2e4")}, {QStringLiteral("Ke7"), QStringLiteral("e8e7")}};
        const std::unique_ptr<SqliteGameDatabase> database =
            SqliteGameDatabase::create(dir.filePath(QStringLiteral("lines.pdb")), {GameRecord(), record}, &error);
        QVERIFY2(database, qPrintable(error));
        const QList<GameLine> lines = database->gameLines();
        QCOMPARE(lines.size(), 2);
        QCOMPARE(lines.at(0).id, database->header(0).id);
        QVERIFY(lines.at(0).startFen.isEmpty());
        QCOMPARE(lines.at(1).startFen, endgame);
        QCOMPARE(lines.at(1).movesUci, QStringLiteral("e2e4 e8e7"));
    }

    void remembersWhoPlayersAre()
    {
        PlayerRoles roles{{QStringLiteral("Bianco Francesco"), PlayerRole::Me},
                          {QStringLiteral("DrNykterstein"), PlayerRole::Friend},
                          {QStringLiteral("Rossi Mario"), PlayerRole::Opponent}};
        GameRecord game;
        game.white = QStringLiteral("Rossi Mario");
        game.black = QStringLiteral("Bianco Francesco");
        QCOMPARE(mySide(game, roles), std::optional<Side>(Side::Black));
        std::swap(game.white, game.black);
        QCOMPARE(mySide(game, roles), std::optional<Side>(Side::White));
        game.black = QStringLiteral("Bianco Francesco");
        QVERIFY(!mySide(game, roles)); // Against myself.
        game.white = game.black = QStringLiteral("Somebody");
        QVERIFY(!mySide(game, roles));

        DatabaseOutline outline;
        GameRecord friendly;
        friendly.white = QStringLiteral("Bianco Francesco");
        friendly.black = QStringLiteral("DrNykterstein");
        outline.add(friendly, roles);
        outline.add(friendly, roles);
        QCOMPARE(outline.players.value(PlayerRole::Me).value(QStringLiteral("Bianco Francesco")), 2);
        QCOMPARE(outline.players.value(PlayerRole::Friend).value(QStringLiteral("DrNykterstein")), 2);
        QVERIFY(!outline.players.contains(PlayerRole::Opponent));
        QVERIFY(DatabaseOutline::hasRole(friendly, roles, PlayerRole::Friend));
        QVERIFY(!DatabaseOutline::hasRole(friendly, roles, PlayerRole::Opponent));

        // Stored in the database, and still there when it is opened again.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("roles.pdb"));
        QString error;
        {
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(path, {friendly}, &error);
            QVERIFY2(database, qPrintable(error));
            QVERIFY(database->playerRoles().isEmpty());
            QVERIFY2(database->setPlayerRole(QStringLiteral("Bianco Francesco"), PlayerRole::Me, &error), qPrintable(error));
            QVERIFY(database->setPlayerRole(QStringLiteral("DrNykterstein"), PlayerRole::Opponent, &error));
            QVERIFY(database->setPlayerRole(QStringLiteral("DrNykterstein"), PlayerRole::Friend, &error));
            QVERIFY(database->setPlayerRole(QStringLiteral("Unknown Player"), PlayerRole::Opponent, &error));
            QVERIFY(database->setPlayerRole(QStringLiteral("Unknown Player"), PlayerRole::None, &error));
        }
        const std::unique_ptr<SqliteGameDatabase> reopened = SqliteGameDatabase::open(path, &error);
        QVERIFY2(reopened, qPrintable(error));
        const PlayerRoles stored = reopened->playerRoles();
        QCOMPARE(stored.size(), 2);
        QCOMPARE(stored.value(QStringLiteral("Bianco Francesco")), PlayerRole::Me);
        QCOMPARE(stored.value(QStringLiteral("DrNykterstein")), PlayerRole::Friend);
    }

    void upgradesDatabasesToPlayerRoles()
    {
        // A version 2 file, as databases were before player roles.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("old.pdb"));
        QString error;
        QVERIFY(SqliteGameDatabase::create(path, {}, &error));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("downgrade"));
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral("DROP TABLE player_roles")));
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 2")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("downgrade"));
        const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
        QVERIFY2(database, qPrintable(error));
        QVERIFY2(database->setPlayerRole(QStringLiteral("Me"), PlayerRole::Me, &error), qPrintable(error));
    }

    void migratesDatabasesInOrder()
    {
        // The migrations are numbered without gaps: user_version is the cursor.
        const QList<DatabaseMigrations::Migration> &migrations = DatabaseMigrations::all();
        for (int i = 0; i < migrations.size(); ++i)
            QCOMPARE(migrations.at(i).version, i + 1);
        const int latest = DatabaseMigrations::latestVersion();
        QCOMPARE(latest, 6);

        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("games.pdb"));
        QString error;
        GameRecord game;
        game.white = QStringLiteral("A");
        game.black = QStringLiteral("B");
        game.moves = {{QStringLiteral("e4"), QStringLiteral("e2e4")}};
        QVERIFY2(SqliteGameDatabase::create(path, {game}, &error), qPrintable(error));
        const auto inFile = [&](const QString &sql) {
            QVariant value;
            {
                QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("peek"));
                db.setDatabaseName(path);
                if (db.open()) {
                    QSqlQuery query(db);
                    if (query.exec(sql) && query.next())
                        value = query.value(0);
                }
                db.close();
            }
            QSqlDatabase::removeDatabase(QStringLiteral("peek"));
            return value;
        };
        // A new file ran every migration and says so.
        QCOMPARE(inFile(QStringLiteral("PRAGMA user_version")).toInt(), latest);
        QCOMPARE(inFile(QStringLiteral("SELECT COUNT(*) FROM migrations")).toInt(), latest);
        QCOMPARE(inFile(QStringLiteral("SELECT name FROM migrations WHERE version = 6")).toString(),
                 QStringLiteral("create_game_states"));

        // A version 5 file, as before the trash and the log of migrations.
        inFile(QStringLiteral("DROP TABLE game_states"));
        inFile(QStringLiteral("DROP TABLE migrations"));
        inFile(QStringLiteral("PRAGMA user_version = 5"));
        QCOMPARE(inFile(QStringLiteral("PRAGMA user_version")).toInt(), 5);
        {
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
            QVERIFY2(database, qPrintable(error));
            QCOMPARE(database->gameCount(), 1);
            QCOMPARE(database->header(0).uid, GameIdentity::uid(game));
            QCOMPARE(database->header(0).state, GameState::Live);
            QVERIFY2(database->setGameState(0, GameState::Trashed, &error), qPrintable(error));
        }
        // Only the missing migration ran, and the file is at the latest version.
        QCOMPARE(inFile(QStringLiteral("PRAGMA user_version")).toInt(), latest);
        QCOMPARE(inFile(QStringLiteral("SELECT COUNT(*) FROM migrations")).toInt(), 1);
        QCOMPARE(inFile(QStringLiteral("SELECT MIN(version) FROM migrations")).toInt(), 6);

        // A file from a later version is refused, untouched.
        inFile(QStringLiteral("PRAGMA user_version = %1").arg(latest + 1));
        QVERIFY(!SqliteGameDatabase::open(path, &error));
        QVERIFY(error.contains(QStringLiteral("newer version")));
        QCOMPARE(inFile(QStringLiteral("PRAGMA user_version")).toInt(), latest + 1);
    }

    void mergesGameStatesByRevision()
    {
        const QString early = QStringLiteral("2026-10-01T10:00:00.000Z");
        const QString late = QStringLiteral("2026-10-02T10:00:00.000Z");
        const QList<GameStateRecord> local = {{QStringLiteral("a"), GameState::Trashed, early},
                                              {QStringLiteral("b"), GameState::Live, late},
                                              {QStringLiteral("c"), GameState::Deleted, early}};
        const QList<GameStateRecord> incoming = {
            {QStringLiteral("a"), GameState::Live, late},     // Restored later: taken.
            {QStringLiteral("b"), GameState::Trashed, early}, // Trashed before our restore: ours stays.
            {QStringLiteral("c"), GameState::Purged, early},  // A tie keeps the local one.
            {QStringLiteral("d"), GameState::Purged, early},  // Unknown here: taken.
            {QStringLiteral("d"), GameState::Live, late},     // Sent twice: the newest.
            {QString(), GameState::Trashed, late}};
        const QList<GameStateRecord> expected = {{QStringLiteral("a"), GameState::Live, late},
                                                 {QStringLiteral("d"), GameState::Live, late}};
        QCOMPARE(GameStates::incomingChanges(local, incoming), expected);
        QVERIFY(GameStates::incomingChanges(local, local).isEmpty());

        // The trash sets apart what went in during the last week.
        const QDateTime now = QDateTime::fromString(QStringLiteral("2026-10-10T12:00:00.000Z"), Qt::ISODateWithMs);
        QVERIFY(GameStates::isRecent(QStringLiteral("2026-10-10T11:59:00.000Z"), now));
        QVERIFY(GameStates::isRecent(QStringLiteral("2026-10-03T12:00:01.000Z"), now));
        QVERIFY(!GameStates::isRecent(QStringLiteral("2026-10-03T12:00:00.000Z"), now));
        QVERIFY(!GameStates::isRecent(QStringLiteral("2026-09-01T12:00:00.000Z"), now));
        QVERIFY(!GameStates::isRecent(QString(), now));

        QCOMPARE(gameStateFromKey(gameStateKey(GameState::Purged)), GameState::Purged);
        QCOMPARE(gameStateFromKey(QStringLiteral("whatever")), GameState::Live);
    }

    void trashesDeletesAndPurgesGames()
    {
        QTemporaryDir dir;
        QString error;
        const auto named = [](const QString &white, const QString &move) {
            GameRecord game;
            game.white = white;
            game.black = QStringLiteral("Common");
            game.event = white + QStringLiteral(" Open");
            game.moves = {{move, move}};
            return game;
        };
        const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(
            dir.filePath(QStringLiteral("games.pdb")),
            {named(QStringLiteral("Keep"), QStringLiteral("e2e4")), named(QStringLiteral("Gone"), QStringLiteral("d2d4"))},
            &error);
        QVERIFY2(database, qPrintable(error));
        GameSource source;
        source.kind = QStringLiteral("chesscom");
        source.account = QStringLiteral("someone");
        QVERIFY2(database->addSource(source, &error), qPrintable(error));
        ImportedGame imported;
        imported.externalId = QStringLiteral("x1");
        imported.game = named(QStringLiteral("Imported"), QStringLiteral("c2c4"));
        QCOMPARE(database->importGames(source.id, {imported}, &error), 1);
        QVERIFY(database->setPlayerRole(QStringLiteral("Gone"), PlayerRole::Friend, &error));

        // The trash takes a game out of the lists and of the position search, not out of the file.
        QVERIFY2(database->setGameState(1, GameState::Trashed, &error), qPrintable(error));
        QVERIFY2(database->setGameState(2, GameState::Trashed, &error), qPrintable(error));
        QCOMPARE(database->gameCount(), 3);
        QCOMPARE(database->countGames(GameState::Live), 1);
        QCOMPARE(database->countGames(GameState::Trashed), 2);
        QVERIFY(GameStates::isRecent(database->header(1).stateModified, QDateTime::currentDateTimeUtc()));
        QVERIFY(database->header(0).stateModified.isEmpty());
        QCOMPARE(database->gameLines().size(), 1);
        QCOMPARE(database->sources().first().importedGames, 0);
        QVERIFY(database->loadGame(1));
        // Nothing to purge yet: games in the trash are not deleted.
        QCOMPARE(database->optimize(&error), 0);
        QCOMPARE(database->gameCount(), 3);

        // Back from the trash.
        QVERIFY(database->setGameState(1, GameState::Live, &error));
        QCOMPARE(database->gameLines().size(), 2);
        QVERIFY(database->setGameState(1, GameState::Trashed, &error));

        // Deleted from the trash: still a row, in no list.
        const QString goneUid = database->header(1).uid;
        const QString importedUid = database->header(2).uid;
        QVERIFY(database->setGameState(1, GameState::Deleted, &error));
        QVERIFY(database->setGameState(2, GameState::Deleted, &error));
        QCOMPARE(database->gameCount(), 3);
        QCOMPARE(database->countGames(GameState::Trashed), 0);
        QVERIFY(!database->setGameState(0, GameState::Purged, &error)); // Only optimize() purges.

        // Optimizing removes them for good and remembers that it did.
        QCOMPARE(database->optimize(&error), 2);
        QCOMPARE(database->gameCount(), 1);
        QCOMPARE(database->header(0).white, QStringLiteral("Keep"));
        QList<GameStateRecord> states = database->gameStates();
        QCOMPARE(states.size(), 2);
        for (const GameStateRecord &state : std::as_const(states))
            QCOMPARE(state.state, GameState::Purged);
        // A player the user named stays known; the source does not import the game again.
        QCOMPARE(database->playerRoles().value(QStringLiteral("Gone")), PlayerRole::Friend);
        QCOMPARE(database->importGames(source.id, {imported}, &error), 0);
        QCOMPARE(database->gameCount(), 1);

        // Saved again by hand, a purged game is a game again, and says so.
        QCOMPARE(database->addGame(named(QStringLiteral("Gone"), QStringLiteral("d2d4")), &error), 1);
        QCOMPARE(database->header(1).uid, goneUid);
        QCOMPARE(database->header(1).state, GameState::Live);
        states = database->gameStates();
        for (const GameStateRecord &state : std::as_const(states))
            QCOMPARE(state.state, state.uid == goneUid ? GameState::Live : GameState::Purged);
        QVERIFY(importedUid != goneUid);
    }

    void trashAndPurgeReachOtherCopies()
    {
        QTemporaryDir dir;
        QString error;
        const QString pathA = dir.filePath(QStringLiteral("a.pdb"));
        const QString pathB = dir.filePath(QStringLiteral("b.pdb"));
        QList<GameRecord> games;
        for (const QString &move : {QStringLiteral("e2e4"), QStringLiteral("d2d4"), QStringLiteral("c2c4")}) {
            GameRecord game;
            game.white = move;
            game.moves = {{move, move}};
            games << game;
        }
        std::unique_ptr<SqliteGameDatabase> a = SqliteGameDatabase::create(pathA, games, &error);
        QVERIFY2(a, qPrintable(error));
        QVERIFY2(a->saveCopy(pathB, &error), qPrintable(error));
        std::unique_ptr<SqliteGameDatabase> b = SqliteGameDatabase::open(pathB, &error);
        QVERIFY2(b, qPrintable(error));
        const QString first = a->header(0).uid;
        const QString second = a->header(1).uid;

        // A trashes one game and deletes another; B only gets a new game.
        QVERIFY(a->setGameState(0, GameState::Trashed, &error));
        QVERIFY(a->setGameState(1, GameState::Trashed, &error));
        QVERIFY(a->setGameState(1, GameState::Deleted, &error));
        GameRecord extra;
        extra.white = QStringLiteral("only on B");
        extra.moves = {{QStringLiteral("g1f3"), QStringLiteral("g1f3")}};
        QCOMPARE(b->addGame(extra, &error), 3);

        // B follows A, and loses nothing of its own.
        QVERIFY2(DatabaseMerge::mergeInto(*b, pathA, &error), qPrintable(error));
        QCOMPARE(b->gameCount(), 4);
        QCOMPARE(b->header(0).state, GameState::Trashed);
        QCOMPARE(b->header(1).state, GameState::Deleted);
        QCOMPARE(b->countGames(GameState::Live), 2);

        // B takes the first game back, later: A follows.
        QTest::qWait(5);
        QVERIFY(b->setGameState(0, GameState::Live, &error));
        QVERIFY2(DatabaseMerge::mergeInto(*a, pathB, &error), qPrintable(error));
        QCOMPARE(a->gameCount(), 4);
        QCOMPARE(a->header(0).state, GameState::Live);
        QCOMPARE(a->header(1).state, GameState::Deleted);

        // A is optimized: the deleted game is gone from its file…
        QTest::qWait(5);
        QCOMPARE(a->optimize(&error), 1);
        QCOMPARE(a->gameCount(), 3);
        // …a copy that still has it does not bring it back…
        const std::optional<DatabaseMerge::Result> merged = DatabaseMerge::mergeInto(*a, pathB, &error);
        QVERIFY2(merged, qPrintable(error));
        QCOMPARE(merged->stored, 0);
        QCOMPARE(a->gameCount(), 3);
        // …and lets it go when it hears about the purge.
        QVERIFY2(DatabaseMerge::mergeInto(*b, pathA, &error), qPrintable(error));
        QCOMPARE(b->gameCount(), 3);
        for (qint64 index = 0; index < b->gameCount(); ++index) {
            QVERIFY(b->header(index).uid != second);
            QCOMPARE(b->header(index).state, GameState::Live);
        }
        QCOMPARE(b->header(0).uid, first);
    }

    void choosesShippedOpeningNames()
    {
        // One per language, English for any language without its own.
        QCOMPARE(ShippedOpeningNames::forLanguage(QStringLiteral("it")).lineage,
                 GameIdentity::kItalianOpeningNamesLineage);
        QCOMPARE(ShippedOpeningNames::forLanguage(QStringLiteral("en")).lineage, GameIdentity::kOpeningNamesLineage);
        QCOMPARE(ShippedOpeningNames::forLanguage(QStringLiteral("de")).lineage, GameIdentity::kOpeningNamesLineage);
        QVERIFY(ShippedOpeningNames::byLineage(GameIdentity::kItalianOpeningNamesLineage));
        QVERIFY(!ShippedOpeningNames::byLineage(GameIdentity::kClassicGamesLineage));

        // What is moved out of the Databases folder: shipped lineages, and the
        // seed's names only while the file has no lineage of its own.
        DatabaseProperties shipped;
        shipped.id = GameIdentity::kOpeningNamesLineage;
        QVERIFY(ShippedOpeningNames::shouldMove(QStringLiteral("anything.pdb"), shipped));
        DatabaseProperties unnamed;
        QVERIFY(ShippedOpeningNames::shouldMove(QStringLiteral("Opening Names.pdb"), unnamed));
        QVERIFY(!ShippedOpeningNames::shouldMove(QStringLiteral("English.pdb"), unnamed));
        QVERIFY(ShippedOpeningNames::shouldMove(QStringLiteral("Nomi delle aperture.pdb"), unnamed));
        QVERIFY(!ShippedOpeningNames::shouldMove(QStringLiteral("My Openings.pdb"), unnamed));
        DatabaseProperties own;
        own.id = QStringLiteral("0f69a9e9-7bf8-40e8-aa9d-841e4edb119a");
        own.type = DatabaseType::OpeningBook;
        QVERIFY(!ShippedOpeningNames::shouldMove(QStringLiteral("Opening Names.pdb"), own));

        // Never overwrite a copy already in the folder.
        const ShippedOpeningNames::Names english = ShippedOpeningNames::forLanguage(QStringLiteral("en"));
        QSet<QString> taken;
        const auto exists = [&taken](const QString &path) { return taken.contains(path); };
        const QString folder = QStringLiteral("/names");
        QCOMPARE(ShippedOpeningNames::moveTarget(folder, english, exists), QStringLiteral("/names/English.pdb"));
        taken.insert(QStringLiteral("/names/English.pdb"));
        QCOMPARE(ShippedOpeningNames::moveTarget(folder, english, exists),
                 QStringLiteral("/names/Old/English.pdb"));
        taken.insert(QStringLiteral("/names/Old/English.pdb"));
        QCOMPARE(ShippedOpeningNames::moveTarget(folder, english, exists),
                 QStringLiteral("/names/Old/English 2.pdb"));

        // A copy can go only when the kept one has all its games, none older.
        const QHash<QString, QString> kept{{QStringLiteral("a"), QStringLiteral("2026-09-29T10:00:00.000Z")},
                                           {QStringLiteral("b"), QString()}};
        QVERIFY(ShippedOpeningNames::addsNothing(kept, {{QStringLiteral("a"), QString()}}));
        QVERIFY(ShippedOpeningNames::addsNothing(kept, kept));
        QVERIFY(!ShippedOpeningNames::addsNothing(kept, {{QStringLiteral("c"), QString()}}));
        QVERIFY(!ShippedOpeningNames::addsNothing(kept, {{QStringLiteral("b"), QStringLiteral("2026-09-29T11:00:00.000Z")}}));
        QVERIFY(!ShippedOpeningNames::addsNothing(kept, {}));

        // Files named by older versions take the shipped name, others stay.
        DatabaseProperties italian;
        italian.id = GameIdentity::kItalianOpeningNamesLineage;
        QCOMPARE(ShippedOpeningNames::renamedFileName(QStringLiteral("Nomi delle aperture.pdb"), italian),
                 QStringLiteral("Italian.pdb"));
        QVERIFY(ShippedOpeningNames::renamedFileName(QStringLiteral("Italian.pdb"), italian).isEmpty());
        QVERIFY(ShippedOpeningNames::renamedFileName(QStringLiteral("Mie aperture.pdb"), italian).isEmpty());
        QVERIFY(ShippedOpeningNames::renamedFileName(QStringLiteral("Opening Names.pdb"), unnamed).isEmpty());
    }

    void namesDatabasesInEveryLanguage()
    {
        DatabaseProperties properties;
        QCOMPARE(properties.displayName(QStringLiteral("it"), QStringLiteral("file")), QStringLiteral("file"));
        properties.name = QStringLiteral("English");
        properties.localizedNames.insert(QStringLiteral("it"), QStringLiteral("Inglese"));
        QCOMPARE(properties.displayName(QStringLiteral("it"), QStringLiteral("file")), QStringLiteral("Inglese"));
        QCOMPARE(properties.displayName(QStringLiteral("it_IT"), QStringLiteral("file")), QStringLiteral("Inglese"));
        QCOMPARE(properties.displayName(QStringLiteral("en"), QStringLiteral("file")), QStringLiteral("English"));
        QCOMPARE(properties.displayName(QStringLiteral("de"), QStringLiteral("file")), QStringLiteral("English"));

        // Stored as name and name.<code>, and read back.
        const QHash<QString, QString> values = properties.values();
        QCOMPARE(values.value(QStringLiteral("name")), QStringLiteral("English"));
        QCOMPARE(values.value(QStringLiteral("name.it")), QStringLiteral("Inglese"));
        QCOMPARE(DatabaseProperties::fromValues(values), properties);

        // The columns a database hides are stored too, and so is showing them all again.
        properties.hiddenColumns = {QStringLiteral("result"), QStringLiteral("site")};
        QCOMPARE(properties.values().value(QStringLiteral("columns.hidden")), QStringLiteral("result,site"));
        QCOMPARE(DatabaseProperties::fromValues(properties.values()), properties);
        properties.hiddenColumns.clear();
        QVERIFY(properties.values().contains(QStringLiteral("columns.hidden")));
        QVERIFY(DatabaseProperties::fromValues(properties.values()).hiddenColumns.isEmpty());

        // The shipped ones carry their names in every language we have.
        for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
            DatabaseProperties shipped;
            names.applyNames(shipped);
            QVERIFY(!shipped.displayName(QStringLiteral("en"), QString()).isEmpty());
            QVERIFY(!shipped.localizedNames.value(QStringLiteral("it")).isEmpty());
        }
    }

    void storesDatabaseProperties()
    {
        QCOMPARE(DatabaseProperties::fromValues({}).type, DatabaseType::GameCollection);
        QCOMPARE(DatabaseProperties::fromValues({{QStringLiteral("type"), QStringLiteral("unknown")}}).type,
                 DatabaseType::GameCollection);
        DatabaseProperties book;
        book.type = DatabaseType::OpeningBook;
        book.description = QStringLiteral("Named lines");
        QCOMPARE(DatabaseProperties::fromValues(book.values()), book);

        // A version 3 file, as databases were before properties, is a game collection.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("names.pdb"));
        QString error;
        QVERIFY(SqliteGameDatabase::create(path, {}, &error));
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("downgrade"));
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral("DROP TABLE properties")));
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 3")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("downgrade"));
        QCOMPARE(SqliteGameDatabase::readProperties(path).type, DatabaseType::GameCollection);
        {
            const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
            QVERIFY2(database, qPrintable(error));
            QCOMPARE(database->properties(), DatabaseProperties());
            QVERIFY2(database->setProperties(book, &error), qPrintable(error));
        }
        // Read without opening (Book ▸ Opening Names), and when opened again.
        QCOMPARE(SqliteGameDatabase::readProperties(path), book);
        const std::unique_ptr<SqliteGameDatabase> reopened = SqliteGameDatabase::open(path, &error);
        QVERIFY2(reopened, qPrintable(error));
        QCOMPARE(reopened->properties(), book);
        QCOMPARE(SqliteGameDatabase::readProperties(dir.filePath(QStringLiteral("missing.pdb"))), DatabaseProperties());
    }

    void makesUniversalGameIds()
    {
        // RFC 4122 / Python's uuid.uuid5(uuid.NAMESPACE_DNS, "www.example.com").
        QCOMPARE(GameIdentity::uuidV5(QUuid(QStringLiteral("6ba7b810-9dad-11d1-80b4-00c04fd430c8")),
                                      QByteArrayLiteral("www.example.com")),
                 QStringLiteral("2ed6657d-e927-568b-95e1-2665a8aea6a2"));

        GameRecord game;
        game.white = QStringLiteral(" Morphy ");
        game.black = QStringLiteral("Duke Karl / Count Isouard");
        game.event = QStringLiteral("Paris");
        game.date = QStringLiteral("1858.??.??");
        game.result = QStringLiteral("1-0");
        game.moves = {{QStringLiteral("e4"), QStringLiteral("e2e4")}, {QStringLiteral("e5"), QStringLiteral("e7e5")}};
        // The documented content, so the phone makes the same uid.
        QCOMPARE(GameIdentity::content(game),
                 QStringLiteral("Morphy\nDuke Karl / Count Isouard\nParis\n\n1858.??.??\n\n1-0\n\ne2e4 e7e5"));
        QCOMPARE(GameIdentity::uid(game),
                 GameIdentity::uuidV5(GameIdentity::kGameNamespace, GameIdentity::content(game).toUtf8()));
        QVERIFY(GameIdentity::uid(game, 2) != GameIdentity::uid(game));
        GameRecord renamed = game;
        renamed.eco = QStringLiteral("C41"); // Not part of the identity, but of the content compared.
        QCOMPARE(GameIdentity::uid(renamed), GameIdentity::uid(game));
        QVERIFY(!GameIdentity::sameContent(renamed, game));
    }

    void plansReconciliation()
    {
        const auto game = [](const QString &uid, const QString &white, const QString &modified) {
            GameRecord record;
            record.uid = uid;
            record.white = white;
            record.modified = modified;
            return record;
        };
        const QList<GameRecord> local{game(QStringLiteral("a"), QStringLiteral("Same"), QString()),
                                      game(QStringLiteral("b"), QStringLiteral("Mine"), QStringLiteral("2026-09-29T10:00:00.000Z")),
                                      game(QStringLiteral("c"), QStringLiteral("Mine"), QStringLiteral("2026-09-29T10:00:00.000Z")),
                                      game(QStringLiteral("d"), QStringLiteral("Only here"), QString())};
        const QList<GameRecord> incoming{game(QStringLiteral("a"), QStringLiteral("Same"), QStringLiteral("2026-09-30T00:00:00Z")),
                                         game(QStringLiteral("b"), QStringLiteral("Theirs"), QStringLiteral("2026-09-29T12:00:00.000+02:00")),
                                         game(QStringLiteral("c"), QStringLiteral("Theirs"), QStringLiteral("2026-09-29T11:00:00.000Z")),
                                         game(QStringLiteral("e"), QStringLiteral("New"), QString()),
                                         game(QStringLiteral("e"), QStringLiteral("New"), QString())};
        const Reconcile::Plan plan = Reconcile::plan(local, incoming);
        QCOMPARE(plan.insert, QList<int>{3});
        // b: 12:00+02:00 is 10:00Z, a tie: local stays. c: theirs is newer.
        QCOMPARE(plan.update, (QList<std::pair<int, int>>{{2, 2}}));
        QCOMPARE(plan.known, 3); // a (same), b (local kept), the second e.
        QCOMPARE(plan.conflicts, (QStringList{QStringLiteral("b"), QStringLiteral("c")}));
    }

    void upgradesDatabasesToGameIdentity()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("v4.pdb"));
        QString error;
        GameRecord game;
        game.white = QStringLiteral("A");
        game.black = QStringLiteral("B");
        game.moves = {{QStringLiteral("e4"), QStringLiteral("e2e4")}};
        {
            const std::unique_ptr<SqliteGameDatabase> created = SqliteGameDatabase::create(path, {game, game}, &error);
            QVERIFY2(created, qPrintable(error));
            // Identical games get the next occurrence, deterministically.
            QCOMPARE(created->header(0).uid, GameIdentity::uid(game));
            QCOMPARE(created->header(1).uid, GameIdentity::uid(game, 2));
            QVERIFY(!created->header(0).modified.isEmpty());
            QVERIFY(!created->properties().id.isEmpty());
        }
        {
            // Back to version 4: no uids, no id.
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("downgrade"));
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec(QStringLiteral("DROP INDEX games_uid")));
            QVERIFY(query.exec(QStringLiteral("ALTER TABLE games DROP COLUMN uid")));
            QVERIFY(query.exec(QStringLiteral("ALTER TABLE games DROP COLUMN modified")));
            QVERIFY(query.exec(QStringLiteral("DELETE FROM properties WHERE key = 'id'")));
            QVERIFY(query.exec(QStringLiteral("PRAGMA user_version = 4")));
            db.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("downgrade"));
        std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
        QVERIFY2(database, qPrintable(error));
        QCOMPARE(database->header(0).uid, GameIdentity::uid(game));
        QCOMPARE(database->header(1).uid, GameIdentity::uid(game, 2));
        QVERIFY(database->header(0).modified.isEmpty());
        QVERIFY(database->properties().id.isEmpty()); // Given when first synced.

        // Edits keep the uid and move the revision; a new copy of the game gets the next occurrence.
        GameRecord edited = *database->loadGame(0);
        edited.white = QStringLiteral("A. Player");
        QVERIFY(database->updateHeader(0, edited, &error));
        QCOMPARE(database->header(0).uid, GameIdentity::uid(game));
        QVERIFY(!database->header(0).modified.isEmpty());
        edited.moves << MoveRecord{QStringLiteral("e5"), QStringLiteral("e7e5")};
        edited.modified.clear();
        QVERIFY(database->replaceGame(0, edited, &error));
        QCOMPARE(database->loadGame(0)->moves.size(), 2);
        QCOMPARE(database->header(0).uid, GameIdentity::uid(game));
        QCOMPARE(database->addGame(game, &error), 2);
        QCOMPARE(database->header(2).uid, GameIdentity::uid(game, 3));

        // The databases we ship take their fixed id once.
        database.reset();
        QVERIFY(SqliteGameDatabase::adoptLineage(path, GameIdentity::kClassicGamesLineage));
        QCOMPARE(SqliteGameDatabase::readProperties(path).id, GameIdentity::kClassicGamesLineage);
        QVERIFY(SqliteGameDatabase::adoptLineage(path, GameIdentity::kOpeningNamesLineage));
        QCOMPARE(SqliteGameDatabase::readProperties(path).id, GameIdentity::kClassicGamesLineage);
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

    void namesOpeningsFromGames()
    {
        const QList<GameRecord> games = OpeningNames::gamesFromTsv(QStringLiteral(
            "eco\tname\tpgn\n"
            "B20\tSicilian Defense\t1. e4 c5\n"
            "B90\tSicilian Defense: Najdorf Variation\t1. e4 c5 2. Nf3 d6 3. d4 cxd4 4. Nxd4 Nf6 5. Nc3 a6\n"
            "B50\tSicilian Defense: Modern Variations\t1. e4 c5 2. Nf3 d6\n"
            "B50\tSicilian Defense: Longer Transposition\t1. Nf3 c5 2. e4 d6\n"
            "A00\tBroken\t1. e5\n"));
        QCOMPARE(games.size(), 4);
        QCOMPARE(games.first().eco, QStringLiteral("B20"));
        QCOMPARE(games.first().event, QStringLiteral("Sicilian Defense"));
        QCOMPARE(games.first().moves.size(), 2);

        const OpeningNames names(games);
        QCOMPARE(names.size(), 3);
        ChessPosition position = ChessPosition::startingPosition();
        QVERIFY(names.name(position).isEmpty());
        for (const char *uci : {"e2e4", "c7c5"})
            position.play(*position.moveFromUci(QString::fromLatin1(uci)));
        QCOMPARE(names.name(position).name, QStringLiteral("Sicilian Defense"));
        QCOMPARE(names.name(position).eco, QStringLiteral("B20"));
        position.play(*position.moveFromUci(u"g1f3"));
        QVERIFY(names.name(position).isEmpty()); // Only positions where a line ends are named.
        position.play(*position.moveFromUci(u"d7d6"));
        // Both lines reach this position: the one written first and shorter or equal keeps it.
        QCOMPARE(names.name(position).name, QStringLiteral("Sicilian Defense: Modern Variations"));
    }

    void computesPolyglotKeys()
    {
        // Reference keys of the Polyglot format specification.
        const QList<std::pair<QStringList, quint64>> lines{
            {{}, Q_UINT64_C(0x463b96181691fc9c)},
            {{"e2e4"}, Q_UINT64_C(0x823c9b50fd114196)},
            {{"e2e4", "d7d5"}, Q_UINT64_C(0x0756b94461c50fb0)},
            {{"e2e4", "d7d5", "e4e5"}, Q_UINT64_C(0x662fafb965db29d4)},
            {{"e2e4", "d7d5", "e4e5", "f7f5"}, Q_UINT64_C(0x22a48b5a8e47ff78)},
            {{"e2e4", "d7d5", "e4e5", "f7f5", "e1e2"}, Q_UINT64_C(0x652a607ca3f242c1)},
            {{"e2e4", "d7d5", "e4e5", "f7f5", "e1e2", "e8f7"}, Q_UINT64_C(0x00fdd303c946bdd9)},
            {{"a2a4", "b7b5", "h2h4", "b5b4", "c2c4"}, Q_UINT64_C(0x3c8123ea7b067637)},
            {{"a2a4", "b7b5", "h2h4", "b5b4", "c2c4", "b4c3", "a1a3"}, Q_UINT64_C(0x5c3f9b829b279560)},
        };
        for (const auto &[moves, key] : lines) {
            ChessPosition position = ChessPosition::startingPosition();
            for (const QString &uci : moves)
                position.play(*position.moveFromUci(uci));
            QCOMPARE(QString::number(PolyglotBook::key(position), 16), QString::number(key, 16));
        }
    }

    void readsPolyglotBooks()
    {
        ChessPosition start = ChessPosition::startingPosition();
        const ChessPosition castling =
            *ChessPosition::fromFen(QStringLiteral("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
        const ChessMove shortCastle = *castling.moveFromUci(u"e1g1");
        QCOMPARE(PolyglotBook::encodeMove(castling, shortCastle), quint16((0 << 9) | (4 << 6) | (0 << 3) | 7));
        QCOMPARE(PolyglotBook::decodeMove(castling, PolyglotBook::encodeMove(castling, shortCastle)), shortCastle);
        const ChessMove longCastle = *castling.moveFromUci(u"e1c1");
        QCOMPARE(PolyglotBook::decodeMove(castling, PolyglotBook::encodeMove(castling, longCastle)), longCastle);

        const quint64 startKey = PolyglotBook::key(start);
        QList<PolyglotBook::Entry> entries{
            {startKey, PolyglotBook::encodeMove(start, *start.moveFromUci(u"d2d4")), 30},
            {Q_UINT64_C(0x0000000000000001), 0, 1},
            {startKey, PolyglotBook::encodeMove(start, *start.moveFromUci(u"e2e4")), 70},
            {Q_UINT64_C(0xffffffffffffffff), 0, 1},
            {startKey, quint16((1 << 9) | (4 << 6) | (4 << 3) | 4), 5}, // e2e5: illegal.
        };
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("book.bin")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(PolyglotBook::write(entries));
        file.close();

        PolyglotBook book;
        QString error;
        QVERIFY2(book.open(file.fileName(), &error), qPrintable(error));
        const QList<PolyglotBook::Move> moves = book.moves(start);
        QCOMPARE(moves.size(), 2);
        QCOMPARE(moves.at(0).move.uci(), QStringLiteral("e2e4"));
        QCOMPARE(moves.at(0).weight, 70);
        QCOMPARE(moves.at(1).move.uci(), QStringLiteral("d2d4"));
        start.play(moves.at(0).move);
        QVERIFY(book.moves(start).isEmpty());

        QFile broken(dir.filePath(QStringLiteral("broken.bin")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not a book");
        broken.close();
        QVERIFY(!book.open(broken.fileName(), &error));
        QVERIFY(!book.isOpen());
    }

    void keepsARepertoireInPolyglotBooks()
    {
        const ChessPosition start = ChessPosition::startingPosition();
        const quint64 startKey = PolyglotBook::key(start);
        const ChessMove e4 = *start.moveFromUci(u"e2e4");
        const ChessMove d4 = *start.moveFromUci(u"d2d4");
        // d4 is lighter and carries a reserved learn bit another tool may have set.
        const QList<PolyglotBook::Entry> entries{
            {startKey, PolyglotBook::encodeMove(start, d4), 30, 0x80},
            {startKey, PolyglotBook::encodeMove(start, e4), 70, 0},
        };
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("book.bin"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(PolyglotBook::write(entries));
        file.close();

        PolyglotBook book;
        QString error;
        QVERIFY2(book.open(path, &error), qPrintable(error));
        QCOMPARE(book.moves(start).at(0).move, e4);
        QVERIFY(!book.moves(start).at(1).inRepertoire());

        // In the repertoire it comes first whatever the weight, and stays so when the book is opened again.
        QVERIFY2(book.setInRepertoire(start, d4, true, &error), qPrintable(error));
        QVERIFY(book.isOpen());
        QCOMPARE(book.moves(start).at(0).move, d4);
        QVERIFY(book.moves(start).at(0).inRepertoire());
        PolyglotBook reopened;
        QVERIFY(reopened.open(path, &error));
        QCOMPARE(reopened.moves(start).at(0).move, d4);
        QCOMPARE(reopened.moves(start).at(0).learn, quint32(0x81));
        QCOMPARE(reopened.moves(start).at(0).weight, 30);
        reopened.close();

        QVERIFY(book.setInRepertoire(start, d4, false, &error));
        QCOMPARE(book.moves(start).at(0).move, e4);
        QCOMPARE(book.moves(start).at(1).learn, quint32(0x80));
        // A move the book does not have cannot be marked.
        QVERIFY(!book.setInRepertoire(start, *start.moveFromUci(u"g1f3"), true, &error));
    }

    void parsesTorneiOnlinePages()
    {
        // Latin-1 page text around UTF-8 data from the site's database.
        const QByteArray mixed = QByteArray("Citt\xe0 ") + QByteArray("54\xc2\xb0 OPEN");
        QCOMPARE(TorneiOnlineFetch::decodePage(mixed), QStringLiteral("Città 54° OPEN"));

        const QString search = QStringLiteral(
            "<table><tr>\n<td><SPAN><B>Nome</B></SPAN></td></tr>\t\t\t<tr>\n"
            "<td bgcolor=FFFFFF><SPAN class=tpolcorpo><A HREF=giocatori_d.php?progre=3489&tipo=a>BIANCO Francesco </A></SPAN></td>\n"
            "<td bgcolor=FFFFFF><SPAN class=tpolcorpo>2N</SPAN></td>\n"
            "<td bgcolor=FFFFFF><SPAN class=tpolcorpo>152159</SPAN></td>\n"
            "<td bgcolor=FFFFFF><SPAN class=tpolcorpo><A HREF=https://ratings.fide.com/profile/896489 target=_blank>896489</A></SPAN></td>\n"
            "</tr></table>");
        const std::optional<TorneiOnlineFetch::Player> player = TorneiOnlineFetch::parsePlayerSearch(search, QStringLiteral("896489"));
        QVERIFY(player);
        QCOMPARE(player->progre, QStringLiteral("3489"));
        QCOMPARE(player->name, QStringLiteral("BIANCO Francesco"));
        QVERIFY(TorneiOnlineFetch::parsePlayerSearch(search, QStringLiteral("152159")));
        QVERIFY(!TorneiOnlineFetch::parsePlayerSearch(search, QStringLiteral("89648")));

        // Rows of the tournament list are not closed.
        const QString tournaments = QStringLiteral(
            "<b>Tornei disputati: 1</b><table><tr>\t\t\t<tr>\n"
            "<td class=tpolthin><SPAN class=tpolcorpo><A HREF=tornei_d.php?codice=2601025A&tipo=p&ord=n&sen=a>2601025A</A>&nbsp;</SPAN></td>\n"
            "<td class=tpolthin><SPAN class=tpolcorpo>CP TRAPANI 2026</SPAN></td>\n"
            "<td class=tpolthin><SPAN class=tpolcorpo>TP</SPAN></td>\n"
            "<td class=tpolthinleft><SPAN class=tpolcorpo>10-01-2026</SPAN></td>\n"
            "<td class=tpolthinright><SPAN class=tpolcorpo>18-01-2026<BR></SPAN></td>\n"
            "<td class=tpolthin><SPAN class=tpolcorpo>0</SPAN></td>\n"
            "<tr>\t\t\t</table>");
        const QList<TorneiOnlineFetch::Tournament> list = TorneiOnlineFetch::parseTournaments(tournaments);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().code, QStringLiteral("2601025A"));
        QCOMPARE(list.first().name, QStringLiteral("CP TRAPANI 2026"));
        QCOMPARE(list.first().start, QDate(2026, 1, 10));
        QCOMPARE(list.first().end, QDate(2026, 1, 18));

        const QString participants = QStringLiteral(
            "<tr><td><A HREF=giocatori_d.php?progre=34890&tipo=a>x</A></td>"
            "<td><A HREF=tornei_d.php?codice=2601025A&gix=3&tipo=g&ord=u&sen=a title=Cartellino>Other</A></td></tr>"
            "<tr><td><A HREF=giocatori_d.php?progre=3489&tipo=a>x</A></td>"
            "<td><A HREF=tornei_d.php?codice=2601025A&gix=8&tipo=g&ord=u&sen=a title=Cartellino>Bianco Francesco</A></td></tr>");
        QCOMPARE(TorneiOnlineFetch::parseParticipantNumber(participants, QStringLiteral("3489")), QStringLiteral("8"));
        QVERIFY(TorneiOnlineFetch::parseParticipantNumber(participants, QStringLiteral("1")).isEmpty());

        const auto cardRow = [](const QStringList &values) {
            QString row = QStringLiteral("<tr>");
            for (const QString &value : values)
                row += QStringLiteral("<td class=tpolthin><SPAN class=tpolcorpo>&nbsp;%1&nbsp;</SPAN></td>").arg(value);
            return row + QStringLiteral("</tr>");
        };
        const QString card = QStringLiteral(
            "<font color=FFFFFF>Cartellini giocatori</font><table><tr><td colspan=16><SPAN><CENTER>"
            "<B>8 - Bianco Francesco &nbsp;&nbsp;<IMG SRC=img/flags/ITA.png></B></CENTER></SPAN></td></tr>"
            "<tr><td colspan=16><SPAN><CENTER>&nbsp;<B>2N</B>&nbsp;&nbsp;Elo FIDE:  <B>1694</B></CENTER></td></tr>")
            + cardRow({QString(), QStringLiteral("T"), QStringLiteral("C"), QStringLiteral("Num"), QStringLiteral("Ban"),
                       QStringLiteral("Cat"), QStringLiteral("Avversario"), QStringLiteral("Fed"), QStringLiteral("FIDE"),
                       QStringLiteral("Italia"), QStringLiteral("Diff"), QStringLiteral("Exp"), QStringLiteral("Ris"),
                       QStringLiteral("F"), QStringLiteral("Elo"), QStringLiteral("Tot")})
            + cardRow({QString(), "1", "N", "25", QString(), "NC", "Cammarata Elisa", "ITA", QString(), "1399",
                       QString(), QString(), "1", QString(), "0", "1"})
            + cardRow({QString(), "2", "B", "15", QString(), "2N", "Mancuso Luigi", "ITA", "1572", QString(), "122",
                       "0.67", "&frac12;", QString(), "0", "1"})
            + cardRow({QString(), "3", "B", "3", QString(), "1N", "Fontana Giulio", "ITA", "1809", QString(), "-115",
                       "0.34", "0", QString(), "0", "1"})
            + cardRow({QString(), "4", QString(), "0", QString(), QString(), QString(), QString(), QString(), QString(),
                       QString(), QString(), "1", "F", "0", "2"}) // Bye.
            + cardRow({QString(), "5", "N", "7", QString(), "NC", "Carollo Vito", "ITA", QString(), "1399", QString(),
                       QString(), QString(), QString(), "0", "2"}) // Not played yet.
            + QStringLiteral("</table>");
        const QList<ImportedGame> games = TorneiOnlineFetch::parseScoreCard(card, list.first());
        QCOMPARE(games.size(), 3);
        QCOMPARE(games.at(0).externalId, QStringLiteral("2601025A/1"));
        QCOMPARE(games.at(0).game.white, QStringLiteral("Cammarata Elisa"));
        QCOMPARE(games.at(0).game.whiteElo, 1399);
        QCOMPARE(games.at(0).game.black, QStringLiteral("Bianco Francesco"));
        QCOMPARE(games.at(0).game.blackElo, 1694);
        QCOMPARE(games.at(0).game.result, QStringLiteral("0-1"));
        QCOMPARE(games.at(0).game.event, QStringLiteral("CP TRAPANI 2026"));
        QCOMPARE(games.at(0).game.date, QStringLiteral("2026.01.10"));
        QCOMPARE(games.at(0).game.round, QStringLiteral("1"));
        QVERIFY(games.at(0).game.moves.isEmpty());
        QCOMPARE(games.at(1).game.white, QStringLiteral("Bianco Francesco"));
        QCOMPARE(games.at(1).game.blackElo, 1572);
        QCOMPARE(games.at(1).game.result, QStringLiteral("1/2-1/2"));
        QCOMPARE(games.at(2).game.result, QStringLiteral("0-1"));
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
        ImportedGame withoutMoves; // Records from torneionline.com have no moves.
        withoutMoves.externalId = QStringLiteral("c");
        withoutMoves.game.white = QStringLiteral("Bianco Francesco");
        withoutMoves.game.result = QStringLiteral("1-0");
        QCOMPARE(database->importGames(source.id, {withoutMoves}, &error), 1);
        QVERIFY(database->loadGame(2)->moves.isEmpty());
        QCOMPARE(database->gameCount(), 3);

        source.state.insert(QStringLiteral("month"), QStringLiteral("2024/05"));
        source.lastError = QStringLiteral("offline");
        QVERIFY(database->updateSource(source, &error));
        const QList<GameSource> stored = database->sources();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored.first().importedGames, 3);
        QCOMPARE(stored.first().state.value(QStringLiteral("month")).toString(), QStringLiteral("2024/05"));
        QCOMPARE(stored.first().lastError, QStringLiteral("offline"));
        QCOMPARE(database->sourceGameIds(source.id).size(), 3);

        QVERIFY(database->removeSource(source.id, &error));
        QVERIFY(database->sources().isEmpty());
        QCOMPARE(database->gameCount(), 3); // Imported games stay.
    }

private:
    static bool isErrorVerdict(MoveExplanation::Verdict verdict)
    {
        return verdict == MoveExplanation::Verdict::Inaccuracy || verdict == MoveExplanation::Verdict::Mistake
            || verdict == MoveExplanation::Verdict::Blunder;
    }
};

void TestChessRules::keepsTheBundledEngine()
{
    EngineCatalog catalog;
    QCOMPARE(catalog.engines().size(), 1);
    QVERIFY(catalog.engines().first().bundled);
    QVERIFY(!catalog.remove(EngineCatalog::kBundledId));

    // Only threads and hash of the bundled engine belong to the user.
    EngineProfile bundled = catalog.engines().first();
    bundled.name = QStringLiteral("Renamed");
    bundled.path = QStringLiteral("/tmp/other");
    bundled.threads = 4;
    catalog.update(bundled);
    QCOMPARE(catalog.engines().first().name, EngineCatalog::bundledEngineName());
    QVERIFY(catalog.engines().first().path.isEmpty());
    QCOMPARE(catalog.engines().first().threads, 4);
}

void TestChessRules::savesAndResolvesEngines()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ini = dir.filePath(QStringLiteral("engines.ini"));

    EngineCatalog catalog;
    EngineProfile lc0;
    lc0.name = QStringLiteral("Leela");
    lc0.path = QStringLiteral("/usr/local/bin/lc0");
    lc0.hashMb = 256;
    const QString id = catalog.add(lc0);
    {
        QSettings settings(ini, QSettings::IniFormat);
        catalog.save(settings);
    }
    QSettings settings(ini, QSettings::IniFormat);
    const EngineCatalog loaded = EngineCatalog::load(settings);
    QCOMPARE(loaded.engines().size(), 2);
    QVERIFY(loaded.engines().first().bundled);
    QCOMPARE(loaded.find(id)->path, lc0.path);
    QCOMPARE(loaded.find(id)->hashMb, 256);

    // Projects travel between computers: an unknown id is the bundled engine,
    // and projects before ids named the engine or its command.
    QCOMPARE(loaded.resolve(id).name, QStringLiteral("Leela"));
    QVERIFY(loaded.resolve(QStringLiteral("no-such-id")).bundled);
    QCOMPARE(loaded.resolve({}, QStringLiteral("lc0")).id, id);
    QVERIFY(loaded.resolve({}, QStringLiteral("stockfish")).bundled);

    QVERIFY(catalog.remove(id));
    QCOMPARE(catalog.engines().size(), 1);
}

void TestChessRules::addsDetectedEnginesOnce()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString engine = dir.filePath(QStringLiteral("stockfish"));
    const QString bundled = dir.filePath(QStringLiteral("bundled-stockfish"));
    const QString other = dir.filePath(QStringLiteral("berserk"));
    for (const QString &path : {engine, bundled, other}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
#ifdef Q_OS_WIN
    // QFile::link makes a shortcut there, not a symlink: the same file is
    // reached twice by writing its path in another case.
    const QString link = engine.toUpper();
#else
    const QString link = dir.filePath(QStringLiteral("stockfish-link"));
    QVERIFY(QFile::link(engine, link));
#endif

    EngineCatalog catalog;
    // The same file reached twice (through a symlink), and the bundled engine, count once.
    const QList<DetectedEngine> found = {{engine, QStringLiteral("Stockfish 17")},
                                         {link, QStringLiteral("Stockfish 17")},
                                         {bundled, QStringLiteral("Stockfish 19")}};
    QCOMPARE(catalog.addDetected(found, bundled), 1);
    QCOMPARE(catalog.engines().size(), 2);
    QCOMPARE(catalog.engines().last().name, QStringLiteral("Stockfish 17"));

    // Detecting again adds only what is new.
    QCOMPARE(catalog.addDetected(found, bundled), 0);
    QCOMPARE(catalog.addDetected({{other, QString()}}, bundled), 1);
    QCOMPARE(catalog.engines().last().name, QStringLiteral("berserk"));
}

void TestChessRules::recognizesEngineFiles()
{
    QVERIFY(EngineDetector::looksLikeEngine(QStringLiteral("stockfish")));
    QVERIFY(EngineDetector::looksLikeEngine(QStringLiteral("/usr/games/stockfish")));
    QVERIFY(EngineDetector::looksLikeEngine(QStringLiteral("stockfish_17_x64.exe")));
    QVERIFY(EngineDetector::looksLikeEngine(QStringLiteral("Lc0.exe")));
    QVERIFY(EngineDetector::looksLikeEngine(QStringLiteral("berserk-13")));
    QVERIFY(!EngineDetector::looksLikeEngine(QStringLiteral("stockfishing")));
    QVERIFY(!EngineDetector::looksLikeEngine(QStringLiteral("firefox")));

#ifndef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("sub")));
    const QString engine = dir.filePath(QStringLiteral("sub/stockfish"));
    const QString notExecutable = dir.filePath(QStringLiteral("lc0"));
    for (const QString &path : {engine, notExecutable}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
    QVERIFY(QFile::setPermissions(engine, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QVERIFY(QFile::link(engine, dir.filePath(QStringLiteral("stockfish"))));
    // Subfolders only as deep as asked; each file once however it is reached.
    QCOMPARE(EngineDetector::candidates({{dir.path(), 0}}).size(), 1);
    QCOMPARE(EngineDetector::candidates({{dir.path(), 1}}).size(), 1);
    QCOMPARE(EngineDetector::candidates({{dir.filePath(QStringLiteral("sub")), 0}}).size(), 1);
#endif
}

QTEST_GUILESS_MAIN(TestChessRules)
#include "tst_chessrules.moc"
