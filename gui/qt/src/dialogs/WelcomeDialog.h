#pragma once

#include <QDialog>

class QCheckBox;
class QFileSystemWatcher;
class QListWidget;
class QListWidgetItem;

/// The window that greets the user at startup (and from Help ▸ Welcome):
/// a shoulder with a picture, the logo and the name, then what Pragma Chess
/// does and a way in — the projects and the databases of the user's folders,
/// read from disk each time they change, and New Project. A click on one
/// asks the main window to open it and closes the welcome.
class WelcomeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WelcomeDialog(QWidget *parent = nullptr);

    /// Whether the window opens at startup (QSettings, per computer).
    static bool showsAtStartup();

Q_SIGNALS:
    void projectChosen(const QString &path);
    /// A database, to open in a new project.
    void databaseChosen(const QString &path);
    void newProjectChosen();

private:
    void fillProjects();
    void fillDatabases();
    void choose(QListWidgetItem *item, bool project);

    QListWidget *m_projects;
    QListWidget *m_databases;
    QCheckBox *m_dontShow;
    QFileSystemWatcher *m_watcher;
};
