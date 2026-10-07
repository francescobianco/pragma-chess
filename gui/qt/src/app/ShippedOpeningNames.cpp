#include "ShippedOpeningNames.h"

#include "GameIdentity.h"

#include <QDir>

namespace ShippedOpeningNames {

QList<Names> all()
{
    Names english;
    english.lineage = GameIdentity::kOpeningNamesLineage;
    english.fileName = QStringLiteral("English.pdb");
    english.language = QStringLiteral("en");
    english.resource = QStringLiteral(":/openings/opening-names.pdb");
    english.tsv = QStringLiteral(":/openings/lichess-openings.tsv");
    english.name = QStringLiteral("English");
    english.localizedNames = {{QStringLiteral("it"), QStringLiteral("Inglese")}};
    english.formerFileNames = {QStringLiteral("Opening Names.pdb")};

    Names italian;
    italian.lineage = GameIdentity::kItalianOpeningNamesLineage;
    italian.fileName = QStringLiteral("Italian.pdb");
    italian.language = QStringLiteral("it");
    italian.resource = QStringLiteral(":/openings/opening-names-it.pdb");
    italian.tsv = QStringLiteral(":/openings/lichess-openings-it.tsv");
    italian.name = QStringLiteral("Italian");
    italian.localizedNames = {{QStringLiteral("it"), QStringLiteral("Italiano")}};
    italian.formerFileNames = {QStringLiteral("Nomi delle aperture.pdb")};

    return {english, italian};
}

void Names::applyNames(DatabaseProperties &properties) const
{
    // Distributed: named in every language, English among them; a name the
    // user gave it stays theirs.
    properties.localizedNames = localizedNames;
    properties.localizedNames.insert(QStringLiteral("en"), name);
}

const Names *byLineage(const QString &lineage)
{
    static const QList<Names> names = all();
    for (const Names &entry : names) {
        if (entry.lineage == lineage)
            return &entry;
    }
    return nullptr;
}

Names forLanguage(const QString &languageCode)
{
    const QList<Names> names = all();
    for (const Names &entry : names) {
        if (entry.language == languageCode)
            return entry;
    }
    return names.first();
}

bool shouldMove(const QString &fileName, const DatabaseProperties &properties)
{
    if (byLineage(properties.id))
        return true;
    if (!properties.id.isEmpty())
        return false;
    // Seeded before lineages, under a name the seed gave them.
    for (const Names &names : all()) {
        if (names.formerFileNames.contains(fileName))
            return true;
    }
    return false;
}

QString renamedFileName(const QString &fileName, const DatabaseProperties &properties)
{
    const Names *names = byLineage(properties.id);
    if (!names || fileName == names->fileName || !names->formerFileNames.contains(fileName))
        return QString();
    return names->fileName;
}

bool addsNothing(const QHash<QString, QString> &kept, const QHash<QString, QString> &copy)
{
    if (copy.isEmpty())
        return false; // Not a readable database: never removed.
    for (auto it = copy.cbegin(); it != copy.cend(); ++it) {
        const auto found = kept.constFind(it.key());
        // ISO 8601 UTC timestamps compare as text.
        if (found == kept.cend() || it.value() > found.value())
            return false;
    }
    return true;
}

QString moveTarget(const QString &folder, const Names &names,
                   const std::function<bool(const QString &path)> &exists)
{
    const QDir dir(folder);
    const QString target = dir.filePath(names.fileName);
    if (!exists(target))
        return target;
    // Kept out of the way (and out of the menu), but kept.
    const QDir old(dir.filePath(QStringLiteral("Old")));
    const QString base = names.fileName.chopped(4); // ".pdb"
    for (int n = 1;; ++n) {
        const QString candidate =
            old.filePath(n == 1 ? names.fileName : QStringLiteral("%1 %2.pdb").arg(base).arg(n));
        if (!exists(candidate))
            return candidate;
    }
}

} // namespace ShippedOpeningNames
