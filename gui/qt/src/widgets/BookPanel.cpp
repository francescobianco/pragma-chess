#include "BookPanel.h"

#include "FigurineFont.h"
#include "PaddedHeaderView.h"
#include "PaddedItemDelegate.h"
#include "platform/SymbolicIcons.h"

#include <QHash>
#include <QHeaderView>
#include <QMenu>
#include <QTreeWidget>
#include <QVariantAnimation>
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
    m_moves->setFont(FigurineFont::apply(m_moves->font())); // The figurines of the move list.
    m_glow = new QVariantAnimation(this);
    m_glow->setDuration(1800);
    m_glow->setStartValue(0.0);
    m_glow->setEndValue(1.0);
    connect(m_glow, &QVariantAnimation::valueChanged, this, &BookPanel::paintGlow);
    connect(m_glow, &QVariantAnimation::finished, this, &BookPanel::paintGlow);
    m_moves->setColumnCount(kColumns);
    m_moves->setHeaderLabels({QString(), tr("Move"), tr("Opening"), tr("Database"), tr("Weight")});
    m_moves->headerItem()->setToolTip(kDatabaseColumn, tr("Games of the open database with the position after the move: "
                                                          "how many, and how many White won, drew and Black won"));
    m_moves->headerItem()->setTextAlignment(kMoveColumn, Qt::AlignCenter); // Only the title: the moves stay left-aligned.
    m_moves->setRootIsDecorated(false);
    m_moves->setUniformRowHeights(true);
    m_moves->setItemDelegate(new PaddedItemDelegate(CellPadding::vertical, CellPadding::horizontal, m_moves));
    m_moves->setFocusPolicy(Qt::NoFocus);
    m_moves->setAccessibleName(tr("Book moves"));
    m_moves->header()->setStretchLastSection(false);
    m_moves->header()->setSectionResizeMode(kMarkColumn, QHeaderView::Fixed);
    m_moves->header()->resizeSection(kMarkColumn, m_moves->fontMetrics().horizontalAdvance(QStringLiteral("↑")) + 2 * CellPadding::horizontal);
    m_moves->header()->setSectionResizeMode(kMoveColumn, QHeaderView::ResizeToContents);
    m_moves->header()->setSectionResizeMode(kNameColumn, QHeaderView::Stretch);
    m_moves->header()->setSectionResizeMode(kDatabaseColumn, QHeaderView::ResizeToContents);
    m_moves->header()->setSectionResizeMode(kWeightColumn, QHeaderView::ResizeToContents);
    connect(m_moves, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (!(item->flags() & Qt::ItemIsEnabled))
            return;
        const int row = m_moves->indexOfTopLevelItem(item);
        if (row == 0)
            Q_EMIT backActivated();
        else if (row >= kFirstMoveRow && row - kFirstMoveRow < m_bookMoves.size())
            Q_EMIT moveActivated(m_bookMoves.at(row - kFirstMoveRow).move);
    });

    // Right click: the repertoire, the moves the user plays whatever the book weighs.
    m_moves->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_moves, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QTreeWidgetItem *item = m_moves->itemAt(point);
        const int row = item ? m_moves->indexOfTopLevelItem(item) - kFirstMoveRow : -1;
        if (!item || !(item->flags() & Qt::ItemIsEnabled) || row < 0 || row >= m_bookMoves.size())
            return;
        const PolyglotBook::Move move = m_bookMoves.at(row);
        QMenu menu(this);
        QAction *toggle = menu.addAction(move.inRepertoire() ? tr("Remove from Repertoire")
                                                             : tr("Add to Repertoire"));
        // The weight: by a share of its own, so the heavy moves move most;
        // a move at zero can grow but not shrink.
        QMenu *weight = menu.addMenu(tr("Adjust &Weight"));
        QHash<QAction *, int> percents;
        for (const int percent : {25, 10, 5, -5, -10, -25}) {
            if (percent == -5)
                weight->addSeparator();
            QAction *action = weight->addAction(QStringLiteral("%1%2%").arg(percent > 0 ? QStringLiteral("+") : QStringLiteral("−")).arg(qAbs(percent)));
            action->setEnabled(percent > 0 || move.weight > 0);
            percents.insert(action, percent);
        }
        weight->addSeparator();
        QAction *zero = weight->addAction(tr("&Zero Weight"));
        zero->setToolTip(tr("Takes the move's weight and gives it to the other moves, in proportion"));
        zero->setEnabled(move.weight > 0);
        QAction *chosen = menu.exec(m_moves->viewport()->mapToGlobal(point));
        if (chosen == toggle)
            Q_EMIT repertoireToggled(move.move, !move.inRepertoire());
        else if (chosen == zero || (chosen && percents.contains(chosen))) {
            m_pendingMark = Mark{move.move, row, 0};
            Q_EMIT weightAdjustRequested(move.move, chosen == zero ? 0 : percents.value(chosen));
        }
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
    const bool samePosition = PolyglotBook::key(position) == PolyglotBook::key(m_position);
    m_position = position;
    m_bookMoves = moves;
    m_names = names;
    m_lastMove = lastMove;
    // The move whose weight changed: where it went in the new order, with a
    // glow that fades, so the eye finds it again. The mark goes with the position.
    if (!samePosition)
        m_mark.reset();
    if (m_pendingMark && samePosition) {
        for (int i = 0; i < moves.size(); ++i) {
            if (moves.at(i).move == m_pendingMark->move) {
                m_mark = Mark{m_pendingMark->move, i, m_pendingMark->row - i};
                m_glow->stop();
                m_glow->start();
            }
        }
    }
    m_pendingMark.reset();
    rebuild();
}

void BookPanel::paintGlow()
{
    if (!m_mark)
        return;
    QTreeWidgetItem *item = m_moves->topLevelItem(m_mark->row + kFirstMoveRow);
    if (!item)
        return;
    QColor glow = palette().color(QPalette::Highlight);
    glow.setAlphaF(0.45f * (1.0f - float(m_glow->currentTime()) / float(m_glow->duration())));
    const QBrush brush = m_glow->state() == QAbstractAnimation::Running ? QBrush(glow) : QBrush();
    for (int column = 0; column < m_moves->columnCount(); ++column)
        item->setBackground(column, brush);
}

void BookPanel::setDatabaseStats(DatabaseState state, const QList<PositionIndex::Stats> &stats)
{
    if (state == m_databaseState && stats == m_stats)
        return;
    m_databaseState = state;
    m_stats = stats;
    rebuild();
}

void BookPanel::rebuild()
{
    m_moves->clear();
    m_moves->viewport()->unsetCursor();

    // A row, not a button, that goes one level up the tree: just an arrow in the Move column.
    auto *back = new QTreeWidgetItem(m_moves);
    back->setTextAlignment(kMoveColumn, Qt::AlignCenter); // The arrow sits in the middle of its cell.
    if (m_lastMove.isEmpty()) {
        // Nothing to take back: the row says where we are, with no arrow.
        back->setText(kNameColumn, tr("Starting position"));
        back->setFlags(Qt::NoItemFlags);
    } else {
        back->setIcon(kMoveColumn, SymbolicIcons::icon(QStringLiteral("go-previous")));
        back->setText(kNameColumn, tr("Take back %1").arg(m_lastMove));
        back->setToolTip(kMoveColumn, tr("Go back one move"));
        back->setToolTip(kNameColumn, tr("Go back one move"));
        back->setFlags(Qt::ItemIsEnabled); // Clickable, never selected.
        QFont font = back->font(kNameColumn);
        font.setItalic(true);
        back->setFont(kNameColumn, font);
        back->setForeground(kNameColumn, m_moves->palette().brush(QPalette::PlaceholderText));
    }

    const QList<PolyglotBook::Move> moves = m_bookName.isEmpty() ? QList<PolyglotBook::Move>() : m_bookMoves;
    int total = 0;
    for (const PolyglotBook::Move &move : moves)
        total += move.weight;
    for (qsizetype i = 0; i < moves.size(); ++i) {
        const PolyglotBook::Move &move = moves.at(i);
        auto *item = new QTreeWidgetItem(m_moves);
        item->setText(kMoveColumn, m_position.moveNumberText() + figurineSan(m_position.san(move.move)));
        item->setToolTip(kMoveColumn, tr("Play %1").arg(m_position.san(move.move)));
        const OpeningNames::Name name = m_names.value(i);
        item->setText(kNameColumn, name.name);
        if (!name.isEmpty())
            item->setToolTip(kNameColumn, QStringLiteral("%1 %2").arg(name.eco, name.name));
        const double share = total > 0 ? 100.0 * move.weight / total : 100.0 / moves.size();
        // A move with next to nothing is not at zero: say so rather than show "0.0 %".
        item->setText(kWeightColumn, (share > 0 && share < 0.05 ? QStringLiteral("< 0.1") : QLocale().toString(share, 'f', 1))
                                         + QStringLiteral(" %"));
        item->setTextAlignment(kWeightColumn, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(kDatabaseColumn, Qt::AlignRight | Qt::AlignVCenter);
        if (m_databaseState == DatabaseState::Indexing) {
            item->setText(kDatabaseColumn, QStringLiteral("…"));
        } else if (m_databaseState == DatabaseState::Ready && i < m_stats.size()) {
            const PositionIndex::Stats &stats = m_stats.at(i);
            if (stats.games == 0) {
                item->setText(kDatabaseColumn, QStringLiteral("–"));
            } else {
                // Games, then White wins / draws / Black wins, as in chess databases.
                const QLocale locale;
                item->setText(kDatabaseColumn, QStringLiteral("%1 (%2/%3/%4)")
                                                   .arg(locale.toString(stats.games), locale.toString(stats.whiteWins),
                                                        locale.toString(stats.draws), locale.toString(stats.blackWins)));
                item->setToolTip(kDatabaseColumn,
                                 tr("%n game(s): White won %1, %2 drawn, Black won %3", nullptr, stats.games)
                                     .arg(stats.whiteWins)
                                     .arg(stats.draws)
                                     .arg(stats.blackWins));
            }
        }
        if (m_mark && m_mark->move == move.move) {
            // Up, down or still, after the weights changed; the arrow has a column of its own.
            const int moved = m_mark->rowsUp;
            item->setText(kMarkColumn, moved > 0 ? QStringLiteral("↑") : moved < 0 ? QStringLiteral("↓") : QStringLiteral("="));
            item->setTextAlignment(kMarkColumn, Qt::AlignCenter);
            item->setToolTip(kMarkColumn, moved > 0 ? tr("Moved up %n row(s)", nullptr, moved)
                                          : moved < 0 ? tr("Moved down %n row(s)", nullptr, -moved)
                                                      : tr("Stayed where it was"));
            item->setForeground(kMarkColumn, moved > 0 ? QBrush(QColor(0x2e, 0x8b, 0x57)) : moved < 0 ? QBrush(QColor(0xc0, 0x39, 0x2b))
                                                                                                        : palette().brush(QPalette::PlaceholderText));
        }
        if (move.inRepertoire()) {
            // Bold and brighter than the rest: pure white on a dark theme, pure black on a light one.
            const bool dark = palette().color(QPalette::Base).lightness() < 128;
            const QBrush bright(dark ? Qt::white : Qt::black);
            for (int column = kMoveColumn; column < m_moves->columnCount(); ++column) {
                QFont font = item->font(column);
                font.setBold(true);
                item->setFont(column, font);
                item->setForeground(column, bright);
            }
            item->setToolTip(kWeightColumn, tr("In your repertoire: listed first whatever the weight"));
        }
    }

    paintGlow();

    if (moves.isEmpty()) { // A greyed row says why there are no moves.
        auto *item = new QTreeWidgetItem(m_moves);
        item->setText(kMoveColumn, QStringLiteral("–"));
        item->setText(kNameColumn, m_bookName.isEmpty() ? tr("No book chosen: choose one from the Book menu")
                                              : tr("The position is not in the book"));
        item->setTextAlignment(kMoveColumn, Qt::AlignCenter);
        item->setFlags(Qt::NoItemFlags);
    }
}
