#include "BoardWidget.h"

#include <QFocusEvent>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

#include <cmath>

#ifdef PRAGMA_HAS_SVG
#include <QSvgRenderer>
#endif

namespace {

const QColor kLightSquare(0xf0, 0xd9, 0xb5);
const QColor kDarkSquare(0xb5, 0x88, 0x63);
const QColor kLastMove(0xcd, 0xd2, 0x6a, 0xb0);

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

} // namespace

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
    , m_board(BoardState::startingPosition())
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Chessboard"));
}

void BoardWidget::setBoard(const BoardState &board, int lastMoveFrom, int lastMoveTo)
{
    m_board = board;
    m_lastMoveFrom = lastMoveFrom;
    m_lastMoveTo = lastMoveTo;
    update();
}

void BoardWidget::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    update();
}

void BoardWidget::setShowCoordinates(bool show)
{
    if (m_showCoordinates == show)
        return;
    m_showCoordinates = show;
    update();
}

QSize BoardWidget::sizeHint() const
{
    return {560, 560};
}

QSize BoardWidget::minimumSizeHint() const
{
    return {200, 200};
}

int BoardWidget::sideForAvailable(int available)
{
    // Squares are whole pixels so that edges stay crisp.
    const int squares = qMax(8, ((available - 2 * kMargin) / 8) * 8);
    return squares + 2 * kMargin;
}

QRectF BoardWidget::boardRect() const
{
    const qreal side = std::floor((qMin(width(), height()) - 2.0 * kMargin) / 8) * 8;
    return {(width() - side) / 2, (height() - side) / 2, side, side};
}

QRectF BoardWidget::squareRect(int square) const
{
    const QRectF board = boardRect();
    const qreal size = board.width() / 8;
    int file = square % 8;
    int rank = square / 8;
    if (m_flipped)
        file = 7 - file;
    else
        rank = 7 - rank;
    return {board.left() + file * size, board.top() + rank * size, size, size};
}

const QPainterPath &BoardWidget::glyphPath(PieceType type) const
{
    QPainterPath &path = m_glyphs[int(type)];
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

QPixmap BoardWidget::piecePixmap(Piece piece, int pixelSize) const
{
#ifdef PRAGMA_HAS_SVG
    static const char roles[] = " PNBRQK";
    if (pixelSize <= 0)
        return {};
    const quint32 key = quint32(pixelSize) << 8 | quint32(piece.type) << 1 | quint32(piece.side);
    if (auto it = m_pieceCache.constFind(key); it != m_pieceCache.cend())
        return *it;
    if (m_pieceCache.size() > 64)
        m_pieceCache.clear(); // Old sizes after a resize.

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
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    m_pieceCache.insert(key, pixmap);
    return pixmap;
#else
    Q_UNUSED(piece)
    Q_UNUSED(pixelSize)
    return {};
#endif
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF board = boardRect();
    const qreal size = board.width() / 8;

    for (int square = 0; square < 64; ++square) {
        const QRectF rect = squareRect(square);
        const bool light = (square / 8 + square % 8) % 2 == 1;
        painter.fillRect(rect, light ? kLightSquare : kDarkSquare);
        if (square == m_lastMoveFrom || square == m_lastMoveTo)
            painter.fillRect(rect, kLastMove);
    }

    if (m_showCoordinates) {
        QFont font = this->font();
        font.setPixelSize(qMax(8, int(size * 0.16)));
        font.setBold(true);
        painter.setFont(font);
        const qreal pad = size * 0.05;
        for (int i = 0; i < 8; ++i) {
            // Files along the bottom row, ranks along the left column as seen on screen.
            const int bottomSquare = m_flipped ? 63 - i : i;
            const int leftSquare = m_flipped ? 63 - i * 8 : i * 8;
            const bool bottomLight = (bottomSquare / 8 + bottomSquare % 8) % 2 == 1;
            const bool leftLight = (leftSquare / 8 + leftSquare % 8) % 2 == 1;

            painter.setPen(bottomLight ? kDarkSquare : kLightSquare);
            painter.drawText(squareRect(bottomSquare).adjusted(pad, pad, -pad, -pad),
                             Qt::AlignRight | Qt::AlignBottom,
                             QString(QChar('a' + bottomSquare % 8)));
            painter.setPen(leftLight ? kDarkSquare : kLightSquare);
            painter.drawText(squareRect(leftSquare).adjusted(pad, pad, -pad, -pad),
                             Qt::AlignLeft | Qt::AlignTop,
                             QString(QChar('1' + leftSquare / 8)));
        }
    }

    for (int square = 0; square < 64; ++square) {
        const Piece piece = m_board.at(square);
        if (piece.isNull())
            continue;
        const QRectF rect = squareRect(square);

        // Vector piece set (Good Companion, in the style of classic chess books).
        const qreal dpr = devicePixelRatioF();
        const QPixmap pixmap = piecePixmap(piece, qRound(size * dpr));
        if (!pixmap.isNull()) {
            painter.drawPixmap(rect.topLeft(), pixmap);
            continue;
        }

        const qreal glyphScale = size * (piece.type == PieceType::Pawn ? 0.66 : 0.8);

        QTransform t;
        t.translate(rect.center().x(), rect.center().y());
        t.scale(glyphScale, glyphScale);
        const QPainterPath path = t.map(glyphPath(piece.type));

        const bool white = piece.side == Side::White;
        painter.setPen(QPen(white ? QColor(0x20, 0x20, 0x20) : QColor(0xf0, 0xf0, 0xf0, 0x90),
                            qMax(1.0, size * 0.025), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(white ? QColor(0xfa, 0xfa, 0xfa) : QColor(0x22, 0x22, 0x22));
        painter.drawPath(path);
    }

    // Like :focus-visible, show the ring only when focus came from the keyboard.
    if (hasFocus() && m_keyboardFocus) {
        painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(board.adjusted(-1, -1, 1, 1));
    }
}

void BoardWidget::wheelEvent(QWheelEvent *event)
{
    m_wheelAccumulator += event->angleDelta().y();
    const int steps = m_wheelAccumulator / 120;
    if (steps != 0) {
        m_wheelAccumulator -= steps * 120;
        // Scrolling down moves forward through the game.
        Q_EMIT navigateRequested(-steps);
    }
    event->accept();
}

void BoardWidget::focusInEvent(QFocusEvent *event)
{
    m_keyboardFocus = event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason;
    QWidget::focusInEvent(event);
}

void BoardWidget::focusOutEvent(QFocusEvent *event)
{
    m_keyboardFocus = false;
    QWidget::focusOutEvent(event);
}
