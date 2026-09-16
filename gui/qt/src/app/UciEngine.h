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
    std::optional<Request> m_pending;
    Side m_searchSide = Side::White;
    bool m_searchLimited = false;
    QByteArray m_buffer;
};
