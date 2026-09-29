#pragma once

#include "app/BoardState.h"

#include <QRectF>

class QColor;
class QPainter;

/// Paints chess pieces: the Good Companion SVG set when Qt SVG is available,
/// outline font glyphs otherwise. Shared by the board and small piece views.
namespace PieceRenderer {

/// Paints `piece` filling the square `rect`, sharp at `devicePixelRatio`.
void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio);

/// Paints `piece` greyed out: in a narrow band of greys set off from
/// `background` (the colour behind it), readable but with little contrast, so
/// it is there without drawing the eye (the captured pieces by the board).
void paintMuted(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio,
                const QColor &background);

} // namespace PieceRenderer
