// pragma-book: builds and reads Polyglot opening books.
//
// Building takes lines of moves, one per line of text: plain SAN/PGN lines or
// the TSV files of lichess-org/chess-openings (columns eco, name, pgn). Every
// move of every line adds one to the weight of that move in its position, so
// moves shared by many named openings weigh more. Pragma Chess ships the book
// built from chess-openings (public domain):
//
//     pragma-book -o "Pragma Openings.bin" a.tsv b.tsv c.tsv d.tsv e.tsv
//
// Reading lists the book moves after a line: pragma-book --probe "1.e4 e5" book.bin

#include "app/ChessPosition.h"
#include "app/Pgn.h"
#include "app/PolyglotBook.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QTextStream>

#include <limits>

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

/// The moves of a line of text: the "pgn" column of a TSV, or the whole line.
QString movesOfLine(const QString &line, int pgnColumn)
{
    if (pgnColumn < 0)
        return line;
    return line.split(QLatin1Char('\t')).value(pgnColumn);
}

int build(const QStringList &inputs, const QString &output)
{
    QHash<std::pair<quint64, quint16>, int> weights;
    int lines = 0;
    for (const QString &input : inputs) {
        QFile file(input);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            err() << input << ": " << file.errorString() << Qt::endl;
            return 1;
        }
        QTextStream stream(&file);
        int pgnColumn = -1;
        bool header = input.endsWith(QLatin1String(".tsv"));
        while (!stream.atEnd()) {
            const QString line = stream.readLine().trimmed();
            if (header) {
                pgnColumn = line.split(QLatin1Char('\t')).indexOf(QStringLiteral("pgn"));
                header = false;
                continue;
            }
            if (line.isEmpty())
                continue;
            QString error;
            const std::optional<Pgn::ParsedLine> parsed = Pgn::parseLine(movesOfLine(line, pgnColumn), QString(), &error);
            if (!parsed || !parsed->startFen.isEmpty()) {
                err() << input << ": skipped \"" << line << "\": " << error << Qt::endl;
                continue;
            }
            ChessPosition position = ChessPosition::startingPosition();
            for (const MoveRecord &record : parsed->moves) {
                const std::optional<ChessMove> move = position.moveFromUci(record.uci);
                if (!move)
                    break;
                ++weights[{PolyglotBook::key(position), PolyglotBook::encodeMove(position, *move)}];
                position.play(*move);
            }
            ++lines;
        }
    }

    QList<PolyglotBook::Entry> entries;
    entries.reserve(weights.size());
    for (auto it = weights.cbegin(); it != weights.cend(); ++it)
        entries << PolyglotBook::Entry{it.key().first, it.key().second,
                                       quint16(qMin(it.value(), int(std::numeric_limits<quint16>::max())))};
    QFile file(output);
    if (!file.open(QIODevice::WriteOnly) || file.write(PolyglotBook::write(entries)) < 0) {
        err() << output << ": " << file.errorString() << Qt::endl;
        return 1;
    }
    out() << lines << " lines, " << entries.size() << " book entries written to " << output << Qt::endl;
    return 0;
}

int probe(const QString &line, const QString &bookPath)
{
    QString error;
    const std::optional<Pgn::ParsedLine> parsed = Pgn::parseLine(line, QString(), &error);
    if (!parsed) {
        err() << error << Qt::endl;
        return 1;
    }
    ChessPosition position = parsed->startFen.isEmpty() ? ChessPosition::startingPosition()
                                                        : *ChessPosition::fromFen(parsed->startFen);
    for (const MoveRecord &record : parsed->moves)
        position.play(*position.moveFromUci(record.uci));

    PolyglotBook book;
    if (!book.open(bookPath, &error)) {
        err() << bookPath << ": " << error << Qt::endl;
        return 1;
    }
    out() << "key " << Qt::hex << PolyglotBook::key(position) << Qt::dec << Qt::endl;
    const QList<PolyglotBook::Move> moves = book.moves(position);
    int total = 0;
    for (const PolyglotBook::Move &move : moves)
        total += move.weight;
    for (const PolyglotBook::Move &move : moves)
        out() << position.san(move.move) << "\t" << move.weight << "\t" << (100.0 * move.weight / total) << "%" << Qt::endl;
    if (moves.isEmpty())
        out() << "not in the book" << Qt::endl;
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pragma-book"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Builds and reads Polyglot opening books."));
    parser.addHelpOption();
    const QCommandLineOption output({QStringLiteral("o"), QStringLiteral("output")},
                                    QStringLiteral("Builds a book into <file> from the input lines."), QStringLiteral("file"));
    const QCommandLineOption probeOption(QStringLiteral("probe"),
                                         QStringLiteral("Lists the book moves after <moves>."), QStringLiteral("moves"));
    parser.addOption(output);
    parser.addOption(probeOption);
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Input lines to build from, or the book to probe."));
    parser.process(app);

    const QStringList files = parser.positionalArguments();
    if (parser.isSet(output) && !files.isEmpty())
        return build(files, parser.value(output));
    if (parser.isSet(probeOption) && files.size() == 1)
        return probe(parser.value(probeOption), files.first());
    parser.showHelp(1);
}
