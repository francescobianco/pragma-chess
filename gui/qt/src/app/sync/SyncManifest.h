#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

#include <optional>

/// A file as the remote folder knows it.
struct SyncFileState {
    /// SHA-256 of the content, hex; empty for a deleted file.
    QString hash;
    qint64 size = 0;
    QDateTime modified;
    /// Device that uploaded (or deleted) it last.
    QString device;
    /// Tombstone: the file was deleted and should be deleted everywhere.
    bool deleted = false;

    bool operator==(const SyncFileState &) const = default;
};

/// `.pragma-chess.sync`: the file in the remote folder that every device
/// syncing with it reads and updates. It lists the files and their content
/// hashes, so each device can tell what changed where since it last synced.
struct SyncManifest {
    static constexpr char fileName[] = ".pragma-chess.sync";
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
        /// Deleted here, so deleted remotely.
        DeleteRemote,
        /// Deleted remotely, so deleted here (moved to the trash).
        DeleteLocal,
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

/// Decides what to do by comparing, for each path, the local content, the
/// remote content and the content both had when this device last synced
/// (`base`). A side that still has the base content did not change it; when
/// both changed, nothing is lost: edits win over deletions and different
/// edits keep both files.
QList<SyncAction> planSync(const QMap<QString, LocalFileState> &local, const QMap<QString, QString> &base,
                           const SyncManifest &remote);

/// "Databases/Games.pdb" → "Databases/Games (conflict, laptop, 2026-09-17 10.30).pdb"
QString conflictPath(const QString &path, const QString &device, const QDateTime &when);
