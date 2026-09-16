#include "UciEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

#include <cmath>

double EngineEvaluation::whiteShare() const
{
    if (isMate)
        return mating == Side::White ? 1.0 : 0.0;
    // Winning chances curve used by lichess, mapped from [-1, 1] to [0, 1].
    const double winningChances = 2.0 / (1.0 + std::exp(-0.00368208 * centipawns)) - 1.0;
    return 0.5 + 0.5 * winningChances;
}

QString EngineEvaluation::text() const
{
    if (isMate)
        return mateIn == 0 ? QStringLiteral("#") : QStringLiteral("M%1").arg(mateIn);
    const double pawns = centipawns / 100.0;
    const QString number = std::abs(pawns) >= 10 ? QString::number(std::abs(pawns), 'f', 0)
                                                 : QString::number(std::abs(pawns), 'f', 1);
    if (number == QLatin1String("0.0"))
        return number;
    return (pawns > 0 ? QStringLiteral("+") : QStringLiteral("−")) + number;
}

UciEngine::UciEngine(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &UciEngine::readOutput);
    connect(m_process, &QProcess::started, this, [this] {
        m_state = State::Initializing;
        send("uci");
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
            m_state = State::Stopped;
            Q_EMIT failed(m_process->errorString());
        }
    });
    connect(m_process, &QProcess::finished, this, [this] { m_state = State::Stopped; });
}

UciEngine::~UciEngine()
{
    shutdown();
}

QString UciEngine::findExecutable(const QString &command)
{
    if (command.isEmpty())
        return {};
    const QFileInfo file(command);
    if (command.contains(QDir::separator()) || command.contains(QLatin1Char('/')))
        return file.isExecutable() ? file.absoluteFilePath() : QString();

    QString found = QStandardPaths::findExecutable(command);
    if (found.isEmpty()) {
        // Common locations outside PATH (Debian's /usr/games, Homebrew).
        found = QStandardPaths::findExecutable(
            command, {QStringLiteral("/usr/games"), QStringLiteral("/opt/homebrew/bin"),
                      QStringLiteral("/usr/local/bin")});
    }
    return found;
}

bool UciEngine::start(const QString &executable)
{
    if (isRunning())
        return true;
    m_name.clear();
    m_options.clear();
    m_buffer.clear();
    m_process->start(executable, {});
    return m_process->waitForStarted(3000);
}

void UciEngine::shutdown()
{
    if (m_process->state() == QProcess::NotRunning)
        return;
    send("stop");
    send("quit");
    if (!m_process->waitForFinished(1000))
        m_process->kill();
    m_state = State::Stopped;
    m_pending.reset();
}

bool UciEngine::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void UciEngine::analyze(const QString &startFen, const QStringList &uciMoves, Side sideToMove)
{
    m_pending = Request{startFen, uciMoves, sideToMove};
    switch (m_state) {
    case State::Idle:
        startPendingSearch();
        break;
    case State::Searching:
        send("stop");
        m_state = State::Stopping;
        break;
    default:
        break; // Picked up once the engine is ready or has stopped.
    }
}

void UciEngine::stopAnalysis()
{
    m_pending.reset();
    if (m_state == State::Searching) {
        send("stop");
        m_state = State::Stopping;
    }
}

void UciEngine::send(const QByteArray &command)
{
    if (m_process->state() == QProcess::Running)
        m_process->write(command + '\n');
}

void UciEngine::readOutput()
{
    m_buffer += m_process->readAllStandardOutput();
    qsizetype newline;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (!line.isEmpty())
            handleLine(line);
    }
}

void UciEngine::handleLine(const QByteArray &line)
{
    const QList<QByteArray> tokens = line.simplified().split(' ');
    const QByteArray &command = tokens.first();

    if (command == "id" && tokens.value(1) == "name") {
        m_name = QString::fromUtf8(line.mid(line.indexOf("name") + 5).trimmed());
        Q_EMIT nameChanged(m_name);
    } else if (command == "option" && tokens.value(1) == "name") {
        m_options << QString::fromUtf8(tokens.value(2));
    } else if (command == "uciok") {
        if (m_options.contains(QLatin1String("Threads"))) {
            const int threads = qMax(1, QThread::idealThreadCount() / 2);
            send("setoption name Threads value " + QByteArray::number(threads));
        }
        if (m_options.contains(QLatin1String("UCI_AnalyseMode")))
            send("setoption name UCI_AnalyseMode value true");
        send("isready");
    } else if (command == "readyok") {
        if (m_state == State::Initializing) {
            m_state = State::Idle;
            startPendingSearch();
        }
    } else if (command == "info") {
        if (m_state == State::Searching)
            handleInfo(tokens);
    } else if (command == "bestmove") {
        if (m_state == State::Searching || m_state == State::Stopping) {
            m_state = State::Idle;
            startPendingSearch();
        }
    }
}

void UciEngine::handleInfo(const QList<QByteArray> &tokens)
{
    EngineEvaluation evaluation;
    bool hasScore = false;
    for (qsizetype i = 1; i < tokens.size(); ++i) {
        const QByteArray &key = tokens.at(i);
        if (key == "depth") {
            evaluation.depth = tokens.value(++i).toInt();
        } else if (key == "multipv") {
            if (tokens.value(++i).toInt() != 1)
                return; // Only the best line drives the evaluation.
        } else if (key == "score") {
            const QByteArray kind = tokens.value(++i);
            const int value = tokens.value(++i).toInt();
            // UCI scores are from the side to move; convert to White's view.
            const bool whiteToMove = m_searchSide == Side::White;
            if (kind == "cp") {
                evaluation.centipawns = whiteToMove ? value : -value;
                hasScore = true;
            } else if (kind == "mate") {
                evaluation.isMate = true;
                evaluation.mateIn = std::abs(value);
                // "mate 0": the side to move is checkmated.
                const bool sideToMoveMates = value > 0;
                evaluation.mating = sideToMoveMates == whiteToMove ? Side::White : Side::Black;
                hasScore = true;
            }
        } else if (key == "lowerbound" || key == "upperbound") {
            // Aspiration window fail: provisional score with a truncated line.
            return;
        } else if (key == "pv") {
            for (++i; i < tokens.size(); ++i)
                evaluation.pv << QString::fromLatin1(tokens.at(i));
        } else if (key == "string") {
            return;
        }
    }
    if (hasScore)
        Q_EMIT evaluationChanged(evaluation);
}

void UciEngine::startPendingSearch()
{
    if (!m_pending || m_state != State::Idle)
        return;

    QByteArray position = m_pending->startFen.isEmpty()
        ? QByteArray("position startpos")
        : "position fen " + m_pending->startFen.toUtf8();
    if (!m_pending->moves.isEmpty())
        position += " moves " + m_pending->moves.join(QLatin1Char(' ')).toLatin1();

    m_searchSide = m_pending->sideToMove;
    m_pending.reset();
    send(position);
    send("go infinite");
    m_state = State::Searching;
}
