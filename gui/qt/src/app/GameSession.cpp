#include "GameSession.h"

#include <QtGlobal>

GameSession::GameSession(QObject *parent)
    : QObject(parent)
{
    m_positions << ChessPosition::startingPosition();
}

void GameSession::setGame(const GameRecord &game)
{
    m_game = game;
    m_positions.clear();
    m_moves.clear();

    std::optional<ChessPosition> start = game.startFen.isEmpty()
        ? ChessPosition::startingPosition()
        : ChessPosition::fromFen(game.startFen);
    m_positions << start.value_or(ChessPosition::startingPosition());

    for (MoveRecord &record : m_game.moves) {
        const ChessPosition &current = m_positions.last();
        const std::optional<ChessMove> move = current.moveFromUci(record.uci);
        if (!move)
            break;
        if (record.san.isEmpty())
            record.san = current.san(*move);
        ChessPosition next = current;
        next.play(*move);
        m_moves << *move;
        m_positions << next;
    }
    m_game.moves.resize(m_moves.size());
    m_game.plyCount = int(m_moves.size());

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

std::optional<ChessMove> GameSession::lastMove() const
{
    if (m_ply == 0)
        return std::nullopt;
    return m_moves.at(m_ply - 1);
}

int GameSession::lastMoveFrom() const
{
    return m_ply == 0 ? -1 : m_moves.at(m_ply - 1).from;
}

int GameSession::lastMoveTo() const
{
    return m_ply == 0 ? -1 : m_moves.at(m_ply - 1).to;
}

bool GameSession::isNextMove(const ChessMove &move) const
{
    return m_ply < plyCount() && m_moves.at(m_ply) == move;
}

bool GameSession::playMove(const ChessMove &move)
{
    if (isNextMove(move)) {
        goForward();
        return true;
    }
    const ChessPosition &current = position();
    if (!current.isLegal(move))
        return false;

    ChessPosition next = current;
    next.play(move);
    const MoveRecord record{current.san(move), move.uci()};

    m_positions.resize(m_ply + 1);
    m_moves.resize(m_ply);
    m_game.moves.resize(m_ply);
    m_positions << next;
    m_moves << move;
    m_game.moves << record;
    m_game.plyCount = int(m_moves.size());
    ++m_ply;

    Q_EMIT gameChanged();
    Q_EMIT plyChanged(m_ply);
    return true;
}

void GameSession::goToPly(int ply)
{
    ply = qBound(0, ply, plyCount());
    if (ply == m_ply)
        return;
    m_ply = ply;
    Q_EMIT plyChanged(m_ply);
}
