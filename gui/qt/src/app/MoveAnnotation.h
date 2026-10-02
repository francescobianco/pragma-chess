#pragma once

#include <QList>
#include <QString>

/// The symbols a move is annotated with ("!", "??", "±"…): the Numeric
/// Annotation Glyphs of PGN that people write as a symbol. A move carries at
/// most one of each kind, kept as its NAG numbers, the move's judgement first.
namespace MoveAnnotation {

enum class Kind {
    Move,     ///< A judgement of the move: "!", "?"…
    Position, ///< An assessment of the position after it: "±", "∞"…
};

struct Glyph {
    /// The number of `$n` in PGN.
    int nag;
    QString symbol;
    Kind kind;
};

/// Every glyph in the order menus list them: the moves, then the positions.
const QList<Glyph> &glyphs();
/// The glyph of a NAG, or nullptr for one that has no symbol here.
const Glyph *glyph(int nag);
/// What a glyph says, in the interface language: "good move".
QString meaning(int nag);

/// The glyphs among `nags`, one of each kind (the last one wins), move first.
QList<int> normalized(const QList<int> &nags);
/// `nags` with `nag` put in the place of the other glyph of its kind, or
/// taken out if it was there: what choosing it in a menu does.
QList<int> toggled(const QList<int> &nags, int nag);

/// As shown after the move: "!", "! ±", " ±".
QString symbols(const QList<int> &nags);
/// As written after the SAN in PGN: "!", "! $16", " $16". Only "!" and "?"
/// are suffixes there, every other glyph is a `$n`.
QString pgnSuffix(const QList<int> &nags);
/// As stored, glued to the SAN so that a move stays one word: "!$16".
QString storedSuffix(const QList<int> &nags);
/// Takes the annotations off a stored or pasted move: "Nf3!$16" gives "Nf3"
/// and adds 1 and 16 to `nags`.
QString split(const QString &token, QList<int> *nags);

} // namespace MoveAnnotation
