#pragma once

#include <QHash>
#include <QString>

/// What a database is for. Game collections are the default; an opening book
/// holds named lines (Event = name, ECO = code) and is what Book ▸ Opening
/// Names offers.
enum class DatabaseType { GameCollection, OpeningBook };

/// Properties of a database, stored inside it (table `properties`, key/value)
/// so they travel with the file: to another computer, through the folder
/// sync and to the phone.
struct DatabaseProperties {
    DatabaseType type = DatabaseType::GameCollection;
    /// Free text shown in Database Settings.
    QString description;

    /// Reads the stored key/value rows; missing or unknown values are defaults.
    static DatabaseProperties fromValues(const QHash<QString, QString> &values);
    /// The rows to store (defaults included, so the file says what it is).
    QHash<QString, QString> values() const;

    /// Stored value of a type: "games", "opening-book".
    static QString typeKey(DatabaseType type);
    static DatabaseType typeFromKey(const QString &key);

    bool operator==(const DatabaseProperties &) const = default;
};
