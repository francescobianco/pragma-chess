#include "LichessStudy.h"

#include "PgnFile.h"

#include <QRegularExpression>

namespace LichessStudy {

std::optional<QString> studyId(const QString &urlOrId)
{
    // Study and chapter ids are eight letters and digits.
    static const QRegularExpression inUrl(QStringLiteral(R"(lichess\.org/study/([A-Za-z0-9]{8})(?:[/?#]|$))"));
    static const QRegularExpression alone(QStringLiteral(R"(^[A-Za-z0-9]{8}$)"));
    const QString text = urlOrId.trimmed();
    if (const QRegularExpressionMatch match = inUrl.match(text); match.hasMatch())
        return match.captured(1);
    if (alone.match(text).hasMatch())
        return text;
    return std::nullopt;
}

QString studyUrl(const QString &studyId)
{
    return QStringLiteral("https://lichess.org/study/%1").arg(studyId);
}

QString tag(const GameRecord &game, const QString &name)
{
    for (const PgnTag &pgnTag : game.tags) {
        if (pgnTag.name == name)
            return pgnTag.value;
    }
    return {};
}

QList<Chapter> chapters(const QByteArray &pgn, int *unreadable)
{
    static const QRegularExpression chapterUrl(QStringLiteral(R"(/study/[A-Za-z0-9]{8}/([A-Za-z0-9]{8}))"));
    QList<Chapter> result;
    int bad = 0;
    for (const PgnFile::Entry &entry : PgnFile::scan(pgn)) {
        if (!entry.isGame)
            continue;
        QString ignored;
        std::optional<GameRecord> game = PgnFile::read(pgn.mid(entry.offset, entry.length), &ignored);
        const QString id = game ? chapterUrl.match(tag(*game, QStringLiteral("ChapterURL"))).captured(1) : QString();
        if (!game || id.isEmpty()) {
            ++bad;
            continue;
        }
        result << Chapter{id, entry.hash, *game};
    }
    if (unreadable)
        *unreadable = bad;
    return result;
}

QString studyName(const QList<Chapter> &chapters)
{
    for (const Chapter &chapter : chapters) {
        if (const QString name = tag(chapter.game, QStringLiteral("StudyName")); !name.isEmpty())
            return name;
    }
    return {};
}

ReadPlan planRead(const QList<Chapter> &chapters, const PgnFilePlan::Base &base,
                  const QHash<QString, PgnFilePlan::DatabaseGame> &games, const QHash<QString, QString> &linked)
{
    ReadPlan plan;
    QSet<QString> seen;
    for (int i = 0; i < chapters.size(); ++i) {
        const Chapter &chapter = chapters.at(i);
        if (seen.contains(chapter.id))
            continue;
        seen.insert(chapter.id);
        const QString uid = linked.value(chapter.id);
        const auto before = base.constFind(chapter.id);
        if (uid.isEmpty()) {
            // In the base without a game: it was imported and is gone for good.
            if (before == base.constEnd())
                plan.imports << i;
            continue;
        }
        const auto game = games.constFind(uid);
        if (game == games.constEnd() || before == base.constEnd())
            continue; // Purged here, or met for the first time: nothing to compare with.
        const bool studyChanged = chapter.hash != before->hash;
        const bool databaseChanged = game->modified != before->modified;
        if (studyChanged && databaseChanged)
            plan.conflicts << i;
        else if (studyChanged)
            plan.updates << qMakePair(i, uid);
    }
    return plan;
}

PgnFilePlan::Base nextBase(const PgnFilePlan::Base &before, const QList<Chapter> &chapters,
                           const QHash<QString, PgnFilePlan::DatabaseGame> &games,
                           const QHash<QString, QString> &linked)
{
    PgnFilePlan::Base base = before;
    for (const Chapter &chapter : chapters) {
        const auto game = games.constFind(linked.value(chapter.id));
        const QString modified = game != games.constEnd() ? game->modified : before.value(chapter.id).modified;
        base.insert(chapter.id, PgnFilePlan::BaseEntry{chapter.hash, modified});
    }
    return base;
}

QString conflictId(const Chapter &chapter)
{
    return chapter.id + QLatin1Char(':') + chapter.hash;
}

} // namespace LichessStudy
