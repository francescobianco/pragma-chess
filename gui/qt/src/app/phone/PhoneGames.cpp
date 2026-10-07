#include "PhoneGames.h"

#include "app/ChessPosition.h"
#include "app/GameDatabase.h"
#include "app/GameIdentity.h"
#include "app/Reconcile.h"

#include <QDateTime>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>

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
        GameRecord &game = imported.game;
        const QString uid = object.value(QStringLiteral("uid")).toString().trimmed().toLower();
        if (!uid.isEmpty()) {
            if (QUuid::fromString(uid).isNull()) {
                setError(errorMessage, QStringLiteral("game %1: bad uid").arg(i));
                return std::nullopt;
            }
            game.uid = QUuid::fromString(uid).toString(QUuid::WithoutBraces);
        }
        imported.externalId = object.value(QStringLiteral("id")).toString().trimmed();
        if (imported.externalId.size() > 64 || (imported.externalId.isEmpty() && game.uid.isEmpty())) {
            setError(errorMessage, QStringLiteral("game %1: missing uid").arg(i));
            return std::nullopt;
        }
        const QDateTime modified =
            QDateTime::fromString(object.value(QStringLiteral("modified")).toString(), Qt::ISODateWithMs);
        if (modified.isValid())
            game.modified = modified.toUTC().toString(Qt::ISODateWithMs);
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
            game.startFen.isEmpty() ? ChessPosition::startingPosition() : ChessPosition::fromFen(game.startFen, ChessPosition::Kings::Optional);
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
            const std::optional<ChessMove> move = position->moveFromUci(text, ChessPosition::NullMoves::Allowed);
            if (!move) {
                setError(errorMessage, QStringLiteral("game %1: illegal move %2").arg(i).arg(text));
                return std::nullopt;
            }
            game.moves.append({position->san(*move), move->uci()});
            position->play(*move);
        }
        game.plyCount = int(game.moves.size());
        if (game.uid.isEmpty())
            game.uid = GameIdentity::uid(game);
        if (imported.externalId.isEmpty())
            imported.externalId = game.uid;
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

    // Only the local games sharing a uid with the put need a closer look.
    QList<GameRecord> incoming;
    QSet<QString> incomingUids;
    for (const ImportedGame &imported : games) {
        incoming << imported.game;
        incomingUids.insert(imported.game.uid);
    }
    QList<GameRecord> local;
    QList<qint64> localIndexes;
    for (qint64 index = 0; index < database.gameCount(); ++index) {
        if (!incomingUids.contains(database.header(index).uid))
            continue;
        if (const std::optional<GameRecord> game = database.loadGame(index)) {
            local << *game;
            localIndexes << index;
        }
    }
    const Reconcile::Plan plan = Reconcile::plan(local, incoming);

    // A game purged here stays purged: the phone does not know about it yet.
    QSet<QString> purged;
    for (const GameStateRecord &state : database.gameStates()) {
        if (state.state == GameState::Purged)
            purged.insert(state.uid);
    }
    PutResult result;
    QList<ImportedGame> inserted;
    for (const int index : plan.insert) {
        if (purged.contains(games.at(index).game.uid))
            ++result.known;
        else
            inserted << games.at(index);
    }
    result.stored = database.importGames(source->id, inserted, errorMessage);
    if (result.stored < 0)
        return std::nullopt;
    for (const auto &[incomingIndex, localIndex] : plan.update) {
        if (!database.replaceGame(localIndexes.at(localIndex), incoming.at(incomingIndex), errorMessage))
            return std::nullopt;
        result.updatedIndexes << localIndexes.at(localIndex);
    }
    result.updated = int(plan.update.size());
    // A game imported from this phone before under another uid is known too.
    result.known += plan.known + int(inserted.size()) - result.stored;
    result.conflicts = plan.conflicts;
    source->lastSyncAt = QDateTime::currentDateTimeUtc();
    source->lastError.clear();
    if (!phoneName.isEmpty())
        source->account = phoneName;
    database.updateSource(*source, nullptr);
    return result;
}

} // namespace PhoneGames
