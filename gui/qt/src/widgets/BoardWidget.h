#pragma once

#include "app/BoardState.h"
#include "app/MoveExplanation.h"

#include <QHash>
#include <QMultiHash>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

#include <array>

class QTimer;
class QVariantAnimation;

/// How a king is marked on the board.
enum class KingMark { None, Check, Mate };

/// A position to show, with the move that led to it and the king in check or mated.
struct BoardFrame {
    BoardState board;
    int lastMoveFrom = -1;
    int lastMoveTo = -1;
    int markedKing = -1;
    KingMark kingMark = KingMark::None;
};

/// Renders a chess position and lets the user pick moves by clicking or
/// dragging pieces. Knows nothing about games, databases or the rules: the
/// moves it offers are the ones given to setLegalMoves().
class BoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit BoardWidget(QWidget *parent = nullptr);

    /// Shows a position; clears the selection and the explanation arrows.
    void setBoard(const BoardFrame &frame);
    const BoardState &board() const { return m_board; }

    /// Moves the user may enter, as origin square → target squares.
    void setLegalMoves(const QMultiHash<int, int> &moves);

    /// Arrows and lost-piece rings explaining the position.
    void setExplanation(const QList<BoardArrow> &arrows, const QList<int> &lostPieces);

    /// Plays positions one after the other, sliding the moving piece, and
    /// holds the last one; e.g. a forced mate. The board frame turns red and
    /// moves cannot be entered until stopSequence() or setBoard().
    void playSequence(const QList<BoardFrame> &frames);
    /// Stops a sequence and shows the position it started from again.
    void stopSequence();
    bool isShowingSequence() const { return m_sequenceActive; }

    bool isFlipped() const { return m_flipped; }
    void setFlipped(bool flipped);

    bool showCoordinates() const { return m_showCoordinates; }
    void setShowCoordinates(bool show);

    /// Area covered by the squares, in widget coordinates.
    QRect boardArea() const { return boardRect().toAlignedRect(); }

    /// Margin around the squares inside the widget.
    static constexpr int kMargin = 8;
    /// Widget side needed to show squares as large as possible within `available` pixels.
    static int sideForAvailable(int available);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// Emitted on mouse wheel: negative steps go back, positive go forward.
    void navigateRequested(int steps);
    /// The user moved a piece from `from` to `to`; `globalPosition` is where
    /// the piece was dropped, e.g. to place a promotion menu.
    void moveRequested(int from, int to, const QPoint &globalPosition);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QRectF boardRect() const;
    QRectF squareRect(int square) const;
    /// Square under a point in widget coordinates, or -1.
    int squareAt(const QPointF &point) const;
    void paintPiece(QPainter &painter, Piece piece, const QRectF &rect) const;
    void paintArrow(QPainter &painter, const BoardArrow &arrow) const;
    /// A soft red glow under a king in check or mated, and a "+" or "#" badge above it.
    void paintKingGlow(QPainter &painter) const;
    void paintKingBadge(QPainter &painter) const;
    void clearSelection();
    void showNextFrame();
    /// Ends a sequence without restoring the board.
    void endSequence();
    const QPainterPath &glyphPath(PieceType type) const;
    /// Piece rendered from the SVG piece set, or a null pixmap if unavailable.
    QPixmap piecePixmap(Piece piece, int pixelSize) const;

    BoardState m_board;
    int m_lastMoveFrom = -1;
    int m_lastMoveTo = -1;
    bool m_flipped = false;
    bool m_showCoordinates = true;
    int m_wheelAccumulator = 0;
    bool m_keyboardFocus = false;

    QMultiHash<int, int> m_legalMoves;
    int m_selected = -1;
    /// Mouse press on the selected piece that may turn into a drag.
    QPointF m_pressPosition;
    bool m_pressed = false;
    bool m_dragging = false;
    QPointF m_dragPosition;
    /// Selecting an already selected piece deselects it on release (click-click).
    bool m_deselectOnRelease = false;

    QList<BoardArrow> m_arrows;
    QList<int> m_lostPieces;

    QList<BoardFrame> m_frames;
    qsizetype m_nextFrame = 0;
    bool m_sequenceActive = false;
    int m_markedKing = -1;
    KingMark m_kingMark = KingMark::None;
    /// Board and last move to show again when the sequence is stopped.
    BoardFrame m_beforeSequence;
    QTimer *m_sequenceTimer;
    /// Progress (0–1) of the piece sliding to the last move's target.
    QVariantAnimation *m_slide;
    mutable std::array<QPainterPath, 7> m_glyphs;
    mutable QHash<quint32, QPixmap> m_pieceCache;
};
