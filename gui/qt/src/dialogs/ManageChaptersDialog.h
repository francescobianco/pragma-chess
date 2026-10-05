#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QListWidget;
class QPushButton;

/// File ▸ Manage Chapters…: the project's chapters in their order, to
/// reorder (drag, or Move Up and Move Down), rename (double click), add and
/// delete. Deleting them all leaves the project without chapters.
class ManageChaptersDialog : public QDialog {
    Q_OBJECT

public:
    struct Entry {
        /// Index of the chapter in the project, -1 for one added here.
        int source = -1;
        QString title;
        /// Games with something in them, to warn before deleting.
        int games = 0;
    };

    ManageChaptersDialog(const QList<Entry> &chapters, int current, QWidget *parent = nullptr);

    /// The chapters as they are to be, in order.
    QList<Entry> entries() const;

private:
    void updateButtons();
    /// The rows that are chapters, not the "(No Chapter)" one.
    int chapterCount() const;

    QListWidget *m_list;
    QPushButton *m_delete;
    QPushButton *m_up;
    QPushButton *m_down;
};
