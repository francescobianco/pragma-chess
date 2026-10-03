#include "OnlineGame.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(OnlineGame)
};

} // namespace

QString OnlineGame::result() const
{
    if (!isOver() || status == QLatin1String("aborted") || status == QLatin1String("noStart"))
        return QStringLiteral("*");
    if (winner == QLatin1String("white"))
        return QStringLiteral("1-0");
    if (winner == QLatin1String("black"))
        return QStringLiteral("0-1");
    return QStringLiteral("1/2-1/2");
}

QString OnlineGame::endText() const
{
    const bool whiteWon = winner == QLatin1String("white");
    const QString winnerName = whiteWon ? Text::tr("White") : Text::tr("Black");
    const QString loserName = whiteWon ? Text::tr("Black") : Text::tr("White");
    if (status == QLatin1String("mate"))
        return Text::tr("Checkmate: %1 wins.").arg(winnerName);
    if (status == QLatin1String("resign"))
        return Text::tr("%1 resigned.").arg(loserName);
    if (status == QLatin1String("outoftime") || status == QLatin1String("timeout"))
        return winner.isEmpty() ? Text::tr("Time ran out: draw.") : Text::tr("%1 ran out of time.").arg(loserName);
    if (status == QLatin1String("stalemate"))
        return Text::tr("Stalemate: draw.");
    if (status == QLatin1String("draw"))
        return Text::tr("Draw.");
    if (status == QLatin1String("aborted") || status == QLatin1String("noStart"))
        return Text::tr("The game was aborted.");
    if (status == QLatin1String("cheat"))
        return Text::tr("The game was ended by the platform.");
    if (isOver())
        return winner.isEmpty() ? Text::tr("The game is over.") : Text::tr("%1 wins.").arg(winnerName);
    return {};
}

namespace LichessBoard {

namespace {

QJsonObject objectOf(const QByteArray &line)
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return {};
    return QJsonDocument::fromJson(trimmed).object();
}

void applyState(const QJsonObject &state, OnlineGame &game)
{
    const QString moves = state.value(QStringLiteral("moves")).toString();
    game.moves = moves.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    game.status = state.value(QStringLiteral("status")).toString();
    game.winner = state.value(QStringLiteral("winner")).toString();
    game.whiteTimeMs = state.value(QStringLiteral("wtime")).toInt();
    game.blackTimeMs = state.value(QStringLiteral("btime")).toInt();
}

} // namespace

std::optional<QString> gameStarted(const QByteArray &line)
{
    const QJsonObject event = objectOf(line);
    if (event.value(QStringLiteral("type")).toString() != QLatin1String("gameStart"))
        return std::nullopt;
    const QString id = event.value(QStringLiteral("game")).toObject().value(QStringLiteral("gameId")).toString();
    return id.isEmpty() ? std::nullopt : std::optional<QString>(id);
}

bool applyGameLine(const QByteArray &line, OnlineGame &game)
{
    const QJsonObject object = objectOf(line);
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("gameFull")) {
        game.id = object.value(QStringLiteral("id")).toString();
        const QJsonObject white = object.value(QStringLiteral("white")).toObject();
        const QJsonObject black = object.value(QStringLiteral("black")).toObject();
        game.white = white.value(QStringLiteral("name")).toString();
        game.black = black.value(QStringLiteral("name")).toString();
        game.whiteRating = white.value(QStringLiteral("rating")).toInt();
        game.blackRating = black.value(QStringLiteral("rating")).toInt();
        game.rated = object.value(QStringLiteral("rated")).toBool();
        const QString fen = object.value(QStringLiteral("initialFen")).toString();
        game.initialFen = fen == QLatin1String("startpos") ? QString() : fen;
        applyState(object.value(QStringLiteral("state")).toObject(), game);
        return true;
    }
    if (type == QLatin1String("gameState")) {
        applyState(object, game);
        return true;
    }
    return false;
}

} // namespace LichessBoard
