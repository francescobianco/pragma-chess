// pragma-explain: the "Explain" command on the command line.
//
// Runs the same explanation as the desktop client on lines pasted as PGN,
// SAN or UCI: smart/EXPLAIN.smart, fed with the ticks of a fixed-depth,
// single-thread search (reproducible while the explanation is being tuned),
// depth after depth as the live analysis feeds the desktop client. Ticks can
// be recorded (--record) and replayed without an engine (--replay), also
// those the desktop client records (PRAGMA_EXPLAIN_RECORD).

#include "app/AdvantageProbe.h"
#include "app/ChessPosition.h"
#include "app/ExplainTicks.h"
#include "app/ExplanationSearch.h"
#include "app/LineInsight.h"
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
    /// Print what every tick made of the explanation.
    bool ticks = false;
    /// Where to append the ticks of each move explained.
    QString record;
    /// A last tick as the desktop client's live analysis would give it.
    std::optional<int> hintMate;
    bool hintDraw = false;
    QString hintLine;
    int hintDepth = 99;
};

/// The hint of the options for the position `after`, or nothing.
std::optional<EngineEvaluation> hintFor(const Options &options, const ChessPosition &after)
{
    if (!options.hintMate && !options.hintDraw)
        return std::nullopt;
    EngineEvaluation hint;
    hint.depth = options.hintDepth;
    if (options.hintMate) {
        hint.isMate = true;
        hint.mateIn = qAbs(*options.hintMate);
        hint.mating = *options.hintMate > 0 ? Side::White : Side::Black;
    }
    QString error;
    const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(options.hintLine, after.fen(), &error);
    if (!line) {
        err() << "Hint line: " << error << '\n';
        return std::nullopt;
    }
    for (const MoveRecord &move : line->moves)
        hint.pv << move.uci;
    return hint;
}

QString arrowsText(const MoveExplanation &explanation)
{
    return ExplainTicks::outcome(explanation).value(1).section(QLatin1Char(' '), 1);
}

/// Feeds the ticks to EXPLAIN.smart, as the desktop client does, and prints
/// the explanation it ends up showing. False if it differs from the
/// record's expectations.
bool explainTicks(ExplainTicks record, const Options &options)
{
    if (options.board)
        printBoard(record.after);

    const QList<ExplanationTick> results = record.replay(SanStyle::Letters, options.trace);
    for (qsizetype i = 0; options.ticks && i < results.size(); ++i) {
        const ExplanationTick &tick = results.at(i);
        out() << QStringLiteral("tick     depth %1 %2  %3  %4\n")
                     .arg(record.ticks.at(i).depth, 2).arg(record.ticks.at(i).text(), 6)
                     .arg(tick.shown ? QStringLiteral("shown") : QStringLiteral("held "), arrowsText(tick.explanation));
    }
    const std::optional<MoveExplanation> shown = ExplainTicks::lastShown(results);
    const QStringList outcome = shown ? ExplainTicks::outcome(*shown) : QStringList();
    if (!shown)
        out() << "summary  (nothing shown: the search is not deep enough)\n";
    for (const QString &line : outcome)
        out() << line.section(QLatin1Char(' '), 0, 0).leftJustified(8) << ' ' << line.section(QLatin1Char(' '), 1) << '\n';
    if (shown && options.trace) {
        // How the last tick shown was reached, and what the ticks after it held back.
        out() << "trace\n";
        for (const QString &text : shown->trace)
            out() << "  " << text << '\n';
        for (qsizetype i = results.size() - 1; i >= 0 && !results.at(i).shown; --i) {
            for (const QString &text : results.at(i).explanation.trace) {
                if (text.startsWith(QLatin1String("other arrows")))
                    out() << "  " << text << '\n';
            }
        }
    }

    bool same = true;
    if (!record.expected.isEmpty()) {
        same = record.expected == outcome;
        out() << (same ? "expected: yes\n" : "expected: NO — the record expects\n");
        for (const QString &line : same ? QStringList() : record.expected)
            out() << "  " << line << '\n';
    }
    if (!options.record.isEmpty()) {
        // Recorded with what was shown: replayed later, it is a test.
        record.expected = outcome;
        QFile file(options.record);
        if (file.open(QIODevice::Append | QIODevice::Text))
            file.write(record.toText().toUtf8());
        else
            err() << "Cannot write " << options.record << ": " << file.errorString() << '\n';
    }
    out() << '\n';
    out().flush();
    return same;
}

void printHeader(const std::optional<ChessPosition> &before, const std::optional<ChessMove> &played, int ply)
{
    out() << "━━ ";
    if (!before || !played)
        out() << "position\n";
    else if (ply > 0)
        out() << before->moveNumberText() << before->san(*played) << "  (ply " << ply << ")\n";
    else
        out() << before->moveNumberText() << before->san(*played) << '\n';
    out().flush();
}

bool explainPly(ExplanationSearch &search, const Pgn::ParsedLine &line, int ply, const Options &options)
{
    ChessPosition start = line.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                  : *ChessPosition::fromFen(line.startFen, ChessPosition::Kings::Optional);
    QList<ChessPosition> positions{start};
    for (const MoveRecord &move : line.moves) {
        ChessPosition next = positions.last();
        next.play(*next.moveFromUci(move.uci));
        positions << next;
    }

    std::optional<ChessPosition> before;
    std::optional<ChessMove> played;
    if (ply > 0) {
        before = positions.at(ply - 1);
        played = before->moveFromUci(line.moves.at(ply - 1).uci);
    }
    printHeader(before, played, ply);

    const std::optional<ExplanationAnalysis> analysis = analyze(search, before, played, positions.at(ply));
    if (!analysis || !analysis->afterEvaluation()) {
        err() << "The engine gave no evaluation.\n";
        return false;
    }
    static bool engineShown = false;
    if (!engineShown) {
        const ExplainSettings &settings = search.settings();
        out() << "engine  " << search.engineName() << "  (depth " << settings.depth << ", threads "
              << settings.threads << ", hash " << settings.hashMb << " MB)\n";
        engineShown = true;
    }
    if (before)
        printSearch("before", *before, analysis->beforeByDepth, options.trace);
    printSearch("after ", analysis->after, analysis->afterByDepth, options.trace);

    // The search after the move, depth by depth, is what the live analysis
    // gives the desktop client: the ticks.
    ExplainTicks record;
    record.before = before;
    record.played = played;
    record.beforeEvaluation = analysis->beforeEvaluation();
    record.after = analysis->after;
    record.ticks = analysis->afterByDepth;
    if (const std::optional<EngineEvaluation> hint = hintFor(options, analysis->after)) {
        out() << "hint    " << hint->text() << " at depth " << hint->depth << "  "
              << analysis->after.lineText(hint->pv, 10) << "   (a last tick)\n";
        record.ticks << *hint;
    }
    explainTicks(record, options);
    return true;
}

/// --replay: the ticks of a file, no engine; exit status 4 if a record's
/// expectations are not met.
int replay(const QString &path, const Options &options)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err() << "Cannot read " << path << ": " << file.errorString() << '\n';
        return 1;
    }
    QString error;
    const std::optional<QList<ExplainTicks>> records = ExplainTicks::fromText(QString::fromUtf8(file.readAll()), &error);
    if (!records) {
        err() << path << ", " << error << '\n';
        return 1;
    }
    int differing = 0;
    for (const ExplainTicks &record : *records) {
        printHeader(record.before, record.played, 0);
        if (record.beforeEvaluation && record.before)
            out() << "before  " << record.beforeEvaluation->text() << " at depth " << record.beforeEvaluation->depth
                  << "  " << record.before->lineText(record.beforeEvaluation->pv, 10) << '\n';
        out() << "after   " << record.after.fen() << "  (" << record.ticks.size() << " ticks)\n";
        differing += explainTicks(record, options) ? 0 : 1;
    }
    if (differing > 0)
        err() << differing << " of " << records->size() << " record(s) explained otherwise than expected.\n";
    return differing > 0 ? 4 : 0;
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
    const QCommandLineOption recordOption(QStringLiteral("record"),
                                          QStringLiteral("Append the ticks of each move explained to a file (with --insight: the line and its plans, a case of smart/tests)."),
                                          QStringLiteral("path"));
    const QCommandLineOption replayOption(QStringLiteral("replay"),
                                          QStringLiteral("Explain the ticks of a file (from --record, or from the "
                                                         "desktop client's PRAGMA_EXPLAIN_RECORD), with no engine."),
                                          QStringLiteral("path"));
    const QCommandLineOption ticksOption(QStringLiteral("ticks"),
                                         QStringLiteral("Show what each tick made of the explanation."));
    const QCommandLineOption threadsOption(QStringLiteral("threads"),
                                           QStringLiteral("Engine threads; 1 keeps results reproducible."),
                                           QStringLiteral("n"), QString::number(ExplainSettings().threads));
    const QCommandLineOption hashOption(QStringLiteral("hash"), QStringLiteral("Engine hash in MB."),
                                        QStringLiteral("mb"), QString::number(ExplainSettings().hashMb));
    const QCommandLineOption engineOption(QStringLiteral("engine"), QStringLiteral("UCI engine: a command in PATH or a path, like the engine set in the desktop client (stockfish)."),
                                          QStringLiteral("path"), QStringLiteral("stockfish"));
    const QCommandLineOption traceOption({QStringLiteral("t"), QStringLiteral("trace")},
                                         QStringLiteral("Show how each explanation was reached."));
    const QCommandLineOption hintMateOption(QStringLiteral("hint-mate"),
                                            QStringLiteral("Hint of the live analysis: mate in n moves, positive "
                                                           "for White, negative for Black."),
                                            QStringLiteral("n"));
    const QCommandLineOption hintDrawOption(QStringLiteral("hint-draw"),
                                            QStringLiteral("Hint of the live analysis: 0.00, a draw."));
    const QCommandLineOption hintLineOption(QStringLiteral("hint-line"),
                                            QStringLiteral("The line of the hint, from the position explained."),
                                            QStringLiteral("moves"));
    const QCommandLineOption hintDepthOption(QStringLiteral("hint-depth"),
                                             QStringLiteral("Depth the hint was found at."), QStringLiteral("d"),
                                             QStringLiteral("99"));
    const QCommandLineOption insightOption(QStringLiteral("insight"),
                                           QStringLiteral("Show the plans INSIGHT.smart draws for the line from --ply "
                                                          "(default the start) to its end, with no engine: the "
                                                          "arrows of the Engine panel's eye."));
    const QCommandLineOption boardOption({QStringLiteral("b"), QStringLiteral("board")},
                                         QStringLiteral("Draw the position after the move."));
    parser.addOptions({fenOption, fileOption, plyOption, allOption, depthOption, recordOption, replayOption, ticksOption,
                       threadsOption, hashOption, engineOption, traceOption, boardOption, hintMateOption,
                       hintDrawOption, hintLineOption, hintDepthOption, insightOption});
    parser.process(app);

    Options options;
    options.trace = parser.isSet(traceOption);
    options.board = parser.isSet(boardOption);
    options.ticks = parser.isSet(ticksOption);
    options.record = parser.value(recordOption);
    if (parser.isSet(hintMateOption))
        options.hintMate = parser.value(hintMateOption).toInt();
    options.hintDraw = parser.isSet(hintDrawOption);
    options.hintLine = parser.value(hintLineOption);
    options.hintDepth = parser.value(hintDepthOption).toInt();
    if (parser.isSet(replayOption))
        return replay(parser.value(replayOption), options);

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

    if (parser.isSet(insightOption)) {
        const int ply = parser.isSet(plyOption) ? parser.value(plyOption).toInt() : 0;
        if (ply < 0 || ply > line->moves.size()) {
            err() << "The line has " << line->moves.size() << " plies.\n";
            return 1;
        }
        ChessPosition start = line->startFen.isEmpty() ? ChessPosition::startingPosition()
                                                       : *ChessPosition::fromFen(line->startFen);
        QStringList moves;
        for (qsizetype i = 0; i < line->moves.size(); ++i) {
            if (i < ply) {
                start.play(*start.moveFromUci(line->moves.at(i).uci));
                continue;
            }
            moves << line->moves.at(i).uci;
        }
        const LineInsight insight = lineInsight(start, moves, options.trace);
        out() << "from    " << start.fen() << '\n';
        out() << "line    " << start.lineText(moves, int(moves.size())) << '\n';
        for (const QString &note : insight.trace)
            out() << "  " << note << '\n';
        if (!insight.error.isEmpty()) {
            err() << "INSIGHT.smart: " << insight.error << '\n';
            return 3;
        }
        for (const QString &arrow : InsightCase::outcome(insight.arrows))
            out() << "plan    " << arrow << '\n';
        if (!options.record.isEmpty()) {
            // The line becomes a case of smart/tests, expecting what is drawn now.
            InsightCase recorded;
            recorded.name = start.lineText(moves, 6);
            recorded.start = start;
            recorded.line = moves;
            recorded.expected = InsightCase::outcome(insight.arrows);
            QFile file(options.record);
            if (!file.open(QIODevice::Append | QIODevice::Text)) {
                err() << "Cannot write " << file.fileName() << ": " << file.errorString() << '\n';
                return 1;
            }
            file.write(recorded.toText().toUtf8());
        }
        return 0;
    }

    const QString executable = UciEngine::findExecutable(parser.value(engineOption));
    if (executable.isEmpty()) {
        err() << "UCI engine not found: " << parser.value(engineOption) << '\n';
        return 2;
    }

    ExplainSettings settings;
    settings.depth = qMax(1, parser.value(depthOption).toInt());
    settings.probeDepth = 0; // The explanation reacts to the ticks; the line probe is not used.
    settings.threads = qMax(1, parser.value(threadsOption).toInt());
    settings.hashMb = qMax(1, parser.value(hashOption).toInt());

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
