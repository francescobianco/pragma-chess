#pragma once

#include "BoardState.h"

#include <QObject>
#include <QStringList>

#include <optional>

class QProcess;

/// An evaluation reported by an engine, always from White's point of view.
struct EngineEvaluation {
    bool isMate = false;
    /// Centipawns, positive when White is better (when !isMate).
    int centipawns = 0;
    /// Moves to mate (when isMate); 0 means the position is already checkmate.
    int mateIn = 0;
    /// Side delivering mate (when isMate).
    Side mating = Side::White;
    int depth = 0;
    /// Principal variation in UCI notation.
    QStringList pv;

    /// Expected share of the game for White in [0, 1], used by evaluation bars.
    double whiteShare() const;
    /// Short human-readable score, e.g. "+1.3", "−0.4", "M3", "#".
    QString text() const;
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

    /// Starts (or restarts) infinite analysis of a position.
    void analyze(const QString &startFen, const QStringList &uciMoves, Side sideToMove);
    void stopAnalysis();

Q_SIGNALS:
    void nameChanged(const QString &name);
    void evaluationChanged(const EngineEvaluation &evaluation);
    void failed(const QString &message);

private:
    enum class State { Stopped, Initializing, Idle, Searching, Stopping };

    struct Request {
        QString startFen;
        QStringList moves;
        Side sideToMove;
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
    QByteArray m_buffer;
};
