#include "SourceKindDelegate.h"

#include "platform/SymbolicIcons.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kPadding = 10;
constexpr int kTile = 48; // About three lines of text.
constexpr int kGap = 12;
constexpr int kLineGap = 2;
constexpr int kTextFlags = Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap;

/// The tile of each kind: a colour and a chess symbol, or the database icon.
struct Tile {
    QColor colour;
    QString glyph; // Empty: the database icon.
};

Tile tileFor(const QString &kind)
{
    if (kind == QLatin1String("lichess"))
        return {QColor(0x3a, 0x3a, 0x3a), QStringLiteral("♞")}; // Knight.
    if (kind == QLatin1String("chesscom"))
        return {QColor(0x5d, 0x8a, 0x3c), QStringLiteral("♟")}; // Pawn.
    if (kind == QLatin1String("torneionline"))
        return {QColor(0x2a, 0x6d, 0xb0), QStringLiteral("♚")}; // King: tournaments.
    return {QColor(0xb0, 0x4a, 0x2a), QString()};
}

void paintTile(QPainter *painter, const QRect &rect, const QString &kind)
{
    const Tile tile = tileFor(kind);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect), 8, 8);
    painter->fillPath(path, tile.colour);
    if (tile.glyph.isEmpty()) {
        // The symbolic icon, in white.
        const int side = rect.width() * 3 / 5;
        const qreal ratio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        QPixmap icon = SymbolicIcons::icon(QStringLiteral("pragma-database")).pixmap(QSize(side, side), ratio);
        QPainter tint(&icon);
        tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
        tint.fillRect(icon.rect(), Qt::white);
        tint.end();
        painter->drawPixmap(QRect(rect.center() - QPoint(side / 2, side / 2), QSize(side, side)), icon);
        return;
    }
    QFont font = painter->font();
    font.setPixelSize(rect.height() * 2 / 3);
    painter->setFont(font);
    painter->setPen(Qt::white);
    painter->drawText(rect, Qt::AlignCenter, tile.glyph);
}

} // namespace

int SourceKindDelegate::textWidth(int width)
{
    return qMax(80, width - 2 * kPadding - kTile - kGap);
}

QFont SourceKindDelegate::titleFont(const QFont &base)
{
    QFont font = base;
    font.setBold(true);
    return font;
}

void SourceKindDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem cell = option;
    initStyleOption(&cell, index);
    const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();
    cell.text.clear();
    cell.icon = QIcon();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &cell, painter, cell.widget);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRect area = option.rect.adjusted(kPadding, kPadding, -kPadding, -kPadding);
    paintTile(painter, QRect(area.left(), area.top(), kTile, kTile), index.data(Qt::UserRole).toString());

    const bool selected = option.state & QStyle::State_Selected;
    const QPalette::ColorGroup group = option.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    painter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
    const int left = area.left() + kTile + kGap;
    const int width = textWidth(option.rect.width());
    const QFont title = titleFont(option.font);
    painter->setFont(title);
    const QRect titleRect = QFontMetrics(title).boundingRect(QRect(left, area.top(), width, 10000), kTextFlags,
                                                             index.data(Qt::DisplayRole).toString());
    painter->drawText(titleRect, kTextFlags, index.data(Qt::DisplayRole).toString());
    painter->setFont(option.font);
    painter->drawText(QRect(left, titleRect.bottom() + 1 + kLineGap, width, area.bottom() - titleRect.bottom()), kTextFlags,
                      index.data(kDescriptionRole).toString());
    painter->restore();
}

int SourceKindDelegate::heightFor(const QStyleOptionViewItem &option, const QModelIndex &index, int width) const
{
    const int text = textWidth(width);
    const int title = QFontMetrics(titleFont(option.font))
                          .boundingRect(QRect(0, 0, text, 10000), kTextFlags, index.data(Qt::DisplayRole).toString())
                          .height();
    const int description = QFontMetrics(option.font)
                                .boundingRect(QRect(0, 0, text, 10000), kTextFlags, index.data(kDescriptionRole).toString())
                                .height();
    return qMax(kTile, title + kLineGap + description) + 2 * kPadding;
}

QSize SourceKindDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // The row is as wide as the list: the description wraps to it.
    int width = option.rect.width();
    if (const auto *view = qobject_cast<const QAbstractItemView *>(option.widget))
        width = view->viewport()->width();
    return {width, heightFor(option, index, width)};
}
