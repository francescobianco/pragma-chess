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
class ManageEnginesDialog : public QDialog {
    Q_OBJECT

public:
    /// @p pragmaDir is searched too (its Engines folder) when detecting.
    ManageEnginesDialog(const EngineCatalog &catalog, const QString &selectedId, const QString &pragmaDir,
                        QWidget *parent = nullptr);

    /// The catalog as edited, to save when the dialog is accepted.
    const EngineCatalog &catalog() const { return m_catalog; }
    /// The engine selected in the list when the dialog closed.
    QString selectedId() const;

private:
    void rebuildList(const QString &selectId);
    void showEngine();
    void storeEngine();
    void addEngine();
    void removeEngine();
    void browse();
    void detect();

    EngineCatalog m_catalog;
    QString m_pragmaDir;
    QString m_shownId;
    bool m_updating = false;

    QListWidget *m_list;
    QLineEdit *m_name;
    QLineEdit *m_path;
    QPushButton *m_browse;
    QSpinBox *m_threads;
    QSpinBox *m_hash;
    QLabel *m_status;
    QPushButton *m_remove;
    QPushButton *m_detect;
};
