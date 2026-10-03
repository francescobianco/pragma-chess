#pragma once

#include "ChessPosition.h"

#include <QFile>
#include <QList>
#include <QString>

#include <array>
#include <optional>

/// An opening book in the Polyglot format (.bin), the one chess GUIs and
/// engines share: 16-byte big-endian entries sorted by position key, each a
/// move, its weight and 32 "learn" bits. The file is memory-mapped, so large
/// books cost nothing to open.
///
/// The format leaves `learn` to the program; engines and other GUIs write it
/// as zero and ignore it, so our marks there keep the book usable by them:
/// bit 0 is kLearnRepertoire, the others are reserved (keep them as read).
class PolyglotBook {
public:
    /// The user's repertoire: listed first whatever the weight.
    static constexpr quint32 kLearnRepertoire = 0x1;

    struct Move {
        ChessMove move;
        int weight = 0;
        quint32 learn = 0;

        bool inRepertoire() const { return learn & kLearnRepertoire; }
    };

    /// A move of a position with its weight, to write a book.
    struct Entry {
        quint64 key = 0;
        quint16 move = 0;
        quint16 weight = 0;
        quint32 learn = 0;
    };

    PolyglotBook() = default;
    PolyglotBook(const PolyglotBook &) = delete;
    PolyglotBook &operator=(const PolyglotBook &) = delete;

    bool open(const QString &path, QString *errorMessage);
    void close();
    bool isOpen() const { return m_data != nullptr; }
    QString path() const { return m_file.fileName(); }

    /// The legal book moves of a position: repertoire moves first, then heaviest first.
    QList<Move> moves(const ChessPosition &position) const;
    /// Marks a book move of a position as part of the user's repertoire (or
    /// not), in the file itself. False if the move is not in the book or the
    /// file cannot be written.
    bool setInRepertoire(const ChessPosition &position, const ChessMove &move, bool inRepertoire,
                         QString *errorMessage);
    /// Writes new weights for the book moves of a position, in the file
    /// itself: `moves` as moves() gave them, with the weights wanted. A move
    /// the book repeats keeps its weight in the first entry, the others go to
    /// zero. False if a move is not in the book or the file cannot be written.
    bool setWeights(const ChessPosition &position, const QList<Move> &moves, QString *errorMessage);

    /// The Polyglot key of a position.
    static quint64 key(const ChessPosition &position);
    /// A move as a book stores it; castling is the king taking its own rook.
    static quint16 encodeMove(const ChessPosition &position, const ChessMove &move);
    /// The legal move a book entry stores, if any.
    static std::optional<ChessMove> decodeMove(const ChessPosition &position, quint16 move);
    /// A book file: entries sorted by key, then heaviest move first.
    static QByteArray write(QList<Entry> entries);

private:
    /// Index of the first entry of `key` (or where it would be).
    qint64 lowerBound(quint64 key) const;

    static const std::array<quint64, 781> kRandom;

    QFile m_file;
    const uchar *m_data = nullptr;
    qint64 m_count = 0;
};
