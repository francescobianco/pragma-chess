#pragma once

#include "DatabaseProperties.h"

#include <QHash>
#include <QList>
#include <QStringList>
#include <QString>

#include <functional>

/// The opening names databases we ship, one per language, and the pure
/// decisions about them: which one a language uses by default, and where a
/// copy found among the databases of games is moved to.
namespace ShippedOpeningNames {

struct Names {
    /// Fixed universal id (GameIdentity), the same on every install.
    QString lineage;
    /// File name in the Opening Names folder.
    QString fileName;
    /// Interface language it is the default for ("en", "it").
    QString language;
    /// Ready-made database and the TSV it was built from, in the resources.
    QString resource;
    QString tsv;
    /// Display name stored in the database (`name`), and its translations (`name.<code>`).
    QString name;
    QHash<QString, QString> localizedNames;
    /// File names older versions seeded it under, renamed to `fileName`.
    QStringList formerFileNames;

    /// Sets the stored name and its translations on `properties`.
    void applyNames(DatabaseProperties &properties) const;
};

/// English first: it is the default for every language without its own names.
QList<Names> all();
/// The names shipped with this lineage, or nullptr.
const Names *byLineage(const QString &lineage);
/// The names an interface language uses when the user has not chosen any.
Names forLanguage(const QString &languageCode);

/// Whether a database in the Databases folder is a shipped names database,
/// to be moved into the Opening Names folder: by lineage, or (files seeded
/// before lineages existed) by the name the seed gave it.
bool shouldMove(const QString &fileName, const DatabaseProperties &properties);

/// The file a shipped database in the Opening Names folder should have, when
/// it is still under a name an older version gave it (e.g. "Opening
/// Names.pdb" → "English.pdb"); empty when nothing is to be renamed.
QString renamedFileName(const QString &fileName, const DatabaseProperties &properties);

/// Whether merging a copy's games (`copy`, uid → modified) into `kept`
/// would change nothing: every game of the copy is there, none of them newer
/// (empty `modified` is the oldest). Then the copy can go without losing data.
bool addsNothing(const QHash<QString, QString> &kept, const QHash<QString, QString> &copy);

/// Where a shipped database moved into `folder` lands: its shipped file name,
/// or, when that is taken, "Old/<name>.pdb", "Old/<name> 2.pdb"…, so no copy
/// is ever overwritten and the menu is not filled with duplicates.
QString moveTarget(const QString &folder, const Names &names,
                   const std::function<bool(const QString &path)> &exists);

} // namespace ShippedOpeningNames
