#include "LobbyPlans.h"

#include "app/ChessPosition.h"

#include <QCoreApplication>
#include <QDate>

namespace {

struct Text {
    Q_DECLARE_TR_FUNCTIONS(LobbyPlans)
};

struct Walk {
    int fromPly;
    Side me;
    Side firstMover;
    LobbyPlans::Prepared result;

    Side moverOf(int ply) const { return (ply % 2 == 1) == (firstMover == Side::White) ? Side::White : Side::Black; }

    /// `moves` are the moves of a line from ply `firstPly` on, played after `before`.
    void line(QStringList before, const QList<MoveRecord> &moves, const QList<Variation> &variations, int firstPly)
    {
        for (int i = 0; i < moves.size(); ++i) {
            const int ply = firstPly + i;
            if (ply > fromPly && moverOf(ply) == me) {
                const QString key = LobbyGame::lineKey(before);
                const auto known = result.plan.constFind(key);
                if (known == result.plan.cend())
                    result.plan.insert(key, moves.at(i).uci);
                else if (known.value() != moves.at(i).uci)
                    ++result.ignored;
            }
            // The variations replacing this move: other cases (the opponent's)
            // or another move of the user's (left out: the line's comes first).
            if (ply > fromPly) {
                for (const Variation &variation : variations) {
                    if (variation.atPly == i + 1)
                        line(before, variation.moves, variation.variations, ply);
                }
            }
            before << moves.at(i).uci;
        }
    }
};

} // namespace

namespace LobbyPlans {

Prepared prepared(const GameRecord &game, int fromPly, Side me)
{
    const std::optional<ChessPosition> start = game.startFen.isEmpty()
        ? std::optional<ChessPosition>(ChessPosition::startingPosition())
        : ChessPosition::fromFen(game.startFen);
    Walk walk{fromPly, me, start ? start->sideToMove() : Side::White, {}};
    walk.line({}, game.moves, game.variations, 1);
    return walk.result;
}

GameRecord record(const LobbyRoom &room, int index)
{
    const LobbyGame &game = room.games.at(index);
    GameRecord record;
    record.uid = QStringLiteral("lobby:%1:%2").arg(room.seed).arg(index);
    record.white = game.white;
    record.black = game.black;
    record.event = room.name();
    record.site = Text::tr("Pragma Chess Lobby");
    record.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    record.result = game.result;
    for (const QString &uci : game.moves)
        record.moves << MoveRecord{QString(), uci}; // SAN is filled in when it is opened.
    return record;
}

} // namespace LobbyPlans
