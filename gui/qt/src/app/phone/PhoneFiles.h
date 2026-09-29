#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <functional>
#include <optional>

/// The databases Phone Link offers: the `.pdb` files under the Databases
/// folder, named relative to it with `/` separators.
class PhoneFiles {
public:
    /// What the host knows of a database file: its universal id (made on
    /// the spot for a file without one) and how many games it holds.
    struct Summary {
        QString id;
        qint64 games = 0;
    };
    using Describe = std::function<std::optional<Summary>(const QString &path)>;

    struct Entry {
        QString name;
        qint64 size = 0;
        QString sha256;
        QDateTime modified;
        /// Universal id (lineage), empty if unknown.
        QString id;
        qint64 games = 0;

        QJsonObject toJson() const;
    };

    /// Where a put addressed to a lineage goes (docs/phone-link.md, "The sync,
    /// from the phone"): the file with that id; else `name` if that file has
    /// no id yet or does not exist; else "name (phone name).pdb" (numbered if
    /// needed), a new file.
    struct Target {
        QString name;
        bool exists = false;
    };
    static Target target(const QList<Entry> &entries, const QString &lineage, const QString &name,
                         const QString &phoneName);

    explicit PhoneFiles(QString root);

    QString root() const { return m_root; }
    /// Every database under the root, hashed and described (unchanged files
    /// are not looked at again). `describe` may write the file (to give it an id).
    QList<Entry> list(const Describe &describe = {});

    /// The absolute path of `name` under `root`, if it is a relative path to a
    /// `.pdb` file with no way out of the folder (no "..", no hidden parts, no
    /// drive or backslash).
    static std::optional<QString> resolve(const QString &root, const QString &name);

private:
    struct Hashed {
        qint64 size = 0;
        QDateTime modified;
        QString sha256;
        QString id;
        qint64 games = 0;
        /// Whether `id` and `games` are known; asked again until they are.
        bool described = false;
    };

    QString m_root;
    QHash<QString, Hashed> m_hashes;
};
