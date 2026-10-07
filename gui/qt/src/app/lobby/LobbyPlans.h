#pragma once

#include "app/BoardState.h"
#include "app/GameRecord.h"
#include "app/lobby/Lobby.h"

/// Between a lobby game and the game on the board: the board's game made
/// from it, and the plan the user prepared on the board.
namespace LobbyPlans {

/// What the user prepared on the board after the moves the game has.
struct Prepared {
    /// Their answers: in the position each line leads to, their move.
    LobbyPlan plan;
    /// Answers left out: a second move of theirs for a position that has
    /// one already (a variation of their own move; the line's move wins).
    int ignored = 0;
};

/// The user's plan in `game` (the board's, variations included) after its
/// first `fromPly` plies: the moves of `me` are answers, the opponent's
/// moves — the line's and its variations — are the cases they answer.
Prepared prepared(const GameRecord &game, int fromPly, Side me);

/// Game `index` of `room` as a game for the board: players, the room as
/// its event, the moves, and a uid that tells it is that game.
GameRecord record(const LobbyRoom &room, int index);

} // namespace LobbyPlans
