#pragma once

#include "app/ChessPosition.h"

#include <QWidget>

/// Pieces captured so far, as small figurines: first those taken by the side
/// at the bottom of the board, then those taken by the other side. Repeated
/// pieces are stacked slightly apart ("two rooks" look like a small pile);
/// one or two pawns are shown as they are, more as a pawn with "×3".
class CapturedPiecesWidget : public QWidget {
    Q_OBJECT

public:
    explicit CapturedPiecesWidget(QWidget *parent = nullptr);

    void setCaptured(const PieceCounts &captured);
    /// With the board flipped Black is at the bottom, so its captures come first.
    void setFlipped(bool flipped);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    /// One figurine or stack in the row.
    struct Item {
        Piece piece;
        int copies = 1;
        /// "×3" for three or more pawns.
        QString label;
    };

    QList<Item> items(Side capturedSide) const;
    /// Lays out (and with a painter, paints) the row; returns its width.
    int layoutRow(QPainter *painter) const;
    int pieceSize() const;
    void updateDescription();

    PieceCounts m_captured{};
    bool m_flipped = false;
};
