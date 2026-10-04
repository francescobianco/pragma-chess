#include "SyncManifest.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSet>

QByteArray SyncManifest::toJson() const
{
    QJsonObject entries;
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        QJsonObject entry;
        entry.insert(QStringLiteral("sha256"), it->hash);
        entry.insert(QStringLiteral("size"), double(it->size));
        entry.insert(QStringLiteral("modified"), it->modified.toUTC().toString(Qt::ISODate));
        entry.insert(QStringLiteral("device"), it->device);
        entries.insert(it.key(), entry);
    }
    QJsonObject mergedEntries;
    for (auto it = merged.cbegin(); it != merged.cend(); ++it) {
        mergedEntries.insert(it.key(), QJsonObject{{QStringLiteral("lineage"), it->lineage},
                                                   {QStringLiteral("into"), it->into},
                                                   {QStringLiteral("intoPath"), it->intoPath},
                                                   {QStringLiteral("at"), it->at.toUTC().toString(Qt::ISODate)},
                                                   {QStringLiteral("device"), it->device}});
    }
    QJsonObject deletedEntries;
    for (auto it = deleted.cbegin(); it != deleted.cend(); ++it) {
        QJsonObject entry{{QStringLiteral("at"), it->at.toUTC().toString(Qt::ISODate)},
                          {QStringLiteral("device"), it->device}};
        if (!it->lineage.isEmpty())
            entry.insert(QStringLiteral("lineage"), it->lineage);
        if (!it->hash.isEmpty())
            entry.insert(QStringLiteral("sha256"), it->hash);
        deletedEntries.insert(it.key(), entry);
    }
    QJsonObject root;
    // Format 1 while nothing was deleted, so older versions keep syncing; they
    // would bring a deleted file back, so they are refused once one is.
    root.insert(QStringLiteral("pragma-chess-sync"), deleted.isEmpty() ? 1 : formatVersion);
    root.insert(QStringLiteral("revision"), revision);
    root.insert(QStringLiteral("updatedBy"), updatedBy);
    root.insert(QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODate));
    root.insert(QStringLiteral("files"), entries);
    // Older versions ignore it: they only put a merged file back, which the
    // next device that knows merges again.
    if (!merged.isEmpty())
        root.insert(QStringLiteral("merged"), mergedEntries);
    if (!deleted.isEmpty())
        root.insert(QStringLiteral("deleted"), deletedEntries);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<SyncManifest> SyncManifest::fromJson(const QByteArray &json, QString *errorMessage)
{
    QJsonParseError parseError;
    const QJsonObject root = QJsonDocument::fromJson(json, &parseError).object();
    if (parseError.error != QJsonParseError::NoError || !root.contains(QStringLiteral("pragma-chess-sync"))) {
        if (errorMessage)
            *errorMessage = QObject::tr("The remote %1 file is not valid.").arg(QLatin1String(fileName));
        return std::nullopt;
    }
    if (root.value(QStringLiteral("pragma-chess-sync")).toInt() > formatVersion) {
        if (errorMessage)
            *errorMessage = QObject::tr("The remote folder was synced by a newer version of Pragma Chess.");
        return std::nullopt;
    }

    SyncManifest manifest;
    manifest.revision = root.value(QStringLiteral("revision")).toInt();
    manifest.updatedBy = root.value(QStringLiteral("updatedBy")).toString();
    manifest.updatedAt = QDateTime::fromString(root.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
    const QJsonObject entries = root.value(QStringLiteral("files")).toObject();
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        SyncFileState state;
        state.hash = entry.value(QStringLiteral("sha256")).toString();
        // A tombstone written by an older version, or a broken entry: forget
        // it, so the file it mentions is uploaded again instead of removed.
        if (state.hash.isEmpty())
            continue;
        state.size = qint64(entry.value(QStringLiteral("size")).toDouble());
        state.modified = QDateTime::fromString(entry.value(QStringLiteral("modified")).toString(), Qt::ISODate);
        state.device = entry.value(QStringLiteral("device")).toString();
        manifest.files.insert(it.key(), state);
    }
    const QJsonObject mergedEntries = root.value(QStringLiteral("merged")).toObject();
    for (auto it = mergedEntries.constBegin(); it != mergedEntries.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        SyncMergeRecord record;
        record.path = it.key();
        record.lineage = entry.value(QStringLiteral("lineage")).toString();
        record.into = entry.value(QStringLiteral("into")).toString();
        record.intoPath = entry.value(QStringLiteral("intoPath")).toString();
        record.at = QDateTime::fromString(entry.value(QStringLiteral("at")).toString(), Qt::ISODate);
        record.device = entry.value(QStringLiteral("device")).toString();
        // A record that cannot say where the games went is not trusted.
        if (record.lineage.isEmpty() || record.into.isEmpty())
            continue;
        manifest.merged.insert(record.path, record);
    }
    const QJsonObject deletedEntries = root.value(QStringLiteral("deleted")).toObject();
    for (auto it = deletedEntries.constBegin(); it != deletedEntries.constEnd(); ++it) {
        const QJsonObject entry = it.value().toObject();
        SyncDeletionRecord record;
        record.path = it.key();
        record.lineage = entry.value(QStringLiteral("lineage")).toString();
        record.at = QDateTime::fromString(entry.value(QStringLiteral("at")).toString(), Qt::ISODate);
        record.device = entry.value(QStringLiteral("device")).toString();
        record.hash = entry.value(QStringLiteral("sha256")).toString();
        // Without a lineage or a content any file at that path could go: not trusted.
        if ((record.lineage.isEmpty() && record.hash.isEmpty()) || manifest.merged.contains(record.path))
            continue;
        manifest.deleted.insert(record.path, record);
    }
    return manifest;
}

QList<SyncAction> planSync(const QMap<QString, LocalFileState> &local, const QMap<QString, QString> &base,
                           const SyncManifest &remote)
{
    QSet<QString> paths;
    for (auto it = local.cbegin(); it != local.cend(); ++it)
        paths.insert(it.key());
    for (auto it = remote.files.cbegin(); it != remote.files.cend(); ++it)
        paths.insert(it.key());
    for (auto it = remote.merged.cbegin(); it != remote.merged.cend(); ++it)
        paths.insert(it.key());
    for (auto it = remote.deleted.cbegin(); it != remote.deleted.cend(); ++it)
        paths.insert(it.key());
    // `base` is deliberately not a source of paths: a file that is gone from
    // both sides is gone, and one that is gone from a single side comes back
    // from the other. Only a merge or a deletion record makes a file disappear.

    QStringList sorted = paths.values();
    sorted.sort();
    QList<SyncAction> actions;
    using Kind = SyncAction::Kind;
    for (const QString &path : std::as_const(sorted)) {
        const QString localHash = local.value(path).hash;
        const QString remoteHash = remote.files.value(path).hash;
        const QString baseHash = base.value(path);

        if (remote.merged.contains(path)) {
            // Merged into another database: its games are there, never here again.
            if (!localHash.isEmpty())
                actions << SyncAction{Kind::Merge, path};
            else if (!remoteHash.isEmpty())
                actions << SyncAction{Kind::Forget, path};
        } else if (remote.deleted.contains(path)) {
            // Deleted by a user who was told it goes from every device.
            if (!localHash.isEmpty())
                actions << SyncAction{Kind::Delete, path};
            else if (!remoteHash.isEmpty())
                actions << SyncAction{Kind::Forget, path};
        } else if (localHash == remoteHash) {
            if (!localHash.isEmpty() && baseHash != localHash)
                actions << SyncAction{Kind::Record, path};
        } else if (localHash.isEmpty()) {
            // Here at the last sync and nobody changed it since: deleted by
            // hand, the user says what happens. Otherwise only there: bring it here.
            actions << SyncAction{remoteHash == baseHash ? Kind::DeletedHere : Kind::Download, path};
        } else if (remoteHash.isEmpty()) {
            actions << SyncAction{Kind::Upload, path}; // Only here: send it there.
        } else if (localHash == baseHash) {
            actions << SyncAction{Kind::Download, path}; // Only the remote side changed it.
        } else if (remoteHash == baseHash) {
            actions << SyncAction{Kind::Upload, path}; // Only this side changed it.
        } else {
            actions << SyncAction{Kind::KeepBoth, path}; // Both changed it: keep both.
        }
    }
    return actions;
}

QStringList dropInapplicableDeletions(SyncManifest &manifest, const QMap<QString, LocalFileState> &local,
                                      const std::function<QString(const QString &path)> &lineage)
{
    QStringList dropped;
    for (auto it = manifest.deleted.begin(); it != manifest.deleted.end();) {
        const SyncDeletionRecord &record = it.value();
        bool applies = true;
        if (!record.lineage.isEmpty()) {
            // A database: whatever its content, as long as it is the same database.
            if (local.contains(it.key()) && lineage)
                applies = lineage(it.key()) == record.lineage;
        } else {
            const QString here = local.value(it.key()).hash;
            const QString there = manifest.files.value(it.key()).hash;
            applies = (here.isEmpty() || here == record.hash) && (there.isEmpty() || there == record.hash);
        }
        if (applies) {
            ++it;
            continue;
        }
        dropped << it.key();
        it = manifest.deleted.erase(it);
    }
    return dropped;
}

QString conflictPath(const QString &path, const QString &device, const QDateTime &when)
{
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    const QString directory = path.left(slash + 1);
    const QString name = path.mid(slash + 1);
    const QFileInfo info(name);
    const QString suffix = info.suffix().isEmpty() ? QString() : QLatin1Char('.') + info.suffix();
    const QString base = suffix.isEmpty() ? name : name.left(name.size() - suffix.size());
    QString safeDevice = device;
    safeDevice.replace(QLatin1Char('/'), QLatin1Char('-'));
    return QStringLiteral("%1%2 (conflict, %3, %4)%5")
        .arg(directory, base, safeDevice, when.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH.mm")), suffix);
}
