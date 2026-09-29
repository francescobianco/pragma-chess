#include "PhoneGames.h"

#include "app/ChessPosition.h"
#include "app/GameDatabase.h"

#include <QDateTime>
#include <QJsonObject>
#include <QRegularExpression>

namespace PhoneGames {

namespace {

constexpr int kMaxText = 256;
constexpr int kMaxPlies = 2000;

QString text(const QJsonObject &game, const char *key)
{
    return game.value(QLatin1String(key)).toString().trimmed().left(kMaxText);
}

int rating(const QJsonObject &game, const char *key)
{
    const int value = game.value(QLatin1String(key)).toInt();
    return value > 0 && value < 4000 ? value : 0;
}

void setError(QString *errorMessage, const QString &text)
{
    if (errorMessage)
        *errorMessage = text;
}

} // namespace

std::optional<QList<ImportedGame>> parse(const QJsonArray &games, QString *errorMessage)
{
    static const QStringList results{QStringLiteral("1-0"), QStringLiteral("0-1"), QStringLiteral("1/2-1/2"),
                                     QStringLiteral("*")};
    QList<ImportedGame> parsed;
    for (qsizetype i = 0; i < games.size(); ++i) {
        const QJsonObject object = games.at(i).toObject();
        ImportedGame imported;
        imported.externalId = object.value(QStringLiteral("id")).toString().trimmed();
        if (imported.externalId.isEmpty() || imported.externalId.size() > 64) {
            setError(errorMessage, QStringLiteral("game %1: missing id").arg(i));
            return std::nullopt;
        }
        GameRecord &game = imported.game;
        game.white = text(object, "white");
        game.black = text(object, "black");
        game.event = text(object, "event");
        game.site = text(object, "site");
        game.date = text(object, "date");
        game.round = text(object, "round");
        game.result = text(object, "result");
        if (!results.contains(game.result))
            game.result = QStringLiteral("*");
        game.whiteElo = rating(object, "white_elo");
        game.blackElo = rating(object, "black_elo");
        game.eco = text(object, "eco");
        game.startFen = text(object, "start_fen");

        std::optional<ChessPosition> position =
            game.startFen.isEmpty() ? ChessPosition::startingPosition() : ChessPosition::fromFen(game.startFen);
        if (!position) {
            setError(errorMessage, QStringLiteral("game %1: bad start_fen").arg(i));
            return std::nullopt;
        }
        const QStringList uci =
            object.value(QStringLiteral("moves_uci")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (uci.size() > kMaxPlies) {
            setError(errorMessage, QStringLiteral("game %1: too many moves").arg(i));
            return std::nullopt;
        }
        for (const QString &text : uci) {
            const std::optional<ChessMove> move = position->moveFromUci(text);
            if (!move) {
                setError(errorMessage, QStringLiteral("game %1: illegal move %2").arg(i).arg(text));
                return std::nullopt;
            }
            game.moves.append({position->san(*move), move->uci()});
            position->play(*move);
        }
        game.plyCount = int(game.moves.size());
        parsed.append(imported);
    }
    return parsed;
}

std::optional<PutResult> store(GameDatabase &database, const QString &phoneKey, const QString &phoneName,
                               const QList<ImportedGame> &games, QString *errorMessage)
{
    std::optional<GameSource> source;
    for (const GameSource &existing : database.sources()) {
        if (existing.kind == QLatin1String(kSourceKind) && existing.uuid == phoneKey)
            source = existing;
    }
    if (!source) {
        GameSource created;
        created.uuid = phoneKey;
        created.kind = QString::fromLatin1(kSourceKind);
        created.account = phoneName;
        if (!database.addSource(created, errorMessage))
            return std::nullopt;
        source = created;
    }
    const int stored = database.importGames(source->id, games, errorMessage);
    if (stored < 0)
        return std::nullopt;
    source->lastSyncAt = QDateTime::currentDateTimeUtc();
    source->lastError.clear();
    if (!phoneName.isEmpty())
        source->account = phoneName;
    database.updateSource(*source, nullptr);
    return PutResult{stored, int(games.size()) - stored};
}

} // namespace PhoneGames
