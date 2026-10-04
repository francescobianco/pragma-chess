#include "DatabaseMerge.h"

#include "Reconcile.h"
#include "SqliteGameDatabase.h"

#include <QObject>

namespace DatabaseMerge {

namespace {

std::optional<QList<GameRecord>> allGames(const GameDatabase &database, QString *errorMessage)
{
    QList<GameRecord> games;
    games.reserve(database.gameCount());
    for (qint64 i = 0; i < database.gameCount(); ++i) {
        const std::optional<GameRecord> game = database.loadGame(i);
        if (!game) {
            if (errorMessage)
                *errorMessage = QObject::tr("Could not read a game of “%1”.").arg(database.name());
            return std::nullopt;
        }
        games << *game;
    }
    return games;
}

} // namespace

std::optional<Result> mergeInto(GameDatabase &into, const QString &from, QString *errorMessage)
{
    const std::unique_ptr<SqliteGameDatabase> source = SqliteGameDatabase::open(from, errorMessage);
    if (!source)
        return std::nullopt;
    // Where the games are first (trash, deleted, purged): a game purged on
    // either side must not come back from the other.
    if (!into.mergeGameStates(source->gameStates(), errorMessage))
        return std::nullopt;
    QSet<QString> purged;
    for (const GameStateRecord &state : into.gameStates()) {
        if (state.state == GameState::Purged)
            purged.insert(state.uid);
    }
    const std::optional<QList<GameRecord>> incoming = allGames(*source, errorMessage);
    const std::optional<QList<GameRecord>> local = allGames(into, errorMessage);
    if (!incoming || !local)
        return std::nullopt;

    const Reconcile::Plan plan = Reconcile::plan(*local, *incoming);
    Result result;
    result.known = plan.known;
    result.conflicts = plan.conflicts;
    for (const auto &[incomingIndex, localIndex] : plan.update) {
        if (!into.replaceGame(localIndex, incoming->at(incomingIndex), errorMessage))
            return std::nullopt;
        ++result.updated;
    }
    for (int incomingIndex : plan.insert) {
        GameRecord game = incoming->at(incomingIndex);
        if (purged.contains(game.uid)) {
            ++result.known;
            continue;
        }
        game.id = 0;
        if (into.addGame(game, errorMessage) < 0)
            return std::nullopt;
        ++result.stored;
    }

    // The sources it connected, and what each imported: without them a sync
    // would import those games again, as copies of the ones just merged.
    QSet<QString> connected;
    for (const GameSource &known : into.sources())
        connected.insert(known.uuid);
    for (GameSource incomingSource : source->sources()) {
        if (connected.contains(incomingSource.uuid))
            continue;
        incomingSource.id = 0;
        if (!into.addSource(incomingSource, errorMessage) || !into.updateSource(incomingSource, errorMessage))
            return std::nullopt;
    }
    if (!into.mergeSourceLinks(source->sourceLinks(), errorMessage))
        return std::nullopt;

    // Who the players are, where this copy does not say.
    const PlayerRoles roles = into.playerRoles();
    for (const auto &[player, role] : source->playerRoles().asKeyValueRange()) {
        if (!roles.contains(player) && !into.setPlayerRole(player, role, errorMessage))
            return std::nullopt;
    }
    return result;
}

bool mergeFile(const QString &from, const QString &into, const QString &lineage, QString *errorMessage)
{
    const std::unique_ptr<SqliteGameDatabase> target = SqliteGameDatabase::open(into, errorMessage);
    if (!target || !mergeInto(*target, from, errorMessage))
        return false;
    DatabaseProperties properties = target->properties();
    if (lineage.isEmpty() || properties.id == lineage)
        return true;
    properties.id = lineage;
    return target->setProperties(properties, errorMessage);
}

FolderSync::DatabaseHooks hooks(std::function<bool(const QString &relativePath)> canonical)
{
    FolderSync::DatabaseHooks result;
    result.lineage = [](const QString &path) { return SqliteGameDatabase::readProperties(path).id; };
    result.uids = [](const QString &path) {
        const QHash<QString, QString> revisions = SqliteGameDatabase::readRevisions(path);
        return QSet<QString>(revisions.keyBegin(), revisions.keyEnd());
    };
    result.canonical = std::move(canonical);
    result.merge = &mergeFile;
    return result;
}

} // namespace DatabaseMerge
