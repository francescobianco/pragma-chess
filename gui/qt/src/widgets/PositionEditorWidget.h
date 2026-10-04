#pragma once

#include "app/PositionSetup.h"

#include <QWidget>

/// A board to set up a position on: a click puts the chosen piece on a
/// square (or takes it off when the square has that piece already), a
/// right click empties it, and a piece is dragged to another square (let go
/// off the board, it goes back where it was).
class PositionEditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit PositionEditorWidget(QWidget *parent = nullptr);

    const PositionSetup &setup() const { return m_setup; }
    void setSetup(const PositionSetup &setup);

    /// The piece a click puts down; a null piece takes pieces away.
    void setBrush(Piece piece) { m_brush = piece; }

    bool isFlipped() const { return m_flipped; }
    void setFlipped(bool flipped);

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return width; }
    QSize sizeHint() const override { return {440, 440}; }
    QSize minimumSizeHint() const override { return {240, 240}; }

Q_SIGNALS:
    void changed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRectF boardRect() const;
    QRectF squareRect(int square) const;
    int squareAt(const QPointF &point) const;

    PositionSetup m_setup;
    Piece m_brush{PieceType::Pawn, Side::White};
    bool m_flipped = false;
    int m_pressSquare = -1;
    QPointF m_pressPoint;
    /// A piece being dragged, lifted off m_pressSquare.
    Piece m_dragged;
    QPointF m_dragPoint;
};
