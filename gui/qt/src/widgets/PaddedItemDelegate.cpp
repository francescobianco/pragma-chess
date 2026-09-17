#include "PaddedItemDelegate.h"

#include <QApplication>
#include <QIcon>
#include <QPainter>

PaddedItemDelegate::PaddedItemDelegate(int vertical, int horizontal, QObject *parent)
    : QStyledItemDelegate(parent)
    , m_vertical(vertical)
    , m_horizontal(horizontal)
{
}

void PaddedItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem cell = option;
    initStyleOption(&cell, index);
    const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();

    // Background over the whole cell, then the contents inside the padding.
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &cell, painter, cell.widget);
    cell.rect.adjust(m_horizontal, m_vertical, -m_horizontal, -m_vertical);
    cell.backgroundBrush = Qt::NoBrush;
    cell.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);

    // The style gives an icon a rectangle its own size against the left edge
    // of the cell, and decorationAlignment only places it inside *that*. An
    // icon standing on its own is placed by hand, so it can be centred.
    if (cell.text.isEmpty() && !cell.icon.isNull()) {
        QIcon::Mode mode = QIcon::Normal;
        if (!(cell.state & QStyle::State_Enabled))
            mode = QIcon::Disabled;
        else if (cell.state & QStyle::State_Selected)
            mode = QIcon::Selected;
        const QIcon::State state = (cell.state & QStyle::State_Open) ? QIcon::On : QIcon::Off;
        QSize size = cell.decorationSize;
        if (!size.isValid()) {
            const int side = style->pixelMetric(QStyle::PM_SmallIconSize, &cell, cell.widget);
            size = QSize(side, side);
        }
        size = cell.icon.actualSize(size, mode, state);
        cell.icon.paint(painter, QStyle::alignedRect(cell.direction, cell.displayAlignment, size, cell.rect),
                        Qt::AlignCenter, mode, state);
        return;
    }
    style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);
}

QSize PaddedItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index) + QSize(2 * m_horizontal, 2 * m_vertical);
}
