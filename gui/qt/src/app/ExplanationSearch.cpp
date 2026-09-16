#include "ExplanationSearch.h"

#include "AdvantageProbe.h"
#include "UciEngine.h"

std::optional<EngineEvaluation> ExplanationAnalysis::beforeEvaluation() const
{
    return beforeByDepth.isEmpty() ? std::nullopt : std::optional<EngineEvaluation>(beforeByDepth.last());
}

std::optional<EngineEvaluation> ExplanationAnalysis::afterEvaluation() const
{
    return afterByDepth.isEmpty() ? std::nullopt : std::optional<EngineEvaluation>(afterByDepth.last());
}

bool ExplanationAnalysis::acceptsHint(const EngineEvaluation &hint) const
{
    const std::optional<EngineEvaluation> searched = afterEvaluation();
    if (hint.pv.isEmpty() || (searched && hint.depth < searched->depth))
        return false;
    const bool mate = hint.isMate && hint.mateIn > 0;
    const bool draw = !hint.isMate && hint.centipawns == 0;
    // Only what a long analysis finds and a fixed-depth search may miss.
    if (mate ? searched && searched->isMate : !draw || (searched && !searched->isMate && searched->centipawns == 0))
        return false;
    ChessPosition position = after;
    for (const QString &uci : hint.pv) {
        const std::optional<ChessMove> move = position.moveFromUci(uci);
        if (!move)
            return false;
        position.play(*move);
    }
    return true;
}

ExplanationInput ExplanationAnalysis::input(SanStyle style, bool trace, const std::optional<EngineEvaluation> &hint) const
{
    ExplanationInput input;
    input.before = before;
    input.played = played;
    input.beforeEvaluation = beforeEvaluation();
    input.after = after;
    if (const std::optional<EngineEvaluation> evaluation = afterEvaluation()) {
        input.afterEvaluation = *evaluation;
        if (!probe.isEmpty())
            input.concretePly = AdvantageProbe::concretePly(probe, *evaluation);
    }
    if (hint && acceptsHint(*hint)) {
        input.evaluationNote = QStringLiteral("hint from the live analysis: %1 at depth %2 replaces %3")
                                   .arg(hint->text()).arg(hint->depth)
                                   .arg(input.afterEvaluation.pv.isEmpty() && !afterEvaluation()
                                            ? QStringLiteral("no evaluation")
                                            : QStringLiteral("%1 at depth %2").arg(input.afterEvaluation.text())
                                                  .arg(input.afterEvaluation.depth));
        input.afterEvaluation = *hint;
        input.concretePly.reset(); // The probe followed the other line.
    }
    input.sanStyle = style;
    input.trace = trace;
    return input;
}

ExplanationSearch::ExplanationSearch(const ExplainSettings &settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_engine(new UciEngine(this))
{
    m_engine->setOption(QStringLiteral("Threads"), QString::number(qMax(1, settings.threads)));
    m_engine->setOption(QStringLiteral("Hash"), QString::number(qMax(1, settings.hashMb)));
    connect(m_engine, &UciEngine::evaluationChanged, this, &ExplanationSearch::onEvaluation);
    connect(m_engine, &UciEngine::searchFinished, this, &ExplanationSearch::onSearchFinished);
    connect(m_engine, &UciEngine::failed, this, [this](const QString &message) {
        m_stage = Stage::Idle;
        Q_EMIT failed(message);
    });
}

ExplanationSearch::~ExplanationSearch() = default;

QString ExplanationSearch::engineName() const
{
    return m_engine->name();
}

bool ExplanationSearch::start(const QString &engineExecutable)
{
    return m_engine->isRunning() || m_engine->start(engineExecutable);
}

void ExplanationSearch::shutdown()
{
    m_stage = Stage::Idle;
    m_engine->shutdown();
}

void ExplanationSearch::analyze(const std::optional<ChessPosition> &before, const std::optional<ChessMove> &played,
                                const ChessPosition &after)
{
    m_analysis = ExplanationAnalysis();
    m_analysis.before = before;
    m_analysis.played = played;
    m_analysis.after = after;
    m_stage = before ? Stage::Before : Stage::After;
    search(before ? *before : after, m_settings.depth);
}

void ExplanationSearch::cancel()
{
    m_stage = Stage::Idle;
    m_engine->stopAnalysis();
}

void ExplanationSearch::search(const ChessPosition &position, int depth)
{
    m_current.clear();
    SearchLimit limit;
    limit.depth = depth;
    limit.clearHash = true;
    m_engine->analyze(position.fen(), {}, position.sideToMove(), limit);
}

void ExplanationSearch::onEvaluation(const EngineEvaluation &evaluation)
{
    if (m_stage == Stage::Idle)
        return;
    // Engines report several lines per depth; keep the last one of each.
    if (!m_current.isEmpty() && m_current.last().depth >= evaluation.depth)
        m_current.last() = evaluation;
    else
        m_current << evaluation;
}

void ExplanationSearch::onSearchFinished()
{
    switch (m_stage) {
    case Stage::Idle:
        return;
    case Stage::Before:
        m_analysis.beforeByDepth = m_current;
        m_stage = Stage::After;
        search(m_analysis.after, m_settings.depth);
        return;
    case Stage::After:
        m_analysis.afterByDepth = m_current;
        m_probePosition = m_analysis.after;
        if (m_settings.probeDepth <= 0 || !m_analysis.afterEvaluation() || m_analysis.afterEvaluation()->pv.isEmpty())
            break;
        m_stage = Stage::Probe;
        search(m_probePosition, m_settings.probeDepth);
        return;
    case Stage::Probe: {
        if (!m_current.isEmpty())
            m_analysis.probe << m_current.last();
        const QStringList pv = m_analysis.afterEvaluation()->pv; // A copy: afterEvaluation() returns a temporary.
        const qsizetype ply = m_analysis.probe.size() - 1; // Moves played to reach the position just searched.
        std::optional<ChessMove> next;
        if (!m_current.isEmpty() && ply < pv.size() && ply < m_settings.probePlies)
            next = m_probePosition.moveFromUci(pv.at(ply));
        if (!next)
            break;
        m_probePosition.play(*next);
        search(m_probePosition, m_settings.probeDepth);
        return;
    }
    }
    m_stage = Stage::Idle;
    Q_EMIT finished(m_analysis);
}
