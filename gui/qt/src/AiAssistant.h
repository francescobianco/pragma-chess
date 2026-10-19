#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <functional>

class AiChatView;
class AiMenu;
class ChatGptClient;
class MainWindow;

/// The AI's hold on Pragma Chess: the chat's questions go to ChatGPT with
/// the functions it may call — read the board, play moves, let the engine
/// move, open databases and games, search them, run any menu command — and
/// the calls it makes are carried out here, on the window, until it
/// answers in words. A friend of MainWindow, as DesktopApi.
class AiAssistant : public QObject {
    Q_OBJECT

public:
    AiAssistant(MainWindow *window, AiChatView *chat, AiMenu *account);

    /// Forgets the conversation (signed out, or a new one).
    void reset();

    using Done = std::function<void(const QJsonObject &)>;
    /// Carries out the function `name` as if the AI had called it (also the
    /// development API's way to try them); `done` gets its output.
    void call(const QString &name, const QJsonObject &arguments, const Done &done);

private:
    void ask(const QString &question);
    void request();
    void answered(const QJsonArray &items);
    /// Carries out the calls from `next` on, then asks again with their outputs.
    void runCalls(const QList<QJsonObject> &calls, int next);
    QString instructions() const;
    static QJsonArray tools();

    // The functions.
    QJsonObject state() const;
    QJsonObject playMoves(const QString &moves);
    void engineMove(const Done &done);
    QJsonObject goTo(const QJsonObject &arguments);
    QJsonObject newGame(const QJsonObject &arguments);
    QJsonObject setOption(const QString &option, bool on);
    QJsonObject listDatabases() const;
    QJsonObject openDatabase(const QString &name);
    QJsonObject searchGames(const QJsonObject &arguments) const;
    QJsonObject openGame(qint64 index);
    QJsonObject gamesWithPosition();
    QJsonObject listMenuCommands() const;
    QJsonObject runMenuCommand(const QString &command);

    MainWindow *m_window;
    AiChatView *m_chat;
    AiMenu *m_account;
    ChatGptClient *m_client;
    /// The conversation as the requests carry it (they are not stored).
    QJsonArray m_items;
    /// Where it stood before the question being answered: a failure goes back there.
    qsizetype m_itemsBeforeQuestion = 0;
    /// Rounds of calls for this question, bounded.
    int m_rounds = 0;
};
