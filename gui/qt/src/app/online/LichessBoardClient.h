#pragma once

#include "OnlineClient.h"

#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/// Plays on lichess.org through its Board API with the user's token
/// (scope board:play): looks for an opponent, follows the game's stream and
/// sends the user's moves. One game at a time.
class LichessBoardClient : public OnlineClient {
    Q_OBJECT

public:
    explicit LichessBoardClient(const QString &token, QObject *parent = nullptr);
    ~LichessBoardClient() override;

    void seek(const Seek &seek) override;
    void cancelSeek() override;
    bool canResume() const override { return true; }
    /// Follows again a game already started, e.g. after the application was
    /// closed during it: the stream sends the whole game (or its end) again.
    void resume(const QString &gameId) override;
    bool isSeeking() const override { return m_seek != nullptr; }

    const OnlineGame &game() const override { return m_game; }
    bool isPlaying() const override { return m_gameStream != nullptr; }
    /// The account's name, as the game's players carry it.
    QString playingAs() const override { return {}; }
    QString gameUrl() const override { return QStringLiteral("https://lichess.org/%1").arg(m_game.id); }

    /// The user's move, UCI; the platform answers through the game stream.
    void move(const QString &uci) override;
    void resign() override;
    void abort() override;
    /// Offers a draw, or accepts the one the opponent offered.
    void offerDraw() override;

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
