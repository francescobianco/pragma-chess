#pragma once

#include "OnlineGame.h"

#include <QObject>
#include <QString>

/// A platform to play on: looks for an opponent, follows the game and sends
/// the user's moves, one game at a time. Each platform has its own
/// (LichessBoardClient, FicsClient); the window only knows this.
class OnlineClient : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    /// How a game is asked for: clock in minutes and seconds of increment,
    /// rated or casual, the colour wanted ("random", "white", "black").
    struct Seek {
        int minutes = 10;
        int increment = 0;
        bool rated = false;
        QString color = QStringLiteral("random");
    };

    /// Looks for an opponent; gameStarted() follows when one is found.
    virtual void seek(const Seek &seek) = 0;
    virtual void cancelSeek() = 0;
    /// Whether a game in progress can be followed again after the
    /// application was closed (resume).
    virtual bool canResume() const { return false; }
    /// Follows again a game already started (canResume()).
    virtual void resume(const QString &gameId) { Q_UNUSED(gameId) }
    virtual bool isSeeking() const = 0;
    virtual bool isPlaying() const = 0;
    virtual const OnlineGame &game() const = 0;
    /// The name the user plays under: the account's, or the one the platform
    /// gave (a guest's); empty until known.
    virtual QString playingAs() const = 0;
    /// Where the game can be found again, for the game's Site.
    virtual QString gameUrl() const = 0;

    /// The user's move, UCI; the platform answers through gameUpdated().
    virtual void move(const QString &uci) = 0;
    virtual void resign() = 0;
    virtual void abort() = 0;
    /// Offers a draw, or accepts the one the opponent offered.
    virtual void offerDraw() = 0;

Q_SIGNALS:
    void seeking();
    void gameStarted(const OnlineGame &game);
    /// The game's state changed: a move was played, a clock ticked, it ended.
    void gameUpdated(const OnlineGame &game);
    void gameFinished(const OnlineGame &game);
    /// Something went wrong with the platform; the game, if any, is over for us.
    void failed(const QString &message);
};
