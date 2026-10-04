#pragma once

#include "app/UserFolders.h"

#include <QDialog>

class QFormLayout;
class QLineEdit;

/// Options ▸ Folder Settings…: where this computer keeps the Pragma folder and,
/// if the user wants them elsewhere, the databases, projects, books and
/// opening names. An empty field keeps the usual place.
class FolderSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit FolderSettingsDialog(const UserFolders::FolderChoice &choice, QWidget *parent = nullptr);

    UserFolders::FolderChoice choice() const;

private:
    QLineEdit *addFolder(QFormLayout *form, const QString &label, const QString &value);
    /// Shows in each empty field where the folder is with the others as they are.
    void updatePlaceholders();

    QLineEdit *m_pragma;
    QLineEdit *m_databases;
    QLineEdit *m_projects;
    QLineEdit *m_books;
    QLineEdit *m_openingNames;
};
