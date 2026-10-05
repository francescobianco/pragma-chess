#pragma once

#include "app/BoardState.h"

#include <QRectF>

#include <QColor>
#include <QString>

class QPainter;

/// How the pieces of a set are drawn on a board style.
struct PieceStyle {
    /// Folder of the SVG pieces under `:/resources/pieces`.
    QString set;
    /// Black pieces in pure black, as printing ink.
    bool solidBlack = false;
    /// What the set draws white — the inside of the white pieces, the
    /// highlights of the black ones — in this colour instead, the paper of a
    /// printed diagram; invalid for white.
    QColor paper = {};
};

/// Paints chess pieces: the SVG set of the board style (BoardTheme) when Qt
/// SVG is available, outline font glyphs otherwise. Shared by the board and small piece views.
namespace PieceRenderer {

/// Paints `piece` filling the square `rect`, sharp at `devicePixelRatio`.
void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio);
/// Same, with the pieces of `style` whatever the board style (a preview of another style).
void paint(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio, const PieceStyle &style);

/// Paints `piece` greyed out: in a narrow band of greys set off from
/// `background` (the colour behind it), readable but with little contrast, so
/// it is there without drawing the eye (the captured pieces by the board).
void paintMuted(QPainter &painter, Piece piece, const QRectF &rect, qreal devicePixelRatio,
                const QColor &background);

} // namespace PieceRenderer
