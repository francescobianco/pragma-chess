#include "PieceRenderer.h"

#include <QFont>
#include <QHash>
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

/// Piece rendered from the SVG piece set, or a null pixmap if unavailable.
QPixmap piecePixmap(Piece piece, int pixelSize, qreal devicePixelRatio)
{
#ifdef PRAGMA_HAS_SVG
    static const char roles[] = " PNBRQK";
    static QHash<quint32, QPixmap> cache;
    if (pixelSize <= 0)
        return {};
    const quint32 key = quint32(pixelSize) << 8 | quint32(piece.type) << 1 | quint32(piece.side);
    if (auto it = cache.constFind(key); it != cache.cend())
        return *it;
    if (cache.size() > 128)
        cache.clear(); // Old sizes after a resize.

    const QString file = QStringLiteral(":/resources/pieces/companion/%1%2.svg")
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
    return {};
#endif
}

} // namespace

namespace PieceRenderer {

void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio)
{
    // Vector piece set (Good Companion, in the style of classic chess books).
    const qreal size = rect.width();
    const QPixmap pixmap = piecePixmap(piece, qRound(size * devicePixelRatio), devicePixelRatio);
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

} // namespace PieceRenderer
