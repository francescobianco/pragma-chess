#include "CentralArea.h"

#include "BoardPanel.h"

#include <QEvent>

namespace {

/// Space between the board and sidebar and whatever is docked below them.
constexpr int kBottomPadding = 12;

} // namespace

CentralArea::CentralArea(BoardPanel *boardPanel, QWidget *sidebar, QWidget *parent)
    : QWidget(parent)
    , m_boardPanel(boardPanel)
    , m_sidebar(sidebar)
{
    m_boardPanel->setParent(this);
    m_sidebar->setParent(this);
}

QSize CentralArea::sizeHint() const
{
    const QSize board = m_boardPanel->sizeHint();
    const QSize sidebar = m_sidebar->sizeHint();
    return {board.width() + sidebar.width(), qMax(board.height(), sidebar.height()) + kBottomPadding};
}

QSize CentralArea::minimumSizeHint() const
{
    const QSize board = m_boardPanel->minimumSizeHint();
    const QSize sidebar = m_sidebar->minimumSizeHint();
    return {board.width() + sidebar.width(), qMax(board.height(), sidebar.height()) + kBottomPadding};
}

bool CentralArea::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest)
        layoutChildren();
    return QWidget::event(event);
}

void CentralArea::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutChildren();
}

void CentralArea::layoutChildren()
{
    // The board commands: as wide as its height allows, while leaving the
    // sidebar at least its minimum width.
    const int contentHeight = qMax(0, height() - kBottomPadding);
    const int sidebarMinimum = qMax(0, m_sidebar->minimumSizeHint().width());
    const int boardWidth = qBound(0, m_boardPanel->widthForHeight(contentHeight), qMax(0, width() - sidebarMinimum));
    m_boardPanel->setGeometry(0, 0, boardWidth, contentHeight);
    m_sidebar->setGeometry(boardWidth, 0, width() - boardWidth, contentHeight);
}
