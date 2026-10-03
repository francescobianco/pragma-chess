#include "BoardPanel.h"

#include "BoardSideColumn.h"
#include "BoardWidget.h"
#include "CapturedPiecesWidget.h"
#include "EvaluationBar.h"
#include "GameHeaderWidget.h"

#include <QAction>
#include <QEvent>
#include <QGridLayout>
#include <QToolButton>

namespace {

// Gap between the evaluation bar and the board, and between the board and the column at its right.
constexpr int kBarSpacing = 6;
/// Width of the turn and captured pieces column, as a share of the board.
constexpr qreal kSideColumnShare = 0.05;
constexpr int kMinimumSideColumn = 20;

/// Free space at the right of everything, so the move list does not crowd the board.
constexpr int kTrailingSpace = 10;
/// Room between the navigation and what sits at the ends of its row.
constexpr int kControlsGap = 8;

int sideColumnWidth(int boardSide)
{
    return qMax(kMinimumSideColumn, qRound(boardSide * kSideColumnShare));
}

/// Everything at the right of the board: gap, column, free space.
int rightOfBoard(int boardSide)
{
    return kBarSpacing + sideColumnWidth(boardSide) + kTrailingSpace;
}

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
                       CapturedPiecesWidget *capturedPieces, BoardSideColumn *sideColumn, const Actions &actions,
                       QWidget *parent)
    : QWidget(parent)
    , m_board(board)
    , m_evaluationBar(evaluationBar)
    , m_header(header)
    , m_capturedPieces(capturedPieces)
    , m_sideColumn(sideColumn)
    , m_controls(new QWidget(this))
{
    m_board->setParent(this);
    m_evaluationBar->setParent(this);
    m_header->setParent(this);
    m_sideColumn->setParent(this);
    m_controls->setAccessibleName(tr("Game controls"));

    // Three columns: captured pieces at the left edge (when they are shown
    // here), navigation centered under the board, the flip button at the
    // right edge. The side columns stretch equally so the navigation stays
    // centered.
    auto *layout = new QGridLayout(m_controls);
    layout->setContentsMargins(8, 4, 8, 4);
    // No spacing between the columns: the grid drops it next to an empty
    // column (the captured pieces, when they are not shown here), which put
    // the navigation off centre by half of it. The gap is in the side
    // columns' widths instead (balanceControls).
    layout->setHorizontalSpacing(0);

    auto *navigation = new QHBoxLayout;
    navigation->setSpacing(2);
    for (QAction *action : {actions.first, actions.previous, actions.explain, actions.next, actions.last})
        navigation->addWidget(controlButton(action));

    layout->addWidget(capturedPieces, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addLayout(navigation, 0, 1, Qt::AlignCenter);
    m_flipButton = controlButton(actions.flip);
    layout->addWidget(m_flipButton, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(2, 1);
    m_controlsLayout = layout;
    balanceControls();
}

void BoardPanel::setCapturedPiecesBelow(bool below)
{
    m_capturedPieces->setVisible(below);
    m_sideColumn->setShowCaptured(!below);
    balanceControls();
}

void BoardPanel::balanceControls()
{
    // Equal stretch shares only the spare room: the side columns must start
    // from the same width too, or the navigation sits off the board's middle
    // by half the flip button (or half the captured pieces).
    const int side = qMax(m_flipButton->sizeHint().width(),
                          m_capturedPieces->isVisibleTo(m_controls) ? m_capturedPieces->sizeHint().width() : 0);
    m_controlsLayout->setColumnMinimumWidth(0, side + kControlsGap);
    m_controlsLayout->setColumnMinimumWidth(2, side + kControlsGap);
}

QSize BoardPanel::sizeHint() const
{
    const QSize board = m_board->sizeHint();
    return {m_evaluationBar->sizeHint().width() + kBarSpacing + board.width() + rightOfBoard(board.width()),
            m_header->sizeHint().height() + board.height() + m_controls->sizeHint().height()};
}

QSize BoardPanel::minimumSizeHint() const
{
    const QSize board = m_board->minimumSizeHint();
    const QSize controls = m_controls->minimumSizeHint();
    return {m_evaluationBar->sizeHint().width() + kBarSpacing + qMax(board.width(), controls.width())
                + kBarSpacing + kMinimumSideColumn + kTrailingSpace,
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
    return barAndGap + qMax(boardSide, m_controls->minimumSizeHint().width()) + rightOfBoard(boardSide);
}

void BoardPanel::layoutChildren()
{
    // Group: [bar][gap][header / board / controls][gap][turn and captures][free space], centered in the panel.
    const int barWidth = m_evaluationBar->sizeHint().width();
    const int headerHeight = m_header->sizeHint().height();
    const int controlsHeight = m_controls->sizeHint().height();
    const int minimumControlsWidth = m_controls->minimumSizeHint().width();
    const int widthForBoard = width() - barWidth - 2 * kBarSpacing - kTrailingSpace;
    int side = qMax(0, qMin(qRound(widthForBoard / (1 + kSideColumnShare)), height() - headerHeight - controlsHeight));
    if (side + sideColumnWidth(side) > widthForBoard)
        side = qMax(0, widthForBoard - kMinimumSideColumn);
    const int columnWidth = sideColumnWidth(side);
    const int controlsWidth = qMin(widthForBoard - columnWidth, qMax(side, minimumControlsWidth));
    const int groupWidth = barWidth + 2 * kBarSpacing + side + columnWidth + kTrailingSpace;
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
    // Level with the outer edge of the board's frame, which sits just outside the squares.
    const int frame = BoardWidget::kFrameWidth;
    m_sideColumn->setGeometry(squares.right() + 1 + kBarSpacing, squares.top() - frame, columnWidth,
                                  squares.height() + 2 * frame);
    m_controls->setGeometry(boardLeft + (side - controlsWidth) / 2, top + headerHeight + side,
                            controlsWidth, controlsHeight);
}
