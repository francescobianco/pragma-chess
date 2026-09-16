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
    QList<MoveRecord> moves;
};
