#pragma once

#include "app/sync/SyncSettings.h"

#include <QDialog>

class FolderSync;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;

/// File ▸ Sync: the server folder (FTP or WebDAV) that keeps the Pragma folder
/// (databases and projects) the same on every computer set up with it.
class SyncDialog : public QDialog {
    Q_OBJECT

public:
    SyncDialog(const SyncSettings &settings, FolderSync *sync, QWidget *parent = nullptr);

    SyncSettings settings() const;

Q_SIGNALS:
    /// Sync now with the settings shown (they are applied first).
    void syncRequested(const SyncSettings &settings);

private:
    void updateStatus();
    void testConnection();
    void updateFields();

    FolderSync *m_sync;
    QComboBox *m_service;
    QStackedWidget *m_pages;
    QLineEdit *m_host;
    QSpinBox *m_port;
    QCheckBox *m_tls;
    QLineEdit *m_folder;
    QLineEdit *m_url;
    QLineEdit *m_repository;
    QLineEdit *m_branch;
    QLabel *m_passwordLabel;
    QLabel *m_note;
    QLineEdit *m_user;
    QLineEdit *m_password;
    QLabel *m_status;
    QPushButton *m_testButton;
    QPushButton *m_syncButton;
};
