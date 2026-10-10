#pragma once

#include "FicsProtocol.h"
#include "OnlineClient.h"

#include <optional>

class QTcpSocket;

/// Plays on the Free Internet Chess Server (freechess.org) through its
/// telnet session: logs in (as a registered player with a password, or as a
/// guest, whose name the server gives), sets the session so the server sends
/// a "style 12" board after every move and nothing else that chats, seeks a
/// game and plays it (FicsProtocol reads the lines). A game ends when the
/// session does: there is nothing to resume.
class FicsClient : public OnlineClient {
    Q_OBJECT

public:
    /// `username` "guest" (or empty) logs in as a guest; `password` is the
    /// registered player's.
    FicsClient(const QString &username, const QString &password, QObject *parent = nullptr);
    ~FicsClient() override;

    void seek(const Seek &seek) override;
    void cancelSeek() override;
    bool isSeeking() const override { return m_seek.has_value() && m_game.id.isEmpty(); }
    bool isPlaying() const override { return !m_game.id.isEmpty() && !m_game.isOver(); }
    const OnlineGame &game() const override { return m_game; }
    QString playingAs() const override { return m_handle; }
    QString gameUrl() const override { return QStringLiteral("https://www.freechess.org"); }

    void move(const QString &uci) override;
    void resign() override;
    void abort() override;
    void offerDraw() override;

    /// The server, freechess.org on port 5000 unless changed (a test's own).
    static constexpr char kHost[] = "freechess.org";
    static constexpr quint16 kPort = 5000;
    void setServer(const QString &host, quint16 port);

private:
    void connectToServer();
    void read();
    void line(const QString &text);
    void prompt(const QString &pending);
    void send(const QString &command);
    void loggedIn(const QString &handle);
    void sendSeek();
    void update(const Fics::Style12 &board);

    QTcpSocket *m_socket;
    QString m_host = QString::fromLatin1(kHost);
    quint16 m_port = kPort;
    QString m_username;
    QString m_password;
    QString m_handle;
    bool m_loggedIn = false;
    QByteArray m_buffer;
    std::optional<Seek> m_seek;
    OnlineGame m_game;
    /// The board of the game, for the user's castling (Style12::squares).
    QString m_squares;
    /// The ratings read when the game was created, for the game's players.
    std::optional<Fics::Creating> m_creating;
};
