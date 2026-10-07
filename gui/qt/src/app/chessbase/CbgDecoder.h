#pragma once

#include "app/GameRecord.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

/// The moves of one game of a ChessBase database, as the `.cbg` file stores
/// them: a 4-byte head (flags and size), the start position when it is not
/// the initial one, then one byte per move, each naming a piece by its
/// number in the side's list of that kind and the way it goes — "the second
/// rook, three squares up" — through a table that also hides the bytes. The
/// decoder follows the lists and the board move by move and gives back the
/// game tree as UCI moves. ChessBase writes the main line first: a branch
/// code opens a block holding the continuation of the line up to the
/// matching end code, and what follows that end is the alternative to the
/// block's first move, which lasts to the end of the line it belongs to.
namespace CbgDecoder {

struct Decoded {
    /// Empty for the initial position.
    QString startFen;
    /// The main line, UCI; a null move is "0000".
    QStringList uciMoves;
    /// The variations, UCI only (SAN empty), as GameRecord holds them.
    QList<Variation> variations;
    /// Why the record could not be read, else empty; the moves read so far stay.
    QString error;
};

/// Decodes a whole record, head included. Only the common encoding (mode 0)
/// is read; other modes and Chess960 games are reported as errors.
Decoded decode(const QByteArray &record);

/// The size field of a record's head, so a caller can cut it out of the file.
int recordSize(const QByteArray &head);

} // namespace CbgDecoder
