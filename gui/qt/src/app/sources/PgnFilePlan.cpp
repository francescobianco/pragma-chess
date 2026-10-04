#include "PgnFilePlan.h"

#include <QJsonArray>

namespace PgnFilePlan {

namespace {

constexpr char kBase[] = "base";

} // namespace

QString modeKey(Mode mode)
{
    switch (mode) {
    case Mode::Read:
        return QStringLiteral("read");
    case Mode::Write:
        return QStringLiteral("write");
    case Mode::ReadWrite:
        break;
    }
    return QStringLiteral("readwrite");
}

Mode modeFromKey(const QString &key)
{
    if (key == QLatin1String("read"))
        return Mode::Read;
    if (key == QLatin1String("write"))
        return Mode::Write;
    return Mode::ReadWrite;
}

QJsonObject baseToJson(const Base &base)
{
    QJsonObject games;
    for (auto it = base.constBegin(); it != base.constEnd(); ++it)
        games.insert(it.key(), QJsonArray{it->hash, it->modified});
    return QJsonObject{{QLatin1String(kBase), games}};
}

Base baseFromJson(const QJsonObject &state)
{
    Base base;
    const QJsonObject games = state.value(QLatin1String(kBase)).toObject();
    for (auto it = games.constBegin(); it != games.constEnd(); ++it) {
        const QJsonArray fields = it->toArray();
        base.insert(it.key(), BaseEntry{fields.at(0).toString(), fields.at(1).toString()});
    }
    return base;
}

QString externalId(const PgnFile::Entry &entry)
{
    return entry.uid.isEmpty() ? QStringLiteral("sha:") + entry.hash : QStringLiteral("uid:") + entry.uid;
}

ReadPlan planRead(Mode mode, const QList<PgnFile::Entry> &entries, const Base &base,
                  const QHash<QString, DatabaseGame> &games, const QSet<QString> &known)
{
    ReadPlan plan;
    QSet<QString> seen;
    for (int i = 0; i < entries.size(); ++i) {
        const PgnFile::Entry &entry = entries.at(i);
        if (!entry.isGame)
            continue;
        if (entry.uid.isEmpty()) {
            // A game the database has not tied to itself: by its content.
            if (reads(mode) && !known.contains(externalId(entry)))
                plan.imports << i;
            continue;
        }
        if (seen.contains(entry.uid))
            continue; // A copy of a game further up: the first one is the game.
        seen.insert(entry.uid);

        const auto game = games.constFind(entry.uid);
        if (game == games.constEnd()) {
            // In the base: the database no longer has it, and that is not undone.
            if (reads(mode) && !base.contains(entry.uid) && !known.contains(externalId(entry)))
                plan.imports << i;
            continue;
        }
        const auto before = base.constFind(entry.uid);
        if (before == base.constEnd())
            continue; // Met for the first time: the same game on both sides.
        const bool fileChanged = entry.hash != before->hash;
        const bool databaseChanged = game->modified != before->modified;
        const bool rewrite = writes(mode) && game->live;
        if (fileChanged && databaseChanged) {
            if (reads(mode))
                plan.conflicts << i;
            if (rewrite)
                plan.rewrites << qMakePair(i, entry.uid);
        } else if (fileChanged) {
            if (reads(mode))
                plan.updates << qMakePair(i, entry.uid);
        } else if (databaseChanged && rewrite) {
            plan.rewrites << qMakePair(i, entry.uid);
        }
    }
    return plan;
}

WritePlan planWrite(Mode mode, const QList<PgnFile::Entry> &entries, const Base &base,
                    const QHash<QString, DatabaseGame> &games, const QHash<QString, QString> &linked,
                    const QStringList &order)
{
    WritePlan plan;
    if (!writes(mode))
        return plan;
    QSet<QString> inFile;
    for (const PgnFile::Entry &entry : entries) {
        if (entry.isGame && !entry.uid.isEmpty())
            inFile.insert(entry.uid);
    }
    if (reads(mode)) {
        // The games read from the file are tied to it by their uid.
        for (int i = 0; i < entries.size(); ++i) {
            const PgnFile::Entry &entry = entries.at(i);
            if (!entry.isGame || !entry.uid.isEmpty())
                continue;
            const QString uid = linked.value(externalId(entry));
            if (uid.isEmpty() || inFile.contains(uid))
                continue;
            plan.tags << qMakePair(i, uid);
            inFile.insert(uid);
        }
    }
    for (const QString &uid : order) {
        const auto game = games.constFind(uid);
        if (game != games.constEnd() && game->live && !inFile.contains(uid) && !base.contains(uid))
            plan.appends << uid;
    }
    return plan;
}

Base nextBase(const Base &before, const QList<PgnFile::Entry> &entries, const QHash<QString, DatabaseGame> &games,
              const QSet<QString> &pending)
{
    Base base;
    for (const PgnFile::Entry &entry : entries) {
        if (!entry.isGame || entry.uid.isEmpty() || base.contains(entry.uid))
            continue;
        const auto old = before.constFind(entry.uid);
        if (pending.contains(entry.uid) && old != before.constEnd()) {
            base.insert(entry.uid, *old);
            continue;
        }
        const auto game = games.constFind(entry.uid);
        base.insert(entry.uid, BaseEntry{entry.hash, game != games.constEnd() ? game->modified
                                                     : old != before.constEnd() ? old->modified
                                                                                : QString()});
    }
    // Gone from the file: remembered, so the game is not written back.
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        if (!base.contains(it.key()))
            base.insert(it.key(), BaseEntry{QString(), it->modified});
    }
    return base;
}

} // namespace PgnFilePlan
