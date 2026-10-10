#pragma once

#include <QString>

#include <optional>

/// The lines of the Free Internet Chess Server (freechess.org), parsed: what
/// FicsClient reads from its telnet session. FICS speaks text, one line a
/// message; the board comes as a "style 12" line after every move. Pure,
/// unit-tested with lines recorded from the server.
namespace Fics {

/// A board update ("<12> rnbqkbnr pppppppp … "), as `set style 12` and
/// `iset ms 1` have the server send it.
struct Style12 {
    /// The 64 squares from a8 to h1, a letter for a piece ("-" for none).
    QString squares;
    bool whiteToMove = true;
    int game = 0;
    QString white;
    QString black;
    /// The user's relation to the game: 1 the user's move, -1 the
    /// opponent's, 0 observed; other values examine or set up.
    int relation = 0;
    int initialMinutes = 0;
    int incrementSeconds = 0;
    int whiteTimeMs = 0;
    int blackTimeMs = 0;
    /// The number of the move about to be made (1 at the start).
    int moveNumber = 1;
    /// The move just played, as FICS writes it in full ("P/e2-e4",
    /// "o-o", "P/e7-e8=Q"), "none" at the start.
    QString verboseMove;

    bool isPlayed() const { return relation == 1 || relation == -1; }
    /// Plies played since the start, from the move number and the side to move.
    int plies() const { return (moveNumber - 1) * 2 + (whiteToMove ? 0 : 1); }
};

/// The update of a "<12>" line; none for any other line.
std::optional<Style12> parseStyle12(const QString &line);

/// The move just played (Style12::verboseMove) in UCI: "e2e4", "e1g1" for
/// White's short castling, "e7e8q"; empty for none. `whiteMoved` is the
/// side that played it.
QString uciOfVerbose(const QString &verbose, bool whiteMoved);

/// What to type for a UCI move, as FICS reads it: castling as "o-o" and
/// "o-o-o" (the king moving two files, `piece` the letter on its square),
/// a promotion as "e7e8=q", anything else as it is.
QString moveCommand(const QString &uci, QChar piece);

/// The players and their ratings when a game is created ("Creating:
/// GuestABCD (++++) frank (1650) unrated blitz 5 0"): 0 for none ("++++",
/// "----").
struct Creating {
    QString white;
    int whiteRating = 0;
    QString black;
    int blackRating = 0;
    bool rated = false;
};
std::optional<Creating> parseCreating(const QString &line);

/// The end of a game ("{Game 12 (frank vs. GuestABCD) frank resigns} 0-1"),
/// in OnlineGame's words: `status` ("mate", "resign", "outoftime",
/// "timeout", "stalemate", "draw", "aborted") and `winner` ("white",
/// "black", empty for none).
struct GameEnd {
    int game = 0;
    QString status;
    QString winner;
    QString reason; // As the server says it.
};
std::optional<GameEnd> parseGameEnd(const QString &line);

/// The name the server gives the session ("**** Starting FICS session as
/// GuestABCD(U) ****" gives "GuestABCD"); none for any other line.
std::optional<QString> parseSessionStart(const QString &line);

/// The name of whoever offers a draw ("frank offers you a draw."); none otherwise.
std::optional<QString> parseDrawOffer(const QString &line);

} // namespace Fics
