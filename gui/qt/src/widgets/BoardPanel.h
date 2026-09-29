#pragma once

#include <QWidget>

class BoardSideColumn;
class BoardWidget;
class CapturedPiecesWidget;
class EvaluationBar;
class GameHeaderWidget;
class QAction;
class QToolButton;

/// The board with the game header above, the evaluation bar on its left, the
/// turn (and the captured pieces, by default) on its right and the game
/// controls directly underneath (captured pieces at the far left, when Board
/// Settings puts them there). The board stays
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
               CapturedPiecesWidget *capturedPieces, BoardSideColumn *sideColumn, const Actions &actions,
               QWidget *parent = nullptr);

    /// Captured pieces under the board, at the left of the controls, instead of beside it.
    void setCapturedPiecesBelow(bool below);

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
    CapturedPiecesWidget *m_capturedPieces;
    BoardSideColumn *m_sideColumn;
    QWidget *m_controls;
};
