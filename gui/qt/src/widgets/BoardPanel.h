#pragma once

#include <QWidget>

class BoardWidget;
class QAction;
class QToolButton;

/// The board with its game controls directly underneath. The board stays
/// square and the control bar always matches its width, whatever the space.
/// The panel never grows wider than the board's available height, so the
/// surrounding docks take up any extra horizontal space.
class BoardPanel : public QWidget {
    Q_OBJECT

public:
    struct Actions {
        QAction *first;
        QAction *previous;
        QAction *next;
        QAction *last;
        QAction *flip;
    };

    BoardPanel(BoardWidget *board, const Actions &actions, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void constrainWidth();
    void layoutChildren();

    BoardWidget *m_board;
    QWidget *m_controls;
};
