#pragma once

#include "app/GameRecord.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

#include "Cba2Decoder.h"
#include "CbgDecoder.h"

/// A ChessBase database — the family of files named after its `.cbh` —
/// opened read-only, enough to import its games: the game headers (`.cbh`),
/// the players (`.cbp`), the tournaments (`.cbt`) and the moves (`.cbg`,
/// through CbgDecoder). Guiding texts and deleted games are skipped. The
/// format of ChessBase 17 and later, named after its `.2cbh`, is read by
/// ChessBase2Database behind the same interface: open() chooses.
class ChessBaseDatabase {
public:
    virtual ~ChessBaseDatabase() = default;

    /// Opens the database `cbhPath` names (a `.cbh` or a `.2cbh`); the other
    /// files are found beside it.
    static std::unique_ptr<ChessBaseDatabase> open(const QString &cbhPath, QString *errorMessage);

    /// Records in the database, games and texts alike, deleted ones included.
    virtual int count() const { return m_count; }

    /// What a record is, before its moves are read.
    struct Entry {
        bool isGame = false;
        bool deleted = false;
        /// Players, event, site, date, round, result, ratings and ECO.
        GameRecord header;
    };
    virtual Entry entry(int index) const;

    /// The game with its moves, SAN filled in and the line cut at the first
    /// move that is not legal; `errorMessage` says why the moves could not be
    /// read, when the header alone is returned.
    virtual GameRecord game(int index, QString *errorMessage) const;

    /// The player name as ChessBase writes it: "Lastname, Firstname".
    static QString playerName(const QString &last, const QString &first);
    /// A ChessBase date (bits: year, month, day) as PGN writes it.
    static QString dateText(quint32 date);

protected:
    ChessBaseDatabase() = default;

    /// Puts the decoded moves into `game`: SAN filled in, the main line cut
    /// at its first move that is not legal, the variations resolved.
    static void fillMoves(GameRecord &game, const CbgDecoder::Decoded &decoded, QString *errorMessage,
                          const Cba2Decoder::Decoded *notes = nullptr);
    /// The file beside `path` with another suffix, whatever its case.
    static QString sibling(const QString &path, const QString &suffix);

private:
    QString m_cbgPath;
    QByteArray m_headers;
    int m_count = 0;
    QStringList m_players;
    struct Tournament {
        QString title;
        QString place;
    };
    QList<Tournament> m_tournaments;
};
