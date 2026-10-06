#pragma once

#include <QObject>
#include <QString>

class LocalHttpServer;
class MainWindow;

/// The development API: the desktop client as a service while it is being
/// developed. A local HTTP API (LocalHttpServer on 127.0.0.1:7457) through
/// which another program — a script, an assistant — reads what the window
/// shows (the position, the engine, Explain's arrows and text, a picture of
/// the window) and drives it (go to a move, play one, load a line, Explain
/// and the analysis on or off).
///
/// On only when PRAGMA_DEV_API=1, which `make start` sets; PRAGMA_DEV_API_PORT
/// changes the port. No token: anything on this computer may call it. The
/// routes are listed by GET /api and in AGENTS.md; scripts/pragma-api.sh
/// calls them.
class DesktopApi : public QObject {
    Q_OBJECT

public:
    explicit DesktopApi(MainWindow *window);

    /// Starts the server when PRAGMA_DEV_API=1; false, with `error`, if it
    /// was asked for and cannot listen.
    bool startIfAsked(QString *error = nullptr);
    bool isListening() const;
    quint16 port() const;

private:
    void addRoutes();

    MainWindow *m_window;
    LocalHttpServer *m_server;
    /// The engine's CPU time at the last GET /api/engines, to report its use since.
    struct CpuSample {
        qint64 pid = 0;
        double cpuSeconds = 0;
        qint64 atMs = 0;
    } m_cpuSample;
};
