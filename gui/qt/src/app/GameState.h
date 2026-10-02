#pragma once

#include <QList>
#include <QString>

/// Where a stored game is: in the lists, in the trash, deleted from the trash
/// (hidden everywhere, still in the file) or purged (optimizing the database
/// removed it; only its state is left, so another copy does not bring it back).
enum class GameState { Live, Trashed, Deleted, Purged };

/// "live", "trashed", "deleted", "purged". How a database stores a state.
QString gameStateKey(GameState state);
/// Live for anything else.
GameState gameStateFromKey(const QString &key);

/// The state of a game, by uid, and when it was set: what the copies of a
/// database exchange so that trashing and deleting reach every device.
struct GameStateRecord {
    QString uid;
    GameState state = GameState::Live;
    /// ISO 8601 UTC.
    QString modified;

    bool operator==(const GameStateRecord &) const = default;
};

namespace GameStates {

/// The records of `incoming` a copy holding `local` takes: those of games it
/// has no state for, and those set later than its own (a tie keeps the local
/// one). Pure: the caller stores them.
QList<GameStateRecord> incomingChanges(const QList<GameStateRecord> &local, const QList<GameStateRecord> &incoming);

} // namespace GameStates
