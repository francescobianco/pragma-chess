#include "DatabaseFolderStore.h"

#include "app/GameIdentity.h"
#include "app/SqliteGameDatabase.h"

#include <QDir>
#include <QFileInfo>

void DatabaseFolderStore::setOpenDatabase(std::function<GameDatabase *()> open,
                                          std::function<void(int, const QList<qint64> &)> stored)
{
    m_openDatabase = std::move(open);
    m_stored = std::move(stored);
}

GameDatabase *DatabaseFolderStore::openInstance(const QString &path) const
{
    GameDatabase *open = m_openDatabase ? m_openDatabase() : nullptr;
    if (open && !open->location().isEmpty() && QFileInfo(open->location()) == QFileInfo(path))
        return open;
    return nullptr;
}

std::optional<PhoneGames::PutResult> DatabaseFolderStore::storeGames(const QString &path,
                                                                     const DatabaseProperties &properties,
                                                                     const QString &phoneKey,
                                                                     const QString &phoneName,
                                                                     const QList<ImportedGame> &games,
                                                                     QString *errorMessage)
{
    if (m_busy && m_busy()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("busy, try again later");
        return std::nullopt;
    }

    // A database without an id yet becomes the lineage the phone names.
    const auto adopt = [&](GameDatabase &database) {
        if (properties.id.isEmpty() || !database.properties().id.isEmpty())
            return true;
        DatabaseProperties adopted = database.properties();
        adopted.id = properties.id;
        return database.setProperties(adopted, errorMessage);
    };

    if (GameDatabase *open = openInstance(path)) {
        if (!adopt(*open))
            return std::nullopt;
        const std::optional<PhoneGames::PutResult> result =
            PhoneGames::store(*open, phoneKey, phoneName, games, errorMessage);
        if (result && m_stored)
            m_stored(result->stored, result->updatedIndexes);
        return result;
    }

    std::unique_ptr<SqliteGameDatabase> database;
    if (QFileInfo::exists(path)) {
        database = SqliteGameDatabase::open(path, errorMessage);
        if (database && !adopt(*database))
            return std::nullopt;
    } else {
        QDir().mkpath(QFileInfo(path).absolutePath());
        database = SqliteGameDatabase::create(path, {}, errorMessage);
        if (database) {
            DatabaseProperties created = properties;
            if (created.id.isEmpty())
                created.id = database->properties().id;
            if (!database->setProperties(created, errorMessage))
                return std::nullopt;
        }
    }
    if (!database)
        return std::nullopt;
    return PhoneGames::store(*database, phoneKey, phoneName, games, errorMessage);
}

std::optional<PhoneFiles::Summary> DatabaseFolderStore::describe(const QString &path)
{
    // Not while a folder sync replaces the files: the next list asks again.
    if (m_busy && m_busy())
        return std::nullopt;
    const auto summary = [](GameDatabase &database) -> std::optional<PhoneFiles::Summary> {
        DatabaseProperties properties = database.properties();
        if (properties.id.isEmpty()) {
            properties.id = GameIdentity::newLineageId();
            if (!database.setProperties(properties, nullptr))
                return std::nullopt;
        }
        return PhoneFiles::Summary{properties.id, database.gameCount()};
    };
    if (GameDatabase *open = openInstance(path))
        return summary(*open);
    QString error;
    const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
    if (!database)
        return std::nullopt;
    return summary(*database);
}
