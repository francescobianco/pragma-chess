#pragma once

#include "app/GameRecord.h"

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QString>

/// The annotations of one game of a ChessBase 17+ database (`.2cba`, next to
/// a `.2cbh`): framed as the `.2cbg`'s records, the content is a block per
/// position — the game's position as a PGN lists its moves, variations
/// included right after the move they replace, from 0, and -1 for the game
/// as a whole — each with its annotations, a 16-bit type and its data. They
/// carry no length, so every type met must be understood to reach the next.
///
/// What PGN can say is read: texts after and before a move (in the
/// interface's language when the comment has several), the symbols (NAGs),
/// coloured squares and arrows ("[%csl]", "[%cal]", as lichess writes them),
/// the engine's evaluation of a move ("[%eval]") and the time spent on it
/// ("[%emt]"). The rest is stepped over. Pure, unit-tested; the layout is in
/// TODO.md, "Formato ChessBase 17+".
namespace Cba2Decoder {

/// What ChessBase says about one position.
struct Notes {
    /// The text before the move.
    QString before;
    /// The text after the move, its commands ("[%eval 0.18]") after it.
    QString after;
    /// The symbols: on the move, on the position, and a prefix.
    QList<int> nags;
};

struct Decoded {
    /// By position, -1 for the game.
    QMap<int, Notes> positions;
    /// Why the record could not be read to its end, else empty; what was
    /// read before stays.
    QString error;
};

/// The language codes of the texts, as the interface names them ("en",
/// "de", …), for the code ChessBase stores; empty for "any language" (7)
/// and the codes not known.
QString languageOf(int code);

/// Decodes a record's content. Of the texts of a position in several
/// languages, those in `language` (an interface code, "it") are kept, else
/// the English ones, else the first language met; texts in any language
/// are kept with them.
Decoded decode(const QByteArray &content, const QString &language);

/// A text of the record as a person reads it: UTF-8, or Windows-1252 when
/// it is not (ChessBase does not say which, and writes both); ChessBase's
/// figurines as piece letters, its line breaks as new lines, the diagram
/// marker "[#]" gone.
QString text(const QByteArray &bytes);

/// Puts the notes on the moves of a line (and of its variations), counting
/// the positions as ChessBase does: each move, then the variations that
/// replace it. A text before a move goes after the move before it, or
/// before the line when it is the first; the game's own notes go before the
/// main line. `startComment` is the line's.
void apply(QList<MoveRecord> &moves, QList<Variation> &variations, QString &startComment, const Decoded &notes);

} // namespace Cba2Decoder
