#include "AiAssistant.h"

#include "MainWindow.h"
#include "app/GameDatabase.h"
#include "app/GameSession.h"
#include "app/Pgn.h"
#include "app/SqliteGameDatabase.h"
#include "app/UiLanguage.h"
#include "app/UciEngine.h"
#include "app/UserFolders.h"
#include "app/ai/ChatGptClient.h"
#include "dialogs/AiAssistantDialog.h"
#include "models/GameFilterProxyModel.h"
#include "models/GameListModel.h"
#include "widgets/AiChatView.h"
#include "widgets/AiMenu.h"
#include "widgets/DatabaseTreeWidget.h"

#include <QAction>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QRegularExpression>
#include <QTimer>

namespace {

/// Calls for one question, at most: an AI going round in circles stops.
constexpr int kMaxRounds = 16;
/// Games a search answers with, at most.
constexpr int kMaxGames = 50;

/// Lower case, without accents: names are found however they are typed.
QString folded(const QString &text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar c : text) {
        const QString base = c.decomposition().isEmpty() ? QString(c) : c.decomposition();
        result += base.at(0).toLower();
    }
    return result;
}

/// Every word of `query` is in `text`, in any order ("bobby fischer" finds "Fischer, Robert James" only with "fischer").
bool matches(const QString &text, const QString &query)
{
    const QString haystack = folded(text);
    const QStringList words = folded(query).split(QRegularExpression(QStringLiteral("[\\s,.]+")), Qt::SkipEmptyParts);
    for (const QString &word : words) {
        if (!haystack.contains(word))
            return false;
    }
    return true;
}

QJsonObject error(const QString &text)
{
    return {{QStringLiteral("error"), text}};
}

QString sideName(Side side)
{
    return side == Side::White ? QStringLiteral("white") : QStringLiteral("black");
}

/// A menu item's text as a menu shows it, without its mnemonic.
QString plainText(const QAction *action)
{
    QString text = action->text();
    text.remove(QRegularExpression(QStringLiteral("&(?!&)")));
    return text.replace(QLatin1String("&&"), QLatin1String("&")).trimmed();
}

/// Every command of the menus with its path ("Game ▸ New Training…").
void collectCommands(QMenu *menu, const QString &path, QList<QPair<QString, QAction *>> &commands)
{
    // Menus filled as they open (Switch Database…) are filled now.
    Q_EMIT menu->aboutToShow();
    for (QAction *action : menu->actions()) {
        if (action->isSeparator() || !action->isVisible() || plainText(action).isEmpty())
            continue;
        const QString name = path + QStringLiteral(" ▸ ") + plainText(action);
        if (action->menu())
            collectCommands(action->menu(), name, commands);
        else
            commands.append({name, action});
    }
}

QList<QPair<QString, QAction *>> menuCommands(QMenuBar *bar)
{
    QList<QPair<QString, QAction *>> commands;
    for (QAction *action : bar->actions()) {
        if (action->menu() && action->isVisible())
            collectCommands(action->menu(), plainText(action), commands);
    }
    return commands;
}

QJsonObject object(const QString &key, const QJsonValue &value)
{
    return {{key, value}};
}

QJsonObject schema(const QJsonObject &properties = {}, const QStringList &required = {})
{
    return {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), properties},
            {QStringLiteral("required"), QJsonArray::fromStringList(required)}};
}

QJsonObject param(const QString &type, const QString &description)
{
    return {{QStringLiteral("type"), type}, {QStringLiteral("description"), description}};
}

} // namespace

AiAssistant::AiAssistant(MainWindow *window, AiChatView *chat, AiMenu *account)
    : QObject(window)
    , m_window(window)
    , m_chat(chat)
    , m_account(account)
    , m_client(new ChatGptClient(this))
{
    connect(m_chat, &AiChatView::asked, this, &AiAssistant::ask);
    connect(m_client, &ChatGptClient::answerDelta, m_chat, &AiChatView::appendAnswer);
    connect(m_client, &ChatGptClient::answerCompleted, this, [this](const QJsonArray &items) {
        // The calls are carried out once the network's slot has returned:
        // one may open a window that waits for the user.
        QTimer::singleShot(0, this, [this, items] { answered(items); });
    });
    connect(m_client, &ChatGptClient::failed, this, [this](const QString &message) {
        while (m_items.size() > m_itemsBeforeQuestion)
            m_items.removeLast();
        m_chat->showError(message);
    });
    connect(m_client, &ChatGptClient::credentialsChanged, m_account, &AiMenu::updateCredentials);
    // The connection gone: the button goes back to signing in.
    connect(m_client, &ChatGptClient::signedOut, this, [this] {
        m_chat->showError(tr("The connection to ChatGPT was lost: sign in again."));
        m_account->signOut();
    });
    connect(m_account, &AiMenu::accountChanged, this, [this] {
        if (!m_account->isSignedIn())
            reset();
    });
}

void AiAssistant::reset()
{
    m_client->cancel();
    m_items = {};
    m_itemsBeforeQuestion = 0;
    m_chat->clear();
}

void AiAssistant::ask(const QString &question)
{
    if (m_client->isBusy())
        return;
    m_chat->addQuestion(question);
    // While a game is played online nothing may help: the engine is off, and so is the AI.
    if (m_window->m_onlinePlay) {
        m_chat->showError(tr("The AI is off while you play online."));
        return;
    }
    m_itemsBeforeQuestion = m_items.size();
    m_items.append(ChatGptResponses::messageItem({true, question}));
    m_rounds = 0;
    request();
}

void AiAssistant::request()
{
    m_client->setCredentials(m_account->credentials());
    m_client->setModel(AiAssistantDialog::chosenModel());
    m_client->ask(instructions(), m_items, tools());
}

void AiAssistant::answered(const QJsonArray &items)
{
    QList<QJsonObject> calls;
    for (const QJsonValue &value : items) {
        const QJsonObject replayed = ChatGptResponses::replayItem(value.toObject());
        if (replayed.isEmpty())
            continue;
        m_items.append(replayed);
        if (replayed.value(QStringLiteral("type")).toString() == QLatin1String("function_call"))
            calls << replayed;
    }
    if (calls.isEmpty()) {
        m_chat->finishAnswer();
        return;
    }
    if (++m_rounds > kMaxRounds) {
        m_chat->showError(tr("The AI stopped: too many steps for one question."));
        return;
    }
    runCalls(calls, 0);
}

void AiAssistant::runCalls(const QList<QJsonObject> &calls, int next)
{
    if (next >= calls.size()) {
        if (m_window->m_onlinePlay) { // An online game started meanwhile.
            m_chat->showError(tr("The AI is off while you play online."));
            return;
        }
        request();
        return;
    }
    const QJsonObject &item = calls.at(next);
    const QString callId = item.value(QStringLiteral("call_id")).toString();
    const QJsonObject arguments = QJsonDocument::fromJson(item.value(QStringLiteral("arguments")).toString().toUtf8()).object();
    call(item.value(QStringLiteral("name")).toString(), arguments, [this, calls, next, callId](const QJsonObject &output) {
        m_items.append(ChatGptResponses::toolOutputItem(callId, output));
        runCalls(calls, next + 1);
    });
}

QString AiAssistant::instructions() const
{
    const QLocale language(UiLanguage::effective());
    QString text = QStringLiteral(
        "You are the assistant inside Pragma Chess, a chess database and analysis program, and you drive it for the "
        "user: with the functions you are given you read the board, play moves, let the engine move, go through the "
        "game, open databases and games, search them, and run any command of the menus. When the user asks for "
        "something the program can do, do it with the functions, then say in a few words what you did; never say you "
        "did something you did not do. Ask before something that cannot be undone (deleting, overwriting). When the "
        "user asks about the position, answer briefly and concretely, as a strong coach would, using the engine's "
        "view when it is there. Answer in %1. Write moves in SAN.\n\n")
                       .arg(QLocale::languageToString(language.language()));
    text += QStringLiteral("The program now: ") + QString::fromUtf8(QJsonDocument(state()).toJson(QJsonDocument::Compact));
    return text;
}

QJsonArray AiAssistant::tools()
{
    using ChatGptResponses::functionTool;
    const QString string = QStringLiteral("string");
    return {
        functionTool(QStringLiteral("get_state"),
                     QStringLiteral("The board and the program as they are now: position (FEN), moves of the line, "
                                    "the game's players, the engine's evaluation and best line, Explain, training, "
                                    "the open database."),
                     schema()),
        functionTool(QStringLiteral("play_moves"),
                     QStringLiteral("Plays moves on the board from the position shown, one after another, as the user "
                                    "would: at the end of the game they extend it, elsewhere they become a variation."),
                     schema({{QStringLiteral("moves"),
                              param(string, QStringLiteral("SAN or UCI moves separated by spaces, move numbers allowed: \"e4 e5 Nf3\"."))}},
                            {QStringLiteral("moves")})),
        functionTool(QStringLiteral("engine_move"),
                     QStringLiteral("The engine plays the move of the side to move on the board (\"play your move\"): "
                                    "the book's in the opening, otherwise its best. Answers once the move is played."),
                     schema()),
        functionTool(QStringLiteral("go_to"),
                     QStringLiteral("Moves through the game on the board: to a ply (0 is the start), or start, back, "
                                    "forward, end."),
                     schema({{QStringLiteral("ply"), param(QStringLiteral("integer"), QStringLiteral("Half-moves from the start."))},
                             {QStringLiteral("where"),
                              QJsonObject{{QStringLiteral("type"), string},
                                          {QStringLiteral("enum"), QJsonArray{QStringLiteral("start"), QStringLiteral("back"),
                                                                              QStringLiteral("forward"), QStringLiteral("end")}}}}})),
        functionTool(QStringLiteral("new_game"),
                     QStringLiteral("Starts a new game on the board: from the initial position, or from a FEN, or "
                                    "with moves already played (it opens at their end)."),
                     schema({{QStringLiteral("fen"), param(string, QStringLiteral("The starting position, optional."))},
                             {QStringLiteral("moves"), param(string, QStringLiteral("Moves played from it, PGN or SAN, optional."))}})),
        functionTool(QStringLiteral("set_option"),
                     QStringLiteral("Turns on or off: the engine's analysis, Explain (arrows explaining the last move), "
                                    "the board turned (Black below)."),
                     schema({{QStringLiteral("option"),
                              QJsonObject{{QStringLiteral("type"), string},
                                          {QStringLiteral("enum"), QJsonArray{QStringLiteral("analysis"), QStringLiteral("explain"),
                                                                              QStringLiteral("flip_board")}}}},
                             {QStringLiteral("on"), param(QStringLiteral("boolean"), QString())}},
                            {QStringLiteral("option"), QStringLiteral("on")})),
        functionTool(QStringLiteral("list_databases"),
                     QStringLiteral("The databases of the user's Databases folder, and the one open."), schema()),
        functionTool(QStringLiteral("open_database"),
                     QStringLiteral("Opens a database by its name (as list_databases gives it) or its file."),
                     schema({{QStringLiteral("name"), param(string, QString())}}, {QStringLiteral("name")})),
        functionTool(QStringLiteral("search_games"),
                     QStringLiteral("Searches the open database's games; every word given must be in the field "
                                    "(any order, case and accents ignored). Answers with each game's index."),
                     schema({{QStringLiteral("player"), param(string, QStringLiteral("Either player, e.g. \"Fischer\"."))},
                             {QStringLiteral("white"), param(string, QString())},
                             {QStringLiteral("black"), param(string, QString())},
                             {QStringLiteral("event"), param(string, QString())},
                             {QStringLiteral("year_from"), param(QStringLiteral("integer"), QString())},
                             {QStringLiteral("year_to"), param(QStringLiteral("integer"), QString())},
                             {QStringLiteral("result"), param(string, QStringLiteral("1-0, 0-1 or 1/2-1/2."))},
                             {QStringLiteral("eco"), param(string, QStringLiteral("An ECO code or its beginning: \"B9\"."))},
                             {QStringLiteral("limit"), param(QStringLiteral("integer"), QStringLiteral("At most 50."))}})),
        functionTool(QStringLiteral("open_game"),
                     QStringLiteral("Opens a game of the open database on the board, by the index search_games gave."),
                     schema({{QStringLiteral("index"), param(QStringLiteral("integer"), QString())}}, {QStringLiteral("index")})),
        functionTool(QStringLiteral("games_with_position"),
                     QStringLiteral("The games of the open database in which the position on the board occurs, in any "
                                    "move order; the games list shows them."),
                     schema()),
        functionTool(QStringLiteral("list_menu_commands"),
                     QStringLiteral("Every command of the program's menus, as \"Menu ▸ Item\", in the interface's "
                                    "language, with whether it is enabled and checked: for what the other functions do not do."),
                     schema()),
        functionTool(QStringLiteral("run_menu_command"),
                     QStringLiteral("Runs a command of the menus by its path as list_menu_commands gives it (or its last "
                                    "part when that is unique). A command may open a window for the user."),
                     schema({{QStringLiteral("command"), param(string, QString())}}, {QStringLiteral("command")})),
    };
}

void AiAssistant::call(const QString &name, const QJsonObject &arguments, const Done &done)
{
    QJsonObject output;
    if (name == QLatin1String("engine_move")) {
        engineMove(done);
        return;
    }
    if (name == QLatin1String("get_state"))
        output = state();
    else if (name == QLatin1String("play_moves"))
        output = playMoves(arguments.value(QStringLiteral("moves")).toString());
    else if (name == QLatin1String("go_to"))
        output = goTo(arguments);
    else if (name == QLatin1String("new_game"))
        output = newGame(arguments);
    else if (name == QLatin1String("set_option"))
        output = setOption(arguments.value(QStringLiteral("option")).toString(), arguments.value(QStringLiteral("on")).toBool());
    else if (name == QLatin1String("list_databases"))
        output = listDatabases();
    else if (name == QLatin1String("open_database"))
        output = openDatabase(arguments.value(QStringLiteral("name")).toString());
    else if (name == QLatin1String("search_games"))
        output = searchGames(arguments);
    else if (name == QLatin1String("open_game"))
        output = openGame(qint64(arguments.value(QStringLiteral("index")).toDouble(-1)));
    else if (name == QLatin1String("games_with_position"))
        output = gamesWithPosition();
    else if (name == QLatin1String("list_menu_commands"))
        output = listMenuCommands();
    else if (name == QLatin1String("run_menu_command"))
        output = runMenuCommand(arguments.value(QStringLiteral("command")).toString());
    else
        output = error(QStringLiteral("no function \"%1\"").arg(name));
    done(output);
}

QJsonObject AiAssistant::state() const
{
    const MainWindow *w = m_window;
    const GameSession *session = w->m_session;
    const ChessPosition &position = session->position();
    QString line;
    for (int ply = 1; ply <= session->plyCount(); ++ply) {
        const ChessPosition &before = session->positionAt(ply - 1);
        if (before.sideToMove() == Side::White || ply == 1)
            line += before.moveNumberText();
        line += session->moveAt(ply).san + QLatin1Char(' ');
    }
    const GameRecord &game = session->game();
    QJsonObject result{{QStringLiteral("fen"), position.fen()},
                       {QStringLiteral("sideToMove"), sideName(position.sideToMove())},
                       {QStringLiteral("ply"), session->ply()},
                       {QStringLiteral("plyCount"), session->plyCount()},
                       {QStringLiteral("line"), line.trimmed()},
                       {QStringLiteral("inVariation"), !session->path().isEmpty()},
                       {QStringLiteral("game"), QJsonObject{{QStringLiteral("white"), game.white},
                                                            {QStringLiteral("black"), game.black},
                                                            {QStringLiteral("event"), game.event},
                                                            {QStringLiteral("date"), game.date},
                                                            {QStringLiteral("result"), game.result}}},
                       {QStringLiteral("boardFlipped"), w->m_flipBoardAction->isChecked()},
                       {QStringLiteral("explain"), w->m_explainAction->isChecked()},
                       {QStringLiteral("training"), w->m_trainingModeAction->isChecked()},
                       {QStringLiteral("checkmate"), position.isCheckmate()},
                       {QStringLiteral("stalemate"), position.isStalemate()}};
    QJsonObject engine{{QStringLiteral("name"), w->m_engineName}, {QStringLiteral("analyzing"), w->m_startEngineAction->isChecked()}};
    if (w->m_startEngineAction->isChecked() && w->m_lastEvaluation.depth > 0) {
        engine.insert(QStringLiteral("evaluation"), w->m_lastEvaluation.text());
        engine.insert(QStringLiteral("depth"), w->m_lastEvaluation.depth);
        engine.insert(QStringLiteral("bestLine"), w->m_engineLine);
    }
    result.insert(QStringLiteral("engine"), engine);
    if (w->m_database) {
        result.insert(QStringLiteral("database"),
                      QJsonObject{{QStringLiteral("name"), w->m_database->properties().label(UiLanguage::effective(), w->m_database->location())},
                                  {QStringLiteral("games"), w->m_database->countGames(GameState::Live)}});
    }
    return result;
}

QJsonObject AiAssistant::playMoves(const QString &moves)
{
    MainWindow *w = m_window;
    if (w->isOpponentTurn())
        return error(QStringLiteral("it is the opponent's move in the game being played"));
    // Move numbers ("12." "12...") and results are not moves.
    static const QRegularExpression notMove(QStringLiteral("^(\\d+\\.+|1-0|0-1|1/2-1/2|\\*)$"));
    QStringList played;
    for (QString token : moves.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        token.remove(QRegularExpression(QStringLiteral("^\\d+\\.+")));
        if (token.isEmpty() || notMove.match(token).hasMatch())
            continue;
        const ChessPosition before = w->m_session->position();
        std::optional<ChessMove> move = before.moveFromSan(token);
        if (!move)
            move = before.moveFromUci(token.toLower());
        if (!move) {
            QJsonObject answer = error(QStringLiteral("\"%1\" is not a legal move in %2").arg(token, before.fen()));
            answer.insert(QStringLiteral("played"), QJsonArray::fromStringList(played));
            return answer;
        }
        played << before.moveNumberText() + before.san(*move);
        w->playMove(*move);
    }
    if (!played.isEmpty())
        m_chat->addAction(tr("Played %1").arg(played.join(QLatin1Char(' '))));
    QJsonObject answer = state();
    answer.insert(QStringLiteral("played"), QJsonArray::fromStringList(played));
    return answer;
}

void AiAssistant::engineMove(const Done &done)
{
    MainWindow *w = m_window;
    const ChessPosition &position = w->m_session->position();
    if (position.isCheckmate() || position.isStalemate() || !position.hasKings()) {
        done(error(QStringLiteral("there is no move to play in this position")));
        return;
    }
    if (w->isOpponentTurn()) {
        done(error(QStringLiteral("it is the opponent's move in the game being played")));
        return;
    }
    // In training the engine answers by itself; it moves here only when asked.
    const int plyBefore = w->m_session->ply();
    const QString fenBefore = position.fen();
    if (!w->m_trainingModeAction->isChecked())
        w->m_trainingBaseline.depth = 0; // No tutor outside training.
    w->playEngineMove();
    if (!w->m_trainingThinking) {
        done(error(QStringLiteral("the engine could not start")));
        return;
    }
    // Answered once the move is on the board (or the search is over without one).
    auto *timer = new QTimer(this);
    auto *clock = new QElapsedTimer;
    clock->start();
    connect(timer, &QTimer::timeout, this, [this, w, timer, clock, done, plyBefore, fenBefore] {
        if (w->m_trainingThinking && clock->elapsed() < 60000)
            return;
        timer->deleteLater();
        delete clock;
        const GameSession *session = w->m_session;
        if (session->ply() == plyBefore + 1 && session->positionAt(plyBefore).fen() == fenBefore) {
            const ChessPosition &before = session->positionAt(plyBefore);
            const QString move = before.moveNumberText() + session->moveAt(plyBefore + 1).san;
            m_chat->addAction(tr("The engine played %1").arg(move));
            QJsonObject answer = state();
            answer.insert(QStringLiteral("played"), move);
            done(answer);
        } else {
            done(error(QStringLiteral("the engine played no move (its search ended, or the tutor held its reply)")));
        }
    });
    timer->start(200);
}

QJsonObject AiAssistant::goTo(const QJsonObject &arguments)
{
    GameSession *session = m_window->m_session;
    int ply = session->ply();
    const QString where = arguments.value(QStringLiteral("where")).toString();
    if (arguments.value(QStringLiteral("ply")).isDouble())
        ply = arguments.value(QStringLiteral("ply")).toInt();
    else if (where == QLatin1String("start"))
        ply = 0;
    else if (where == QLatin1String("back"))
        ply -= 1;
    else if (where == QLatin1String("forward"))
        ply += 1;
    else if (where == QLatin1String("end"))
        ply = session->plyCount();
    else
        return error(QStringLiteral("give a ply or where"));
    session->goToPly(qBound(0, ply, session->plyCount()));
    return state();
}

QJsonObject AiAssistant::newGame(const QJsonObject &arguments)
{
    MainWindow *w = m_window;
    const QString fen = arguments.value(QStringLiteral("fen")).toString();
    const QString moves = arguments.value(QStringLiteral("moves")).toString();
    if (fen.isEmpty() && moves.isEmpty()) {
        w->m_newGameAction->trigger();
        m_chat->addAction(tr("New game"));
        return state();
    }
    QString problem;
    const std::optional<Pgn::ParsedLine> line = Pgn::parseLine(moves, fen, &problem);
    if (!line)
        return error(problem);
    if (!w->canLeaveGame())
        return error(QStringLiteral("the board cannot leave the game being played online"));
    GameRecord game;
    game.startFen = line->startFen;
    game.moves = line->moves;
    game.variations = line->variations;
    game.result = QStringLiteral("*");
    w->startGame(game);
    w->m_session->goToPly(w->m_session->plyCount());
    m_chat->addAction(tr("New game"));
    return state();
}

QJsonObject AiAssistant::setOption(const QString &option, bool on)
{
    MainWindow *w = m_window;
    QAction *action = option == QLatin1String("analysis") ? w->m_startEngineAction
        : option == QLatin1String("explain")              ? w->m_explainAction
        : option == QLatin1String("flip_board")           ? w->m_flipBoardAction
                                                          : nullptr;
    if (!action)
        return error(QStringLiteral("no option \"%1\"").arg(option));
    if (!action->isEnabled())
        return error(QStringLiteral("\"%1\" cannot be changed now").arg(option));
    action->setChecked(on);
    // Named as the feature, not as the action's text, which says what a click would do (Stop).
    const QString feature = option == QLatin1String("analysis") ? tr("Analysis")
        : option == QLatin1String("explain")                    ? tr("Explain")
                                                                : tr("Board turned");
    m_chat->addAction(QStringLiteral("%1: %2").arg(feature, on ? tr("on") : tr("off")));
    return state();
}

QJsonObject AiAssistant::listDatabases() const
{
    const QString open = m_window->m_database ? m_window->m_database->location() : QString();
    QJsonArray list;
    const QDir folder(UserFolders::databasesDir());
    for (const QFileInfo &file : folder.entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)},
                                                      QDir::Files, QDir::Name)) {
        const QString path = file.absoluteFilePath();
        list.append(QJsonObject{{QStringLiteral("name"), SqliteGameDatabase::readProperties(path).label(UiLanguage::effective(), path)},
                                {QStringLiteral("file"), file.fileName()},
                                {QStringLiteral("open"), path == open}});
    }
    return object(QStringLiteral("databases"), list);
}

QJsonObject AiAssistant::openDatabase(const QString &name)
{
    const QDir folder(UserFolders::databasesDir());
    QString found;
    for (const QFileInfo &file : folder.entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)},
                                                      QDir::Files, QDir::Name)) {
        const QString path = file.absoluteFilePath();
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(path);
        if (folded(file.fileName()) == folded(name) || folded(file.completeBaseName()) == folded(name)
            || folded(properties.label(UiLanguage::effective(), path)) == folded(name)
            || folded(properties.displayName(UiLanguage::effective(), file.completeBaseName())) == folded(name)) {
            found = path;
            break;
        }
    }
    if (found.isEmpty() && QFileInfo(name).isAbsolute() && QFileInfo::exists(name))
        found = name;
    if (found.isEmpty())
        return error(QStringLiteral("no database \"%1\": list_databases says which there are").arg(name));
    if (!m_window->openDatabaseFile(found))
        return error(QStringLiteral("the database could not be opened"));
    m_chat->addAction(tr("Opened %1").arg(QFileInfo(found).completeBaseName()));
    return state();
}

QJsonObject AiAssistant::searchGames(const QJsonObject &arguments) const
{
    const GameDatabase *database = m_window->m_database.get();
    if (!database)
        return error(QStringLiteral("no database is open"));
    const auto text = [&](const char *key) { return arguments.value(QLatin1String(key)).toString().trimmed(); };
    const QString player = text("player"), white = text("white"), black = text("black"), event = text("event"),
                  result = text("result"), eco = text("eco");
    const int yearFrom = arguments.value(QStringLiteral("year_from")).toInt(0);
    const int yearTo = arguments.value(QStringLiteral("year_to")).toInt(0);
    const int limit = qBound(1, arguments.value(QStringLiteral("limit")).toInt(20), kMaxGames);
    QJsonArray games;
    qint64 total = 0;
    for (qint64 index = 0; index < database->gameCount(); ++index) {
        if (database->stateOf(index) != GameState::Live)
            continue;
        const GameRecord game = database->brief(index);
        if (!player.isEmpty() && !matches(game.white, player) && !matches(game.black, player))
            continue;
        if ((!white.isEmpty() && !matches(game.white, white)) || (!black.isEmpty() && !matches(game.black, black))
            || (!event.isEmpty() && !matches(game.event, event)) || (!result.isEmpty() && game.result != result)
            || (!eco.isEmpty() && !game.eco.startsWith(eco, Qt::CaseInsensitive)))
            continue;
        const int year = game.date.left(4).toInt();
        if ((yearFrom && (!year || year < yearFrom)) || (yearTo && (!year || year > yearTo)))
            continue;
        ++total;
        if (games.size() < limit)
            games.append(QJsonObject{{QStringLiteral("index"), index},
                                     {QStringLiteral("white"), game.white},
                                     {QStringLiteral("black"), game.black},
                                     {QStringLiteral("whiteElo"), game.whiteElo},
                                     {QStringLiteral("blackElo"), game.blackElo},
                                     {QStringLiteral("event"), game.event},
                                     {QStringLiteral("date"), game.date},
                                     {QStringLiteral("result"), game.result},
                                     {QStringLiteral("eco"), game.eco},
                                     {QStringLiteral("plies"), game.plyCount}});
    }
    return {{QStringLiteral("found"), total}, {QStringLiteral("games"), games}};
}

QJsonObject AiAssistant::openGame(qint64 index)
{
    MainWindow *w = m_window;
    if (!w->m_database || index < 0 || index >= w->m_database->gameCount())
        return error(QStringLiteral("no game %1 in the open database").arg(index));
    QModelIndex row = w->m_gameListProxy->mapFromSource(w->m_gameListModel->index(int(index), 0));
    if (!row.isValid()) {
        // Hidden by the part of the database chosen in the tree: all the games are shown.
        w->showCategory(GameCategory{});
        row = w->m_gameListProxy->mapFromSource(w->m_gameListModel->index(int(index), 0));
    }
    if (!row.isValid())
        return error(QStringLiteral("game %1 is not among the live games").arg(index));
    w->openGame(row);
    const GameRecord &game = w->m_session->game();
    m_chat->addAction(tr("Opened %1 – %2").arg(game.white, game.black));
    return state();
}

QJsonObject AiAssistant::gamesWithPosition()
{
    MainWindow *w = m_window;
    if (!w->m_database)
        return error(QStringLiteral("no database is open"));
    GameCategory category;
    category.kind = GameCategory::Kind::Position;
    w->showCategory(category);
    QJsonArray games;
    const int rows = w->m_gameListProxy->rowCount();
    for (int row = 0; row < rows && games.size() < kMaxGames; ++row) {
        const qint64 index = w->m_gameListProxy->mapToSource(w->m_gameListProxy->index(row, 0)).row();
        const GameRecord game = w->m_database->brief(index);
        games.append(QJsonObject{{QStringLiteral("index"), index},
                                 {QStringLiteral("white"), game.white},
                                 {QStringLiteral("black"), game.black},
                                 {QStringLiteral("date"), game.date},
                                 {QStringLiteral("result"), game.result}});
    }
    m_chat->addAction(tr("Listed the games with this position"));
    return {{QStringLiteral("found"), rows}, {QStringLiteral("games"), games}};
}

QJsonObject AiAssistant::listMenuCommands() const
{
    QJsonArray list;
    for (const auto &[name, action] : menuCommands(m_window->menuBar())) {
        QJsonObject command{{QStringLiteral("command"), name}};
        if (!action->isEnabled())
            command.insert(QStringLiteral("enabled"), false);
        if (action->isCheckable())
            command.insert(QStringLiteral("checked"), action->isChecked());
        list.append(command);
    }
    return object(QStringLiteral("commands"), list);
}

QJsonObject AiAssistant::runMenuCommand(const QString &command)
{
    // The whole path, else its last part when only one command has it.
    const QList<QPair<QString, QAction *>> commands = menuCommands(m_window->menuBar());
    QAction *found = nullptr;
    QString foundName;
    int lastPartMatches = 0;
    for (const auto &[name, action] : commands) {
        if (folded(name) == folded(command)) {
            found = action;
            foundName = name;
            lastPartMatches = 1;
            break;
        }
        if (folded(name.section(QStringLiteral(" ▸ "), -1)) == folded(command.section(QStringLiteral(" ▸ "), -1))) {
            ++lastPartMatches;
            found = action;
            foundName = name;
        }
    }
    if (!found || lastPartMatches != 1)
        return error(QStringLiteral("no single command \"%1\": list_menu_commands gives them").arg(command));
    if (!found->isEnabled())
        return error(QStringLiteral("\"%1\" is not available now").arg(foundName));
    // After this answer: a command may open a window that waits for the user.
    QTimer::singleShot(0, found, &QAction::trigger);
    m_chat->addAction(foundName);
    return {{QStringLiteral("ran"), foundName}};
}
