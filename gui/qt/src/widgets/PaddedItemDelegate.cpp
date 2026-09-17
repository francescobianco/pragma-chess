#include "PaddedItemDelegate.h"

#include <QApplication>
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
    style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);
}

QSize PaddedItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return QStyledItemDelegate::sizeHint(option, index) + QSize(2 * m_horizontal, 2 * m_vertical);
}
