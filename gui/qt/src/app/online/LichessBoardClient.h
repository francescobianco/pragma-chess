#pragma once

#include "OnlineGame.h"

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/// Plays on lichess.org through its Board API with the user's token
/// (scope board:play): looks for an opponent, follows the game's stream and
/// sends the user's moves. One game at a time.
class LichessBoardClient : public QObject {
    Q_OBJECT

public:
    explicit LichessBoardClient(const QString &token, QObject *parent = nullptr);
    ~LichessBoardClient() override;

    /// How a game is asked for: clock in minutes and seconds of increment,
    /// rated or casual, the colour wanted ("random", "white", "black").
    struct Seek {
        int minutes = 10;
        int increment = 0;
        bool rated = false;
        QString color = QStringLiteral("random");
    };

    /// Looks for an opponent; gameStarted() follows when one is found.
    void seek(const Seek &seek);
    void cancelSeek();
    /// Follows again a game already started, e.g. after the application was
    /// closed during it: the stream sends the whole game (or its end) again.
    void resume(const QString &gameId);
    bool isSeeking() const { return m_seek != nullptr; }

    const OnlineGame &game() const { return m_game; }
    bool isPlaying() const { return m_gameStream != nullptr; }

    /// The user's move, UCI; the platform answers through the game stream.
    void move(const QString &uci);
    void resign();
    void abort();
    /// Offers a draw, or accepts the one the opponent offered.
    void offerDraw();

Q_SIGNALS:
    void seeking();
    void gameStarted(const OnlineGame &game);
    /// The game's state changed: a move was played, a clock ticked, it ended.
    void gameUpdated(const OnlineGame &game);
    void gameFinished(const OnlineGame &game);
    /// Something went wrong with the platform; the game, if any, is over for us.
    void failed(const QString &message);

private:
    void openEventStream();
    void openGameStream(const QString &gameId);
    void post(const QString &path, const QByteArray &form = {});
    void readLines(QNetworkReply *reply, QByteArray &buffer, void (LichessBoardClient::*line)(const QByteArray &));
    void eventLine(const QByteArray &line);
    void gameLine(const QByteArray &line);
    void closeGame();

    QString m_token;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_events = nullptr;
    QNetworkReply *m_seek = nullptr;
    QNetworkReply *m_gameStream = nullptr;
    QByteArray m_eventBuffer;
    QByteArray m_gameBuffer;
    OnlineGame m_game;
};
