#include "Explainer.h"

#include "smart/SmartPrograms.h"

namespace {

constexpr qsizetype kMaxRememberedEvaluations = 5000;

} // namespace

Explainer::Explainer(QObject *parent)
    : QObject(parent)
{
}

void Explainer::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_shown.reset();
    if (enabled)
        start();
}

void Explainer::setPosition(const ChessPosition &position, const std::optional<ChessPosition> &before,
                            const std::optional<ChessMove> &played)
{
    m_position = position;
    m_before = before;
    m_played = played;
    if (!m_enabled)
        return;
    // Old arrows must not linger on a new position, not even for a moment.
    m_shown.reset();
    start();
}

void Explainer::setLiveEvaluation(const EngineEvaluation &evaluation)
{
    // Only a search at least as deep as the one known adds anything.
    const QString key = m_position.positionKey();
    const auto known = m_evaluations.constFind(key);
    if (known != m_evaluations.cend() && known->depth > evaluation.depth)
        return;
    if (m_evaluations.size() >= kMaxRememberedEvaluations)
        m_evaluations.clear();
    m_evaluations.insert(key, evaluation);
    if (m_enabled)
        tick();
}

std::optional<EngineEvaluation> Explainer::known(const ChessPosition &position) const
{
    const auto found = m_evaluations.constFind(position.positionKey());
    return found == m_evaluations.cend() ? std::nullopt : std::optional<EngineEvaluation>(*found);
}

void Explainer::start()
{
    if (SmartProgram *explain = SmartPrograms::program(QStringLiteral("EXPLAIN.smart")))
        explain->interpreter.call(QStringLiteral("Start"));
    MoveExplanation waiting;
    waiting.summary = tr("Analyzing…");
    show(waiting);
    tick(); // A position searched before is explained at once.
}

void Explainer::tick()
{
    const std::optional<EngineEvaluation> evaluation = known(m_position);
    if (!evaluation)
        return;
    SmartProgram *explain = SmartPrograms::program(QStringLiteral("EXPLAIN.smart"));
    if (!explain) {
        MoveExplanation failed;
        failed.summary = tr("Explain cannot run: its program has a mistake (see the log).");
        show(failed);
        return;
    }
    const std::optional<EngineEvaluation> beforeEvaluation = m_before ? known(*m_before) : std::nullopt;
    const bool comparable = m_before && m_played && beforeEvaluation;
    explain->output.clear();
    explain->output.sanStyle = SanStyle::Figurines;
    QString error;
    const std::optional<SmartValue> shown = explain->interpreter.call(
        QStringLiteral("Tick"),
        {comparable ? SmartChess::position(*m_before) : SmartValue(),
         comparable ? SmartValue(m_played->uci()) : SmartValue(),
         comparable ? SmartChess::evaluation(*beforeEvaluation) : SmartValue(), SmartChess::position(m_position),
         SmartChess::evaluation(*evaluation)},
        &error);
    if (!shown) {
        qWarning("SMART EXPLAIN.smart Tick: %s", qPrintable(error));
        MoveExplanation failed;
        failed.summary = tr("Explain stopped on a mistake of its program: %1").arg(error);
        show(failed);
        return;
    }
    if (shown->isNumber() && shown->number() != 0)
        show(explain->output.explanation);
}

void Explainer::show(const MoveExplanation &explanation)
{
    if (m_shown && *m_shown == explanation)
        return;
    m_shown = explanation;
    Q_EMIT explanationChanged(explanation);
}
