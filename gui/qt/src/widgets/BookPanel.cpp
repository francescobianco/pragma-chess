#include "BookPanel.h"

#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

BookPanel::BookPanel(QWidget *parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget)
    , m_message(new QLabel)
    , m_moves(new QTreeWidget)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_stack, 1);

    m_message->setAlignment(Qt::AlignCenter);
    m_message->setWordWrap(true);
    m_message->setEnabled(false);
    m_message->setContentsMargins(12, 12, 12, 12);
    m_stack->addWidget(m_message);

    m_moves->setColumnCount(3);
    m_moves->setHeaderLabels({tr("Move"), tr("Opening"), tr("Weight")});
    m_moves->setRootIsDecorated(false);
    m_moves->setUniformRowHeights(true);
    m_moves->setFocusPolicy(Qt::NoFocus);
    m_moves->setAccessibleName(tr("Book moves"));
    m_moves->header()->setStretchLastSection(false);
    m_moves->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_moves->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_moves->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_stack->addWidget(m_moves);
    connect(m_moves, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const int row = m_moves->indexOfTopLevelItem(item);
        if (row >= 0 && row < m_shownMoves.size())
            Q_EMIT moveActivated(m_shownMoves.at(row));
    });

    setBookName(QString());
}

void BookPanel::setBookName(const QString &name)
{
    m_bookName = name;
    if (name.isEmpty()) {
        m_moves->clear();
        m_shownMoves.clear();
    }
    updateMessage();
}

void BookPanel::setMoves(const ChessPosition &position, const QList<PolyglotBook::Move> &moves,
                         const QList<OpeningNames::Name> &names)
{
    m_moves->clear();
    m_shownMoves.clear();
    int total = 0;
    for (const PolyglotBook::Move &move : moves)
        total += move.weight;
    for (const PolyglotBook::Move &move : moves) {
        auto *item = new QTreeWidgetItem(m_moves);
        item->setText(0, position.moveNumberText() + figurineSan(position.san(move.move)));
        const double share = total > 0 ? 100.0 * move.weight / total : 100.0 / moves.size();
        const OpeningNames::Name name = names.value(m_shownMoves.size());
        item->setText(1, name.name);
        if (!name.isEmpty())
            item->setToolTip(1, QStringLiteral("%1 %2").arg(name.eco, name.name));
        item->setText(2, QLocale().toString(share, 'f', 1) + QStringLiteral(" %"));
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
        item->setToolTip(0, tr("Play %1").arg(position.san(move.move)));
        m_shownMoves << move.move;
    }
    updateMessage();
}

void BookPanel::updateMessage()
{
    if (m_bookName.isEmpty())
        m_message->setText(tr("Choose an opening book from the Book menu to see its moves here."));
    else
        m_message->setText(tr("The position is not in the book."));
    m_stack->setCurrentWidget(!m_bookName.isEmpty() && !m_shownMoves.isEmpty() ? static_cast<QWidget *>(m_moves)
                                                                                 : m_message);
}
