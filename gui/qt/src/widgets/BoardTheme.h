#pragma once

#include <QColor>
#include <QList>
#include <QString>

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

    static constexpr const char *kDefaultId = "pragma-classic";

    /// Every style, the default first.
    static const QList<BoardTheme> &all();
    /// The style with `id`, or the default for an unknown or empty id.
    static const BoardTheme &byId(const QString &id);

    static const BoardTheme &current();
    /// Makes `id` the style every board paints with; the caller repaints them.
    static void setCurrent(const QString &id);
};
