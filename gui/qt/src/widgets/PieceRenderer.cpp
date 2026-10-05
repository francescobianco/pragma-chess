#include "PieceRenderer.h"

#include "BoardTheme.h"

#include <QFont>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <array>

#ifdef PRAGMA_HAS_SVG
#include <QSvgRenderer>
#endif

namespace {

// Solid glyphs are used for both colors; the fill tells the sides apart.
char16_t glyphFor(PieceType type)
{
    switch (type) {
    case PieceType::King: return u'♚';
    case PieceType::Queen: return u'♛';
    case PieceType::Rook: return u'♜';
    case PieceType::Bishop: return u'♝';
    case PieceType::Knight: return u'♞';
    case PieceType::Pawn: return u'♟';
    case PieceType::None: break;
    }
    return u' ';
}

const QPainterPath &glyphPath(PieceType type)
{
    static std::array<QPainterPath, 7> glyphs;
    QPainterPath &path = glyphs[int(type)];
    if (path.isEmpty() && type != PieceType::None) {
        // DejaVu Sans ships outline glyphs for the chess symbols; a color emoji
        // fallback would produce an empty path.
        QFont font(QStringLiteral("DejaVu Sans"));
        font.setPixelSize(100);
        font.setStyleStrategy(QFont::PreferOutline);
        path.addText(0, 0, font, QString(QChar(glyphFor(type))));
        // Normalize to a unit box centered on the origin.
        const QRectF bounds = path.boundingRect();
        const qreal scale = 1.0 / qMax(bounds.width(), bounds.height());
        QTransform t;
        t.scale(scale, scale);
        t.translate(-bounds.center().x(), -bounds.center().y());
        path = t.map(path);
    }
    return path;
}

/// Piece rendered from the SVG piece set `pieceSet`, or a null pixmap if unavailable.
QPixmap piecePixmap(Piece piece, int pixelSize, qreal devicePixelRatio, const QString &pieceSet)
{
#ifdef PRAGMA_HAS_SVG
    static const char roles[] = " PNBRQK";
    static QHash<QString, QPixmap> cache;
    if (pixelSize <= 0)
        return {};
    const QString key = QStringLiteral("%1/%2-%3-%4").arg(pieceSet).arg(pixelSize).arg(int(piece.type)).arg(int(piece.side));
    if (auto it = cache.constFind(key); it != cache.cend())
        return *it;
    if (cache.size() > 128)
        cache.clear(); // Old sizes after a resize, or another set.

    const QString file = QStringLiteral(":/resources/pieces/%1/%2%3.svg")
                             .arg(pieceSet)
                             .arg(piece.side == Side::White ? QLatin1Char('w') : QLatin1Char('b'))
                             .arg(QLatin1Char(roles[int(piece.type)]));
    QSvgRenderer renderer(file);
    if (!renderer.isValid())
        return {};
    QPixmap pixmap(pixelSize, pixelSize);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(0, 0, pixelSize, pixelSize));
    }
    pixmap.setDevicePixelRatio(devicePixelRatio);
    cache.insert(key, pixmap);
    return pixmap;
#else
    Q_UNUSED(piece)
    Q_UNUSED(pixelSize)
    Q_UNUSED(devicePixelRatio)
    Q_UNUSED(pieceSet)
    return {};
#endif
}

} // namespace

namespace PieceRenderer {

void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio)
{
    paint(painter, piece, rect, devicePixelRatio, BoardTheme::current().pieceSet);
}

void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio, const QString &pieceSet)
{
    // The vector piece set of the board style.
    const qreal size = rect.width();
    const QPixmap pixmap = piecePixmap(piece, qRound(size * devicePixelRatio), devicePixelRatio, pieceSet);
    if (!pixmap.isNull()) {
        painter.drawPixmap(rect.topLeft(), pixmap);
        return;
    }

    const qreal glyphScale = size * (piece.type == PieceType::Pawn ? 0.66 : 0.8);
    QTransform t;
    t.translate(rect.center().x(), rect.center().y());
    t.scale(glyphScale, glyphScale);
    const QPainterPath path = t.map(glyphPath(piece.type));

    const bool white = piece.side == Side::White;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(white ? QColor(0x20, 0x20, 0x20) : QColor(0xf0, 0xf0, 0xf0, 0x90),
                        qMax(1.0, size * 0.025), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(white ? QColor(0xfa, 0xfa, 0xfa) : QColor(0x22, 0x22, 0x22));
    painter.drawPath(path);
    painter.restore();
}

void paintMuted(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio,
                const QColor &background)
{
    // The piece's greys are squeezed into a narrow band set off from the
    // background (lighter on dark themes, darker on light ones): both colours
    // stay readable, with little contrast.
    constexpr qreal kSpread = 0.36;
    constexpr int kOffset = 72;
    constexpr qreal kBlackGreyOut = 0.18;
    const QSize pixels = (rect.size() * devicePixelRatio).toSize();
    if (pixels.isEmpty())
        return;

    // Pieces are few and small: cache them per piece, size and background.
    static QHash<QString, QImage> cache;
    const QString key = QStringLiteral("%1-%2-%3-%4x%5-%6")
                            .arg(BoardTheme::current().pieceSet)
                            .arg(int(piece.type))
                            .arg(int(piece.side))
                            .arg(pixels.width())
                            .arg(pixels.height())
                            .arg(background.rgb());
    QImage image = cache.value(key);
    if (image.isNull()) {
        image = QImage(pixels, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        {
            QPainter imagePainter(&image);
            imagePainter.setRenderHint(QPainter::Antialiasing);
            imagePainter.setRenderHint(QPainter::SmoothPixmapTransform);
            paint(imagePainter, piece, QRectF(QPointF(0, 0), QSizeF(pixels)), 1.0);
        }
        const int backgroundGrey = qGray(background.rgb());
        const int middle = backgroundGrey < 128 ? backgroundGrey + kOffset : backgroundGrey - kOffset;
        for (int y = 0; y < image.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const int original = qGray(line[x]);
                qreal grey = middle + (original - 128) * kSpread;
                // Black pieces keep their own greys, only a very light touch towards the background.
                if (piece.side == Side::Black)
                    grey = original + (backgroundGrey - original) * kBlackGreyOut;
                grey = qBound(0.0, grey, 255.0);
                line[x] = qRgba(qRound(grey), qRound(grey), qRound(grey), qAlpha(line[x]));
            }
        }
        image.setDevicePixelRatio(devicePixelRatio);
        if (cache.size() > 256)
            cache.clear();
        cache.insert(key, image);
    }
    painter.drawImage(rect, image);
}

} // namespace PieceRenderer
