#pragma once

#include <QHash>
#include <QString>

/// What a database is for. Game collections are the default; an opening book
/// holds named lines (Event = name, ECO = code) and is what Options ▸ Opening
/// Names offers.
enum class DatabaseType { GameCollection, OpeningBook };

/// Properties of a database, stored inside it (table `properties`, key/value)
/// so they travel with the file: to another computer, through the folder
/// sync and to the phone.
struct DatabaseProperties {
    /// Universal id: copies of the same database on any device share it,
    /// whatever the file is called (docs/phone-link.md, "Identity"). Empty
    /// for files made before version 5 until they are first synced.
    QString id;
    DatabaseType type = DatabaseType::GameCollection;
    /// Free text shown in Database Settings.
    QString description;
    /// Display name, stored as `name`; empty to show the file name.
    QString name;
    /// The name in other languages, stored as `name.<code>` (e.g. "name.it").
    QHash<QString, QString> localizedNames;

    /// The name to show in the interface language `languageCode` ("it", "en"
    /// or "it_IT"): that translation, else the default name, else `fileBaseName`.
    QString displayName(const QString &languageCode, const QString &fileBaseName) const;

    /// Reads the stored key/value rows; missing or unknown values are defaults.
    static DatabaseProperties fromValues(const QHash<QString, QString> &values);
    /// The rows to store (defaults included, so the file says what it is; an
    /// empty id or name is left out, so storing never erases one).
    QHash<QString, QString> values() const;

    /// Stored value of a type: "games", "opening-book".
    static QString typeKey(DatabaseType type);
    static DatabaseType typeFromKey(const QString &key);

    bool operator==(const DatabaseProperties &) const = default;
};
