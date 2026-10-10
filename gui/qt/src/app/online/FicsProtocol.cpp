#include "FicsProtocol.h"

#include <QRegularExpression>
#include <QStringList>

namespace Fics {

namespace {

int ratingOf(const QString &text)
{
    bool ok = false;
    const int rating = text.toInt(&ok);
    return ok ? rating : 0; // "++++" (a guest), "----" (none yet), "1650E" (estimated) give 0.
}

} // namespace

std::optional<Style12> parseStyle12(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1String("<12> ")))
        return std::nullopt;
    const QStringList fields = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 30)
        return std::nullopt;
    Style12 update;
    for (int rank = 1; rank <= 8; ++rank) {
        if (fields.at(rank).size() != 8)
            return std::nullopt;
        update.squares += fields.at(rank);
    }
    update.whiteToMove = fields.at(9) == QLatin1String("W");
    update.game = fields.at(16).toInt();
    update.white = fields.at(17);
    update.black = fields.at(18);
    update.relation = fields.at(19).toInt();
    update.initialMinutes = fields.at(20).toInt();
    update.incrementSeconds = fields.at(21).toInt();
    update.whiteTimeMs = fields.at(24).toInt();
    update.blackTimeMs = fields.at(25).toInt();
    update.moveNumber = fields.at(26).toInt();
    update.verboseMove = fields.at(27);
    return update;
}

QString uciOfVerbose(const QString &verbose, bool whiteMoved)
{
    const QString move = verbose.trimmed();
    if (move == QLatin1String("o-o"))
        return whiteMoved ? QStringLiteral("e1g1") : QStringLiteral("e8g8");
    if (move == QLatin1String("o-o-o"))
        return whiteMoved ? QStringLiteral("e1c1") : QStringLiteral("e8c8");
    // "P/e2-e4", "N/g1-f3", "P/e7-e8=Q".
    static const QRegularExpression full(QStringLiteral(R"(^[PNBRQK]/([a-h][1-8])-([a-h][1-8])(?:=([NBRQ]))?$)"));
    const QRegularExpressionMatch match = full.match(move);
    if (!match.hasMatch())
        return {};
    return match.captured(1) + match.captured(2) + match.captured(3).toLower();
}

QString moveCommand(const QString &uci, QChar piece)
{
    if (uci.size() < 4)
        return uci;
    if (piece.toUpper() == QLatin1Char('K') && uci.at(0) == QLatin1Char('e')) {
        if (uci.mid(2, 1) == QLatin1String("g") && uci.at(1) == uci.at(3))
            return QStringLiteral("o-o");
        if (uci.mid(2, 1) == QLatin1String("c") && uci.at(1) == uci.at(3))
            return QStringLiteral("o-o-o");
    }
    if (uci.size() == 5)
        return uci.left(4) + QLatin1Char('=') + uci.at(4).toLower();
    return uci;
}

std::optional<Creating> parseCreating(const QString &line)
{
    static const QRegularExpression creating(
        QStringLiteral(R"(^Creating: (\S+) \((\S+)\) (\S+) \((\S+)\) (rated|unrated))"));
    const QRegularExpressionMatch match = creating.match(line.trimmed());
    if (!match.hasMatch())
        return std::nullopt;
    Creating game;
    game.white = match.captured(1);
    game.whiteRating = ratingOf(match.captured(2));
    game.black = match.captured(3);
    game.blackRating = ratingOf(match.captured(4));
    game.rated = match.captured(5) == QLatin1String("rated");
    return game;
}

std::optional<GameEnd> parseGameEnd(const QString &line)
{
    static const QRegularExpression end(
        QStringLiteral(R"(^\{Game (\d+) \((\S+) vs\. (\S+)\) ([^}]*)\} (1-0|0-1|1/2-1/2|\*))"));
    const QRegularExpressionMatch match = end.match(line.trimmed());
    if (!match.hasMatch())
        return std::nullopt;
    GameEnd result;
    result.game = match.captured(1).toInt();
    result.reason = match.captured(4);
    const QString score = match.captured(5);
    if (score == QLatin1String("1-0"))
        result.winner = QStringLiteral("white");
    else if (score == QLatin1String("0-1"))
        result.winner = QStringLiteral("black");
    const QString reason = result.reason.toLower();
    if (reason.contains(QLatin1String("checkmated")))
        result.status = QStringLiteral("mate");
    else if (reason.contains(QLatin1String("resigns")))
        result.status = QStringLiteral("resign");
    else if (reason.contains(QLatin1String("forfeits on time")))
        result.status = QStringLiteral("outoftime");
    else if (reason.contains(QLatin1String("disconnection")))
        result.status = QStringLiteral("timeout");
    else if (reason.contains(QLatin1String("stalemate")))
        result.status = QStringLiteral("stalemate");
    else if (score == QLatin1String("1/2-1/2"))
        result.status = QStringLiteral("draw");
    else if (score == QLatin1String("*"))
        result.status = QStringLiteral("aborted"); // Aborted, or adjourned: no result either way.
    else
        result.status = QStringLiteral("resign"); // A win by another name: the end text says who won.
    return result;
}

std::optional<QString> parseSessionStart(const QString &line)
{
    static const QRegularExpression start(QStringLiteral(R"(\*\*\*\* Starting FICS session as ([A-Za-z]+))"));
    const QRegularExpressionMatch match = start.match(line);
    return match.hasMatch() ? std::optional<QString>(match.captured(1)) : std::nullopt;
}

std::optional<QString> parseDrawOffer(const QString &line)
{
    static const QRegularExpression offer(QStringLiteral(R"(^(\S+) offers you a draw\.)"));
    const QRegularExpressionMatch match = offer.match(line.trimmed());
    return match.hasMatch() ? std::optional<QString>(match.captured(1)) : std::nullopt;
}

} // namespace Fics
