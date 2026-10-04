#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

#include <functional>

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

/// A database file that was merged into another database (the same one under
/// two files, e.g. a duplicate made by a sync): its games live on in `into`,
/// so every device may drop the file once it has merged its own copy.
struct SyncMergeRecord {
    /// Path of the merged file, relative to the synced folder.
    QString path;
    /// Lineage (DatabaseProperties::id) of the merged file: only a file with
    /// this lineage is merged; one with another id is a new database.
    QString lineage;
    /// Lineage of the database it was merged into, and where that was.
    QString into;
    QString intoPath;
    QDateTime at;
    QString device;

    bool operator==(const SyncMergeRecord &) const = default;
};

/// A file the user deleted on purpose, after being warned that it goes from
/// every device (a database a phone deleted, a file deleted by hand from the
/// Pragma folder, one deleted in Manage Files): every device that still has
/// that file moves it to the trash, and the remote copy is removed.
struct SyncDeletionRecord {
    /// Path of the deleted file, relative to the synced folder.
    QString path;
    /// Lineage of a deleted database: a file at this path with another id is
    /// a new database and syncs as any other.
    QString lineage;
    QDateTime at;
    QString device;
    /// Content of the deleted file, when it has no lineage (any file, or a
    /// database that was already gone): only a copy with this content goes;
    /// one changed since is a change the user never saw, and syncs as any other.
    QString hash = {};

    bool operator==(const SyncDeletionRecord &) const = default;
};

/// `.pragma-chess.sync`: the file in the remote folder that every device
/// syncing with it reads and updates. It lists the files and their content
/// hashes, so each device can tell what changed where since it last synced.
///
/// A sync never removes anything that has not been merged or that the user
/// did not delete knowingly, so the manifest has no tombstones, only merge
/// and deletion records: entries left over from older versions are dropped
/// when it is read, which puts the files they mention back where they belong.
/// A manifest with deletion records is written as format 2, which older
/// versions refuse to sync with rather than bring the files back.
struct SyncManifest {
    static constexpr char fileName[] = ".pragma-chess.sync";
    /// This device's own record of the folder (what it last synced, hashes),
    /// in the root of the local folder and never uploaded (see FolderSync).
    static constexpr char localStateFileName[] = ".pragma-chess.local";
    /// The user's personal settings (PersonalSettings): hidden like the
    /// state, but synced, the one hidden file that is. Changed on two
    /// devices, the newer wins: a copy beside it would never be read.
    static constexpr char personalFileName[] = ".pragma-chess.conf";
    /// Held while a device syncs with a store that cannot publish atomically.
    static constexpr char lockFileName[] = ".pragma-chess.lock";
    static constexpr int formatVersion = 2;

    /// Incremented by every sync that changes the remote folder; a device
    /// that finds a different revision than the one it planned with starts over.
    int revision = 0;
    QString updatedBy;
    QDateTime updatedAt;
    /// By path relative to the synced folder, with '/' separators.
    QMap<QString, SyncFileState> files;
    /// Files merged into another database, by path (see SyncMergeRecord).
    QMap<QString, SyncMergeRecord> merged;
    /// Files deleted by a user, by path (see SyncDeletionRecord).
    QMap<QString, SyncDeletionRecord> deleted;

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
        /// A merged file (SyncMergeRecord) still here: merge it into its
        /// database, then remove it here and, if it is there, remotely.
        Merge,
        /// A merged or deleted file only the remote folder still has: remove it there.
        Forget,
        /// A deleted file (SyncDeletionRecord) still here: move it to the
        /// trash, then remove it remotely if it is there.
        Delete,
        /// Here at the last sync, unchanged remotely since, missing now: the
        /// user deleted it by hand. Nothing moves until they say whether it
        /// goes from every device or comes back (FolderSync::deletedByHand).
        DeletedHere,
    };

    Kind kind;
    QString path;

    bool operator==(const SyncAction &) const = default;
};

/// Reconciles the two sides, file by file, and **never deletes anything that
/// has not been merged or deleted by the user**.
///
/// A folder full of databases is not a working copy: a file missing on one
/// side means that side has yet to receive it, not that it should go. So the
/// result is the union of both sides — a file only here is uploaded, a file
/// only there is downloaded — and `base`, the content both sides had when
/// this device last synced, decides who changed a file that exists on both
/// sides. When both changed it, both versions are kept.
///
/// The base also tells a file deleted by hand from one not received yet: a
/// file this device had at its last sync, that nobody changed remotely since
/// and that is missing here now was deleted here (DeletedHere), and the user
/// is asked; one changed remotely since comes back (Download).
///
/// The only way out is a merge: a path in `remote.merged` is never uploaded
/// or downloaded again; a local copy is merged into its database first
/// (Merge) and a remote one removed (Forget). A path in `remote.deleted` is
/// the same, except that the local copy goes to the trash (Delete). The
/// caller drops the records whose local file has another lineage before planning.
QList<SyncAction> planSync(const QMap<QString, LocalFileState> &local, const QMap<QString, QString> &base,
                           const SyncManifest &remote);

/// Drops the deletion records that do not apply to the file at their path
/// (from `manifest.deleted`): a record with a lineage only takes a file of
/// that lineage (`lineage` reads a local file's), one without only a copy
/// with its content, here (`local`) and on the server (`manifest.files`). A
/// file changed since it was deleted is a change its deleter never saw: it
/// stays and syncs as any other, and the record goes. Returns the paths dropped.
QStringList dropInapplicableDeletions(SyncManifest &manifest, const QMap<QString, LocalFileState> &local,
                                      const std::function<QString(const QString &path)> &lineage);

/// "Databases/Games.pdb" → "Databases/Games (conflict, laptop, 2026-09-17 10.30).pdb"
QString conflictPath(const QString &path, const QString &device, const QDateTime &when);
