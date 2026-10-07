#pragma once

#include "ChessBaseDatabase.h"

#include <QFile>

#include <memory>
#include <vector>

/// A database of ChessBase 17 and later — the family of files named after
/// its `.2cbh` — opened read-only behind ChessBaseDatabase's interface: the
/// game headers (`.2cbh`), the moves (`.2cbg`, through Cbg2Decoder) and the
/// players and tournaments (`.2lid`). The files are mapped, not read: in a
/// big database each is gigabytes, and a batch of the import reads a few
/// hundred records of them. Little-endian throughout, but for the header of
/// the `.2lid`; texts are UTF-8 with their length in front. The annotations
/// (`.2cba`, through Cba2Decoder) come with the moves, as PGN comments,
/// symbols and commands. The layout is in TODO.md, "Formato ChessBase".
class ChessBase2Database : public ChessBaseDatabase {
public:
    static std::unique_ptr<ChessBaseDatabase> open(const QString &path, QString *errorMessage);

    int count() const override { return m_count; }
    Entry entry(int index) const override;
    GameRecord game(int index, QString *errorMessage) const override;

private:
    ChessBase2Database() = default;

    /// A file and where it is mapped.
    struct Mapped {
        std::unique_ptr<QFile> file;
        const uchar *data = nullptr;
        qint64 size = 0;
        bool open(const QString &path);
        QByteArray bytes(qint64 offset, qint64 length) const;
    };

    /// The record of `index` in the `.2cbh` (the file's own header is record 0).
    const uchar *record(int index) const;
    /// An entity of the `.2lid`: its record's bytes, empty if there is none.
    QByteArray entity(int type, qint64 id) const;
    QString playerName(qint64 id) const;

    Mapped m_headers;
    Mapped m_moves;
    Mapped m_annotations;
    Mapped m_entities;
    int m_count = 0;
    int m_recordSize = 0;
    /// The `.2lid`: its header's size, and each type's container size and count.
    qint64 m_entityHeader = 0;
    std::vector<qint64> m_containerSizes;
    std::vector<qint64> m_entityCounts;
    qint64 m_blockSize = 0;
};
