#pragma once

#include "app/sources/GameSource.h"

#include <QJsonArray>
#include <QList>
#include <QStringList>
#include <QString>

#include <optional>

class GameDatabase;

/// Games sent by the phone with a "put" (docs/phone-link.md): how they are
/// read from JSON and stored, once, in a database.
namespace PhoneGames {

/// GameSource::kind of a paired phone; its uuid is the phone's public key.
inline constexpr char kSourceKind[] = "phone";

struct PutResult {
    /// Games the database lacked.
    int stored = 0;
    /// Games replaced by a newer version (a conflict the phone won).
    int updated = 0;
    /// Games the database already had, as they were or newer.
    int known = 0;
    /// Uids whose two versions differed (see Reconcile).
    QStringList conflicts;
    /// Indexes of the games replaced, for views caching them.
    QList<qint64> updatedIndexes;
};

/// The games of a put. The UCI moves are replayed from the start position and
/// the SAN is written again from them, so a game is stored only if every move
/// is legal. Each game needs a `uid` or (older phones) an `id`; a game without
/// a uid gets the one its content gives (GameIdentity::uid). Nothing on the
/// first malformed game (with `errorMessage`).
std::optional<QList<ImportedGame>> parse(const QJsonArray &games, QString *errorMessage);

/// Merges the games into `database` by uid (Reconcile): new ones are stored
/// through the phone's source (connected on first use), newer versions replace
/// the stored ones, and a repeated put changes nothing.
std::optional<PutResult> store(GameDatabase &database, const QString &phoneKey, const QString &phoneName,
                               const QList<ImportedGame> &games, QString *errorMessage);

} // namespace PhoneGames
