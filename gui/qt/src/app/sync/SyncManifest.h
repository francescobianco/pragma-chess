#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

#include <optional>

/// A file as the remote folder knows it.
struct SyncFileState {
    /// SHA-256 of the content, hex.
    QString hash;
    qint64 size = 0;
    QDateTime modified;
    /// Device that uploaded it last.
    QString device;

    bool operator==(const SyncFileState &) const = default;
};

/// `.pragma-chess.sync`: the file in the remote folder that every device
/// syncing with it reads and updates. It lists the files and their content
/// hashes, so each device can tell what changed where since it last synced.
///
/// A sync never removes anything, so the manifest has no tombstones: entries
/// left over from older versions are dropped when it is read, which puts the
/// files they mention back where they belong.
struct SyncManifest {
    static constexpr char fileName[] = ".pragma-chess.sync";
    /// Held while a device syncs with a store that cannot publish atomically.
    static constexpr char lockFileName[] = ".pragma-chess.lock";
    static constexpr int formatVersion = 1;

    /// Incremented by every sync that changes the remote folder; a device
    /// that finds a different revision than the one it planned with starts over.
    int revision = 0;
    QString updatedBy;
    QDateTime updatedAt;
    /// By path relative to the synced folder, with '/' separators.
    QMap<QString, SyncFileState> files;

    QByteArray toJson() const;
    static std::optional<SyncManifest> fromJson(const QByteArray &json, QString *errorMessage);
};

/// A local file of the synced folder.
struct LocalFileState {
    QString hash;
    qint64 size = 0;
    QDateTime modified;
};

/// Something a sync has to do for one path.
struct SyncAction {
    enum class Kind {
        /// Local changes (or a new local file) go to the remote folder.
        Upload,
        /// Remote changes (or a new remote file) come to this device.
        Download,
        /// Changed on both sides: the local file stays and is uploaded, the
        /// remote version is kept beside it under a conflict name.
        KeepBoth,
        /// Same content on both sides; only this device's record is updated.
        Record,
    };

    Kind kind;
    QString path;

    bool operator==(const SyncAction &) const = default;
};

/// Reconciles the two sides, file by file, and **never deletes anything**.
///
/// A folder full of databases is not a working copy: a file missing on one
/// side means that side has yet to receive it, not that it should go. So the
/// result is the union of both sides — a file only here is uploaded, a file
/// only there is downloaded (a database deleted by hand comes back on the
/// next sync) — and `base`, the content both sides had when this device last
/// synced, only decides who changed a file that exists on both sides. When
/// both changed it, both versions are kept.
QList<SyncAction> planSync(const QMap<QString, LocalFileState> &local, const QMap<QString, QString> &base,
                           const SyncManifest &remote);

/// "Databases/Games.pdb" → "Databases/Games (conflict, laptop, 2026-09-17 10.30).pdb"
QString conflictPath(const QString &path, const QString &device, const QDateTime &when);
