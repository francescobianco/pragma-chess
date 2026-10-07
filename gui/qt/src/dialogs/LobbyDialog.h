#pragma once

#include "app/lobby/Lobby.h"

#include <QDialog>

class BoardWidget;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTreeWidget;

/// Game ▸ Enter the Lobby…: the tournaments of four players with a free
/// seat (IDEA.md). Entering a room shows its seats and its games, each
/// game's position on a small board; a free seat can be taken, and the
/// user's games with everyone there appear. For now the user experience
/// only: the rooms are Lobby::sample(), kept while the application runs.
class LobbyDialog : public QDialog {
    Q_OBJECT

public:
    /// `me` is the name the user sits with.
    explicit LobbyDialog(const QString &me, QWidget *parent = nullptr);

    /// What the user does, for the development API: back to the list, the
    /// room on row `row` of the list, the game on row `row` of the room.
    void showLobbyPage();
    void enterRoomAt(int row);
    void selectGame(int row);
    /// Takes a free seat in the room shown.
    void join();
    /// Plays the game selected, when it is the user's.
    void play();
    /// The Play Now button of row `row` of the list: the one game waiting
    /// for the user's move, or a menu of them when there are several.
    void playNow(int row);

private:
    void fillLobby();
    /// Shows room `index` of the lobby, -1 a new room nobody sits in yet.
    void enterRoom(int index);
    void enterSelectedRoom();
    void showRoom();
    void showSelectedGame();
    /// Enters room `room` (an index of the lobby) on its game `game`, and plays it.
    void playGame(int room, int game);
    /// The room shown: the lobby's, or the new room offered.
    LobbyRoom shownRoom() const;

    Lobby m_lobby;
    QString m_me;
    /// The room shown: an index of the lobby, -1 - k the new room offered k.
    int m_room = -1;

    QStackedWidget *m_pages;
    QTreeWidget *m_rooms;
    QPushButton *m_enterButton;
    QLabel *m_roomTitle;
    QTreeWidget *m_standings;
    QPushButton *m_joinButton;
    QLabel *m_joinHint;
    QTreeWidget *m_games;
    BoardWidget *m_board;
    QLabel *m_gameLine;
    QPushButton *m_playButton;
    QLabel *m_notice;
};
