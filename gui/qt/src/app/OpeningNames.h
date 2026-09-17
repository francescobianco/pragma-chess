#pragma once

#include "ChessPosition.h"
#include "GameRecord.h"

#include <QHash>
#include <QList>
#include <QString>

/// The names of openings and variations, read from a database of games where
/// each game is a named line: Event holds the name, ECO the code and the moves
/// lead to the position the name belongs to. Names live in an ordinary .pdb, so
/// they can be edited like any database and kept in one per language.
class OpeningNames {
public:
    struct Name {
        QString eco;
        QString name;

        bool isEmpty() const { return name.isEmpty(); }
    };

    OpeningNames() = default;
    /// Indexes the final position of each game; where lines transpose, the
    /// shortest one names the position.
    explicit OpeningNames(const QList<GameRecord> &games);

    bool isEmpty() const { return m_names.isEmpty(); }
    qsizetype size() const { return m_names.size(); }

    /// The name of exactly this position, if a line ends there.
    Name name(const ChessPosition &position) const;

    /// Named lines from the TSV files of lichess-org/chess-openings (columns
    /// eco, name, pgn), as games for a names database.
    static QList<GameRecord> gamesFromTsv(const QString &tsv);

private:
    struct Entry {
        Name name;
        int plies = 0;
    };
    QHash<quint64, Entry> m_names;
};
