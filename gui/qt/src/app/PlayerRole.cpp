#include "PlayerRole.h"

QString playerRoleKey(PlayerRole role)
{
    switch (role) {
    case PlayerRole::Me:
        return QStringLiteral("me");
    case PlayerRole::Friend:
        return QStringLiteral("friend");
    case PlayerRole::Opponent:
        return QStringLiteral("opponent");
    case PlayerRole::None:
        break;
    }
    return QString();
}

PlayerRole playerRoleFromKey(const QString &key)
{
    for (PlayerRole role : {PlayerRole::Me, PlayerRole::Friend, PlayerRole::Opponent}) {
        if (playerRoleKey(role) == key)
            return role;
    }
    return PlayerRole::None;
}

std::optional<Side> mySide(const GameRecord &game, const PlayerRoles &roles)
{
    const bool white = roles.value(game.white) == PlayerRole::Me;
    const bool black = roles.value(game.black) == PlayerRole::Me;
    if (white == black)
        return std::nullopt; // Neither, or a game against myself.
    return white ? Side::White : Side::Black;
}
