#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

/// What a database is for. Game collections are the default; an opening book
/// holds named lines (Event = name, ECO = code) and is what Options ▸ Opening
/// Names offers; a training database holds puzzles and exercises: its games
/// list does not show their moves, and a game opened from it is played in
/// Training Mode by the side to move.
enum class DatabaseType { GameCollection, OpeningBook, Training };

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
    /// The name the user gave the database, stored as `name`; empty to show
    /// the file name, or the distributed name. Any database may have one, and
    /// when it does it is the one shown.
    QString name;
    /// The name in every language, stored as `name.<code>` ("name.en",
    /// "name.it"): what makes a database one we distribute (isDistributed).
    /// It is a separate carrier from `name`: one read with `name` and no
    /// `name.en` (written before they were told apart) takes `name` as its
    /// English name.
    QHash<QString, QString> localizedNames;
    /// The columns of the games list this database hides, by their key
    /// (GameListModel::columnKey: "result", "site"…), stored as
    /// `columns.hidden`, comma separated: each database opens with its own.
    QStringList hiddenColumns;
    /// The columns a database we distribute hides by default that were
    /// given to it (`columns.shipped`, comma separated): each once, so a
    /// column the user shows again stays shown — on every computer, since
    /// the list travels with the file — and one added to the defaults later
    /// still reaches the copies made before.
    QStringList shippedColumns;

    /// A database we distribute (Classic Games, the training sets, the
    /// opening names): it is named in every language, not by the user.
    bool isDistributed() const { return !localizedNames.isEmpty(); }

    /// The name to show in the interface language `languageCode` ("it", "en"
    /// or "it_IT"): the name the user gave it; else, for a distributed
    /// database, that translation or the English one; else `fileBaseName`.
    QString displayName(const QString &languageCode, const QString &fileBaseName) const;
    /// The name given in Database Settings (displayName), or empty when the
    /// database has none but its file's: the interface then shows the file.
    QString givenName(const QString &languageCode, const QString &fileBaseName) const;
    /// How the menus name the database file `path`: "My Games (games.pdb)"
    /// when the user named it; a distributed database not renamed, by its
    /// name in the language alone ("Allenati sui finali"); else the
    /// file's base name.
    QString label(const QString &languageCode, const QString &path) const;

    /// Reads the stored key/value rows; missing or unknown values are defaults.
    static DatabaseProperties fromValues(const QHash<QString, QString> &values);
    /// The rows to store (defaults included, so the file says what it is and
    /// hidden columns can all be shown again; an empty id or name is left
    /// out, so storing never erases one).
    QHash<QString, QString> values() const;

    /// Stored value of a type: "games", "opening-book", "training".
    static QString typeKey(DatabaseType type);
    static DatabaseType typeFromKey(const QString &key);

    bool operator==(const DatabaseProperties &) const = default;
};
