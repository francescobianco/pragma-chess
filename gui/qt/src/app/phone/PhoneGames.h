#pragma once

#include "app/sources/GameSource.h"

#include <QJsonArray>
#include <QList>
#include <QString>

#include <optional>

class GameDatabase;

/// Games sent by the phone with a "put" (docs/phone-link.md): how they are
/// read from JSON and stored, once, in a database.
namespace PhoneGames {

/// GameSource::kind of a paired phone; its uuid is the phone's public key.
inline constexpr char kSourceKind[] = "phone";

struct PutResult {
    int stored = 0;
    int known = 0;
};

/// The games of a put. The UCI moves are replayed from the start position and
/// the SAN is written again from them, so a game is stored only if every move
/// is legal. Nothing on the first malformed game (with `errorMessage`).
std::optional<QList<ImportedGame>> parse(const QJsonArray &games, QString *errorMessage);

/// Stores the games the phone had not sent before, through the phone's source
/// in `database` (connected on first use): a repeated put stores nothing new.
std::optional<PutResult> store(GameDatabase &database, const QString &phoneKey, const QString &phoneName,
                               const QList<ImportedGame> &games, QString *errorMessage);

} // namespace PhoneGames
