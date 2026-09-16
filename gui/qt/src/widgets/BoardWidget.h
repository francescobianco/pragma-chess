#pragma once

#include "app/BoardState.h"

#include <QHash>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

#include <array>

/// Renders a chess position. Knows nothing about games or databases.
class BoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit BoardWidget(QWidget *parent = nullptr);

    void setBoard(const BoardState &board, int lastMoveFrom = -1, int lastMoveTo = -1);
    const BoardState &board() const { return m_board; }

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

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QRectF boardRect() const;
    QRectF squareRect(int square) const;
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
    mutable std::array<QPainterPath, 7> m_glyphs;
    mutable QHash<quint32, QPixmap> m_pieceCache;
};
