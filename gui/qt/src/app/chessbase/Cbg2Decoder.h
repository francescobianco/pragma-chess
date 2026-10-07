#pragma once

#include "CbgDecoder.h"

#include <QByteArray>
#include <QString>

/// The moves of one game of a ChessBase 17+ database (`.2cbg`, next to a
/// `.2cbh`). A record is framed — a magic number, the size of its content
/// and of spare room, a checksum, a tag (1 a normal game, 2 Chess960) — and
/// the content is a stream of little-endian 16-bit words. Unlike the older
/// `.cbg`, a move word does not depend on the position: the words enumerate
/// every move a piece could make from every square ("the white knight from
/// b1 to c3"), so a move decodes on its own. Markers in the stream: 0xFFFC
/// starts the moves, 0xFFFA is a null move, 0xFFFB a set-up position,
/// 0xFFFD after a move says that move has an alternative (pushing the
/// position before it), 0xFFFF ends a line — then the alternative follows,
/// from the position pushed. The main line comes first. Pure, unit-tested.
///
/// Written from the format's facts (TODO.md, "Formato ChessBase"), no code
/// of other readers.
namespace Cbg2Decoder {

/// The move a word stands for, in UCI ("g1f3", "e7e8q", "e1g1"); empty for
/// a marker or a word no move has.
QString moveOf(quint16 word);

/// The highest word that is a move: the table's size, for the tests.
int lastMoveWord();

/// The framed record at `offset` of the `.2cbg` file `data`: its content
/// (the words), and whether the game is Chess960 (`chess960` may be null).
/// Empty with `error` set when the frame is not there. The `.2cba`'s
/// records have the same frame.
QByteArray contentAt(const QByteArray &data, qint64 offset, bool *chess960, QString *error);

/// Decodes the words of a record's content: the main line and its
/// variations as UCI (SAN left empty), as CbgDecoder gives them. Set-up
/// positions are not read yet: they are reported as errors.
CbgDecoder::Decoded decode(const QByteArray &content);

} // namespace Cbg2Decoder
