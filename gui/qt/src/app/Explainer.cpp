#include "Explainer.h"

#include "ExplainTicks.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimer>

namespace {

constexpr qsizetype kMaxRememberedEvaluations = 5000;

} // namespace

Explainer::Explainer(QObject *parent)
    : QObject(parent)
{
}

Explainer::~Explainer()
{
    if (m_saveTimer && m_saveTimer->isActive())
        save();
}

void Explainer::setStorage(const QString &path)
{
    m_storage = path;
    if (!m_saveTimer) {
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        m_saveTimer->setInterval(5000);
        connect(m_saveTimer, &QTimer::timeout, this, [this] { save(); });
    }
    // One line a position: its key, a tab, the evaluation as records write it.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QStringList lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const qsizetype tab = line.indexOf(QLatin1Char('\t'));
        if (tab <= 0 || m_evaluations.size() >= kMaxRememberedEvaluations)
            continue;
        if (const std::optional<EngineEvaluation> evaluation = ExplainTicks::parseEvaluation(line.mid(tab + 1))) {
            const QString key = line.left(tab);
            const auto known = m_evaluations.constFind(key);
            if (known == m_evaluations.cend() || known->depth < evaluation->depth)
                m_evaluations.insert(key, *evaluation);
        }
    }
}

void Explainer::save() const
{
    if (m_storage.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_storage).absolutePath());
    QSaveFile file(m_storage);
    if (!file.open(QIODevice::WriteOnly))
        return;
    for (auto it = m_evaluations.cbegin(); it != m_evaluations.cend(); ++it)
        file.write((it.key() + QLatin1Char('\t') + ExplainTicks::evaluationText(*it) + QLatin1Char('\n')).toUtf8());
    file.commit();
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
    setEvaluation(m_position, evaluation);
}

std::optional<ChessPosition> Explainer::unjudgedBefore(int depth) const
{
    if (!m_enabled || !m_before || !m_played)
        return std::nullopt;
    const std::optional<EngineEvaluation> evaluation = known(*m_before);
    if (evaluation && evaluation->depth >= depth)
        return std::nullopt;
    return m_before;
}

void Explainer::setEvaluation(const ChessPosition &position, const EngineEvaluation &evaluation)
{
    // Only a search at least as deep as the one known adds anything.
    const QString key = position.positionKey();
    const auto known = m_evaluations.constFind(key);
    if (known != m_evaluations.cend() && known->depth > evaluation.depth)
        return;
    if (m_evaluations.size() >= kMaxRememberedEvaluations)
        m_evaluations.clear();
    m_evaluations.insert(key, evaluation);
    if (m_saveTimer && !m_saveTimer->isActive())
        m_saveTimer->start();
    // The position before the move: the next tick of the board judges with it.
    if (m_enabled && key == m_position.positionKey())
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

void Explainer::setViewer(std::optional<Side> viewer)
{
    if (viewer == m_viewer)
        return;
    m_viewer = viewer;
    if (m_enabled)
        tick();
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
    input.viewer = m_viewer;
    record(input);
    const ExplanationTick tick = explainTick(input);
    if (tick.shown)
        show(tick.explanation);
}

void Explainer::record(const ExplanationInput &input)
{
    // The ticks of every move explained are kept (Engine ▸ Copy Explain's
    // Ticks); with PRAGMA_EXPLAIN_RECORD=<folder> they are also written
    // there, to replay with pragma-explain --replay (docs/tech/explain-tuning.md).
    const QString folder = qEnvironmentVariable("PRAGMA_EXPLAIN_RECORD");
    const QString key = (input.before ? input.before->positionKey() : QString()) + QLatin1Char('|')
        + input.after.positionKey();
    // One record per move, kept when the board leaves it and comes back.
    if (m_recordings.size() >= kMaxRememberedEvaluations && !m_recordings.contains(key))
        m_recordings.clear();
    m_recordingKey = key;
    ExplainTicks &recording = m_recordings[key];
    recording.before = input.before;
    recording.played = input.played;
    recording.beforeEvaluation = input.beforeEvaluation;
    recording.after = input.after;
    recording.viewer = input.viewer;
    const auto same = [](const EngineEvaluation &a, const EngineEvaluation &b) {
        return a.depth == b.depth && a.isMate == b.isMate && a.centipawns == b.centipawns && a.mateIn == b.mateIn
            && a.mating == b.mating && a.pv == b.pv;
    };
    if (recording.ticks.isEmpty() || !same(recording.ticks.last(), input.afterEvaluation))
        recording.ticks << input.afterEvaluation;
    if (folder.isEmpty())
        return;
    const QString name = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
    QDir().mkpath(folder);
    QFile file(QDir(folder).filePath(name + QStringLiteral(".ticks")));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(recording.toText().toUtf8());
}

QString Explainer::recordedTicks() const
{
    const auto found = m_recordings.constFind(m_recordingKey);
    if (found == m_recordings.cend())
        return {};
    // What was shown, as comments (in the interface's language: the record's
    // expectations are written in English, once the right answer is known).
    QString shown;
    if (m_shown) {
        for (const QString &line : ExplainTicks::outcome(*m_shown))
            shown += QStringLiteral("# shown: ") + line + QLatin1Char('\n');
    }
    return shown + found->toText();
}

void Explainer::show(const MoveExplanation &explanation)
{
    if (m_shown && *m_shown == explanation)
        return;
    m_shown = explanation;
    Q_EMIT explanationChanged(explanation);
}
