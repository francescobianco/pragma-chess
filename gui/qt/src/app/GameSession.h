#pragma once

#include "ChessPosition.h"
#include "GameRecord.h"

#include <QList>
#include <QObject>

#include <optional>

/// The game currently open in a window and the ply being viewed.
class GameSession : public QObject {
    Q_OBJECT

public:
    explicit GameSession(QObject *parent = nullptr);

    /// Opens a game. Moves are replayed from the start position and the game
    /// is cut at the first illegal one; missing SAN is filled in.
    void setGame(const GameRecord &game);
    /// Updates players, event, date, … of the open game, keeping moves and ply.
    void setHeader(const GameRecord &header);
    const GameRecord &game() const { return m_game; }

    int ply() const { return m_ply; }
    int plyCount() const { return int(m_positions.size()) - 1; }
    const ChessPosition &position() const { return m_positions.at(m_ply); }
    const ChessPosition &positionAt(int ply) const { return m_positions.at(ply); }
    const ChessPosition &initialPosition() const { return m_positions.first(); }
    BoardState board() const { return position().boardState(); }

    /// The move that led to the current ply, if any.
    std::optional<ChessMove> lastMove() const;
    int lastMoveFrom() const;
    int lastMoveTo() const;

    /// Whether `move` is the move the game continues with from the current ply.
    bool isNextMove(const ChessMove &move) const;
    /// Plays a legal move at the current ply: steps forward if it is the next
    /// move of the game, otherwise replaces the moves after the current ply.
    /// Returns false if the move is illegal.
    bool playMove(const ChessMove &move);

    void goToPly(int ply);
    void goToStart() { goToPly(0); }
    void goToEnd() { goToPly(plyCount()); }
    void goForward() { goToPly(m_ply + 1); }
    void goBack() { goToPly(m_ply - 1); }

Q_SIGNALS:
    void gameChanged();
    void headerChanged();
    void plyChanged(int ply);

private:
    GameRecord m_game;
    QList<ChessPosition> m_positions;
    QList<ChessMove> m_moves;
    int m_ply = 0;
};
