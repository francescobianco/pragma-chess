#include "Explainer.h"

#include "UciEngine.h"

#include <QTimer>

namespace {

/// Evaluations shallower than this are too unstable to explain.
constexpr int kMinimumDepth = 12;
/// Bounds of the search of the previous position.
constexpr int kPreviousDepth = 18;
constexpr int kPreviousTimeMs = 3000;
/// Coalesces the stream of engine updates into steady arrows.
constexpr int kUpdateDelayMs = 250;
constexpr qsizetype kMaxRememberedPositions = 5000;

} // namespace

Explainer::Explainer(QObject *parent)
    : QObject(parent)
    , m_engine(new UciEngine(this))
    , m_updateTimer(new QTimer(this))
{
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(kUpdateDelayMs);
    connect(m_updateTimer, &QTimer::timeout, this, &Explainer::update);

    connect(m_engine, &UciEngine::evaluationChanged, this, [this](const EngineEvaluation &evaluation) {
        if (m_searchingKey.isEmpty())
            return;
        remember(m_searchingKey, evaluation);
        if (m_before && m_before->positionKey() == m_searchingKey)
            scheduleUpdate();
    });
    connect(m_engine, &UciEngine::searchFinished, this, [this] {
        m_searchingKey.clear();
        scheduleUpdate();
    });
    connect(m_engine, &UciEngine::failed, this, [this] { m_searchingKey.clear(); });
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

    if (!enabled) {
        m_updateTimer->stop();
        m_searchingKey.clear();
        m_engine->shutdown();
        return;
    }
    searchPreviousPosition();
    update();
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
    searchPreviousPosition();
    update();
}

void Explainer::setCurrentEvaluation(const EngineEvaluation &evaluation)
{
    remember(m_position.positionKey(), evaluation);
    if (m_enabled)
        scheduleUpdate();
}

void Explainer::remember(const QString &positionKey, const EngineEvaluation &evaluation)
{
    const auto it = m_evaluations.constFind(positionKey);
    if (it != m_evaluations.cend() && it->depth > evaluation.depth)
        return;
    if (m_evaluations.size() >= kMaxRememberedPositions)
        m_evaluations.clear();
    m_evaluations.insert(positionKey, evaluation);
}

std::optional<EngineEvaluation> Explainer::deepEnough(const ChessPosition &position) const
{
    const auto it = m_evaluations.constFind(position.positionKey());
    if (it == m_evaluations.cend() || it->depth < kMinimumDepth)
        return std::nullopt;
    return *it;
}

void Explainer::searchPreviousPosition()
{
    if (!m_before) {
        if (!m_searchingKey.isEmpty())
            m_engine->stopAnalysis();
        m_searchingKey.clear();
        return;
    }

    const QString key = m_before->positionKey();
    const auto known = m_evaluations.constFind(key);
    if ((known != m_evaluations.cend() && known->depth >= kPreviousDepth) || key == m_searchingKey)
        return;

    if (!m_engine->isRunning() && (m_engineExecutable.isEmpty() || !m_engine->start(m_engineExecutable)))
        return;
    m_searchingKey = key;
    m_engine->analyze(m_before->fen(), {}, m_before->sideToMove(), {kPreviousDepth, kPreviousTimeMs});
}

void Explainer::scheduleUpdate()
{
    if (!m_updateTimer->isActive())
        m_updateTimer->start();
}

void Explainer::update()
{
    if (!m_enabled)
        return;

    MoveExplanation explanation;
    const bool gameOver = m_position.isCheckmate() || m_position.isStalemate();
    const std::optional<EngineEvaluation> current = deepEnough(m_position);
    const std::optional<EngineEvaluation> previous = m_before ? deepEnough(*m_before) : std::nullopt;
    // Without an evaluation of the previous position there is nothing to compare with
    // yet; unless the engine cannot search it, wait rather than change the verdict later.
    const bool waitingForPrevious = m_before && !previous && !m_searchingKey.isEmpty();

    if (gameOver || (current && !waitingForPrevious)) {
        ExplanationInput input;
        input.after = m_position;
        if (current)
            input.afterEvaluation = *current;
        input.before = m_before;
        input.played = m_played;
        input.beforeEvaluation = previous;
        explanation = explainPosition(input);
    } else {
        explanation.summary = tr("Analyzing…");
    }

    if (m_shown && *m_shown == explanation)
        return;
    m_shown = explanation;
    Q_EMIT explanationChanged(explanation);
}
