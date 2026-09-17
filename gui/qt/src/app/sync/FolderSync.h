#pragma once

#include "SyncManifest.h"

#include <QObject>
#include <QPointer>

class RemoteStore;

/// Keeps a local folder (the Pragma folder, with its databases and projects)
/// in sync with a remote folder shared by several devices.
///
/// Each sync compares the local files, the remote `.pragma-chess.sync`
/// manifest and what this device last synced (kept in `statePath`), then
/// uploads, downloads or deletes files (see planSync). The manifest is written
/// last, and only if no other device changed it meanwhile; otherwise the sync
/// starts over. Nothing is ever lost: conflicting edits keep both files and
/// deleted files go to the trash.
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

    QMap<QString, LocalFileState> scanLocal();
    QString hashFile(const QString &absolutePath) const;
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
    QDateTime m_lastSync;
    QString m_lastError;
};
