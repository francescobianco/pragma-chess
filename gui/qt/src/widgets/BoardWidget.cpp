#include "BoardWidget.h"

#include <QApplication>
#include <QFocusEvent>
#include <QFont>
#include <QMouseEvent>
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
const QColor kSelected(0x64, 0x9f, 0x5a, 0xa0);
const QColor kMoveHint(0x14, 0x33, 0x0f, 0x48);

QColor arrowColor(BoardArrow::Kind kind)
{
    switch (kind) {
    case BoardArrow::Kind::Refutation: return QColor(0xd4, 0x3f, 0x32, 0xd0);
    case BoardArrow::Kind::Idea: return QColor(0x2f, 0x8f, 0x44, 0xd0);
    case BoardArrow::Kind::Reply: return QColor(0x3a, 0x6e, 0xb5, 0xc0);
    case BoardArrow::Kind::Alternative: return QColor(0x2f, 0x8f, 0x44, 0xa8);
    }
    return {};
}

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
    m_arrows.clear();
    m_lostPieces.clear();
    clearSelection();
    update();
}

void BoardWidget::setLegalMoves(const QMultiHash<int, int> &moves)
{
    m_legalMoves = moves;
    if (m_selected >= 0 && !m_legalMoves.contains(m_selected))
        clearSelection();
    update();
}

void BoardWidget::setExplanation(const QList<BoardArrow> &arrows, const QList<int> &lostPieces)
{
    if (m_arrows == arrows && m_lostPieces == lostPieces)
        return;
    m_arrows = arrows;
    m_lostPieces = lostPieces;
    update();
}

void BoardWidget::clearSelection()
{
    m_selected = -1;
    m_pressed = false;
    m_dragging = false;
    m_deselectOnRelease = false;
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

int BoardWidget::squareAt(const QPointF &point) const
{
    const QRectF board = boardRect();
    if (!board.contains(point))
        return -1;
    const qreal size = board.width() / 8;
    int file = qBound(0, int((point.x() - board.left()) / size), 7);
    int rank = 7 - qBound(0, int((point.y() - board.top()) / size), 7);
    if (m_flipped) {
        file = 7 - file;
        rank = 7 - rank;
    }
    return rank * 8 + file;
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

    if (m_selected >= 0)
        painter.fillRect(squareRect(m_selected), kSelected);

    for (int square = 0; square < 64; ++square) {
        const Piece piece = m_board.at(square);
        if (piece.isNull() || (m_dragging && square == m_selected))
            continue;
        paintPiece(painter, piece, squareRect(square));
    }

    // Targets of the selected piece: dots on empty squares, rings on captures.
    if (m_selected >= 0) {
        painter.setPen(Qt::NoPen);
        for (int target : m_legalMoves.values(m_selected)) {
            const QRectF rect = squareRect(target);
            if (m_board.at(target).isNull()) {
                painter.setBrush(kMoveHint);
                painter.drawEllipse(rect.center(), size * 0.16, size * 0.16);
            } else {
                QPainterPath ring;
                ring.addRect(rect);
                ring.addEllipse(rect.center(), size * 0.56, size * 0.56);
                painter.fillPath(ring, kMoveHint);
            }
        }
    }

    for (int square : std::as_const(m_lostPieces)) {
        painter.setPen(QPen(arrowColor(BoardArrow::Kind::Refutation), qMax(2.0, size * 0.06)));
        painter.setBrush(Qt::NoBrush);
        const qreal inset = size * 0.07;
        painter.drawEllipse(squareRect(square).adjusted(inset, inset, -inset, -inset));
    }
    for (const BoardArrow &arrow : std::as_const(m_arrows))
        paintArrow(painter, arrow);

    if (m_dragging && m_selected >= 0) {
        QRectF rect(0, 0, size, size);
        rect.moveCenter(m_dragPosition);
        paintPiece(painter, m_board.at(m_selected), rect);
    }

    // Like :focus-visible, show the ring only when focus came from the keyboard.
    if (hasFocus() && m_keyboardFocus) {
        painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(board.adjusted(-1, -1, 1, 1));
    }
}

void BoardWidget::paintPiece(QPainter &painter, Piece piece, const QRectF &rect) const
{
    // Vector piece set (Good Companion, in the style of classic chess books).
    const qreal size = rect.width();
    const QPixmap pixmap = piecePixmap(piece, qRound(size * devicePixelRatioF()));
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
    painter.setPen(QPen(white ? QColor(0x20, 0x20, 0x20) : QColor(0xf0, 0xf0, 0xf0, 0x90),
                        qMax(1.0, size * 0.025), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(white ? QColor(0xfa, 0xfa, 0xfa) : QColor(0x22, 0x22, 0x22));
    painter.drawPath(path);
}

void BoardWidget::paintArrow(QPainter &painter, const BoardArrow &arrow) const
{
    if (arrow.from < 0 || arrow.to < 0 || arrow.from == arrow.to)
        return;
    const qreal size = boardRect().width() / 8;
    const QColor color = arrowColor(arrow.kind);

    // Knight moves bend like the knight goes: along the long leg first.
    QList<QPointF> points{squareRect(arrow.from).center()};
    const int fileDistance = qAbs(arrow.to % 8 - arrow.from % 8);
    const int rankDistance = qAbs(arrow.to / 8 - arrow.from / 8);
    if (fileDistance + rankDistance == 3 && fileDistance > 0 && rankDistance > 0) {
        const int corner = rankDistance == 2 ? (arrow.to / 8) * 8 + arrow.from % 8
                                             : (arrow.from / 8) * 8 + arrow.to % 8;
        points << squareRect(corner).center();
    }
    points << squareRect(arrow.to).center();

    const auto unitVector = [](const QPointF &from, const QPointF &to) {
        const QPointF delta = to - from;
        const qreal length = std::hypot(delta.x(), delta.y());
        return length > 0 ? delta / length : QPointF();
    };
    const QPointF lastFrom = points.at(points.size() - 2);
    const QPointF lastUnit = unitVector(lastFrom, points.last());
    const qreal lastLength = std::hypot(points.last().x() - lastFrom.x(), points.last().y() - lastFrom.y());
    const QPointF tip = points.last() - lastUnit * size * 0.1;
    // Arrows to a neighbouring square get a shorter head, leaving room for the step number.
    const QPointF headBase = tip - lastUnit * qMin(size * 0.42, lastLength * 0.3);
    const QPointF start = points.first() + unitVector(points.at(0), points.at(1)) * size * 0.2;

    QPainterPath shaft(start);
    for (qsizetype i = 1; i + 1 < points.size(); ++i)
        shaft.lineTo(points.at(i));
    shaft.lineTo(headBase);
    QPen pen(color, size * 0.15, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
    if (arrow.kind == BoardArrow::Kind::Alternative)
        pen.setDashPattern({0.9, 0.6});
    painter.strokePath(shaft, pen);

    const QPointF normal(-lastUnit.y(), lastUnit.x());
    const qreal halfHead = size * 0.22;
    QPainterPath head(tip);
    head.lineTo(headBase + normal * halfHead);
    head.lineTo(headBase - normal * halfHead);
    head.closeSubpath();
    painter.fillPath(head, color);

    if (arrow.step <= 0)
        return;
    // Step number where the arrow leaves its square, so that it stays readable
    // when several arrows end on the same square.
    const qreal radius = size * 0.15;
    const QPointF badge = points.first() + unitVector(points.at(0), points.at(1)) * size * 0.4;
    QColor badgeColor = color;
    badgeColor.setAlpha(255);
    painter.setPen(QPen(QColor(255, 255, 255, 220), qMax(1.0, size * 0.02)));
    painter.setBrush(badgeColor.darker(115));
    painter.drawEllipse(badge, radius, radius);
    QFont font = this->font();
    font.setPixelSize(qMax(8, int(radius * 1.3)));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRectF(badge.x() - radius, badge.y() - radius, 2 * radius, 2 * radius), Qt::AlignCenter,
                     QString::number(arrow.step));
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        clearSelection();
        QWidget::mousePressEvent(event);
        return;
    }
    const int square = squareAt(event->position());
    if (m_selected >= 0 && square >= 0 && m_legalMoves.contains(m_selected, square)) {
        const int from = m_selected;
        clearSelection();
        Q_EMIT moveRequested(from, square, event->globalPosition().toPoint());
        return;
    }
    if (square >= 0 && m_legalMoves.contains(square)) {
        m_deselectOnRelease = square == m_selected;
        m_selected = square;
        m_pressed = true;
        m_pressPosition = event->position();
        update();
        return;
    }
    clearSelection();
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_pressed)
        return;
    if (!m_dragging && (event->position() - m_pressPosition).manhattanLength() < QApplication::startDragDistance())
        return;
    m_dragging = true;
    m_dragPosition = event->position();
    update();
}

void BoardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (!m_dragging) {
        if (m_deselectOnRelease)
            clearSelection();
        return;
    }

    m_dragging = false;
    const int from = m_selected;
    const int square = squareAt(event->position());
    if (square == from) {
        update(); // Dropped back: keep the piece selected for a click on the target.
        return;
    }
    clearSelection();
    if (square >= 0 && m_legalMoves.contains(from, square))
        Q_EMIT moveRequested(from, square, event->globalPosition().toPoint());
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
