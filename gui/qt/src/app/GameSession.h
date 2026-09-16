#pragma once

#include "BoardState.h"
#include "GameRecord.h"

#include <QList>
#include <QObject>

/// The game currently open in a window and the ply being viewed.
class GameSession : public QObject {
    Q_OBJECT

public:
    explicit GameSession(QObject *parent = nullptr);

    void setGame(const GameRecord &game);
    /// Updates players, event, date, … of the open game, keeping moves and ply.
    void setHeader(const GameRecord &header);
    const GameRecord &game() const { return m_game; }

    int ply() const { return m_ply; }
    int plyCount() const { return int(m_positions.size()) - 1; }
    const BoardState &board() const { return m_positions.at(m_ply); }
    const BoardState &initialBoard() const { return m_positions.first(); }

    /// Squares of the move that led to the current ply, or -1 at the start.
    int lastMoveFrom() const;
    int lastMoveTo() const;

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
    QList<BoardState> m_positions;
    int m_ply = 0;
};
