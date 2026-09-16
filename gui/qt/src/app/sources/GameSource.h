#pragma once

#include "app/GameRecord.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>

/// An external source of games connected to a database, e.g. the games of a
/// lichess.org or chess.com account. Stored in the database with its sync
/// state; credentials are never stored there (see SourceCredentials).
struct GameSource {
    qint64 id = 0;
    /// Stable identity, also used to find the source's credentials.
    QString uuid;
    /// SourceCatalog kind: "lichess", "chesscom", …
    QString kind;
    /// Account on the site whose games are imported.
    QString account;
    /// Kind-specific options chosen when connecting (see SourceSettings).
    QJsonObject settings;
    /// Sync cursor, owned by the kind's fetcher.
    QJsonObject state;
    bool enabled = true;
    QDateTime createdAt;
    QDateTime lastSyncAt;
    /// Error of the last sync, empty when it succeeded.
    QString lastError;
    /// Games imported from this source so far.
    qint64 importedGames = 0;
};

/// Keys of GameSource::settings shared by the kinds.
namespace SourceSettings {
/// ISO date: only games played since then; absent for all games.
inline constexpr char since[] = "since";
/// Only rated games.
inline constexpr char ratedOnly[] = "ratedOnly";
} // namespace SourceSettings

/// A game fetched from a source, with the identifier the source gives it.
struct ImportedGame {
    QString externalId;
    GameRecord game;
};
