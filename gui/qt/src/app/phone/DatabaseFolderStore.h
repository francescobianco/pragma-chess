#pragma once

#include "PhoneGameStore.h"

#include <functional>

class GameDatabase;

/// Stores games from a phone by opening the database file (or creating it).
///
/// A database already open elsewhere in the process must not be written
/// behind its back (it caches its game list): the host names it with
/// setOpenDatabase, and games for it go through that instance, followed by
/// `stored` so the host can refresh its views. Everything runs on the thread
/// of PhoneLink, the GUI thread in the desktop client.
class DatabaseFolderStore : public PhoneGameStore {
public:
    /// The database open in the host (may return nullptr), and what to do
    /// after games were added to it or replaced in it (by index).
    void setOpenDatabase(std::function<GameDatabase *()> open,
                         std::function<void(int added, const QList<qint64> &updated)> stored);
    /// While this returns true (e.g. a folder sync replacing files), puts are
    /// refused and the phone tries again at its next sync.
    void setBusy(std::function<bool()> busy) { m_busy = std::move(busy); }

    std::optional<PhoneGames::PutResult> storeGames(const QString &path, const DatabaseProperties &properties,
                                                    const QString &phoneKey, const QString &phoneName,
                                                    const QList<ImportedGame> &games,
                                                    QString *errorMessage) override;
    std::optional<PhoneFiles::Summary> describe(const QString &path) override;

private:
    /// The host's instance if `path` is the database open in it.
    GameDatabase *openInstance(const QString &path) const;

    std::function<GameDatabase *()> m_openDatabase;
    std::function<void(int, const QList<qint64> &)> m_stored;
    std::function<bool()> m_busy;
};
