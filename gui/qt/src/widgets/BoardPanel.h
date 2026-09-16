#pragma once

#include <QWidget>

class BoardWidget;
class EvaluationBar;
class GameHeaderWidget;
class QAction;
class QToolButton;

/// The board with the game header above, the evaluation bar on its left and
/// the game controls directly underneath. The board stays
/// square and the control bar always matches its width, whatever the space.
/// Its ideal width follows the height available to the board (see
/// widthForHeight), so the board fills the panel without empty space.
class BoardPanel : public QWidget {
    Q_OBJECT

public:
    struct Actions {
        QAction *first;
        QAction *previous;
        /// Sits between previous and next: explaining a move is part of stepping through a game.
        QAction *explain;
        QAction *next;
        QAction *last;
        QAction *flip;
    };

    BoardPanel(BoardWidget *board, EvaluationBar *evaluationBar, GameHeaderWidget *header,
               const Actions &actions, QWidget *parent = nullptr);

    /// Width at which the board fills the panel for the given height.
    int widthForHeight(int height) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void layoutChildren();

    BoardWidget *m_board;
    EvaluationBar *m_evaluationBar;
    GameHeaderWidget *m_header;
    QWidget *m_controls;
};
