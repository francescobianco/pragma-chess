#pragma once

#include "GameState.h"

#include <QList>
#include <QString>

struct MoveRecord {
    QString san;
    QString uci;
    /// Annotations of the move ("!", "±"…) as PGN NAGs, see MoveAnnotation.
    QList<int> nags = {};
    /// The comment after the move, as PGN's braces hold it: text, and the
    /// commands sites put there ("[%eval 0.18]"), see MoveComment.
    QString comment = {};
};

/// A PGN tag the game's fields do not hold: "StudyName", "ChapterURL",
/// "TimeControl"… Kept in the order the game came with.
struct PgnTag {
    QString name;
    QString value;
    bool operator==(const PgnTag &) const = default;
};

/// A line of moves that branches off another: an alternative to the move
/// at `atPly` (1-based) of the line it belongs to, played from the position
/// before that move. Its own alternatives hang off it the same way, so the
/// game is a tree whose trunk is GameRecord::moves. See GameVariations.
struct Variation {
    int atPly = 0;
    QList<MoveRecord> moves;
    QList<Variation> variations;
    /// The comment before the variation's first move.
    QString startComment = {};
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
    /// How the game begins, as a numbered line in letters ("1.e4 e5 2.Nf3…",
    /// ending in "…" when the game goes on): what lists show without loading
    /// the moves. Filled in for stored games (Pgn::preview).
    QString linePreview;
    /// Empty means the standard starting position.
    QString startFen;
    /// Universal id of the game (docs/phone-link.md, "Identity"): the same in
    /// every copy of the database, made from the content when the game is
    /// created and never changed. Empty until the game is stored.
    QString uid;
    /// When the game was created or last changed, ISO 8601 UTC; empty = never.
    QString modified;
    /// Trashed and deleted games stay in the database, out of the lists.
    GameState state = GameState::Live;
    /// When the game was put where it is (trashed, restored, deleted), ISO
    /// 8601 UTC; empty for a game that was never in the trash.
    QString stateModified;
    QList<MoveRecord> moves;
    /// Alternatives to moves of the main line, in the order they are shown.
    QList<Variation> variations;
    /// The comment before the first move (a chapter's introduction).
    QString startComment;
    /// The PGN tags that have no field of their own.
    QList<PgnTag> tags;
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
