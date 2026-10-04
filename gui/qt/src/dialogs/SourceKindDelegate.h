#pragma once

#include <QStyledItemDelegate>

/// Draws a kind of source in the Connect Source list: a tile with its icon,
/// as tall as three lines, the name in bold and the description under it.
/// Items carry the kind's id in Qt::UserRole and its description in kDescriptionRole.
class SourceKindDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    static constexpr int kDescriptionRole = Qt::UserRole + 1;

    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    /// The height of the item at `index` when the list is `width` wide.
    int heightFor(const QStyleOptionViewItem &option, const QModelIndex &index, int width) const;

private:
    /// Where the title and the description go in a row of `width`.
    static int textWidth(int width);
    static QFont titleFont(const QFont &base);
};
