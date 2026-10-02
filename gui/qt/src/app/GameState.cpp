#include "GameState.h"

#include "Reconcile.h"

#include <QDateTime>
#include <QHash>

QString gameStateKey(GameState state)
{
    switch (state) {
    case GameState::Live: return QStringLiteral("live");
    case GameState::Trashed: return QStringLiteral("trashed");
    case GameState::Deleted: return QStringLiteral("deleted");
    case GameState::Purged: return QStringLiteral("purged");
    }
    return {};
}

GameState gameStateFromKey(const QString &key)
{
    for (const GameState state : {GameState::Trashed, GameState::Deleted, GameState::Purged}) {
        if (key == gameStateKey(state))
            return state;
    }
    return GameState::Live;
}

namespace GameStates {

bool isRecent(const QString &modified, const QDateTime &now)
{
    const QDateTime when = QDateTime::fromString(modified, Qt::ISODateWithMs);
    return when.isValid() && when.secsTo(now) < qint64(kRecentDays) * 24 * 3600;
}

QList<GameStateRecord> incomingChanges(const QList<GameStateRecord> &local, const QList<GameStateRecord> &incoming)
{
    QHash<QString, GameStateRecord> current;
    for (const GameStateRecord &record : local)
        current.insert(record.uid, record);

    QList<GameStateRecord> changes;
    QHash<QString, int> taken; // A uid sent twice: its newest record.
    for (const GameStateRecord &record : incoming) {
        if (record.uid.isEmpty())
            continue;
        const auto mine = current.constFind(record.uid);
        if (mine != current.cend() && !Reconcile::newer(record.modified, mine->modified))
            continue;
        if (const auto earlier = taken.constFind(record.uid); earlier != taken.cend())
            changes[*earlier] = record;
        else {
            taken.insert(record.uid, int(changes.size()));
            changes << record;
        }
        current.insert(record.uid, record);
    }
    return changes;
}

} // namespace GameStates
