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
        if (it->deleted) {
            entry.insert(QStringLiteral("deleted"), true);
        } else {
            entry.insert(QStringLiteral("sha256"), it->hash);
            entry.insert(QStringLiteral("size"), double(it->size));
        }
        entry.insert(QStringLiteral("modified"), it->modified.toUTC().toString(Qt::ISODate));
        entry.insert(QStringLiteral("device"), it->device);
        entries.insert(it.key(), entry);
    }
    QJsonObject root;
    root.insert(QStringLiteral("pragma-chess-sync"), formatVersion);
    root.insert(QStringLiteral("revision"), revision);
    root.insert(QStringLiteral("updatedBy"), updatedBy);
    root.insert(QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODate));
    root.insert(QStringLiteral("files"), entries);
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
        state.deleted = entry.value(QStringLiteral("deleted")).toBool();
        state.hash = state.deleted ? QString() : entry.value(QStringLiteral("sha256")).toString();
        state.size = qint64(entry.value(QStringLiteral("size")).toDouble());
        state.modified = QDateTime::fromString(entry.value(QStringLiteral("modified")).toString(), Qt::ISODate);
        state.device = entry.value(QStringLiteral("device")).toString();
        manifest.files.insert(it.key(), state);
    }
    return manifest;
}

QList<SyncAction> planSync(const QMap<QString, LocalFileState> &local, const QMap<QString, QString> &base,
                           const SyncManifest &remote)
{
    QSet<QString> paths;
    for (auto it = local.cbegin(); it != local.cend(); ++it)
        paths.insert(it.key());
    for (auto it = base.cbegin(); it != base.cend(); ++it)
        paths.insert(it.key());
    for (auto it = remote.files.cbegin(); it != remote.files.cend(); ++it)
        paths.insert(it.key());

    QStringList sorted = paths.values();
    sorted.sort();
    QList<SyncAction> actions;
    for (const QString &path : std::as_const(sorted)) {
        const QString localHash = local.value(path).hash;
        const QString remoteHash = remote.files.value(path).hash; // Empty when absent or deleted.
        const QString baseHash = base.value(path);

        if (localHash == remoteHash) {
            if (!localHash.isEmpty() && baseHash != localHash)
                actions << SyncAction{SyncAction::Kind::Record, path};
            continue;
        }
        using Kind = SyncAction::Kind;
        const bool remoteDeleted = remote.files.contains(path) && remote.files.value(path).deleted;
        if (localHash == baseHash && remoteHash.isEmpty() && !remoteDeleted) {
            // Missing from the manifest without a deletion record: another device's
            // manifest replaced ours. Never delete on that; put the file back.
            actions << SyncAction{Kind::Upload, path};
        } else if (localHash == baseHash) // Only the remote side changed.
            actions << SyncAction{remoteHash.isEmpty() ? Kind::DeleteLocal : Kind::Download, path};
        else if (remoteHash == baseHash) // Only this side changed.
            actions << SyncAction{localHash.isEmpty() ? Kind::DeleteRemote : Kind::Upload, path};
        else if (localHash.isEmpty()) // Deleted here, edited there: keep the edit.
            actions << SyncAction{Kind::Download, path};
        else if (remoteHash.isEmpty()) // Edited here, deleted there: keep the edit.
            actions << SyncAction{Kind::Upload, path};
        else
            actions << SyncAction{Kind::KeepBoth, path};
    }
    return actions;
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
