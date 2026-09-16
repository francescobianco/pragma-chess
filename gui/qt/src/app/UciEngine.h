#pragma once

#include "EngineEvaluation.h"

#include <QObject>
#include <QStringList>

#include <optional>

class QProcess;

/// Bounds a search; zero fields mean no bound, all zero means infinite.
struct SearchLimit {
    int depth = 0;
    int moveTimeMs = 0;
    /// Forget earlier searches first ("ucinewgame"), so that the result does
    /// not depend on what was searched before (reproducible fixed-depth searches).
    bool clearHash = false;
};

/// Drives an external UCI engine (Stockfish or any other) through QProcess.
///
/// Positions are sent as a starting FEN plus the moves played, so the GUI
/// never needs to compute castling rights or en passant squares itself.
class UciEngine : public QObject {
    Q_OBJECT

public:
    explicit UciEngine(QObject *parent = nullptr);
    ~UciEngine() override;

    /// Resolves a command ("stockfish") or path to an executable, or returns empty.
    static QString findExecutable(const QString &command);

    bool start(const QString &executable);
    void shutdown();
    bool isRunning() const;

    /// Sets a UCI option ("Threads", "Hash", …), sent when the engine is ready
    /// (and right away if it already is). Overrides the defaults chosen here.
    void setOption(const QString &name, const QString &value);
    /// Name reported by the engine ("id name"), empty until known.
    QString name() const { return m_name; }

    /// Starts (or restarts) the analysis of a position, infinite unless limited.
    void analyze(const QString &startFen, const QStringList &uciMoves, Side sideToMove,
                 SearchLimit limit = {});
    void stopAnalysis();

Q_SIGNALS:
    void nameChanged(const QString &name);
    void evaluationChanged(const EngineEvaluation &evaluation);
    /// A limited search reached its bound.
    void searchFinished();
    void failed(const QString &message);

private:
    enum class State { Stopped, Initializing, Idle, Searching, Stopping };

    struct Request {
        QString startFen;
        QStringList moves;
        Side sideToMove;
        SearchLimit limit;
    };

    void send(const QByteArray &command);
    void readOutput();
    void handleLine(const QByteArray &line);
    void handleInfo(const QList<QByteArray> &tokens);
    void startPendingSearch();

    QProcess *m_process;
    State m_state = State::Stopped;
    QString m_name;
    QStringList m_options;
    QList<std::pair<QString, QString>> m_optionValues;
    std::optional<Request> m_pending;
    Side m_searchSide = Side::White;
    bool m_searchLimited = false;
    QByteArray m_buffer;
};
