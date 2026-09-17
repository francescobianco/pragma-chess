#pragma once

#include "BoardState.h"
#include "GameRecord.h"

#include <QHash>
#include <QString>

#include <optional>

/// Who a player of a database is to the user, as the user said
/// ("Who Is This?" on a player of the games list).
enum class PlayerRole { None, Me, Friend, Opponent };

/// Roles by player name, as the games store the names.
using PlayerRoles = QHash<QString, PlayerRole>;

/// "me", "friend", "opponent"; empty for None. How a database stores a role.
QString playerRoleKey(PlayerRole role);
PlayerRole playerRoleFromKey(const QString &key);

/// The side the user played in `game`: the one whose player is "me", when
/// exactly one side is.
std::optional<Side> mySide(const GameRecord &game, const PlayerRoles &roles);
