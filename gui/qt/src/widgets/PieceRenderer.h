#pragma once

#include "app/BoardState.h"

#include <QRectF>

class QPainter;

/// Paints chess pieces: the Good Companion SVG set when Qt SVG is available,
/// outline font glyphs otherwise. Shared by the board and small piece views.
namespace PieceRenderer {

/// Paints `piece` filling the square `rect`, sharp at `devicePixelRatio`.
void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio);

} // namespace PieceRenderer
