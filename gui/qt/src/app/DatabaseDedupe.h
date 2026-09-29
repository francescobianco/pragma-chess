#pragma once

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

/// Finds the database files that are one database under two files (a
/// duplicate made by a sync, a copy kept aside, a conflict copy), and says
/// which file keeps it. Pure and unit-tested; the same rule as the phone's
/// `Dedupe` (docs/phone-link.md, "Duplicates"), so every device keeps the
/// same lineage.
namespace DatabaseDedupe {

struct Candidate {
    /// Path relative to the synced folder, with '/' separators.
    QString path;
    /// DatabaseProperties::id; empty when unknown.
    QString lineage;
    /// Every game uid of the database.
    QSet<QString> uids;
    /// Where a database we ship lives (e.g. Books/Opening Names/English.pdb):
    /// always the file kept.
    bool canonical = false;
};

/// One database kept in `keeper`: the files of `others` are merged into it
/// and removed. `lineage` is the id it keeps: the smallest of the group, the
/// one the phone keeps too, which `keeper` may not have yet.
struct Group {
    QString keeper;
    QString lineage;
    QStringList others;

    bool operator==(const Group &) const = default;
};

/// "chess-com (Samsung SM-N960F)" → "chess-com", "Opening Names (old 2)" →
/// "Opening Names": the name without the parenthesised suffixes a sync, a
/// conflict or a copy kept aside adds.
QString baseTitle(const QString &fileBaseName);

/// Files are one database when they have the same lineage, or the same base
/// title (case-insensitive) and exactly the same games (both empty counts:
/// the same database made twice before a sync). The file
/// kept is the canonical one, else the one with the plain name, outside an
/// "Old" folder, then the shortest path; it takes the smallest lineage of the
/// group.
QList<Group> groups(const QList<Candidate> &candidates);

} // namespace DatabaseDedupe
