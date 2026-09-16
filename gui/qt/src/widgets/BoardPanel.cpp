#include "BoardPanel.h"

#include "BoardWidget.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QToolButton>

namespace {

QToolButton *controlButton(QAction *action)
{
    auto *button = new QToolButton;
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(20, 20));
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

} // namespace

BoardPanel::BoardPanel(BoardWidget *board, const Actions &actions, QWidget *parent)
    : QWidget(parent)
    , m_board(board)
    , m_controls(new QWidget(this))
{
    m_board->setParent(this);
    m_controls->setAccessibleName(tr("Game controls"));

    auto *layout = new QHBoxLayout(m_controls);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    // Keep the navigation buttons centered under the board, with the flip
    // button at the edge; the leading spacer balances its width.
    QToolButton *flip = controlButton(actions.flip);
    layout->addSpacing(flip->sizeHint().width());
    layout->addStretch();
    for (QAction *action : {actions.first, actions.previous, actions.next, actions.last})
        layout->addWidget(controlButton(action));
    layout->addStretch();
    layout->addWidget(flip);
}

QSize BoardPanel::sizeHint() const
{
    const QSize board = m_board->sizeHint();
    return {board.width(), board.height() + m_controls->sizeHint().height()};
}

QSize BoardPanel::minimumSizeHint() const
{
    const QSize board = m_board->minimumSizeHint();
    const QSize controls = m_controls->minimumSizeHint();
    return {qMax(board.width(), controls.width()), board.height() + controls.height()};
}

bool BoardPanel::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest) {
        constrainWidth();
        layoutChildren();
    }
    return QWidget::event(event);
}

void BoardPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    constrainWidth();
    layoutChildren();
}

void BoardPanel::constrainWidth()
{
    // The panel is never wider than the board can be tall. As the central
    // widget of the main window, the space it cannot take goes to the docks.
    const int maxWidth = qMax(minimumSizeHint().width(), height() - m_controls->sizeHint().height());
    if (maximumWidth() != maxWidth)
        setMaximumWidth(maxWidth);
}

void BoardPanel::layoutChildren()
{
    const int controlsHeight = m_controls->sizeHint().height();
    const int minimumWidth = m_controls->minimumSizeHint().width();
    const int side = qMax(0, qMin(width(), height() - controlsHeight));
    const int controlsWidth = qMin(width(), qMax(side, minimumWidth));
    const int top = (height() - side - controlsHeight) / 2;

    m_board->setGeometry((width() - side) / 2, top, side, side);
    m_controls->setGeometry((width() - controlsWidth) / 2, top + side, controlsWidth, controlsHeight);
}
