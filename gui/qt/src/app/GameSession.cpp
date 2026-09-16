#include "GameSession.h"

#include <QtGlobal>

GameSession::GameSession(QObject *parent)
    : QObject(parent)
{
    m_positions << BoardState::startingPosition();
}

void GameSession::setGame(const GameRecord &game)
{
    m_game = game;
    m_positions.clear();

    std::optional<BoardState> start = game.startFen.isEmpty()
        ? BoardState::startingPosition()
        : BoardState::fromFen(game.startFen);
    m_positions << start.value_or(BoardState::startingPosition());

    for (const MoveRecord &move : std::as_const(m_game.moves)) {
        BoardState next = m_positions.last();
        if (!next.applyUci(move.uci))
            break;
        m_positions << next;
    }
    m_game.moves.resize(m_positions.size() - 1);

    m_ply = 0;
    Q_EMIT gameChanged();
    Q_EMIT plyChanged(m_ply);
}

void GameSession::setHeader(const GameRecord &header)
{
    const QList<MoveRecord> moves = m_game.moves;
    const QString startFen = m_game.startFen;
    m_game = header;
    m_game.moves = moves;
    m_game.startFen = startFen;
    m_game.plyCount = int(moves.size());
    Q_EMIT headerChanged();
}

int GameSession::lastMoveFrom() const
{
    if (m_ply == 0)
        return -1;
    return BoardState::squareFromName(QStringView(m_game.moves.at(m_ply - 1).uci).mid(0, 2));
}

int GameSession::lastMoveTo() const
{
    if (m_ply == 0)
        return -1;
    return BoardState::squareFromName(QStringView(m_game.moves.at(m_ply - 1).uci).mid(2, 2));
}

void GameSession::goToPly(int ply)
{
    ply = qBound(0, ply, plyCount());
    if (ply == m_ply)
        return;
    m_ply = ply;
    Q_EMIT plyChanged(m_ply);
}
