#pragma once

#include <QWidget>

class BoardPanel;

/// Central area of the main window: the board on the left and the sidebar
/// (moves, engine, opening tree) on the right.
///
/// Both are laid out together in a single pass: the board takes exactly the
/// width its height allows and the sidebar takes the rest. There is no
/// splitter between them; resizing the space around the area (e.g. the games
/// list below) resizes the board and the sidebar at the same time.
class CentralArea : public QWidget {
    Q_OBJECT

public:
    CentralArea(BoardPanel *boardPanel, QWidget *sidebar, QWidget *parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void layoutChildren();

    BoardPanel *m_boardPanel;
    QWidget *m_sidebar;
};
