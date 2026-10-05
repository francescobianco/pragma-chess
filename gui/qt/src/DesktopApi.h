#pragma once

#include <QObject>
#include <QString>

class LocalHttpServer;
class MainWindow;

/// The desktop client as a service: a local HTTP API (LocalHttpServer, on
/// 127.0.0.1 only, with a token) through which another program — a script,
/// an assistant — reads what the window shows (the position, the engine,
/// Explain's arrows and text, a picture of the window) and drives it (go to a
/// move, play one, load a line, Explain and the analysis on or off).
///
/// Off unless the user turns it on (Options ▸ Local API) or PRAGMA_API=1 is
/// set. While on, `api.json` in the application's data folder (readable by
/// the user only) holds the port and the token; scripts/pragma-api.sh reads
/// it. The routes are listed by GET /api and in AGENTS.md.
class DesktopApi : public QObject {
    Q_OBJECT

public:
    explicit DesktopApi(MainWindow *window);

    /// Starts or stops the server; false, with `error`, if it cannot listen.
    bool setEnabled(bool enabled, QString *error = nullptr);
    bool isEnabled() const;
    quint16 port() const;
    /// Where the port and the token are written while the API is on.
    static QString infoPath();

private:
    void addRoutes();

    MainWindow *m_window;
    LocalHttpServer *m_server;
};
