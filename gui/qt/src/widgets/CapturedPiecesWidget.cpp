#include "CapturedPiecesWidget.h"

#include "PieceRenderer.h"

#include <QEvent>
#include <QPainter>

namespace {

/// Offset of each extra copy in a stack, as a share of the piece size.
constexpr qreal kStackOffset = 0.28;
/// Pawns up to this many are drawn one by one, more get a count.
constexpr int kPawnsShownOneByOne = 2;
/// The rounded, light spot holding each group of captured pieces.
const QColor kSpotColor(0xf0, 0xd9, 0xb5, 0xe6);
const QColor kSpotText(0x3a, 0x2f, 0x24);
constexpr int kSpotPadding = 3;
constexpr qreal kSpotRadius = 4;

} // namespace

CapturedPiecesWidget::CapturedPiecesWidget(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setAccessibleName(tr("Captured pieces"));
    updateDescription();
}

void CapturedPiecesWidget::setCaptured(const PieceCounts &captured)
{
    if (m_captured == captured)
        return;
    m_captured = captured;
    updateDescription();
    updateGeometry();
    update();
}

void CapturedPiecesWidget::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    update();
}

int CapturedPiecesWidget::pieceSize() const
{
    return qMax(16, fontMetrics().height() + 4);
}

QSize CapturedPiecesWidget::sizeHint() const
{
    return {layoutRow(nullptr), pieceSize() + 2 * kSpotPadding};
}

QSize CapturedPiecesWidget::minimumSizeHint() const
{
    return sizeHint();
}

QList<CapturedPiecesWidget::Item> CapturedPiecesWidget::items(Side capturedSide) const
{
    QList<Item> result;
    const auto &counts = m_captured[int(capturedSide)];
    for (PieceType type : {PieceType::Queen, PieceType::Rook, PieceType::Bishop, PieceType::Knight}) {
        if (counts[int(type)] > 0)
            result << Item{{type, capturedSide}, counts[int(type)], QString()};
    }
    const int pawns = counts[int(PieceType::Pawn)];
    if (pawns > kPawnsShownOneByOne) {
        result << Item{{PieceType::Pawn, capturedSide}, 1, QStringLiteral("×%1").arg(pawns)};
    } else {
        for (int i = 0; i < pawns; ++i)
            result << Item{{PieceType::Pawn, capturedSide}, 1, QString()};
    }
    return result;
}

int CapturedPiecesWidget::layoutRow(QPainter *painter) const
{
    const int size = pieceSize();
    const int stackStep = qRound(size * kStackOffset);
    const int itemGap = qMax(1, size / 8);
    const int groupGap = size / 3;
    const qreal dpr = devicePixelRatioF();
    const int top = (height() - size) / 2;

    // Pieces taken by the side at the bottom are the other side's pieces.
    const Side bottom = m_flipped ? Side::Black : Side::White;
    const Side upper = bottom == Side::White ? Side::Black : Side::White;

    // Lays out one group from `left`, painting it if asked; returns its width.
    const auto layoutGroup = [&](const QList<Item> &row, int left, bool paint) {
        int x = left;
        for (qsizetype i = 0; i < row.size(); ++i) {
            const Item &item = row.at(i);
            for (int copy = 0; copy < item.copies && paint; ++copy)
                PieceRenderer::paint(*painter, item.piece, QRectF(x + copy * stackStep, top, size, size), dpr);
            x += size + (item.copies - 1) * stackStep;
            if (!item.label.isEmpty()) {
                const int labelWidth = fontMetrics().horizontalAdvance(item.label);
                if (paint) {
                    painter->setPen(kSpotText);
                    painter->drawText(QRect(x, 0, labelWidth, height()), Qt::AlignVCenter | Qt::AlignLeft, item.label);
                }
                x += labelWidth;
            }
            if (i + 1 < row.size()) {
                // Pawns are narrow: side by side they sit closer than other pieces.
                x += item.piece.type == PieceType::Pawn ? itemGap - size / 4 : itemGap;
            }
        }
        return x - left;
    };

    int x = 0;
    for (Side capturedSide : {upper, bottom}) {
        const QList<Item> row = items(capturedSide);
        if (row.isEmpty())
            continue;
        if (x > 0)
            x += groupGap;
        // Each group sits in a light spot, so dark pieces show on dark themes too.
        const int spotWidth = layoutGroup(row, 0, false) + 2 * kSpotPadding;
        if (painter) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(kSpotColor);
            painter->drawRoundedRect(QRectF(x, 0, spotWidth, height()), kSpotRadius, kSpotRadius);
            layoutGroup(row, x + kSpotPadding, true);
        }
        x += spotWidth;
    }
    return x;
}

void CapturedPiecesWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    layoutRow(&painter);
}

void CapturedPiecesWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange || event->type() == QEvent::PaletteChange) {
        updateGeometry();
        update();
    }
    QWidget::changeEvent(event);
}

void CapturedPiecesWidget::updateDescription()
{
    const auto describe = [this](Side side) {
        QStringList parts;
        const auto &counts = m_captured[int(side)];
        const std::pair<PieceType, QString> names[] = {{PieceType::Queen, tr("queen")},
                                                       {PieceType::Rook, tr("rook")},
                                                       {PieceType::Bishop, tr("bishop")},
                                                       {PieceType::Knight, tr("knight")},
                                                       {PieceType::Pawn, tr("pawn")}};
        for (const auto &[type, name] : names) {
            if (counts[int(type)] > 0)
                parts << QStringLiteral("%1 %2").arg(counts[int(type)]).arg(name);
        }
        return parts.isEmpty() ? tr("nothing") : parts.join(QStringLiteral(", "));
    };
    const QString text = tr("White has captured: %1\nBlack has captured: %2")
                             .arg(describe(Side::Black), describe(Side::White));
    setToolTip(text);
    setAccessibleDescription(text);
}
