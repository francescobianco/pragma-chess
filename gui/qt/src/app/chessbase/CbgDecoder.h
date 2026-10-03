#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

/// The moves of one game of a ChessBase database, as the `.cbg` file stores
/// them: a 4-byte head (flags and size), the start position when it is not
/// the initial one, then one byte per move, each naming a piece by its
/// number in the side's list of that kind and the way it goes — "the second
/// rook, three squares up" — through a table that also hides the bytes. The
/// decoder follows the lists and the board move by move and gives back the
/// main line as UCI moves. ChessBase writes the main line first and the
/// variations after it, each introduced by the end of the line it is an
/// alternative to, so the main line is everything before the first end of
/// line, and the variations are left where they are.
namespace CbgDecoder {

struct Decoded {
    /// Empty for the initial position.
    QString startFen;
    /// The main line, UCI; cut before a null move.
    QStringList uciMoves;
    /// Why the record could not be read, else empty; the moves read so far stay.
    QString error;
};

/// Decodes a whole record, head included. Only the common encoding (mode 0)
/// is read; other modes and Chess960 games are reported as errors.
Decoded decode(const QByteArray &record);

/// The size field of a record's head, so a caller can cut it out of the file.
int recordSize(const QByteArray &head);

} // namespace CbgDecoder
