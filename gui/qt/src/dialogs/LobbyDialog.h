#pragma once

#include "app/lobby/LobbyService.h"

#include <QDialog>
#include <QSet>

class BoardWidget;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QVariantAnimation;

/// Game ▸ Enter the Lobby…: the tournaments of four players with a free
/// seat (IDEA.md). Entering a room shows its seats and its games, each
/// game's position on a small board; a free seat can be taken, and the
/// user's games with everyone there appear. The lobby is the network's,
/// through the main window's LobbyService; the window follows it as events
/// come in.
class LobbyDialog : public QDialog {
    Q_OBJECT

public:
    /// `service` is the main window's lobby, on the network.
    LobbyDialog(LobbyService *service, QWidget *parent = nullptr);

    /// What the user does, for the development API: back to the list, the
    /// room on row `row` of the list, the game on row `row` of the room.
    void showLobbyPage();
    /// Shows the list or the room again as the lobby has it now.
    void refresh();
    void enterRoomAt(int row);
    void selectGame(int row);
    /// Takes a free seat in the room shown.
    void join();
    /// Plays the game selected, when it is the user's.
    void play();
    /// The Play Now button of row `row` of the list: the one game waiting
    /// for the user's move, or a menu of them when there are several.
    void playNow(int row);

Q_SIGNALS:
    /// Play: the user's game `white` against `black` in room `roomId`, to
    /// bring to the board.
    void playRequested(const QString &roomId, const QString &white, const QString &black);

protected:
    /// The lobby may have moved while the window was away (moves sent from the board).
    void showEvent(QShowEvent *event) override;

private:
    void fillLobby();
    /// Shows room `index` of the lobby, -1 a new room nobody sits in yet.
    void enterRoom(int index);
    void enterSelectedRoom();
    void showRoom();
    void showSelectedGame();
    void showNetwork();
    /// The lobby changed: the window follows it, and what turned to the
    /// user's move since glows.
    void lobbyChanged();
    /// The games waiting for the user's move now, by "room|white|black".
    QSet<QString> waitingGames() const;
    void paintGlow();
    /// Enters room `room` (an index of the lobby) on its game `game`, and plays it.
    void playGame(int room, int game);
    /// The room shown: the lobby's, or the new room offered.
    LobbyRoom shownRoom() const;
    const Lobby &lobby() const { return m_service->lobby(); }
    /// The index of the room shown in the lobby now, -1 for a new room.
    int roomIndex() const;

    LobbyService *m_service;
    QString m_me;
    /// The room shown, by id (indexes move as rooms come from the network),
    /// or the new room offered `m_offer`.
    QString m_roomId;
    int m_offer = -1;

    QStackedWidget *m_pages;
    QTreeWidget *m_rooms;
    QPushButton *m_enterButton;
    QLabel *m_roomTitle;
    QTreeWidget *m_standings;
    QPushButton *m_joinButton;
    QLabel *m_joinHint;
    QTreeWidget *m_games;
    BoardWidget *m_board;
    /// The players of the game shown, over the board.
    QLabel *m_gameNames;
    QLabel *m_gameLine;
    QPushButton *m_playButton;
    QLabel *m_notice;
    /// The network as it is: relays and peers, or none.
    QLabel *m_network;
    /// The window is live, the only place the application tells of what
    /// happens in the lobby (the rest of it does not disturb): a game that
    /// turns to the user's move glows, and its room in the list, fading.
    QSet<QString> m_waiting;
    QSet<QString> m_glowGames;
    QSet<QString> m_glowRooms;
    QVariantAnimation *m_glow;
};
