#pragma once

#include <QStyledItemDelegate>

/// Room around the text of the move list and the book moves, cells and headers.
namespace CellPadding {
inline constexpr int vertical = 3;
inline constexpr int horizontal = 6;
} // namespace CellPadding

/// Draws item view cells with room around the text: the selection and hover
/// background still fill the whole cell.
class PaddedItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    /// `vertical` pixels above and below the text, `horizontal` left and right.
    PaddedItemDelegate(int vertical, int horizontal, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    int m_vertical;
    int m_horizontal;
};
