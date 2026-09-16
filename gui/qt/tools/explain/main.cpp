// pragma-explain: the "Explain" command on the command line.
//
// Runs the same explanation as the desktop client on lines pasted as PGN,
// SAN or UCI, with fixed-depth, single-thread searches so that results are
// reproducible while the explanation is being tuned. It also runs the
// AdvantageProbe searches and prints how the explanation was reached.

#include "app/AdvantageProbe.h"
#include "app/ChessPosition.h"
#include "app/ExplanationSearch.h"
#include "app/MoveExplanation.h"
#include "app/Pgn.h"
#include "app/UciEngine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTextStream>
#include <QTimer>

namespace {

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

/// Runs the explanation searches, waiting for the result.
std::optional<ExplanationAnalysis> analyze(ExplanationSearch &search, const std::optional<ChessPosition> &before,
                                           const std::optional<ChessMove> &played, const ChessPosition &after)
{
    std::optional<ExplanationAnalysis> result;
    QEventLoop loop;
    const QList<QMetaObject::Connection> connections{
        QObject::connect(&search, &ExplanationSearch::finished, &loop, [&](const ExplanationAnalysis &analysis) {
            result = analysis;
            loop.quit();
        }),
        QObject::connect(&search, &ExplanationSearch::failed, &loop, [&](const QString &message) {
            err() << "The engine stopped: " << message << '\n';
            loop.quit();
        }),
    };
    search.analyze(before, played, after);
    loop.exec();
    for (const QMetaObject::Connection &connection : connections)
        QObject::disconnect(connection);
    return result;
}

QString arrowKindName(BoardArrow::Kind kind)
{
    switch (kind) {
    case BoardArrow::Kind::Refutation: return QStringLiteral("refutation");
    case BoardArrow::Kind::Idea: return QStringLiteral("idea");
    case BoardArrow::Kind::Reply: return QStringLiteral("reply");
    case BoardArrow::Kind::Alternative: return QStringLiteral("better");
    }
    return {};
}

QString verdictName(MoveExplanation::Verdict verdict)
{
    switch (verdict) {
    case MoveExplanation::Verdict::None: return QStringLiteral("-");
    case MoveExplanation::Verdict::Best: return QStringLiteral("best");
    case MoveExplanation::Verdict::Good: return QStringLiteral("good");
    case MoveExplanation::Verdict::Inaccuracy: return QStringLiteral("inaccuracy");
    case MoveExplanation::Verdict::Mistake: return QStringLiteral("mistake");
    case MoveExplanation::Verdict::Blunder: return QStringLiteral("blunder");
    }
    return {};
}

void printBoard(const ChessPosition &position)
{
    for (int rank = 7; rank >= 0; --rank) {
        out() << "    " << rank + 1 << ' ';
        for (int file = 0; file < 8; ++file) {
            const Piece piece = position.at(rank * 8 + file);
            QChar c = QLatin1Char(" pnbrqk"[int(piece.type)]);
            if (piece.isNull())
                c = (rank + file) % 2 ? QLatin1Char('.') : QLatin1Char(':');
            else if (piece.side == Side::White)
                c = c.toUpper();
            out() << ' ' << c;
        }
        out() << '\n';
    }
    out() << "       a b c d e f g h\n";
}

void printSearch(const char *label, const ChessPosition &position, const QList<EngineEvaluation> &byDepth, bool trace)
{
    out() << label << "  " << position.fen() << '\n';
    if (byDepth.isEmpty()) {
        out() << "        no evaluation\n";
        return;
    }
    const EngineEvaluation &final = byDepth.last();
    out() << "        depth " << final.depth << "  " << final.text() << "  " << position.lineText(final.pv, 10);
    if (const std::optional<int> depth = AdvantageProbe::settledDepth(byDepth))
        out() << "   (settled from depth " << *depth << ')';
    out() << '\n';
    if (trace) {
        QStringList depths;
        for (const EngineEvaluation &evaluation : byDepth)
            depths << QStringLiteral("%1:%2").arg(evaluation.depth).arg(evaluation.text());
        out() << "        by depth  " << depths.join(QLatin1Char(' ')) << '\n';
    }
}

struct Options {
    bool trace = false;
    bool board = false;
};

bool explainPly(ExplanationSearch &search, const Pgn::ParsedLine &line, int ply, const Options &options)
{
    ChessPosition start = line.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                  : *ChessPosition::fromFen(line.startFen);
    QList<ChessPosition> positions{start};
    for (const MoveRecord &move : line.moves) {
        ChessPosition next = positions.last();
        next.play(*next.moveFromUci(move.uci));
        positions << next;
    }

    std::optional<ChessPosition> before;
    std::optional<ChessMove> played;
    out() << "━━ ";
    if (ply == 0) {
        out() << "start position\n";
    } else {
        before = positions.at(ply - 1);
        played = before->moveFromUci(line.moves.at(ply - 1).uci);
        out() << before->moveNumberText() << line.moves.at(ply - 1).san << "  (ply " << ply << ")\n";
    }
    out().flush();

    const std::optional<ExplanationAnalysis> analysis = analyze(search, before, played, positions.at(ply));
    if (!analysis || !analysis->afterEvaluation()) {
        err() << "The engine gave no evaluation.\n";
        return false;
    }
    static bool engineShown = false;
    if (!engineShown) {
        const ExplainSettings &settings = search.settings();
        out() << "engine  " << search.engineName() << "  (depth " << settings.depth << ", probe depth "
              << settings.probeDepth << " × " << settings.probePlies << " plies, threads " << settings.threads
              << ", hash " << settings.hashMb << " MB)\n";
        engineShown = true;
    }
    if (before)
        printSearch("before", *before, analysis->beforeByDepth, options.trace);
    printSearch("after ", analysis->after, analysis->afterByDepth, options.trace);
    if (options.board)
        printBoard(analysis->after);

    const ExplanationInput input = analysis->input(SanStyle::Letters, options.trace);
    if (!analysis->probe.isEmpty()) {
        out() << "line probe  depth " << search.settings().probeDepth << ", agreeing within "
              << AdvantageProbe::kAgreement << " points with " << input.afterEvaluation.text() << '\n';
        ChessPosition position = analysis->after;
        for (qsizetype k = 0; k < analysis->probe.size(); ++k) {
            QString label = QStringLiteral("(on the board)");
            if (k > 0) {
                const std::optional<ChessMove> move = position.moveFromUci(input.afterEvaluation.pv.at(k - 1));
                label = position.moveNumberText() + position.san(*move);
                position.play(*move);
            }
            out() << QStringLiteral("  %1  %2 %3").arg(k, 2).arg(label, -16).arg(analysis->probe.at(k).text(), 6);
            if (input.concretePly && k == *input.concretePly)
                out() << "   <- concrete";
            out() << '\n';
        }
    }

    const MoveExplanation explanation = explainPosition(input);
    out() << "verdict  " << verdictName(explanation.verdict) << '\n';
    QStringList arrows;
    for (const BoardArrow &arrow : explanation.arrows) {
        QString text = BoardState::squareName(arrow.from) + BoardState::squareName(arrow.to) + QLatin1Char(' ')
            + arrowKindName(arrow.kind);
        if (arrow.step > 0)
            text += QStringLiteral(" #%1").arg(arrow.step);
        arrows << text;
    }
    out() << "arrows   " << (arrows.isEmpty() ? QStringLiteral("-") : arrows.join(QStringLiteral(", "))) << '\n';
    QStringList lost;
    for (int square : explanation.lostPieces)
        lost << BoardState::squareName(square);
    if (!lost.isEmpty())
        out() << "lost     " << lost.join(QStringLiteral(", ")) << '\n';
    out() << "summary  " << explanation.summary << '\n';
    if (!explanation.playback.isEmpty())
        out() << "playback " << analysis->after.lineText(explanation.playback) << '\n';
    if (options.trace) {
        out() << "trace\n";
        for (const QString &text : explanation.trace)
            out() << "  " << text << '\n';
    }
    out() << '\n';
    out().flush();
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pragma-explain"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Explains moves like the Explain command of Pragma Chess.\n"
        "Moves are PGN, SAN or UCI; move numbers, comments and variations are skipped.\n"
        "Without moves on the command line they are read from standard input."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("moves"), QStringLiteral("The line, e.g. 1.e4 e5 2.Nf3 d6 3.Nxe5"));
    const QCommandLineOption fenOption(QStringLiteral("fen"), QStringLiteral("Start position."), QStringLiteral("fen"));
    const QCommandLineOption fileOption(QStringLiteral("file"), QStringLiteral("Read the line from a file."),
                                        QStringLiteral("path"));
    const QCommandLineOption plyOption(QStringLiteral("ply"),
                                       QStringLiteral("Move to explain, counted in plies (1 = first move, "
                                                      "0 = start position). Default: the last move."),
                                       QStringLiteral("n"));
    const QCommandLineOption allOption(QStringLiteral("all"), QStringLiteral("Explain every move."));
    const QCommandLineOption depthOption(QStringLiteral("depth"), QStringLiteral("Depth of the searches."),
                                         QStringLiteral("d"), QString::number(ExplainSettings().depth));
    const QCommandLineOption probeDepthOption(QStringLiteral("probe-depth"),
                                              QStringLiteral("Depth of the line probe, 0 to skip it."),
                                              QStringLiteral("d"), QString::number(ExplainSettings().probeDepth));
    const QCommandLineOption probePliesOption(QStringLiteral("probe-plies"),
                                              QStringLiteral("Plies of the line probe."),
                                              QStringLiteral("n"), QString::number(ExplainSettings().probePlies));
    const QCommandLineOption threadsOption(QStringLiteral("threads"),
                                           QStringLiteral("Engine threads; 1 keeps results reproducible."),
                                           QStringLiteral("n"), QString::number(ExplainSettings().threads));
    const QCommandLineOption hashOption(QStringLiteral("hash"), QStringLiteral("Engine hash in MB."),
                                        QStringLiteral("mb"), QString::number(ExplainSettings().hashMb));
    const QCommandLineOption engineOption(QStringLiteral("engine"), QStringLiteral("UCI engine: a command in PATH or a path, like the engine set in the desktop client (stockfish)."),
                                          QStringLiteral("path"), QStringLiteral("stockfish"));
    const QCommandLineOption traceOption({QStringLiteral("t"), QStringLiteral("trace")},
                                         QStringLiteral("Show how each explanation was reached."));
    const QCommandLineOption boardOption({QStringLiteral("b"), QStringLiteral("board")},
                                         QStringLiteral("Draw the position after the move."));
    parser.addOptions({fenOption, fileOption, plyOption, allOption, depthOption, probeDepthOption, probePliesOption,
                       threadsOption, hashOption, engineOption, traceOption, boardOption});
    parser.process(app);

    QString text = parser.positionalArguments().join(QLatin1Char(' '));
    if (parser.isSet(fileOption)) {
        QFile file(parser.value(fileOption));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err() << "Cannot read " << file.fileName() << ": " << file.errorString() << '\n';
            return 1;
        }
        text = QString::fromUtf8(file.readAll());
    } else if (text.trimmed().isEmpty()) {
        QFile input;
        if (input.open(stdin, QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(input.readAll());
    }

    QString error;
    const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(text, parser.value(fenOption), &error);
    if (!line) {
        err() << error << '\n';
        return 1;
    }

    const QString executable = UciEngine::findExecutable(parser.value(engineOption));
    if (executable.isEmpty()) {
        err() << "UCI engine not found: " << parser.value(engineOption) << '\n';
        return 2;
    }

    ExplainSettings settings;
    settings.depth = qMax(1, parser.value(depthOption).toInt());
    settings.probeDepth = qMax(0, parser.value(probeDepthOption).toInt());
    settings.probePlies = qMax(0, parser.value(probePliesOption).toInt());
    settings.threads = qMax(1, parser.value(threadsOption).toInt());
    settings.hashMb = qMax(1, parser.value(hashOption).toInt());

    Options options;
    options.trace = parser.isSet(traceOption);
    options.board = parser.isSet(boardOption);

    ExplanationSearch search(settings);
    if (!search.start(executable)) {
        err() << "Could not start " << executable << '\n';
        return 2;
    }

    const int plyCount = int(line->moves.size());
    QList<int> plies;
    if (parser.isSet(allOption)) {
        for (int ply = 1; ply <= plyCount; ++ply)
            plies << ply;
    } else if (parser.isSet(plyOption)) {
        const int ply = parser.value(plyOption).toInt();
        if (ply < 0 || ply > plyCount) {
            err() << "The line has " << plyCount << " plies.\n";
            return 1;
        }
        plies << ply;
    } else {
        plies << plyCount;
    }

    for (int ply : std::as_const(plies)) {
        if (!explainPly(search, *line, ply, options))
            return 3;
    }
    search.shutdown();
    return 0;
}
