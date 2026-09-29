#include "DatabaseDedupe.h"

#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>
#include <numeric>

namespace DatabaseDedupe {

namespace {

QString fileBaseName(const QString &path)
{
    return QFileInfo(path).completeBaseName();
}

bool inOldFolder(const QString &path)
{
    return path.split(QLatin1Char('/')).contains(QLatin1String("Old"));
}

} // namespace

QString baseTitle(const QString &fileBaseName)
{
    static const QRegularExpression suffix(QStringLiteral("\\s+\\([^()]*\\)$"));
    QString title = fileBaseName.trimmed();
    for (;;) {
        const QString stripped = QString(title).remove(suffix).trimmed();
        if (stripped == title || stripped.isEmpty())
            return title;
        title = stripped;
    }
}

QList<Group> groups(const QList<Candidate> &candidates)
{
    // Union-find over the candidates.
    QList<qsizetype> parent(candidates.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto find = [&parent](qsizetype i) {
        while (parent[i] != i)
            i = parent[i] = parent[parent[i]];
        return i;
    };
    const auto unite = [&](qsizetype a, qsizetype b) { parent[find(a)] = find(b); };

    for (qsizetype i = 0; i < candidates.size(); ++i) {
        const Candidate &a = candidates.at(i);
        for (qsizetype j = i + 1; j < candidates.size(); ++j) {
            const Candidate &b = candidates.at(j);
            const bool sameLineage = !a.lineage.isEmpty() && a.lineage == b.lineage;
            // Empty ones too: two empty databases of one name have nothing to lose.
            const bool sameGames = a.uids == b.uids
                && baseTitle(fileBaseName(a.path)).compare(baseTitle(fileBaseName(b.path)), Qt::CaseInsensitive) == 0;
            if (sameLineage || sameGames)
                unite(i, j);
        }
    }

    QMap<qsizetype, QList<qsizetype>> byRoot;
    for (qsizetype i = 0; i < candidates.size(); ++i)
        byRoot[find(i)] << i;

    QList<Group> result;
    for (const QList<qsizetype> &members : std::as_const(byRoot)) {
        if (members.size() < 2)
            continue;
        // The plain name, not a copy kept aside, then the shortest path.
        const auto rank = [&](qsizetype i) {
            const QString &path = candidates.at(i).path;
            const bool plain = baseTitle(fileBaseName(path)) == fileBaseName(path);
            return std::make_tuple(!candidates.at(i).canonical, inOldFolder(path), !plain, path.size(), path);
        };
        const qsizetype keeper = *std::min_element(members.cbegin(), members.cend(),
                                                   [&](qsizetype a, qsizetype b) { return rank(a) < rank(b); });
        QString lineage;
        for (qsizetype i : members) {
            const QString &id = candidates.at(i).lineage;
            if (!id.isEmpty() && (lineage.isEmpty() || id < lineage))
                lineage = id;
        }
        Group group{candidates.at(keeper).path, lineage, {}};
        for (qsizetype i : members) {
            if (i != keeper)
                group.others << candidates.at(i).path;
        }
        group.others.sort();
        result << group;
    }
    std::sort(result.begin(), result.end(), [](const Group &a, const Group &b) { return a.keeper < b.keeper; });
    return result;
}

} // namespace DatabaseDedupe
