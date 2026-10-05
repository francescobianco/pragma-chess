#include "PositionEditorWidget.h"

#include "BoardWidget.h"
#include "BoardTheme.h"
#include "PieceRenderer.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

PositionEditorWidget::PositionEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setMouseTracking(false);
}

void PositionEditorWidget::setSetup(const PositionSetup &setup)
{
    m_setup = setup;
    update();
}

void PositionEditorWidget::setFlipped(bool flipped)
{
    m_flipped = flipped;
    update();
}

QRectF PositionEditorWidget::boardRect() const
{
    const qreal side = qMin(width(), height()) - 2.0;
    return QRectF((width() - side) / 2.0, (height() - side) / 2.0, side, side);
}

QRectF PositionEditorWidget::squareRect(int square) const
{
    const QRectF board = boardRect();
    const qreal size = board.width() / 8.0;
    const int file = m_flipped ? 7 - square % 8 : square % 8;
    const int rank = m_flipped ? square / 8 : 7 - square / 8;
    return QRectF(board.left() + file * size, board.top() + rank * size, size, size);
}

int PositionEditorWidget::squareAt(const QPointF &point) const
{
    const QRectF board = boardRect();
    if (!board.contains(point))
        return -1;
    const qreal size = board.width() / 8.0;
    const int column = qBound(0, int((point.x() - board.left()) / size), 7);
    const int row = qBound(0, int((point.y() - board.top()) / size), 7);
    const int file = m_flipped ? 7 - column : column;
    const int rank = m_flipped ? row : 7 - row;
    return rank * 8 + file;
}

void PositionEditorWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal ratio = devicePixelRatioF();
    const BoardTheme &theme = BoardTheme::current();
    QList<QRectF> darkSquares;
    QList<BoardTheme::PlacedPiece> piecesOnDark;
    for (int square = 0; square < 64; ++square) {
        if ((square / 8 + square % 8) % 2 == 1)
            continue;
        darkSquares << squareRect(square);
        const bool lifted = square == m_pressSquare && !m_dragged.isNull();
        if (const Piece piece = m_setup.at(square); !piece.isNull() && !lifted)
            piecesOnDark << BoardTheme::PlacedPiece{squareRect(square), piece};
    }
    theme.paintSquares(painter, boardRect(), darkSquares, piecesOnDark, ratio);
    for (int square = 0; square < 64; ++square) {
        const QRectF rect = squareRect(square);
        if (square == m_pressSquare && !m_dragged.isNull())
            continue; // Lifted: drawn under the pointer.
        if (const Piece piece = m_setup.at(square); !piece.isNull())
            PieceRenderer::paint(painter, piece, rect, ratio);
    }
    // The coordinates in the corner squares of the edges, small; a book's diagram has none.
    if (theme.showsCoordinates()) {
        QFont font = this->font();
        font.setPixelSize(qMax(8, int(boardRect().width() / 48)));
        painter.setFont(font);
        for (int i = 0; i < 8; ++i) {
            // The files along the bottom row, the ranks down the left column.
            const int bottom = m_flipped ? 63 - i : i;
            const QRectF fileRect = squareRect(bottom);
            const bool light = (bottom / 8 + bottom % 8) % 2 == 1;
            painter.setPen(theme.coordinateColor(light));
            painter.drawText(fileRect.adjusted(2, 0, -2, -1), Qt::AlignRight | Qt::AlignBottom,
                             QString(QChar('a' + bottom % 8)));
            const int left = m_flipped ? 7 + 8 * i : 56 - 8 * i;
            const QRectF rankRect = squareRect(left);
            const bool leftLight = (left / 8 + left % 8) % 2 == 1;
            painter.setPen(theme.coordinateColor(leftLight));
            painter.drawText(rankRect.adjusted(2, 1, -2, 0), Qt::AlignLeft | Qt::AlignTop,
                             QString::number(left / 8 + 1));
        }
    }
    if (!m_dragged.isNull()) {
        const qreal size = boardRect().width() / 8.0;
        PieceRenderer::paint(painter, m_dragged, QRectF(m_dragPoint - QPointF(size / 2, size / 2), QSizeF(size, size)),
                             ratio);
    }
}

void PositionEditorWidget::mousePressEvent(QMouseEvent *event)
{
    const int square = squareAt(event->position());
    if (square < 0)
        return;
    if (event->button() == Qt::RightButton) {
        m_setup.setPiece(square, Piece());
        update();
        Q_EMIT changed();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_pressSquare = square;
    m_pressPoint = event->position();
}

void PositionEditorWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_pressSquare < 0)
        return;
    if (m_dragged.isNull()) {
        const Piece piece = m_setup.at(m_pressSquare);
        if (piece.isNull()
            || (event->position() - m_pressPoint).manhattanLength() < QApplication::startDragDistance())
            return;
        m_dragged = piece;
    }
    m_dragPoint = event->position();
    update();
}

void PositionEditorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_pressSquare < 0)
        return;
    const int from = m_pressSquare;
    m_pressSquare = -1;
    const int to = squareAt(event->position());
    const Piece pressed = m_setup.at(from);
    // A piece taken and let go elsewhere moves, however short the way: only
    // a press and release on the same square is a click.
    if (!m_dragged.isNull() || (!pressed.isNull() && to != from)) {
        // On a square it goes there; off the board it goes back home.
        if (to >= 0 && to != from) {
            m_setup.setPiece(from, Piece());
            m_setup.setPiece(to, pressed);
        }
        m_dragged = Piece();
    } else {
        m_setup.setPiece(from, pressed == m_brush ? Piece() : m_brush);
    }
    update();
    Q_EMIT changed();
}
