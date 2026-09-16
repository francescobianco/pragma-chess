#include "Explainer.h"

namespace {

constexpr qsizetype kMaxRememberedAnalyses = 500;

QString moveKey(const std::optional<ChessPosition> &before, const std::optional<ChessMove> &played,
                const ChessPosition &after)
{
    return QStringLiteral("%1|%2|%3").arg(before ? before->positionKey() : QString(),
                                          played ? played->uci() : QString(), after.positionKey());
}

} // namespace

Explainer::Explainer(QObject *parent)
    : QObject(parent)
    , m_search(new ExplanationSearch(ExplainSettings(), this))
{
    connect(m_search, &ExplanationSearch::finished, this, [this](const ExplanationAnalysis &analysis) {
        if (m_analyses.size() >= kMaxRememberedAnalyses)
            m_analyses.clear();
        m_analyses.insert(keyFor(analysis), analysis);
        if (m_enabled && keyFor(analysis) == currentKey())
            explain();
    });
    connect(m_search, &ExplanationSearch::failed, this, [this](const QString &message) {
        MoveExplanation explanation;
        explanation.summary = tr("The engine stopped: %1").arg(message);
        show(explanation);
    });
}

Explainer::~Explainer() = default;

void Explainer::setEnabled(bool enabled, const QString &engineExecutable)
{
    if (!engineExecutable.isEmpty())
        m_engineExecutable = engineExecutable;
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_shown.reset();
    if (enabled)
        explain();
    else
        m_search->cancel();
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
    explain();
}

QString Explainer::currentKey() const
{
    return moveKey(m_before, m_played, m_position);
}

QString Explainer::keyFor(const ExplanationAnalysis &analysis)
{
    return moveKey(analysis.before, analysis.played, analysis.after);
}

void Explainer::explain()
{
    const auto known = m_analyses.constFind(currentKey());
    if (known != m_analyses.cend()) {
        show(explainPosition(known->input(SanStyle::Figurines)));
        return;
    }

    MoveExplanation waiting;
    if (m_engineExecutable.isEmpty() || !m_search->start(m_engineExecutable)) {
        waiting.summary = tr("No UCI engine to explain with.");
        show(waiting);
        return;
    }
    waiting.summary = tr("Analyzing…");
    show(waiting);
    m_search->analyze(m_before, m_played, m_position);
}

void Explainer::show(const MoveExplanation &explanation)
{
    if (m_shown && *m_shown == explanation)
        return;
    m_shown = explanation;
    Q_EMIT explanationChanged(explanation);
}
