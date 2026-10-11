#include "MoveGames.h"

#include "app/UserFolders.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(MoveGames)
};

} // namespace

namespace MoveGames {

Result move(GameDatabase &from, const QStringList &uids, GameDatabase &to)
{
    Result result;
    for (const QString &uid : uids) {
        const qint64 index = from.indexOfUid(uid);
        if (index < 0 || from.stateOf(index) != GameState::Live)
            continue;
        const qint64 there = to.indexOfUid(uid);
        if (there >= 0 && to.stateOf(there) == GameState::Live) {
            ++result.alreadyThere;
        } else {
            std::optional<GameRecord> game = from.loadGame(index);
            if (!game) {
                result.error = Text::tr("Could not read the game.");
                return result;
            }
            game->state = GameState::Live;
            if (there >= 0)
                game->uid.clear(); // The one there, trashed or deleted, keeps its identity.
            QString error;
            if (to.addGame(*game, &error) < 0) {
                result.error = error;
                return result;
            }
            ++result.moved;
        }
        QString error;
        if (!from.setGameState(index, GameState::Deleted, &error)) {
            result.error = error;
            return result;
        }
    }
    return result;
}

QString newDatabasePath(const QString &folder, const QString &name, QString *problem)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        *problem = Text::tr("Give the new database a name.");
        return {};
    }
    if (trimmed.contains(QLatin1Char('/')) || trimmed.contains(QLatin1Char('\\')) || trimmed.startsWith(QLatin1Char('.'))) {
        *problem = Text::tr("“%1” is not a valid name.").arg(trimmed);
        return {};
    }
    const QString path = QDir(folder).filePath(trimmed + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
    if (QFileInfo::exists(path)) {
        *problem = Text::tr("A database named “%1” already exists.").arg(trimmed);
        return {};
    }
    return path;
}

} // namespace MoveGames
