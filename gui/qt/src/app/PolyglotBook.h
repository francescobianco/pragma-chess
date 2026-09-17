#pragma once

#include "ChessPosition.h"

#include <QFile>
#include <QList>
#include <QString>

#include <array>
#include <optional>

/// An opening book in the Polyglot format (.bin), the one chess GUIs and
/// engines share: 16-byte big-endian entries sorted by position key, each a
/// move and its weight. The file is memory-mapped, so large books cost nothing
/// to open.
class PolyglotBook {
public:
    struct Move {
        ChessMove move;
        int weight = 0;
    };

    /// A move of a position with its weight, to write a book.
    struct Entry {
        quint64 key = 0;
        quint16 move = 0;
        quint16 weight = 0;
    };

    PolyglotBook() = default;
    PolyglotBook(const PolyglotBook &) = delete;
    PolyglotBook &operator=(const PolyglotBook &) = delete;

    bool open(const QString &path, QString *errorMessage);
    void close();
    bool isOpen() const { return m_data != nullptr; }
    QString path() const { return m_file.fileName(); }

    /// The legal book moves of a position, heaviest first.
    QList<Move> moves(const ChessPosition &position) const;

    /// The Polyglot key of a position.
    static quint64 key(const ChessPosition &position);
    /// A move as a book stores it; castling is the king taking its own rook.
    static quint16 encodeMove(const ChessPosition &position, const ChessMove &move);
    /// The legal move a book entry stores, if any.
    static std::optional<ChessMove> decodeMove(const ChessPosition &position, quint16 move);
    /// A book file: entries sorted by key, then heaviest move first.
    static QByteArray write(QList<Entry> entries);

private:
    static const std::array<quint64, 781> kRandom;

    QFile m_file;
    const uchar *m_data = nullptr;
    qint64 m_count = 0;
};
