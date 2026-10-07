#pragma once

#include "app/Drawers.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

/// Edit ▸ Drawers…: the user's drawers, each a name and its content,
/// listed on the left and edited on the right. Nothing is kept until OK,
/// which refuses drawers without a name or with the same one (Drawers::problem).
class DrawersDialog : public QDialog {
    Q_OBJECT

public:
    explicit DrawersDialog(const Drawers &drawers, QWidget *parent = nullptr);

    Drawers drawers() const { return m_drawers; }

    void accept() override;

private:
    void fillList(int current);
    void showDrawer(int row);
    void addDrawer();
    void deleteDrawer();

    Drawers m_drawers;
    /// The drawer being edited, -1 when there is none.
    int m_current = -1;
    QListWidget *m_list;
    QPushButton *m_deleteButton;
    QLineEdit *m_name;
    QPlainTextEdit *m_content;
    QLabel *m_problem;
};
