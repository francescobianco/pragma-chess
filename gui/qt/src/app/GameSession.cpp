#include "GameSession.h"

#include "GameVariations.h"
#include "MoveAnnotation.h"
#include "MoveComment.h"

#include <QtGlobal>

GameSession::GameSession(QObject *parent)
    : QObject(parent)
{
    m_positions << ChessPosition::startingPosition();
}

GameRecord GameSession::resolved(const GameRecord &game)
{
    GameRecord result = game;
    const std::optional<ChessPosition> start = game.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                        : ChessPosition::fromFen(game.startFen);
    const ChessPosition initial = start.value_or(ChessPosition::startingPosition());

    // The main line is cut at the first illegal move; the variations are
    // resolved from the positions it goes through.
    ChessPosition position = initial;
    qsizetype legal = 0;
    for (MoveRecord &record : result.moves) {
        std::optional<ChessMove> move = position.moveFromUci(record.uci);
        if (!move && !record.san.isEmpty())
            move = position.moveFromSan(record.san);
        if (!move)
            break;
        record.uci = move->uci();
        record.san = position.san(*move);
        position.play(*move);
        ++legal;
    }
    result.moves.resize(legal);
    result.plyCount = int(legal);
    GameVariations::resolve(result, initial);
    return result;
}

void GameSession::setGame(const GameRecord &game)
{
    m_game = resolved(game);
    followLine({});
    m_ply = 0;
    Q_EMIT gameChanged();
    Q_EMIT plyChanged(m_ply);
}

void GameSession::setHeader(const GameRecord &header)
{
    const QList<MoveRecord> moves = m_game.moves;
    const QList<Variation> variations = m_game.variations;
    const QString startFen = m_game.startFen;
    const QString startComment = m_game.startComment;
    const QList<PgnTag> tags = m_game.tags; // The header shows the fields, not these.
    m_game = header;
    m_game.moves = moves;
    m_game.variations = variations;
    m_game.startFen = startFen;
    m_game.startComment = startComment;
    m_game.tags = tags;
    m_game.plyCount = int(moves.size());
    Q_EMIT headerChanged();
}

void GameSession::followLine(const QList<int> &path)
{
    m_path = GameVariations::variationsOf(m_game, path) ? path : QList<int>();
    m_line = GameVariations::lineMoves(m_game, m_path);
    m_branchPly = GameVariations::branchPly(m_game, m_path);
    const std::optional<ChessPosition> start = m_game.startFen.isEmpty() ? ChessPosition::startingPosition()
                                                                          : ChessPosition::fromFen(m_game.startFen);
    m_positions = {start.value_or(ChessPosition::startingPosition())};
    m_moves.clear();
    for (qsizetype i = 0; i < m_line.size(); ++i) {
        const std::optional<ChessMove> move = m_positions.last().moveFromUci(m_line.at(i).uci);
        if (!move) {
            m_line.resize(i); // Resolved lines are legal; this is only a guard.
            break;
        }
        ChessPosition next = m_positions.last();
        next.play(*move);
        m_moves << *move;
        m_positions << next;
    }
}

QList<Variation> &GameSession::lineVariations()
{
    return *GameVariations::variationsOf(m_game, m_path);
}

QList<MoveRecord> &GameSession::ownMoves()
{
    QList<MoveRecord> *moves = &m_game.moves;
    QList<Variation> *variations = &m_game.variations;
    for (const int index : m_path) {
        moves = &(*variations)[index].moves;
        variations = &(*variations)[index].variations;
    }
    return *moves;
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
    const int ply = m_ply;
    const MoveRecord record{current.san(move), move.uci(), {}};

    // At the very branch of a variation the alternatives are the parent
    // line's move and the sibling variations, not lines of this one.
    if (!m_path.isEmpty() && ply == m_branchPly) {
        QList<int> parentPath = m_path;
        const int taken = parentPath.takeLast();
        const QList<MoveRecord> parentLine = GameVariations::lineMoves(m_game, parentPath);
        if (ply < parentLine.size() && parentLine.at(ply).uci == record.uci) {
            goToLine(parentPath, ply + 1);
            return true;
        }
        QList<Variation> &siblings = *GameVariations::variationsOf(m_game, parentPath);
        const int atPly = siblings.at(taken).atPly;
        for (int i = 0; i < siblings.size(); ++i) {
            if (siblings.at(i).atPly == atPly && siblings.at(i).moves.first().uci == record.uci) {
                goToLine(parentPath + QList<int>{i}, ply + 1);
                return true;
            }
        }
        siblings << Variation{atPly, {record}, {}};
        Q_EMIT gameChanged();
        goToLine(parentPath + QList<int>{int(siblings.size()) - 1}, ply + 1);
        return true;
    }

    // Further along the line: a variation here that begins with the move.
    const int ownPly = ply + 1 - m_branchPly;
    QList<Variation> &variations = lineVariations();
    for (int i = 0; i < variations.size(); ++i) {
        if (variations.at(i).atPly == ownPly && variations.at(i).moves.first().uci == record.uci) {
            goToLine(m_path + QList<int>{i}, ply + 1);
            return true;
        }
    }
    if (ply == plyCount()) {
        // The line goes on.
        ownMoves() << record;
        followLine(m_path);
        m_ply = ply + 1;
        m_game.plyCount = int(m_game.moves.size());
        Q_EMIT gameChanged();
        Q_EMIT plyChanged(m_ply);
        return true;
    }
    variations << Variation{ownPly, {record}, {}};
    Q_EMIT gameChanged();
    goToLine(m_path + QList<int>{int(variations.size()) - 1}, ply + 1);
    return true;
}

void GameSession::setComment(const QList<int> &path, int index, const QString &comment)
{
    if (MoveComment::at(m_game, path, index) == comment || !MoveComment::set(m_game, path, index, comment))
        return;
    m_line = GameVariations::lineMoves(m_game, m_path); // The line followed may hold it.
    Q_EMIT commentsChanged();
}

void GameSession::setAnnotations(int ply, const QList<int> &nags)
{
    if (ply < 1 || ply > plyCount())
        return;
    const QList<int> annotations = MoveAnnotation::normalized(nags);
    // The record itself: in the main line, or in the variation that owns the ply.
    QList<MoveRecord> *moves = &m_game.moves;
    QList<Variation> *variations = &m_game.variations;
    int base = 0;
    for (const int index : m_path) {
        Variation &variation = (*variations)[index];
        const int branch = base + variation.atPly - 1;
        if (ply - 1 < branch)
            break;
        moves = &variation.moves;
        variations = &variation.variations;
        base = branch;
    }
    MoveRecord &record = (*moves)[ply - 1 - base];
    if (record.nags == annotations)
        return;
    record.nags = annotations;
    m_line[ply - 1].nags = annotations;
    Q_EMIT annotationsChanged(ply);
}

void GameSession::goToPly(int ply)
{
    ply = qBound(0, ply, plyCount());
    if (ply == m_ply)
        return;
    m_ply = ply;
    Q_EMIT plyChanged(m_ply);
}

void GameSession::goToLine(const QList<int> &path, int ply)
{
    followLine(path);
    m_ply = qBound(0, ply, plyCount());
    Q_EMIT plyChanged(m_ply);
}
