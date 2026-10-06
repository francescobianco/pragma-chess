#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

/// A game being played online, as the platform tells it: who plays, the
/// moves so far, the clocks and how it ended. Pure; filled by the parsers
/// of each platform's stream (LichessBoardClient).
struct OnlineGame {
    QString id;
    QString white;
    QString black;
    int whiteRating = 0;
    int blackRating = 0;
    /// Empty for the standard start position.
    QString initialFen;
    /// The moves played, UCI, from the start.
    QStringList moves;
    /// "started" while it goes on; then "mate", "resign", "draw",
    /// "stalemate", "outoftime", "timeout", "aborted"… (lichess's words).
    QString status;
    /// "white" or "black" once there is one.
    QString winner;
    int whiteTimeMs = 0;
    int blackTimeMs = 0;
    bool rated = false;
    /// As PGN's TimeControl tag writes it: "300+3", "1/259200"; empty if unknown.
    QString timeControl;

    bool isOver() const { return !status.isEmpty() && status != QLatin1String("started") && status != QLatin1String("created"); }
    /// The PGN result of a finished game, "*" otherwise.
    QString result() const;
    /// What happened, for people: "Checkmate", "White resigned", …
    QString endText() const;
};

/// The lines of lichess's Board API streams, parsed. Each stream is NDJSON:
/// one JSON object a line, empty lines to keep the connection alive.
namespace LichessBoard {

/// The event stream (/api/stream/event): the id of a game that just started, if the line says so.
std::optional<QString> gameStarted(const QByteArray &line);

/// The game stream (/api/board/game/stream/{id}): applies a "gameFull" or
/// "gameState" line to `game`. Returns false for lines that are not about
/// the game's state (chat, keep-alives).
bool applyGameLine(const QByteArray &line, OnlineGame &game);

} // namespace LichessBoard
