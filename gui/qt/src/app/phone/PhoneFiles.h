#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/// The databases Phone Link offers: the `.pdb` files under the Databases
/// folder, named relative to it with `/` separators.
class PhoneFiles {
public:
    struct Entry {
        QString name;
        qint64 size = 0;
        QString sha256;
        QDateTime modified;

        QJsonObject toJson() const;
    };

    explicit PhoneFiles(QString root);

    QString root() const { return m_root; }
    /// Every database under the root, hashed (unchanged files are not hashed again).
    QList<Entry> list();

    /// The absolute path of `name` under `root`, if it is a relative path to a
    /// `.pdb` file with no way out of the folder (no "..", no hidden parts, no
    /// drive or backslash).
    static std::optional<QString> resolve(const QString &root, const QString &name);

private:
    struct Hashed {
        qint64 size = 0;
        QDateTime modified;
        QString sha256;
    };

    QString m_root;
    QHash<QString, Hashed> m_hashes;
};
