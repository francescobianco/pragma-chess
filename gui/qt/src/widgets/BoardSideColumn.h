#pragma once

#include "app/ChessPosition.h"

#include <QWidget>

/// The column at the right of the board: a dot on the side of the player to
/// move (white for White, black for Black) and, from the rank of each
/// player's pawns inwards, the pieces they have captured, stacked
/// vertically. Repeated pieces overlap slightly ("two rooks" look like a small
/// pile); one or two pawns are shown as they are, more as a pawn with "×3".
class BoardSideColumn : public QWidget {
    Q_OBJECT

public:
    explicit BoardSideColumn(QWidget *parent = nullptr);

    void setCaptured(const PieceCounts &captured);
    void setSideToMove(Side side);
    /// With the board flipped Black is at the bottom.
    void setFlipped(bool flipped);
    /// Board Settings: the captured pieces may be shown under the board instead.
    void setShowCaptured(bool show);
    void setShowTurn(bool show);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    /// One figurine or stack in a column.
    struct Item {
        Piece piece;
        int copies = 1;
        /// "×3" for three or more pawns.
        QString label;
    };

    QList<Item> items(Side capturedSide) const;
    /// Paints the pieces of `row` in a spot growing down from `y` (or up, when `upwards`).
    void paintColumn(QPainter &painter, const QList<Item> &row, qreal y, bool upwards) const;
    qreal pieceSize() const;
    void updateDescription();

    PieceCounts m_captured{};
    Side m_sideToMove = Side::White;
    bool m_flipped = false;
    bool m_showCaptured = true;
    bool m_showTurn = true;
};
