#pragma once

#include "app/EngineCatalog.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

/// Engine ▸ Manage Engines…: the engines of this computer. Each one can be
/// renamed and pointed at another executable, with its threads and hash;
/// "Detect Engines" adds the UCI engines installed here that are not listed
/// yet. The bundled engine is always first and cannot be removed.
///
/// The list marks the engine the application is using (the one of the open
/// project). Selecting another one offers "Use This Engine", which switches
/// to it at once, with the engines as edited so far.
class ManageEnginesDialog : public QDialog {
    Q_OBJECT

public:
    /// @p activeId is the engine in use; @p pragmaDir is searched too (its
    /// Engines folder) when detecting.
    ManageEnginesDialog(const EngineCatalog &catalog, const QString &activeId, const QString &pragmaDir,
                        QWidget *parent = nullptr);

    /// The catalog as edited, to save when the dialog is accepted.
    const EngineCatalog &catalog() const { return m_catalog; }
    /// The engine in use: the one given, another one chosen with "Use This
    /// Engine", or the bundled one if it was removed.
    QString activeId() const { return m_activeId; }

Q_SIGNALS:
    /// "Use This Engine" was clicked: activeId() is the engine to switch to,
    /// and catalog() what it is.
    void useEngineRequested();

private:
    void rebuildList(const QString &selectId);
    /// Names in the list, the engine in use set apart, and what is under the form.
    void showActive();
    void useEngine();
    void showEngine();
    void storeEngine();
    void addEngine();
    void removeEngine();
    void browse();
    void detect();

    EngineCatalog m_catalog;
    QString m_pragmaDir;
    QString m_shownId;
    QString m_activeId;
    bool m_updating = false;

    QListWidget *m_list;
    QLineEdit *m_name;
    QLineEdit *m_path;
    QPushButton *m_browse;
    QSpinBox *m_threads;
    QSpinBox *m_hash;
    QLabel *m_status;
    QPushButton *m_use;
    QLabel *m_inUse;
    QPushButton *m_remove;
    QPushButton *m_detect;
};
