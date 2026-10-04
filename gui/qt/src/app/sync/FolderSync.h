#pragma once

#include "SyncManifest.h"

#include <QObject>
#include <QPointer>
#include <QSet>

#include <functional>

class RemoteStore;

/// Keeps a local folder (the Pragma folder, with its databases and projects)
/// in sync with a remote folder shared by several devices.
///
/// Each sync compares the local files, the remote `.pragma-chess.sync`
/// manifest and what this device last synced (kept in `statePath`), then
/// uploads or downloads files (see planSync). The manifest is written last,
/// and only if no other device changed it meanwhile; otherwise the sync
/// starts over. Nothing is ever lost: conflicting edits keep both files, and
/// the only files that go are databases merged into another one — a
/// duplicate — whose games live on there (SyncMergeRecord), and databases
/// the user deleted knowing they go from every device (SyncDeletionRecord),
/// which go to the trash.
class FolderSync : public QObject {
    Q_OBJECT

public:
    FolderSync(const QString &localRoot, const QString &statePath, const QString &deviceName,
               QObject *parent = nullptr);
    ~FolderSync() override;

    /// The remote folder to sync with (owned by the caller); null stops syncing.
    void setStore(RemoteStore *store);
    /// Whether a server is configured, i.e. whether syncing the folder means anything.
    bool hasStore() const { return !m_store.isNull(); }

    /// What the sync needs to know about database files, which it cannot read
    /// itself (SQLite lives in the app). Without them duplicates are left
    /// alone and a merged file is kept here, but still never synced again.
    struct DatabaseHooks {
        /// DatabaseProperties::id of a database file; empty if unknown.
        std::function<QString(const QString &absolutePath)> lineage;
        /// Every game uid of a database file.
        std::function<QSet<QString>(const QString &absolutePath)> uids;
        /// Whether a relative path is where a database we ship lives (it is always kept).
        std::function<bool(const QString &relativePath)> canonical;
        /// Merges every game of `from` into `into` by uid, losing none, and gives
        /// `into` the id `lineage` if not empty. False, with an error, if it could not.
        std::function<bool(const QString &from, const QString &into, const QString &lineage, QString *error)> merge;
        /// Takes a deleted database out of the folder; empty: the system
        /// trash, or removed where there is none.
        std::function<bool(const QString &absolutePath)> discard;
    };
    void setDatabaseHooks(DatabaseHooks hooks) { m_hooks = std::move(hooks); }

    /// Merges the database files that are one database (DatabaseDedupe) into
    /// one and removes the others; the next sync tells the other devices,
    /// which merge their copies too. Returns how many files were merged.
    int mergeDuplicates();

    /// Deletes the database at `absolutePath` (under the synced folder) on
    /// every device: it goes to the trash here, and the next sync records
    /// the deletion, so the server and the other devices drop it too. False,
    /// with an error, if it has no lineage or could not be moved. Not while
    /// a sync runs.
    bool deleteDatabase(const QString &absolutePath, QString *errorMessage);

    /// Starts a sync, or runs another one right after the current one.
    void sync();
    bool isRunning() const { return m_running; }

    QDateTime lastSync() const;
    QString lastError() const;

Q_SIGNALS:
    void started();
    /// `changes` counts files transferred or deleted; `errorMessage` is empty on success.
    void finished(const QString &errorMessage, int changes);
    void progress(const QString &text);
    /// A local file is about to be replaced or deleted (e.g. to close an open database).
    void localFileAboutToChange(const QString &absolutePath);
    void localFileChanged(const QString &absolutePath);
    /// A database file was deleted here, by the user or by a sync.
    void localFileDeleted(const QString &absolutePath);
    /// A database file was merged into another and removed.
    void localFileMerged(const QString &fromAbsolutePath, const QString &intoAbsolutePath);

private:
    struct Run;

    void attempt(int round);
    /// Takes the remote lock (stores that cannot publish atomically), waiting
    /// while another device holds it; `done` gets an error or nothing.
    void acquireLock(int tries, std::function<void(const QString &error)> done);
    /// Removes the remote lock if this device still holds it, then calls `then`.
    void releaseLock(std::function<void()> then);
    void execute(std::shared_ptr<Run> run, qsizetype index);
    void commit(std::shared_ptr<Run> run);
    void finish(const QString &errorMessage, int changes);
    /// Drops the merges and deletions a published manifest now records.
    void forgetPublishedMerges(const SyncManifest &published);

    QMap<QString, LocalFileState> scanLocal();
    /// The local database with this lineage other than `except`, preferring `hint`; relative, or empty.
    QString findDatabase(const QString &lineage, const QString &hint, const QString &except) const;
    QString hashFile(const QString &absolutePath) const;
    bool discard(const QString &absolutePath) const;
    void loadState();
    void saveState() const;

    QString m_root;
    QString m_statePath;
    QString m_device;
    QPointer<RemoteStore> m_store;
    bool m_running = false;
    bool m_again = false;
    int m_generation = 0;
    /// Token written in the lock file while this device holds it.
    QByteArray m_lockToken;

    /// Content hash of each file as of the last sync, by relative path.
    QMap<QString, QString> m_base;
    /// Cached hashes: path → (size, modification time, hash).
    QMap<QString, std::tuple<qint64, qint64, QString>> m_hashes;
    QString m_storeIdentity;
    DatabaseHooks m_hooks;
    /// Merges made here that the remote manifest does not record yet, by path.
    QMap<QString, SyncMergeRecord> m_pendingMerges;
    /// Deletions made here that the remote manifest does not record yet, by path.
    QMap<QString, SyncDeletionRecord> m_pendingDeletions;
    QDateTime m_lastSync;
    QString m_lastError;
};
