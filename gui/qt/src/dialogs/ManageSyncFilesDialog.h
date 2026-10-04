#pragma once

#include <QDialog>

class FolderSync;
class QLabel;
class QPushButton;
class QTreeWidget;

/// Manage Files, from Options ▸ Sync Settings: the files in the folder on the
/// server, which can only be deleted — from every synced device, after a
/// warning, so a file nobody wants any more stops travelling between them.
class ManageSyncFilesDialog : public QDialog {
    Q_OBJECT

public:
    ManageSyncFilesDialog(FolderSync *sync, QWidget *parent = nullptr);

private:
    void load();
    void deleteSelected();
    void setBusy(bool busy, const QString &status);
    void updateButtons();

    FolderSync *m_sync;
    QTreeWidget *m_files;
    QLabel *m_status;
    QPushButton *m_deleteButton;
    QPushButton *m_refreshButton;
    /// Waiting for a sync to finish before listing again.
    bool m_waiting = false;
    bool m_busy = false;
};
