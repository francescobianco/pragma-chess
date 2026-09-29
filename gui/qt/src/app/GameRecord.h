#pragma once

#include <QList>
#include <QString>

struct MoveRecord {
    QString san;
    QString uci;
};

/// A game as seen by the GUI. Header-only records (for game lists) leave
/// `moves` empty; `GameDatabase::loadGame` fills it.
struct GameRecord {
    qint64 id = 0;
    QString white;
    QString black;
    int whiteElo = 0;
    int blackElo = 0;
    QString event;
    QString site;
    QString date;
    QString round;
    QString result;
    QString eco;
    int plyCount = 0;
    /// Empty means the standard starting position.
    QString startFen;
    /// Universal id of the game (docs/phone-link.md, "Identity"): the same in
    /// every copy of the database, made from the content when the game is
    /// created and never changed. Empty until the game is stored.
    QString uid;
    /// When the game was created or last changed, ISO 8601 UTC; empty = never.
    QString modified;
    QList<MoveRecord> moves;
};

/// The moves of a stored game, for indexing positions and lines.
struct GameLine {
    qint64 id = 0;
    /// Empty means the standard starting position.
    QString startFen;
    /// UCI moves separated by spaces, as stored.
    QString movesUci;
    /// "1-0", "0-1", "1/2-1/2" or anything else for no result.
    QString result;
};
