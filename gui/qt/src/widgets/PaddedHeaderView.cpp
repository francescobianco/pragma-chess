#include "PaddedHeaderView.h"

#include <QPainter>
#include <QStyleOptionHeader>

PaddedHeaderView::PaddedHeaderView(Qt::Orientation orientation, int vertical, int horizontal, QWidget *parent)
    : QHeaderView(orientation, parent)
    , m_vertical(vertical)
    , m_horizontal(horizontal)
{
}

void PaddedHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid())
        return;
    QStyleOptionHeader option;
    initStyleOption(&option);
    initStyleOptionForIndex(&option, logicalIndex);
    option.rect = rect;

    // The section background over the whole rect, the title inside the padding.
    painter->save();
    if (option.state & QStyle::State_On) { // A highlighted section, as QHeaderView draws it.
        QFont font = painter->font();
        font.setBold(true);
        painter->setFont(font);
    }
    style()->drawControl(QStyle::CE_HeaderSection, &option, painter, this);
    QStyleOptionHeader label = option;
    label.rect = rect.adjusted(m_horizontal, m_vertical, -m_horizontal, -m_vertical);
    if (label.sortIndicator != QStyleOptionHeader::None) {
        QStyleOptionHeader arrow = label;
        arrow.rect = style()->subElementRect(QStyle::SE_HeaderArrow, &label, this);
        style()->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &arrow, painter, this);
        label.rect = style()->subElementRect(QStyle::SE_HeaderLabel, &label, this);
    }
    style()->drawControl(QStyle::CE_HeaderLabel, &label, painter, this);
    painter->restore();
}

QSize PaddedHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    return QHeaderView::sectionSizeFromContents(logicalIndex) + QSize(2 * m_horizontal, 2 * m_vertical);
}
