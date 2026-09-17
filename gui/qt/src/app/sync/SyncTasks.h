#pragma once

#include "SyncPipeline.h"

#include <QPointer>

class FolderSync;
class SourceSync;

/// Brings down the games of the sources connected to the open database.
class SourceSyncTask : public SyncTask {
    Q_OBJECT

public:
    explicit SourceSyncTask(SourceSync *sources, QObject *parent = nullptr);

    bool isNeeded() const override;
    void run() override;

private:
    QPointer<SourceSync> m_sources;
};

/// Sends the Pragma folder to the configured server (FTP, WebDAV or Git) and
/// brings down what the other devices changed.
class FolderSyncTask : public SyncTask {
    Q_OBJECT

public:
    explicit FolderSyncTask(FolderSync *folder, QObject *parent = nullptr);

    bool isNeeded() const override;
    void run() override;

private:
    QPointer<FolderSync> m_folder;
};
