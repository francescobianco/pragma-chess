#include "BoardPanel.h"

#include "BoardWidget.h"
#include "EvaluationBar.h"
#include "GameHeaderWidget.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QToolButton>

namespace {

// Gap between the evaluation bar and the board.
constexpr int kBarSpacing = 6;

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

BoardPanel::BoardPanel(BoardWidget *board, EvaluationBar *evaluationBar, GameHeaderWidget *header,
                       const Actions &actions, QWidget *parent)
    : QWidget(parent)
    , m_board(board)
    , m_evaluationBar(evaluationBar)
    , m_header(header)
    , m_controls(new QWidget(this))
{
    m_board->setParent(this);
    m_evaluationBar->setParent(this);
    m_header->setParent(this);
    m_controls->setAccessibleName(tr("Game controls"));

    auto *layout = new QHBoxLayout(m_controls);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(2);

    // Keep the navigation buttons centered under the board, with the flip
    // button at the edge; the leading spacer balances its width.
    QToolButton *flip = controlButton(actions.flip);
    layout->addSpacing(flip->sizeHint().width());
    layout->addStretch();
    for (QAction *action : {actions.first, actions.previous, actions.explain, actions.next, actions.last})
        layout->addWidget(controlButton(action));
    layout->addStretch();
    layout->addWidget(flip);
}

QSize BoardPanel::sizeHint() const
{
    const QSize board = m_board->sizeHint();
    return {m_evaluationBar->sizeHint().width() + kBarSpacing + board.width(),
            m_header->sizeHint().height() + board.height() + m_controls->sizeHint().height()};
}

QSize BoardPanel::minimumSizeHint() const
{
    const QSize board = m_board->minimumSizeHint();
    const QSize controls = m_controls->minimumSizeHint();
    return {m_evaluationBar->sizeHint().width() + kBarSpacing + qMax(board.width(), controls.width()),
            m_header->sizeHint().height() + board.height() + controls.height()};
}

bool BoardPanel::event(QEvent *event)
{
    if (event->type() == QEvent::LayoutRequest)
        layoutChildren();
    return QWidget::event(event);
}

void BoardPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutChildren();
}

int BoardPanel::widthForHeight(int height) const
{
    const int available = height - m_header->sizeHint().height() - m_controls->sizeHint().height();
    const int boardSide = BoardWidget::sideForAvailable(available);
    const int barAndGap = m_evaluationBar->sizeHint().width() + kBarSpacing;
    return barAndGap + qMax(boardSide, m_controls->minimumSizeHint().width());
}

void BoardPanel::layoutChildren()
{
    // Group: [bar][gap][header / board / controls], centered in the panel.
    const int barWidth = m_evaluationBar->sizeHint().width();
    const int headerHeight = m_header->sizeHint().height();
    const int controlsHeight = m_controls->sizeHint().height();
    const int minimumControlsWidth = m_controls->minimumSizeHint().width();
    const int side = qMax(0, qMin(width() - barWidth - kBarSpacing, height() - headerHeight - controlsHeight));
    const int controlsWidth = qMin(width() - barWidth - kBarSpacing, qMax(side, minimumControlsWidth));
    const int groupWidth = barWidth + kBarSpacing + side;
    const int left = (width() - groupWidth) / 2;
    const int top = (height() - headerHeight - side - controlsHeight) / 2;
    const int boardLeft = left + barWidth + kBarSpacing;
    const int boardTop = top + headerHeight;

    m_board->setGeometry(boardLeft, boardTop, side, side);
    // The board paints inside a small margin; align the bar and header with the squares.
    const QRect squares = m_board->boardArea().translated(boardLeft, boardTop);
    m_evaluationBar->setGeometry(squares.left() - kBarSpacing - barWidth, squares.top(), barWidth,
                                 squares.height());
    m_header->setGeometry(squares.left(), top, squares.width(), headerHeight);
    m_controls->setGeometry(boardLeft + (side - controlsWidth) / 2, top + headerHeight + side,
                            controlsWidth, controlsHeight);
}
