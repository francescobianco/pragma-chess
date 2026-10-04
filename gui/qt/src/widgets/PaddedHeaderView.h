#pragma once

#include <QHeaderView>

class QTableView;
class QTreeView;

/// A header whose section titles have room around them, matching the cells
/// drawn by PaddedItemDelegate.
class PaddedHeaderView : public QHeaderView {
    Q_OBJECT

public:
    PaddedHeaderView(Qt::Orientation orientation, int vertical, int horizontal, QWidget *parent = nullptr);

    /// Gives a view the padded header (CellPadding), keeping what its own
    /// header was set to; the cells get PaddedItemDelegate when `cells`.
    static void install(QTreeView *view, bool cells = true);
    static void install(QTableView *view, bool cells = true);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    QSize sectionSizeFromContents(int logicalIndex) const override;

private:
    int m_vertical;
    int m_horizontal;
};
