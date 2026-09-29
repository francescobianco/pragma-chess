#include "DatabaseFolderStore.h"

#include "app/SqliteGameDatabase.h"

#include <QDir>
#include <QFileInfo>

void DatabaseFolderStore::setOpenDatabase(std::function<GameDatabase *()> open, std::function<void(int)> stored)
{
    m_openDatabase = std::move(open);
    m_stored = std::move(stored);
}

std::optional<PhoneGames::PutResult> DatabaseFolderStore::storeGames(const QString &path, const QString &phoneKey,
                                                                     const QString &phoneName,
                                                                     const QList<ImportedGame> &games,
                                                                     QString *errorMessage)
{
    if (m_busy && m_busy()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("busy, try again later");
        return std::nullopt;
    }
    if (GameDatabase *open = m_openDatabase ? m_openDatabase() : nullptr;
        open && !open->location().isEmpty() && QFileInfo(open->location()) == QFileInfo(path)) {
        const std::optional<PhoneGames::PutResult> result =
            PhoneGames::store(*open, phoneKey, phoneName, games, errorMessage);
        if (result && m_stored)
            m_stored(result->stored);
        return result;
    }

    std::unique_ptr<SqliteGameDatabase> database;
    if (QFileInfo::exists(path)) {
        database = SqliteGameDatabase::open(path, errorMessage);
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        database = SqliteGameDatabase::create(path, {}, errorMessage);
    }
    if (!database)
        return std::nullopt;
    return PhoneGames::store(*database, phoneKey, phoneName, games, errorMessage);
}
