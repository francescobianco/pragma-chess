#include "Reconcile.h"

#include "GameIdentity.h"

#include <QDateTime>
#include <QHash>

namespace Reconcile {

bool newer(const QString &a, const QString &b)
{
    if (a.isEmpty())
        return false;
    if (b.isEmpty())
        return true;
    return QDateTime::fromString(a, Qt::ISODateWithMs) > QDateTime::fromString(b, Qt::ISODateWithMs);
}

Plan plan(const QList<GameRecord> &local, const QList<GameRecord> &incoming)
{
    QHash<QString, int> byUid;
    for (int i = 0; i < local.size(); ++i)
        byUid.insert(local.at(i).uid, i);

    Plan result;
    QHash<QString, int> seen; // A uid sent twice counts once.
    for (int i = 0; i < incoming.size(); ++i) {
        const GameRecord &game = incoming.at(i);
        if (seen.contains(game.uid)) {
            ++result.known;
            continue;
        }
        seen.insert(game.uid, i);
        const auto it = byUid.constFind(game.uid);
        if (it == byUid.cend()) {
            result.insert << i;
            continue;
        }
        const GameRecord &mine = local.at(*it);
        if (GameIdentity::sameContent(mine, game)) {
            ++result.known;
            continue;
        }
        result.conflicts << game.uid;
        if (newer(game.modified, mine.modified))
            result.update << std::pair{i, *it};
        else
            ++result.known;
    }
    return result;
}

} // namespace Reconcile
