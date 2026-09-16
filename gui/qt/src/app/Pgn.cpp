#include "Pgn.h"

#include "ChessPosition.h"

#include <QObject>
#include <QRegularExpression>
#include <QStringList>

namespace {

QString tagValue(const QString &value, const QString &unknown = QStringLiteral("?"))
{
    const QString text = value.trimmed().isEmpty() ? unknown : value.trimmed();
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    return escaped;
}

QString tag(const char *name, const QString &value)
{
    return QStringLiteral("[%1 \"%2\"]\n").arg(QLatin1String(name), value);
}

QString wrap(const QString &text, int width)
{
    QString result;
    int lineLength = 0;
    for (const QString &token : text.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (lineLength > 0 && lineLength + 1 + token.size() > width) {
            result += QLatin1Char('\n');
            lineLength = 0;
        } else if (lineLength > 0) {
            result += QLatin1Char(' ');
            ++lineLength;
        }
        result += token;
        lineLength += int(token.size());
    }
    return result;
}

} // namespace

namespace Pgn {

QString moveText(const GameRecord &game, int plies)
{
    const std::optional<ChessPosition> start = game.startFen.isEmpty()
        ? ChessPosition::startingPosition()
        : ChessPosition::fromFen(game.startFen);
    if (!start)
        return {};
    QStringList moves;
    for (const MoveRecord &move : game.moves)
        moves << move.uci;
    return start->lineText(moves, plies);
}

std::optional<ParsedLine> parseLine(const QString &text, const QString &startFen, QString *errorMessage)
{
    const auto fail = [&](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return std::nullopt;
    };

    ParsedLine line;
    line.startFen = startFen;
    static const QRegularExpression fenTag(QStringLiteral(R"re(\[\s*FEN\s+"([^"]*)"\s*\])re"));
    if (const QRegularExpressionMatch match = fenTag.match(text); match.hasMatch())
        line.startFen = match.captured(1).trimmed();

    // Strip tags, comments, variations and line comments, keeping the main line.
    QString movetext;
    int depth = 0;
    bool inComment = false;
    bool inTag = false;
    bool inLineComment = false;
    for (QChar c : text) {
        if (inLineComment) {
            inLineComment = c != QLatin1Char('\n');
            continue;
        }
        if (inComment) {
            inComment = c != QLatin1Char('}');
            continue;
        }
        if (inTag) {
            inTag = c != QLatin1Char(']');
            continue;
        }
        if (c == QLatin1Char('{')) {
            inComment = true;
        } else if (c == QLatin1Char('[') && depth == 0) {
            inTag = true;
        } else if (c == QLatin1Char(';')) {
            inLineComment = true;
        } else if (c == QLatin1Char('(')) {
            ++depth;
        } else if (c == QLatin1Char(')')) {
            depth = qMax(0, depth - 1);
        } else if (depth == 0) {
            movetext += c;
        }
        if (depth > 0 || inComment || inTag || inLineComment)
            movetext += QLatin1Char(' ');
    }

    std::optional<ChessPosition> position = line.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                     : ChessPosition::fromFen(line.startFen);
    if (!position)
        return fail(QObject::tr("Invalid FEN: %1").arg(line.startFen));

    // Move numbers may be glued to the move: "12.Nf3", "12...Nc6", "12…Nc6".
    static const QRegularExpression moveNumber(QStringLiteral(R"(^\d+\s*(\.+|…)\s*)"));
    static const QRegularExpression result(QStringLiteral(R"(^(1-0|0-1|1/2-1/2|½-½|\*)$)"));
    static const QRegularExpression separators(QStringLiteral(R"([\s,]+)"));
    for (QString token : movetext.split(separators, Qt::SkipEmptyParts)) {
        token.remove(moveNumber);
        if (token.isEmpty() || token.startsWith(QLatin1Char('$')) || result.match(token).hasMatch())
            continue;
        if (token.front().isDigit() && token.back() == QLatin1Char('.'))
            continue;
        if (token.count(QLatin1Char('.')) + token.count(QChar(0x2026)) == token.size())
            continue; // "..." or "…" standing alone before a Black move.
        std::optional<ChessMove> move = position->moveFromSan(token);
        if (!move)
            move = position->moveFromUci(token);
        if (!move) {
            GameRecord sofar;
            sofar.startFen = line.startFen;
            sofar.moves = line.moves;
            return fail(QObject::tr("“%1” is not a legal move after %2")
                            .arg(token, line.moves.isEmpty() ? QObject::tr("the start position") : moveText(sofar)));
        }
        line.moves << MoveRecord{position->san(*move), move->uci()};
        position->play(*move);
    }
    return line;
}

QString game(const GameRecord &game, int plies)
{
    const bool complete = plies < 0 || plies >= game.moves.size();
    const QString result = complete && !game.result.trimmed().isEmpty() ? game.result.trimmed()
                                                                        : QStringLiteral("*");
    QString pgn;
    pgn += tag("Event", tagValue(game.event));
    pgn += tag("Site", tagValue(game.site));
    pgn += tag("Date", tagValue(game.date, QStringLiteral("????.??.??")));
    pgn += tag("Round", tagValue(game.round));
    pgn += tag("White", tagValue(game.white));
    pgn += tag("Black", tagValue(game.black));
    pgn += tag("Result", result);
    if (game.whiteElo > 0)
        pgn += tag("WhiteElo", QString::number(game.whiteElo));
    if (game.blackElo > 0)
        pgn += tag("BlackElo", QString::number(game.blackElo));
    if (!game.eco.trimmed().isEmpty())
        pgn += tag("ECO", tagValue(game.eco));
    if (!game.startFen.isEmpty()) {
        pgn += tag("SetUp", QStringLiteral("1"));
        pgn += tag("FEN", tagValue(game.startFen));
    }
    pgn += QLatin1Char('\n');

    const QString moves = moveText(game, plies);
    pgn += wrap(moves.isEmpty() ? result : moves + QLatin1Char(' ') + result, 80);
    pgn += QLatin1Char('\n');
    return pgn;
}

} // namespace Pgn
