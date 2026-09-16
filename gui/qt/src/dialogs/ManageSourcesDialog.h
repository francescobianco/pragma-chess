#pragma once

#include <QDialog>

class GameDatabase;
class QPushButton;
class QTreeWidget;
class SourceSync;

/// The sources connected to a database: their state and last sync, and the
/// commands to sync now, edit, sign in again or remove them.
class ManageSourcesDialog : public QDialog {
    Q_OBJECT

public:
    ManageSourcesDialog(GameDatabase *database, SourceSync *sync, QWidget *parent = nullptr);

Q_SIGNALS:
    /// The user asked to connect a new source.
    void connectRequested();

private:
    void reload();
    void updateButtons();
    qint64 selectedSourceId() const;
    void editSource();
    void signInAgain();
    void removeSource();

    GameDatabase *m_database;
    SourceSync *m_sync;
    QTreeWidget *m_list;
    QPushButton *m_syncButton;
    QPushButton *m_editButton;
    QPushButton *m_signInButton;
    QPushButton *m_removeButton;
};
