#pragma once

#include <QHeaderView>

/// A header whose section titles have room around them, matching the cells
/// drawn by PaddedItemDelegate.
class PaddedHeaderView : public QHeaderView {
    Q_OBJECT

public:
    PaddedHeaderView(Qt::Orientation orientation, int vertical, int horizontal, QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;

private:
    int m_vertical;
    int m_horizontal;
};
