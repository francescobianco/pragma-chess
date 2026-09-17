#pragma once

#include <QList>
#include <QObject>

class GameDatabase;
class QNetworkAccessManager;
class QTimer;
class SourceFetch;

/// Keeps the sources of the open database in sync, in the background: all of
/// them when the database is opened and periodically after that, one source
/// at a time. New games are appended to the database as they arrive.
class SourceSync : public QObject {
    Q_OBJECT

public:
    explicit SourceSync(QObject *parent = nullptr);
    ~SourceSync() override;

    /// Stops syncing the previous database and starts syncing this one (may be null).
    void setDatabase(GameDatabase *database);

    void syncAll();
    void syncSource(qint64 sourceId);
    /// Stops the sync of a source, e.g. before removing it.
    void cancelSource(qint64 sourceId);
    bool isSyncing() const { return m_fetch != nullptr; }
    /// Whether the open database has any source to sync at all.
    bool hasSources() const;
    /// Source being synced, or 0.
    qint64 currentSource() const { return m_current; }

Q_SIGNALS:
    /// `count` games were appended to the database.
    void gamesImported(int count);
    /// A source's state, error or sync time changed.
    void sourcesChanged();
    /// What is being synced, for the status bar; empty when idle.
    void activityChanged(const QString &text);
    /// Every queued source has been synced. A manual sync waits for this.
    void idle();

private:
    void startNext();
    void finishCurrent(const QString &errorMessage);

    GameDatabase *m_database = nullptr;
    QNetworkAccessManager *m_network;
    QTimer *m_periodic;
    QList<qint64> m_queue;
    SourceFetch *m_fetch = nullptr;
    qint64 m_current = 0;
    int m_importedThisSync = 0;
};
