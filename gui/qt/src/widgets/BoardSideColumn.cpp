#include "BoardSideColumn.h"

#include "BoardWidget.h"
#include "PieceRenderer.h"

#include <QPainter>

#include <cmath>

namespace {

/// Offset of each extra copy in a stack, as a share of the piece size.
constexpr qreal kStackOffset = 0.3;
/// Pawns up to this many are drawn one by one, more get a count.
constexpr int kPawnsShownOneByOne = 2;
/// The rounded, light spot holding each group of captured pieces.
const QColor kSpotColor(0xf0, 0xd9, 0xb5, 0xe6);
const QColor kSpotText(0x3a, 0x2f, 0x24);
constexpr qreal kSpotRadius = 4;
/// Padding inside the spots, in pixels.
constexpr qreal kPadding = 2;
/// The turn dot, as a share of the width, and its smallest size in pixels.
constexpr qreal kDotSize = 0.3;
constexpr qreal kMinimumDot = 8;
const QColor kWhiteDot(0xfa, 0xfa, 0xf7);
const QColor kBlackDot(0x1f, 0x1f, 0x1f);

} // namespace

BoardSideColumn::BoardSideColumn(QWidget *parent)
    : QWidget(parent)
{
    setAccessibleName(tr("Turn and captured pieces"));
    updateDescription();
}

void BoardSideColumn::setCaptured(const PieceCounts &captured)
{
    if (m_captured == captured)
        return;
    m_captured = captured;
    updateDescription();
    update();
}

void BoardSideColumn::setSideToMove(Side side)
{
    if (m_sideToMove == side)
        return;
    m_sideToMove = side;
    updateDescription();
    update();
}

void BoardSideColumn::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    update();
}

void BoardSideColumn::setShowCaptured(bool show)
{
    if (m_showCaptured == show)
        return;
    m_showCaptured = show;
    update();
}

void BoardSideColumn::setShowTurn(bool show)
{
    if (m_showTurn == show)
        return;
    m_showTurn = show;
    update();
}

QSize BoardSideColumn::sizeHint() const
{
    return {28, 200};
}

QSize BoardSideColumn::minimumSizeHint() const
{
    return {20, 100};
}

qreal BoardSideColumn::pieceSize() const
{
    return width() - 2 * kPadding;
}

QList<BoardSideColumn::Item> BoardSideColumn::items(Side capturedSide) const
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

void BoardSideColumn::paintColumn(QPainter &painter, const QList<Item> &row, qreal y, bool upwards) const
{
    if (row.isEmpty())
        return;
    const qreal size = pieceSize();
    const qreal pad = kPadding;
    const qreal stackStep = size * kStackOffset;
    QFont font = this->font();
    font.setPixelSize(qMax(8, qRound(size * 0.42)));
    const qreal labelHeight = QFontMetricsF(font).height();

    qreal height = 0;
    for (const Item &item : row)
        height += size * 0.82 + (item.copies - 1) * stackStep + (item.label.isEmpty() ? 0 : labelHeight);
    height += size * 0.18 + 2 * pad;
    const qreal top = upwards ? y - height : y;

    // Each group sits in a light spot, so dark pieces show on dark themes too.
    painter.setPen(Qt::NoPen);
    painter.setBrush(kSpotColor);
    painter.drawRoundedRect(QRectF(0, top, width(), height), kSpotRadius, kSpotRadius);

    const qreal dpr = devicePixelRatioF();
    // Pieces overlap a little: figurines have air above and below.
    qreal cursor = top + pad;
    for (const Item &item : row) {
        for (int copy = 0; copy < item.copies; ++copy)
            PieceRenderer::paint(painter, item.piece, QRectF(pad, cursor + copy * stackStep, size, size), dpr);
        cursor += size * 0.82 + (item.copies - 1) * stackStep;
        if (!item.label.isEmpty()) {
            painter.setFont(font);
            painter.setPen(kSpotText);
            painter.drawText(QRectF(0, cursor + size * 0.18, width(), labelHeight), Qt::AlignCenter, item.label);
            cursor += labelHeight;
        }
    }
}

void BoardSideColumn::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const Side bottom = m_flipped ? Side::Black : Side::White;
    const Side upper = bottom == Side::White ? Side::Black : Side::White;
    const qreal dot = qMax(kMinimumDot, std::round(width() * kDotSize));
    // The column spans the board's frame; its squares are what is inside.
    const qreal frame = BoardWidget::kFrameWidth;
    const qreal square = (height() - 2 * frame) / 8;

    // The dot sits next to the board, level with its top or bottom edge on
    // the side of the player to move.
    const qreal dotY = m_sideToMove == upper ? 0.5 : height() - dot - 0.5;
    if (m_showTurn) {
        QColor outline = palette().color(QPalette::WindowText);
        outline.setAlphaF(0.45);
        painter.setPen(QPen(outline, 1));
        painter.setBrush(m_sideToMove == Side::White ? kWhiteDot : kBlackDot);
        painter.drawEllipse(QRectF(0.5, dotY, dot, dot));
    }
    if (!m_showCaptured)
        return;

    // What each player has taken sits next to them, from the rank of their
    // pawns inwards: the upper player took the bottom side's pieces, the
    // lower player the upper side's.
    paintColumn(painter, items(bottom), frame + square, false);
    paintColumn(painter, items(upper), height() - frame - square, true);
}

void BoardSideColumn::updateDescription()
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
    const QString text = (m_sideToMove == Side::White ? tr("White to move") : tr("Black to move"))
                         + QLatin1Char('\n')
                         + tr("White has captured: %1\nBlack has captured: %2")
                               .arg(describe(Side::Black), describe(Side::White));
    setToolTip(text);
    setAccessibleDescription(text);
}
