#pragma once

#include "ChessPosition.h"
#include "GameRecord.h"

#include <QList>
#include <QObject>

#include <optional>

/// The game currently open in a window and the ply being viewed.
///
/// A game is a tree (GameVariations): the main line and, off its moves, the
/// variations. The session follows one line at a time — the main line, or
/// the main line up to a branch and the variation taken, and so on (`path`)
/// — and the ply is a place on that line. Playing a move that is not the next
/// one of the line takes the variation that begins with it, or starts a new
/// variation there: a game's moves are never overwritten.
class GameSession : public QObject {
    Q_OBJECT

public:
    explicit GameSession(QObject *parent = nullptr);

    /// Opens a game. Moves are replayed from the start position and the game
    /// is cut at the first illegal one; missing SAN is filled in. Variations
    /// are resolved the same way (GameVariations::resolve).
    void setGame(const GameRecord &game);
    /// `game` as setGame() makes it: replayed, cut at its first illegal move,
    /// SAN filled in, variations resolved. For games shown but not opened.
    static GameRecord resolved(const GameRecord &game);
    /// Updates players, event, date, … of the open game, keeping moves and ply.
    void setHeader(const GameRecord &header);
    /// Replaces the open game's other PGN tags (TimeControl, StudyName…),
    /// which setHeader() keeps as they are.
    void setTags(const QList<PgnTag> &tags);
    const GameRecord &game() const { return m_game; }

    /// The line being followed: the variation taken at each branch from the
    /// main line down, empty for the main line.
    const QList<int> &path() const { return m_path; }
    /// The moves of that line, from the game's first move.
    const QList<MoveRecord> &lineMoves() const { return m_line; }
    /// The move that leads to `ply` on that line (1 to plyCount()).
    const MoveRecord &moveAt(int ply) const { return m_line.at(ply - 1); }
    /// The ply at which the line branched off its parent, 0 for the main line.
    int branchPly() const { return m_branchPly; }

    int ply() const { return m_ply; }
    int plyCount() const { return int(m_positions.size()) - 1; }
    const ChessPosition &position() const { return m_positions.at(m_ply); }
    const ChessPosition &positionAt(int ply) const { return m_positions.at(ply); }
    const ChessPosition &initialPosition() const { return m_positions.first(); }
    BoardState board() const { return position().boardState(); }
    /// The moves played from the initial position up to the current ply.
    QList<ChessMove> movesToHere() const { return m_moves.first(m_ply); }

    /// The move that led to the current ply, if any.
    std::optional<ChessMove> lastMove() const;
    int lastMoveFrom() const;
    int lastMoveTo() const;

    /// Whether `move` is the move the line continues with from the current ply.
    bool isNextMove(const ChessMove &move) const;
    /// Plays a legal move at the current ply: steps forward if it is the next
    /// move of the line, takes the variation that begins with it if there is
    /// one, appends it at the end of the line, or else starts a variation
    /// with it. Returns false if the move is illegal. A null move is played
    /// wherever the side to move may pass.
    bool playMove(const ChessMove &move);
    /// Whether playMove() would start a new variation with `move`: a legal
    /// move other than the next one, in the middle of a line, that no
    /// variation there begins with. The board then asks what to do with it.
    bool wouldBranch(const ChessMove &move) const;
    /// Plays `move` in place of the rest of the line: the line's moves after
    /// the current ply, and the variations hanging off them, are gone; the
    /// alternatives to the move replaced stay, as alternatives to this one.
    /// Returns false if the move is illegal.
    bool replaceLine(const ChessMove &move);
    /// Annotates the move that leads to `ply` (1 to plyCount()) of the line
    /// with these NAGs (MoveAnnotation), in place of the ones it had.
    void setAnnotations(int ply, const QList<int> &nags);
    /// Sets the comment of the line `path` after its own move `index`, or
    /// before its first move for 0 (MoveComment::at); any line of the game,
    /// not only the one followed.
    void setComment(const QList<int> &path, int index, const QString &comment);

    void goToPly(int ply);
    /// Follows another line of the game and goes to `ply` on it.
    void goToLine(const QList<int> &path, int ply);
    void goToStart() { goToPly(0); }
    void goToEnd() { goToPly(plyCount()); }
    void goForward() { goToPly(m_ply + 1); }
    void goBack() { goToPly(m_ply - 1); }

Q_SIGNALS:
    /// The moves of the game changed (a game opened, a move or a variation added).
    void gameChanged();
    void headerChanged();
    /// The annotations of the move leading to `ply` of the line changed.
    void annotationsChanged(int ply);
    /// A comment of the game changed.
    void commentsChanged();
    /// The ply changed; so may the line (path()).
    void plyChanged(int ply);

private:
    /// Whether `move` can be played from `position`: legal, or a null move
    /// where the side may pass.
    static bool isPlayable(const ChessPosition &position, const ChessMove &move);
    /// Replays the line `path` leads to and makes it the one followed.
    void followLine(const QList<int> &path);
    /// The variations hanging off the line followed.
    QList<Variation> &lineVariations();
    /// The moves of the line followed, where its own moves start (the main
    /// line, or the variation taken last).
    QList<MoveRecord> &ownMoves();

    GameRecord m_game;
    QList<int> m_path;
    QList<MoveRecord> m_line;
    int m_branchPly = 0;
    QList<ChessPosition> m_positions;
    QList<ChessMove> m_moves;
    int m_ply = 0;
};
