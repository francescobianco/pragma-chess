#pragma once

#include "GameRecord.h"

#include <QString>

#include <optional>

/// The time control of a game, as PGN keeps it in its TimeControl tag
/// (games.tags: no column of its own): "600+5" is ten minutes and five
/// seconds a move, "40/7200:3600" forty moves in two hours then an hour,
/// "1/86400" a day a move (correspondence), "-" no clock. The imports set
/// it (lichess, chess.com), Game Information edits it, the tree filters by
/// it. Pure, unit-tested.
namespace TimeControl {

/// How fast a game is, as lichess counts it: the base time plus forty
/// increments.
enum class Speed { Unknown, UltraBullet, Bullet, Blitz, Rapid, Classical, Correspondence, Unlimited };

/// The game's TimeControl tag; empty when it has none, or "?".
QString of(const GameRecord &game);
/// Sets the tag (empty takes it away), in place if the game has it.
void set(GameRecord &game, const QString &value);

Speed speed(const QString &value);
/// "Blitz", "Classical"… in the interface language; empty for Unknown.
QString speedName(Speed speed);
/// How long a player has for forty moves, in seconds: what orders time
/// controls from the fastest; correspondence and no clock come last.
qint64 estimatedSeconds(const QString &value);

/// As players say it: "3+2", "90+30", "½+0", "40/120:60+30", "1 day";
/// minutes, then the seconds added a move.
QString shortText(const QString &value);
/// The tree's name for it: "Blitz 3+2", "Correspondence 1 day".
QString label(const QString &value);

/// The text Game Information shows for the tag, which fromInput() reads
/// back the same: "90+30" for a base and an increment; the tag as it is for
/// periods of moves, correspondence or no clock.
QString inputText(const QString &value);
/// What the user typed, as players say it (shortText's form: "3+2",
/// "90+30", "0.5+0"), or already in PGN's form when it holds "/" or ":",
/// or "-"; as the tag's value. Empty text is no time control (an empty
/// string); nothing when it cannot be read.
std::optional<QString> fromInput(const QString &text);

} // namespace TimeControl
