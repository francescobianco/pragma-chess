#include "BoardTheme.h"

#include "PieceRenderer.h"

#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {

QString s_current = QLatin1String(BoardTheme::kDefaultId);

/// Lines across a square of a hatched style: about as many as a printed
/// diagram has, whatever the size of the board.
constexpr qreal kHatchLinesPerSquare = 14.0;
/// The white left around a piece on a hatched square, in parts of the square.
constexpr qreal kHaloWidth = 0.045;

/// The piece's silhouette grown by the halo, as an opaque mask a little larger
/// than the square (by `margin` on each side). Kept per piece and size: it is
/// drawn at every frame of a move.
QImage haloMask(const PieceStyle &style, Piece piece, int pixels, int margin)
{
    static QHash<QString, QImage> cache;
    const QString key = QStringLiteral("%1/%2/%3/%4/%5")
                            .arg(style.set)
                            .arg(int(piece.type))
                            .arg(int(piece.side))
                            .arg(pixels)
                            .arg(margin);
    if (const auto it = cache.constFind(key); it != cache.cend())
        return *it;
    if (cache.size() > 64)
        cache.clear(); // Old sizes after a resize.

    QImage pieceImage(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    pieceImage.fill(Qt::transparent);
    {
        QPainter painter(&pieceImage);
        painter.setRenderHint(QPainter::Antialiasing);
        PieceRenderer::paint(painter, piece, QRectF(0, 0, pixels, pixels), 1.0, style);
    }
    // Grown by drawing the silhouette around a few circles: a dilation, soft
    // at its edge like ink that stopped short of the piece.
    QImage mask(pixels + 2 * margin, pixels + 2 * margin, QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter painter(&mask);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const int steps = 24;
        for (const qreal radius : {qreal(margin), margin * 0.5}) {
            for (int i = 0; i < steps; ++i) {
                const qreal angle = 2 * M_PI * i / steps;
                painter.drawImage(QPointF(margin + radius * std::cos(angle), margin + radius * std::sin(angle)),
                                  pieceImage);
            }
        }
        painter.drawImage(QPoint(margin, margin), pieceImage);
        // Only the shape matters: every pixel the piece reaches is cleared whole.
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(mask.rect(), Qt::black);
    }
    cache.insert(key, mask);
    return mask;
}

} // namespace

const QList<BoardTheme> &BoardTheme::all()
{
    static const QList<BoardTheme> themes = [] {
        QList<BoardTheme> list{
            // The browns of the classic wooden board, with the pieces of chess books.
            {QLatin1String(kDefaultId), QStringLiteral("Pragma Classic"), QColor(0xf0, 0xd9, 0xb5),
             QColor(0xb5, 0x88, 0x63), QStringLiteral("companion")},
            // lichess.org's green board (public/images/board/green.png) and its Alpha pieces.
            {QStringLiteral("lichess-alpha"), QStringLiteral("Lichess Alpha"), QColor(0xff, 0xff, 0xdd),
             QColor(0x86, 0xa6, 0x66), QStringLiteral("alpha")},
        };
        // The diagram of a printed chess book: one old paper, the dark squares
        // hatched in ink, the Good Companion pieces of those books.
        BoardTheme book{QStringLiteral("classic-book"), QStringLiteral("Classic Book"), QColor(0xf8, 0xf2, 0xe4),
                        QColor(0xf8, 0xf2, 0xe4), QStringLiteral("companion")};
        book.hatched = true;
        book.solidBlack = true;
        book.paperWhite = true;
        book.ink = QColor(0x3a, 0x33, 0x2b);
        list << book;
        return list;
    }();
    return themes;
}

const BoardTheme &BoardTheme::byId(const QString &id)
{
    for (const BoardTheme &theme : all()) {
        if (theme.id == id)
            return theme;
    }
    return all().first();
}

const BoardTheme &BoardTheme::current()
{
    return byId(s_current);
}

void BoardTheme::setCurrent(const QString &id)
{
    s_current = byId(id).id;
}

QColor BoardTheme::coordinateColor(bool onLightSquare) const
{
    if (hatched)
        return ink;
    return onLightSquare ? darkSquare : lightSquare;
}

void BoardTheme::paintSquares(QPainter &painter, const QRectF &board, const QList<QRectF> &darkSquares,
                              const QList<PlacedPiece> &piecesOnDark, qreal devicePixelRatio) const
{
    painter.fillRect(board, lightSquare);
    if (!hatched) {
        for (const QRectF &square : darkSquares)
            painter.fillRect(square, darkSquare);
        return;
    }
    for (const QRectF &square : darkSquares)
        painter.fillRect(square, darkSquare);

    // The hatching on a layer of its own, so the halos can take it away.
    const QSize pixels = (board.size() * devicePixelRatio).toSize();
    if (pixels.isEmpty())
        return;
    QImage layer(pixels, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    layer.setDevicePixelRatio(devicePixelRatio);
    const qreal size = board.width() / 8;
    {
        QPainter ink(&layer);
        ink.setRenderHint(QPainter::Antialiasing);
        ink.translate(-board.topLeft());
        QPainterPath dark;
        for (const QRectF &square : darkSquares)
            dark.addRect(square);
        ink.setClipPath(dark);
        // Lines rising to the right, as in the diagrams of chess books, one
        // pattern across the whole board so they run on from square to square.
        ink.setPen(QPen(this->ink, qMax(0.6, size * 0.014), Qt::SolidLine, Qt::FlatCap));
        const qreal step = size / kHatchLinesPerSquare * std::sqrt(2.0);
        for (qreal x = -board.height(); x < board.width() + step; x += step)
            ink.drawLine(QPointF(board.left() + x, board.bottom()),
                         QPointF(board.left() + x + board.height(), board.top()));

        // Around each piece on a dark square the paper shows: the lines stop
        // short of its outline.
        ink.setClipping(false);
        ink.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        for (const PlacedPiece &placed : piecesOnDark) {
            const int piecePixels = qRound(placed.rect.width() * devicePixelRatio);
            const int margin = qMax(1, qRound(placed.rect.width() * kHaloWidth * devicePixelRatio));
            if (piecePixels <= 0)
                continue;
            const QImage mask = haloMask(pieceStyle(), placed.piece, piecePixels, margin);
            const qreal outset = margin / devicePixelRatio;
            ink.drawImage(placed.rect.adjusted(-outset, -outset, outset, outset), mask);
        }
    }
    painter.drawImage(board.topLeft(), layer);
}
