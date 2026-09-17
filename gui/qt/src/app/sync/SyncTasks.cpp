#include "SyncTasks.h"

#include "FolderSync.h"
#include "app/sources/SourceSync.h"

SourceSyncTask::SourceSyncTask(SourceSync *sources, QObject *parent)
    : SyncTask(tr("Sources"), parent)
    , m_sources(sources)
{
}

bool SourceSyncTask::isNeeded() const
{
    return !m_sources.isNull() && m_sources->hasSources();
}

void SourceSyncTask::run()
{
    if (m_sources.isNull()) {
        Q_EMIT finished(QString());
        return;
    }
    connect(m_sources, &SourceSync::activityChanged, this, [this](const QString &text) {
        if (!text.isEmpty())
            Q_EMIT progress(text);
    });
    // syncAll() may empty the queue before it returns, so listen first.
    connect(m_sources, &SourceSync::idle, this, [this] {
        m_sources->disconnect(this);
        Q_EMIT finished(QString()); // Each source records its own error in the database.
    });
    m_sources->syncAll();
}

FolderSyncTask::FolderSyncTask(FolderSync *folder, QObject *parent)
    : SyncTask(tr("Folder"), parent)
    , m_folder(folder)
{
}

bool FolderSyncTask::isNeeded() const
{
    return !m_folder.isNull() && m_folder->hasStore();
}

void FolderSyncTask::run()
{
    if (m_folder.isNull()) {
        Q_EMIT finished(QString());
        return;
    }
    connect(m_folder, &FolderSync::progress, this, &FolderSyncTask::progress);
    connect(m_folder, &FolderSync::finished, this, [this](const QString &errorMessage, int) {
        m_folder->disconnect(this);
        Q_EMIT finished(errorMessage);
    });
    m_folder->sync();
}
