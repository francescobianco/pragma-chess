#include "Explainer.h"

#include "ExplainTicks.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

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
    startExplanation();
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
    ExplanationInput input;
    input.before = m_before;
    input.played = m_played;
    input.beforeEvaluation = m_before ? known(*m_before) : std::nullopt;
    input.after = m_position;
    input.afterEvaluation = *evaluation;
    input.sanStyle = SanStyle::Figurines;
    record(input);
    const ExplanationTick tick = explainTick(input);
    if (tick.shown)
        show(tick.explanation);
}

void Explainer::record(const ExplanationInput &input)
{
    // PRAGMA_EXPLAIN_RECORD=<folder>: every move explained leaves its ticks
    // there, to replay with pragma-explain --replay (docs/tech/explain-tuning.md).
    const QString folder = qEnvironmentVariable("PRAGMA_EXPLAIN_RECORD");
    if (folder.isEmpty())
        return;
    const QString key = (input.before ? input.before->positionKey() : QString()) + QLatin1Char('|')
        + input.after.positionKey();
    // One record per move, kept when the board leaves it and comes back.
    if (m_recordings.size() >= kMaxRememberedEvaluations && !m_recordings.contains(key))
        m_recordings.clear();
    ExplainTicks &recording = m_recordings[key];
    recording.before = input.before;
    recording.played = input.played;
    recording.beforeEvaluation = input.beforeEvaluation;
    recording.after = input.after;
    const auto same = [](const EngineEvaluation &a, const EngineEvaluation &b) {
        return a.depth == b.depth && a.isMate == b.isMate && a.centipawns == b.centipawns && a.mateIn == b.mateIn
            && a.mating == b.mating && a.pv == b.pv;
    };
    if (recording.ticks.isEmpty() || !same(recording.ticks.last(), input.afterEvaluation))
        recording.ticks << input.afterEvaluation;
    const QString name = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    QDir().mkpath(folder);
    QFile file(QDir(folder).filePath(name + QStringLiteral(".ticks")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(recording.toText().toUtf8());
}

void Explainer::show(const MoveExplanation &explanation)
{
    if (m_shown && *m_shown == explanation)
        return;
    m_shown = explanation;
    Q_EMIT explanationChanged(explanation);
}
