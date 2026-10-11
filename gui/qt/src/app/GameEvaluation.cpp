#include "GameEvaluation.h"

#include "app/UciEngine.h"

#include <algorithm>

const QList<int> &GameEvaluation::passDepths()
{
    static const QList<int> depths{8, 12, 16, 20, 24};
    return depths;
}

std::optional<GameEvaluation::Step> GameEvaluation::nextStep(const QList<int> &knownDepths)
{
    for (const int depth : passDepths()) {
        for (qsizetype ply = 0; ply < knownDepths.size(); ++ply) {
            if (knownDepths.at(ply) < depth)
                return Step{int(ply), depth};
        }
    }
    return std::nullopt;
}

std::optional<EngineEvaluation> GameEvaluation::finalEvaluation(const ChessPosition &position)
{
    if (!position.hasKings() || !position.legalMoves().isEmpty())
        return std::nullopt;
    EngineEvaluation evaluation;
    // Known for good: no pass searches it again.
    evaluation.depth = passDepths().constLast();
    if (position.isCheckmate()) {
        evaluation.isMate = true;
        evaluation.mateIn = 0;
        evaluation.mating = position.sideToMove() == Side::White ? Side::Black : Side::White;
    }
    return evaluation;
}

GameEvaluation::GameEvaluation(QObject *parent)
    : QObject(parent)
{
}

GameEvaluation::~GameEvaluation()
{
    stop();
}

bool GameEvaluation::start(const Engine &engine, const QString &startFen, const QStringList &uciMoves)
{
    stop();
    std::optional<ChessPosition> position = startFen.isEmpty()
        ? std::optional<ChessPosition>(ChessPosition::startingPosition())
        : ChessPosition::fromFen(startFen, ChessPosition::Kings::Optional);
    if (!position || !position->hasKings())
        return false;
    m_startFen = startFen.isEmpty() ? position->fen() : startFen;
    m_moves = uciMoves;
    m_positions = {*position};
    for (const QString &uci : uciMoves) {
        const std::optional<ChessMove> move = position->moveFromUci(uci, ChessPosition::NullMoves::Allowed);
        if (!move)
            return false;
        position->play(*move);
        m_positions << *position;
    }
    m_depths = QList<int>(m_positions.size(), 0);
    if (m_knownDepth) {
        for (qsizetype ply = 0; ply < m_positions.size(); ++ply)
            m_depths[ply] = m_knownDepth(m_positions.at(ply));
    }
    // Positions with no legal moves are known at once.
    for (qsizetype ply = 0; ply < m_positions.size(); ++ply) {
        if (const std::optional<EngineEvaluation> known = finalEvaluation(m_positions.at(ply))) {
            m_depths[ply] = known->depth;
            Q_EMIT evaluated(int(ply), m_positions.at(ply), *known);
        }
    }

    m_engine = new UciEngine(this);
    m_engine->setOption(QStringLiteral("Threads"), QString::number(std::max(1, engine.threads)));
    if (engine.hashMb > 0)
        m_engine->setOption(QStringLiteral("Hash"), QString::number(engine.hashMb));
    m_engine->setLowPriority(engine.lowPriority);
    m_engine->setCpuLimit(engine.cpuPercent, engine.quotaOfOneCore);
    connect(m_engine, &UciEngine::evaluationChanged, this, [this](const EngineEvaluation &evaluation) {
        m_last = evaluation;
    });
    connect(m_engine, &UciEngine::searchFinished, this, &GameEvaluation::searchDone);
    connect(m_engine, &UciEngine::failed, this, [this](const QString &message) {
        stop();
        Q_EMIT failed(message);
    });
    if (engine.executable.isEmpty() || !m_engine->start(engine.executable)) {
        stop();
        return false;
    }
    m_running = true;
    searchNext();
    return true;
}

void GameEvaluation::stop()
{
    m_running = false;
    m_step.reset();
    if (m_engine) {
        m_engine->disconnect(this);
        m_engine->shutdown();
        m_engine->deleteLater();
        m_engine = nullptr;
    }
}

int GameEvaluation::completedDepth() const
{
    return m_depths.isEmpty() ? 0 : *std::min_element(m_depths.cbegin(), m_depths.cend());
}

void GameEvaluation::searchNext()
{
    if (!m_running)
        return;
    m_step = nextStep(m_depths);
    if (!m_step) {
        stop();
        Q_EMIT finished();
        return;
    }
    m_last.reset();
    const ChessPosition &position = m_positions.at(m_step->ply);
    m_engine->analyze(m_startFen, m_moves.mid(0, m_step->ply), position.sideToMove(), SearchLimit{m_step->depth});
}

void GameEvaluation::searchDone()
{
    if (!m_running || !m_step)
        return;
    const Step step = *m_step;
    const int before = completedDepth();
    // A search gives at least its depth; one that said nothing is not asked again in this pass.
    m_depths[step.ply] = std::max(step.depth, m_last ? m_last->depth : 0);
    if (m_last)
        Q_EMIT evaluated(step.ply, m_positions.at(step.ply), *m_last);
    const int after = completedDepth();
    for (const int depth : passDepths()) {
        if (before < depth && after >= depth)
            Q_EMIT passFinished(depth);
    }
    searchNext();
}
