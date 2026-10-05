#include "ManageChaptersDialog.h"

#include "app/Chapters.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr int kSourceRole = Qt::UserRole;
constexpr int kGamesRole = Qt::UserRole + 1;
/// The grey "(No Chapter)" row of a list without chapters.
constexpr int kPlaceholderRole = Qt::UserRole + 2;

} // namespace

ManageChaptersDialog::ManageChaptersDialog(const QList<Entry> &chapters, int current, QWidget *parent)
    : QDialog(parent)
    , m_list(new QListWidget(this))
    , m_delete(new QPushButton(tr("&Delete…"), this))
    , m_up(new QPushButton(tr("Move &Up"), this))
    , m_down(new QPushButton(tr("Move D&own"), this))
{
    setWindowTitle(tr("Manage Chapters"));
    resize(440, 360);

    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    for (const Entry &entry : chapters) {
        auto *item = new QListWidgetItem(entry.title, m_list);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setData(kSourceRole, entry.source);
        item->setData(kGamesRole, entry.games);
        item->setToolTip(tr("%n game(s)", nullptr, entry.games));
    }
    m_list->setCurrentRow(qBound(0, current, int(m_list->count()) - 1));

    auto *add = new QPushButton(tr("&New"), this);
    auto *rename = new QPushButton(tr("&Rename"), this);
    connect(add, &QPushButton::clicked, this, [this] {
        auto *item = new QListWidgetItem(ChapterBook::defaultTitle(chapterCount() + 1), m_list);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setData(kSourceRole, -1);
        item->setData(kGamesRole, 0);
        updateButtons();
        m_list->setCurrentItem(item);
        m_list->editItem(item);
    });
    connect(rename, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem *item = m_list->currentItem())
            m_list->editItem(item);
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        QListWidgetItem *item = m_list->currentItem();
        if (!item || item->data(kPlaceholderRole).toBool())
            return;
        const int games = item->data(kGamesRole).toInt();
        if (games > 0
            && QMessageBox::question(this, tr("Delete Chapter"),
                                     tr("Delete the chapter “%1”, with its %n game(s) and its paragraphs? "
                                        "Games stored in a database stay there.",
                                        nullptr, games)
                                         .arg(item->text()),
                                     QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
                   != QMessageBox::Yes)
            return;
        delete item;
        updateButtons();
    });
    const auto move = [this](int step) {
        const int row = m_list->currentRow();
        if (row < 0 || row + step < 0 || row + step >= m_list->count())
            return;
        QListWidgetItem *item = m_list->takeItem(row);
        m_list->insertItem(row + step, item);
        m_list->setCurrentItem(item);
    };
    connect(m_up, &QPushButton::clicked, this, [move] { move(-1); });
    connect(m_down, &QPushButton::clicked, this, [move] { move(1); });
    connect(m_list, &QListWidget::currentRowChanged, this, &ManageChaptersDialog::updateButtons);

    auto *side = new QVBoxLayout;
    side->addWidget(add);
    side->addWidget(rename);
    side->addWidget(m_delete);
    side->addSpacing(12);
    side->addWidget(m_up);
    side->addWidget(m_down);
    side->addStretch();
    auto *row = new QHBoxLayout;
    row->addWidget(m_list, 1);
    row->addLayout(side);

    auto *note = new QLabel(tr("Drag a chapter to move it; double-click it to rename it."), this);
    note->setEnabled(false);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(row, 1);
    layout->addWidget(note);
    layout->addWidget(buttons);
    updateButtons();
}

int ManageChaptersDialog::chapterCount() const
{
    int count = 0;
    for (int i = 0; i < m_list->count(); ++i)
        count += m_list->item(i)->data(kPlaceholderRole).toBool() ? 0 : 1;
    return count;
}

void ManageChaptersDialog::updateButtons()
{
    // No chapter left: the list says so, in grey, and nothing can be done with it.
    for (int i = m_list->count() - 1; i >= 0; --i) {
        if (m_list->item(i)->data(kPlaceholderRole).toBool() && chapterCount() > 0)
            delete m_list->item(i);
    }
    if (m_list->count() == 0) {
        auto *item = new QListWidgetItem(tr("(No Chapter)"), m_list);
        item->setFlags(Qt::NoItemFlags);
        item->setData(kPlaceholderRole, true);
    }
    const int row = m_list->currentRow();
    m_delete->setEnabled(row >= 0 && chapterCount() > 0);
    m_up->setEnabled(row > 0);
    m_down->setEnabled(row >= 0 && row + 1 < m_list->count());
}

QList<ManageChaptersDialog::Entry> ManageChaptersDialog::entries() const
{
    QList<Entry> result;
    for (int i = 0; i < m_list->count(); ++i) {
        const QListWidgetItem *item = m_list->item(i);
        if (item->data(kPlaceholderRole).toBool())
            continue;
        result << Entry{item->data(kSourceRole).toInt(), item->text(), item->data(kGamesRole).toInt()};
    }
    return result;
}
