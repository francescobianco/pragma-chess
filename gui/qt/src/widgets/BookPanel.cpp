#include "BookPanel.h"

#include "PaddedHeaderView.h"
#include "PaddedItemDelegate.h"
#include "platform/SymbolicIcons.h"

#include <QHeaderView>
#include <QTreeWidget>
#include <QVBoxLayout>

BookPanel::BookPanel(QWidget *parent)
    : QWidget(parent)
    , m_moves(new QTreeWidget)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_moves, 1);

    auto *header = new PaddedHeaderView(Qt::Horizontal, CellPadding::vertical, CellPadding::horizontal, m_moves);
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter); // As a tree's own header.
    m_moves->setHeader(header);
    m_moves->setColumnCount(3);
    m_moves->setHeaderLabels({tr("Move"), tr("Opening"), tr("Weight")});
    m_moves->headerItem()->setTextAlignment(0, Qt::AlignCenter); // Only the title: the moves stay left-aligned.
    m_moves->setRootIsDecorated(false);
    m_moves->setUniformRowHeights(true);
    m_moves->setItemDelegate(new PaddedItemDelegate(CellPadding::vertical, CellPadding::horizontal, m_moves));
    m_moves->setFocusPolicy(Qt::NoFocus);
    m_moves->setAccessibleName(tr("Book moves"));
    m_moves->header()->setStretchLastSection(false);
    m_moves->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_moves->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_moves->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    connect(m_moves, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (!(item->flags() & Qt::ItemIsEnabled))
            return;
        const int row = m_moves->indexOfTopLevelItem(item);
        if (row == 0)
            Q_EMIT backActivated();
        else if (row >= kFirstMoveRow && row - kFirstMoveRow < m_bookMoves.size())
            Q_EMIT moveActivated(m_bookMoves.at(row - kFirstMoveRow).move);
    });

    // Rows that do something show the hand, as links do.
    m_moves->setMouseTracking(true);
    connect(m_moves, &QTreeWidget::itemEntered, this, [this](QTreeWidgetItem *item) {
        const bool clickable = item && (item->flags() & Qt::ItemIsEnabled);
        m_moves->viewport()->setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    });

    rebuild();
}

void BookPanel::setBookName(const QString &name)
{
    m_bookName = name;
    rebuild();
}

void BookPanel::setMoves(const ChessPosition &position, const QList<PolyglotBook::Move> &moves,
                         const QList<OpeningNames::Name> &names, const QString &lastMove)
{
    m_position = position;
    m_bookMoves = moves;
    m_names = names;
    m_lastMove = lastMove;
    rebuild();
}

void BookPanel::rebuild()
{
    m_moves->clear();
    m_moves->viewport()->unsetCursor();

    // A row, not a button, that goes one level up the tree: just an arrow in the Move column.
    auto *back = new QTreeWidgetItem(m_moves);
    back->setIcon(0, SymbolicIcons::icon(QStringLiteral("go-previous")));
    if (m_lastMove.isEmpty()) {
        back->setText(1, tr("Starting position"));
        back->setFlags(Qt::NoItemFlags);
    } else {
        back->setText(1, tr("Take back %1").arg(m_lastMove));
        back->setToolTip(0, tr("Go back one move"));
        back->setToolTip(1, tr("Go back one move"));
        back->setFlags(Qt::ItemIsEnabled); // Clickable, never selected.
        QFont font = back->font(1);
        font.setItalic(true);
        back->setFont(1, font);
        back->setForeground(1, m_moves->palette().brush(QPalette::PlaceholderText));
    }

    const QList<PolyglotBook::Move> moves = m_bookName.isEmpty() ? QList<PolyglotBook::Move>() : m_bookMoves;
    int total = 0;
    for (const PolyglotBook::Move &move : moves)
        total += move.weight;
    for (qsizetype i = 0; i < moves.size(); ++i) {
        const PolyglotBook::Move &move = moves.at(i);
        auto *item = new QTreeWidgetItem(m_moves);
        item->setText(0, m_position.moveNumberText() + figurineSan(m_position.san(move.move)));
        item->setToolTip(0, tr("Play %1").arg(m_position.san(move.move)));
        const OpeningNames::Name name = m_names.value(i);
        item->setText(1, name.name);
        if (!name.isEmpty())
            item->setToolTip(1, QStringLiteral("%1 %2").arg(name.eco, name.name));
        const double share = total > 0 ? 100.0 * move.weight / total : 100.0 / moves.size();
        item->setText(2, QLocale().toString(share, 'f', 1) + QStringLiteral(" %"));
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    }

    if (moves.isEmpty()) { // A greyed row says why there are no moves.
        auto *item = new QTreeWidgetItem(m_moves);
        item->setText(0, QStringLiteral("–"));
        item->setText(1, m_bookName.isEmpty() ? tr("No book chosen: choose one from the Book menu")
                                              : tr("The position is not in the book"));
        item->setTextAlignment(0, Qt::AlignCenter);
        item->setFlags(Qt::NoItemFlags);
    }
}
