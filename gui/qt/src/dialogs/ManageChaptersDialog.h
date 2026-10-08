#pragma once

#include "app/LocalizedText.h"

#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QPushButton;

/// File ▸ Manage Chapters…: the project's chapters in their order, to
/// reorder (drag, or Move Up and Move Down), rename (double click), add and
/// delete. Deleting them all leaves the project without chapters. The titles
/// are shown and written in the language of the project's texts (Project
/// Settings), the others kept.
class ManageChaptersDialog : public QDialog {
    Q_OBJECT

public:
    struct Entry {
        /// Index of the chapter in the project, -1 for one added here.
        int source = -1;
        LocalizedText title;
        /// Games with something in them, to warn before deleting.
        int games = 0;
    };

    /// `language`: the one the texts are written in now (LocalizedText::languages).
    ManageChaptersDialog(const QList<Entry> &chapters, int current, const QString &language, QWidget *parent = nullptr);

    /// The chapters as they are to be, in order, each title with what was
    /// written in each language.
    QList<Entry> entries() const;

private:
    /// The titles of a row, with what the row shows written in `language`
    /// when it was renamed.
    LocalizedText titles(const QListWidgetItem *item, const QString &language) const;

    void updateButtons();
    /// The rows that are chapters, not the "(No Chapter)" one.
    int chapterCount() const;

    QListWidget *m_list;
    /// The language the titles are shown and written in.
    QString m_language;
    QPushButton *m_delete;
    QPushButton *m_up;
    QPushButton *m_down;
};
