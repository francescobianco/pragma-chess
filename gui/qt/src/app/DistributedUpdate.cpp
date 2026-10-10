#include "DistributedUpdate.h"

#include "app/GameIdentity.h"

#include <QSet>

namespace DistributedUpdate {

int addMissingGames(GameDatabase &database, const QList<GameRecord> &shipped, QString *error)
{
    QSet<QString> present;
    QSet<QString> positions;
    for (qint64 i = 0; i < database.gameCount(); ++i) {
        const GameRecord header = database.header(i);
        present.insert(header.uid);
        if (!header.startFen.isEmpty())
            positions.insert(header.startFen);
    }
    for (const GameStateRecord &state : database.gameStates())
        present.insert(state.uid); // Thrown away, even for good: not brought back.
    int added = 0;
    for (const GameRecord &game : shipped) {
        const QString uid = game.uid.isEmpty() ? GameIdentity::uid(game) : game.uid;
        if (present.contains(uid) || (!game.startFen.isEmpty() && positions.contains(game.startFen)))
            continue;
        if (database.addGame(game, error) >= 0) {
            ++added;
            present.insert(uid);
        }
    }
    return added;
}

} // namespace DistributedUpdate
