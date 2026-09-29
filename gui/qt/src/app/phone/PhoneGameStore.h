#pragma once

#include "PhoneFiles.h"
#include "PhoneGames.h"

#include "app/DatabaseProperties.h"

#include <optional>

/// Where PhoneLink stores the games a phone puts: the host decides, because
/// the database may be the one open in the window (see DatabaseFolderStore).
class PhoneGameStore {
public:
    virtual ~PhoneGameStore() = default;

    /// Merges `games` from a phone into the database at `path` (absolute,
    /// under the Databases folder). A database that does not exist is created
    /// with `properties` (its universal id among them); one without an id yet
    /// takes `properties.id`.
    virtual std::optional<PhoneGames::PutResult> storeGames(const QString &path, const DatabaseProperties &properties,
                                                            const QString &phoneKey, const QString &phoneName,
                                                            const QList<ImportedGame> &games,
                                                            QString *errorMessage) = 0;

    /// The id and game count of the database at `path` for "list"; a database
    /// without an id gets one. Nothing if it cannot be read (or not now).
    virtual std::optional<PhoneFiles::Summary> describe(const QString &path) = 0;
};
