#pragma once

#include "PieceRenderer.h"

#include "app/BoardState.h"

#include <QColor>
#include <QList>
#include <QRectF>
#include <QString>

class QPainter;

/// A board style: the colours of the squares and the set of pieces drawn on
/// them, chosen as one in Options ▸ Personal Settings…. Every board of the
/// client (the main board, the position editor, the captured pieces) paints
/// with the current one.
struct BoardTheme {
    /// Kept in the personal settings; never changes once released.
    QString id;
    /// Shown in the interface: a proper name, not translated.
    QString name;
    QColor lightSquare;
    QColor darkSquare;
    /// Folder of the SVG pieces under `:/resources/pieces`.
    QString pieceSet;
    /// Dark squares drawn as the diagonal hatching of chess books, in `ink`
    /// on the paper of `darkSquare`, instead of a flat colour.
    bool hatched = false;
    /// The ink of the hatching (hatched styles).
    QColor ink = {};
    /// Black pieces in solid black, as printing ink, rather than the
    /// charcoal their set draws them in.
    bool solidBlack = false;
    /// Whether the white of the pieces is the paper of the light squares.
    bool paperWhite = false;

    /// How this style draws its pieces.
    PieceStyle pieceStyle() const { return {pieceSet, solidBlack, paperWhite ? lightSquare : QColor()}; }
    /// Whether the files and ranks are written on the squares: a book's
    /// diagram has none.
    bool showsCoordinates() const { return !hatched; }

    /// The colour of a coordinate written on a light or a dark square.
    QColor coordinateColor(bool onLightSquare) const;

    /// A piece standing on a dark square: a hatched style keeps its lines
    /// off it, as a printed diagram leaves white around each piece.
    struct PlacedPiece {
        QRectF rect;
        Piece piece;
    };
    /// Paints the squares of `board` (the whole area): the light colour, the
    /// `darkSquares` over it, and around `piecesOnDark` the halo of a hatched style.
    void paintSquares(QPainter &painter, const QRectF &board, const QList<QRectF> &darkSquares,
                      const QList<PlacedPiece> &piecesOnDark, qreal devicePixelRatio) const;

    static constexpr const char *kDefaultId = "pragma-classic";

    /// Every style, the default first.
    static const QList<BoardTheme> &all();
    /// The style with `id`, or the default for an unknown or empty id.
    static const BoardTheme &byId(const QString &id);

    static const BoardTheme &current();
    /// Makes `id` the style every board paints with; the caller repaints them.
    static void setCurrent(const QString &id);
};
