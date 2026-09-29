#pragma once

#include "PhoneGames.h"

#include <optional>

/// Where PhoneLink stores the games a phone puts: the host decides, because
/// the database may be the one open in the window (see DatabaseFolderStore).
class PhoneGameStore {
public:
    virtual ~PhoneGameStore() = default;

    /// Stores `games` from a phone into the database at `path` (absolute,
    /// under the Databases folder), creating it if it does not exist.
    virtual std::optional<PhoneGames::PutResult> storeGames(const QString &path, const QString &phoneKey,
                                                            const QString &phoneName,
                                                            const QList<ImportedGame> &games,
                                                            QString *errorMessage) = 0;
};
