#pragma once

#include "app/StandInNames.h"
#include "app/GraphicsSettings.h"
#include "app/Chapters.h"
#include "app/EngineCatalog.h"
#include "app/EngineEvaluation.h"
#include "app/GameVariations.h"
#include "app/online/LichessBoardClient.h"
#include "app/online/OnlineAccount.h"
#include "app/WorkspaceLayout.h"
#include "app/PlayerRole.h"
#include "app/TrainingTutor.h"
#include "app/lobby/LobbyService.h"
#include "dialogs/NewGameChoiceDialog.h"
#include "dialogs/NewTrainingDialog.h"
#include "widgets/BoardWidget.h"
#include "widgets/DatabaseTreeWidget.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QMainWindow>
#include <QPointer>

#include <functional>
#include <memory>

struct GameSource;
struct PersonalSettings;
struct Project;

class BoardWidget;
class GameFilterProxyModel;
class QSplitter;
class BoardPanel;
class BoardSideColumn;
class CapturedPiecesWidget;
class EnginePanel;
class EvaluationBar;
class Explainer;
class GameHeaderWidget;
class UciEngine;
class GameDatabase;
class GameListModel;
class GameSession;
class HelpDialog;
class WelcomeDialog;
class LobbyDialog;
class LobbyNetwork;
class MoveTreeView;
class BookPanel;
struct ChessMove;
class OpeningNames;
class PolyglotBook;
class QAction;
class QDockWidget;
class QLabel;
class QMainWindow;
class QMenu;
class QTableView;
class QToolButton;
class QTimer;
class SourceSync;
class PositionIndexBuilder;
class FolderSync;
class RemoteStore;
class SyncPipeline;
class PhoneLink;
class DatabaseFolderStore;

class DesktopApi;

class MainWindow : public QMainWindow {
    Q_OBJECT
    friend class DesktopApi; // The development API reads and drives the window.

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Opens a .pch project file, e.g. one passed on the command line.
    bool openProjectFile(const QString &path);
    /// Opens a document the system hands over (command line, Finder): a
    /// project, or a database in the project open — the one restored from
    /// the last session. False for a file that is neither.
    bool openDocument(const QString &path);

    /// Closes without asking anything: the app was stopped from outside
    /// (SIGTERM from `make start`, a session logout), not by the user.
    void quitWithoutAsking();

protected:
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void createActions();
    void createMenus();
    void createToolBar();
    void createDocks();
    void createStatusBar();
    /// Full-length, visible lines between docked panels, in the palette's colors.
    void updateSeparatorStyle();

    QDockWidget *addDock(QMainWindow *host, const QString &objectName, const QString &title,
                         QWidget *widget, Qt::DockWidgetArea area);

    /// The panels as they are now: shown or not, and their shares of the
    /// usable area (WorkspaceLayout). Hidden panels keep their last share.
    WorkspaceLayout captureLayout();
    /// Shows the panels of `layout` in the fixed arrangement and gives them
    /// their shares; the sizes are set once the window is laid out.
    void applyLayout(const WorkspaceLayout &layout);
    /// Qt's opaque state of projects written before the shares.
    void restoreLegacyLayout(const QByteArray &layout);

    void setDatabase(std::unique_ptr<GameDatabase> database);
    void openGame(const QModelIndex &proxyIndex);
    void syncBoard();
    /// While the Engine panel's eye is on, the board shows where the best line ends.
    void peekAtEngineLine(bool held);
    void updateNavigationActions();
    void updateGameCount();
    /// Shows the games of a part of the database chosen in the tree.
    void showCategory(const GameCategory &category);
    /// Indexes the positions of the open database again, in the background.
    void rebuildPositionIndex();
    /// The database tree's open nodes and selected filter, per database and
    /// per computer (QSettings): not the project's, they change at every click.
    QString treeStateKey() const;
    void saveTreeState();
    void restoreTreeState();
    /// The Database column of the Opening Tree: how the open database's games went after each book move.
    void updateBookDatabaseStats();
    /// Counts the games matching the board for Position and Variant and, when
    /// one of them is shown, filters the list again.
    void updateBoardFilters();
    /// "Who Is This?" on a player of the games list.
    void showGameListMenu(const QPoint &position);
    /// Right click on a column title of the games list: Hide it, or Show one
    /// of the hidden ones. Which columns are shown is stored in the database
    /// (DatabaseProperties::hiddenColumns), so each one opens with its own.
    void showGameColumnsMenu(const QPoint &position);
    void setGameColumnsHidden(const QStringList &hidden);
    /// Shows the columns of the games list the open database does not hide.
    void applyGameColumns();
    /// Right click on a move of the move list: Copy and Annotations.
    void showMoveListMenu(const QPoint &position);
    /// Annotates the move leading to `ply` ("!", "±"…, as NAGs), in the
    /// database too when the game is stored.
    void annotateMove(int ply, const QList<int> &nags);
    /// Puts a game changed by a variation command on the board and stores it,
    /// after `question` (empty: none) is answered Yes.
    void applyGameEdit(const std::optional<GameVariations::Edit> &edit, const QString &question);
    /// Writes the text of a comment of the game on the board (MoveComment::at)
    /// and saves the game, keeping the comment's commands.
    void writeComment(const QList<int> &path, int index, const QString &text);
    /// Draws on the board the circles and arrows of the comment of the
    /// position shown (lichess's [%csl] and [%cal]).
    void updateCommentMarks();
    /// Plays a line written in a comment, from the ply `basePly` of the line
    /// `path`: it becomes a variation of the game, or takes the one it is.
    void playCommentLine(int game, const QList<int> &path, int basePly, const QStringList &uci);
    /// Writes the game on the board back to the database when it comes from
    /// there (moves, variations, annotations); otherwise the project keeps it.
    bool storeOpenGame(QString *error);
    /// The move leading to `ply` as copied to the clipboard: "12.Nf3!".
    QString moveText(int ply) const;
    void setPlayerRole(const QString &player, PlayerRole role);
    /// Index of the game with `uid` in the open database, -1 if it has none.
    qint64 gameIndexOf(const QString &uid) const;
    /// Moves games to the trash, back to the lists, or out of the trash.
    void setGameState(const QStringList &uids, GameState state);
    /// Asks, then deletes games of the trash (GameState::Deleted).
    void deleteGames(const QStringList &uids);
    /// Database Settings ▸ Optimize Database, with `dialog` as the parent of
    /// what it asks and reports.
    void optimizeDatabase(QWidget *dialog);
    /// Turns the board so the user plays from the bottom, when the database knows who they are.
    void orientBoardForMe(const GameRecord &game);

    // Entering games move by move.
    void newGame();
    /// Opens `game` as the game being entered, unlinked from the database: a
    /// new game at the end of the chapter (or in place of an empty one).
    void startGame(const GameRecord &game);
    /// Without chapters a new or opened game takes the place of the one on
    /// the board: when that one is saved nowhere, asks to save it to the
    /// database or let it go. False: cancelled, nothing is to change.
    bool mayReplaceBoardGame();
    /// Whether the game on the board stays, the next one coming after it
    /// (with chapters, or a game saved nowhere that was not let go).
    bool keepsBoardGame();
    /// Set when the user let the game on the board go (mayReplaceBoardGame).
    bool m_replaceBoardGame = false;
    void saveGameToDatabase();
    /// Game ▸ Set Up Position…: draws a position and starts a game from it.
    void setUpPosition();
    /// Game ▸ Save Game to Another Database…: saves the game on the board to
    /// a database chosen by the user, without opening it. False when nothing
    /// was saved.
    bool saveGameToAnotherDatabase();

    // Chapters: the project's games one after the other, with text between
    // their moves (ChapterBook). The session is the chapter's current game.
    /// The game on the board goes into the chapter as it is now.
    void syncChapterGame();
    /// The chapter remembers where the board is — the line, variations
    /// included, and the ply on it —, and the board goes back there.
    void rememberPlace();
    void returnToPlace();
    /// Puts the chapter's current game on the board: from the database when
    /// it is stored there, else as the chapter keeps it.
    void loadChapterGame();
    /// Goes to another game of the chapter, at `ply` of its line `path`.
    void switchToChapterGame(int game, const QList<int> &path, int ply);
    /// The database changed: the game on the board is found in it again, by uid.
    void relinkChapterGame();
    /// The chapters changed: the project has changes, the moves show them.
    void chapterChanged();
    /// Insert Game Break: a new game, from the starting position, at the end
    /// of the chapter (ChapterBook::breakGame).
    /// A new, empty game right after the game `after` of the chapter (the
    /// current one for -1), from the move list's Insert ▸ Game Break.
    void insertGameBreak(int after = -1);
    /// Deletes the game `index` of the chapter with its break (the move
    /// list's menu: Delete Game or Line, Delete Game Break, Delete Following
    /// Game, `title`), asking first when work would be lost.
    void deleteChapterGame(int index, const QString &title);
    /// The number of the first move of the chapter's game `index`: its start
    /// position's, 1 for the standard one.
    int chapterGameStartNumber(int index) const;
    /// Change Move Number…: the game `index` starts from another move number
    /// (its start FEN's), e.g. a line from a position set up.
    void changeMoveNumber(int index);
    void newChapter();
    void switchChapter(int index);
    void manageChapters();
    void editProjectSettings();
    /// The language a project's texts are shown in when it opens: the interface's,
    /// or English when the project's texts are not written in it (LocalizedText).
    static QString contentLanguage();
    void fillChapterMenu();
    /// Whether the board may leave its game for another (not while playing online).
    bool canLeaveGame();
    /// Plays the move the user made on the board, asking for the promotion piece if needed.
    void playBoardMove(int from, int to, const QPoint &globalPosition);
    void updateGameActions();

    // Training: the user plays a colour and the engine answers with the other.
    // There is no session, only the "Training Mode" flag of the Engine menu:
    // "New Training" is a new game with the flag on, a plain new game turns it off.
    /// Asks for the colour and starts a game against the engine. The toolbar
    /// passes false: it does not ask when a choice was remembered for the session.
    void newTraining(bool alwaysAsk);

    // Online play (Game ▸ Play Online…): a game against a person on a
    // platform, through the account's client. `m_onlinePlay` is the flag
    // against cheating: while it is on, the engine, Explain and the opening
    // book are off and cannot be turned on.
    /// Game ▸ Play Online… always asks; the toolbar's button (`alwaysAsk`
    /// false) looks for an opponent with the remembered choices.
    void playOnline(bool alwaysAsk);
    /// Leaves online play, asking first whether to keep playing or resign a
    /// game in progress, then runs `next` (after the game ended, if resigned).
    void leaveOnlineThen(std::function<void()> next);
    /// Makes the client for m_onlineAccount; false (and says so) without a sign-in.
    bool startOnlineClient();
    /// Follows again, at startup, the online game left in progress.
    void resumeOnlineGame();
    void forgetActiveOnlineGame();
    void stopOnline();
    void setOnlinePlay(bool on);
    bool isOpponentTurn() const;
    void onlineGameStarted(const OnlineGame &game);
    void onlineGameUpdated(const OnlineGame &game);
    void onlineGameFinished(const OnlineGame &game);
    void onlineFailed(const QString &message);
    void updateOnlineStatus(const OnlineGame &game);
    /// The header of a training game: the user on their side, the engine on the other.
    GameRecord trainingHeader() const;
    void setTrainingMode(bool enabled);
    /// Whether the engine, not the user, owns the side to move.
    bool isEngineTurn() const;
    /// Hides the engine's line while the user thinks and lets the engine answer.
    void updateTraining();
    void playEngineMove();
    /// The move the opening book plays in the position on the board, drawn
    /// by weight; nothing when the book has no move there.
    std::optional<ChessMove> bookReply() const;
    /// Plays the move the engine chose; `evaluation`, the search it came
    /// from, becomes what the user's next move is judged against.
    void playEngineReply(const ChessMove &move, const EngineEvaluation &evaluation);
    /// The tutor: the user's move was an error, so the engine's answer waits
    /// while the Engine panel offers to take the move back, explain it or go on.
    void holdEngineReply(const ChessMove &reply, const EngineEvaluation &evaluation, TrainingTutor::Alert alert);
    void clearTutor();
    /// Puts the tutor's alert of a project back up, if it is still about the
    /// move on the board; before training is turned on, so the engine waits.
    void restoreTutorHold(const Project &project);
    /// The board's border: what Explain says while it is on (thinking,
    /// explained), else red while the tutor's alert is up, else plain.
    void updateBoardBorder();
    void takeBackTutorMove();
    void ignoreTutorAlert();
    /// Plays the move of the search started by playEngineMove().
    void finishEngineMove();
    /// Stores a finished training game in the open database, once.
    void recordTrainingResult();

    /// "Explain": arrows on the board that justify the evaluation.
    void setExplainEnabled(bool enabled);
    void updateExplainer();

    // Projects (.pch): the File menu saves and restores the whole environment.
    void newProject();
    void openProject();
    bool saveProject();
    /// Writes the project to its file, read-only or not (saveProject asks).
    bool writeProject();
    /// Whether the project may be changed; when it is read-only, says so in
    /// the status bar and returns false.
    bool projectEditable();
    /// Greys what in `menu` would change a read-only project.
    void lockForReadOnly(QMenu &menu) const;
    bool saveProjectAs();
    /// Asks to save a modified project. Returns false if the user cancels.
    bool maybeSaveProject();
    void addRecentProject(const QString &path);
    void rebuildRecentProjectsMenu();
    Project captureProject();
    /// The database a project opens: its path, or the database with its
    /// lineage when the path is missing, gone or another database's.
    QString projectDatabasePath(const Project &project) const;
    /// The names the games list shows for the players a training database
    /// leaves unnamed (StandInNames): the user's, and "Your Trainer".
    /// Hides `hidden` columns in the database we distribute at `path`, and
    /// marks them given, so each is hidden once (DatabaseProperties::shippedColumns);
    /// gives its columns our `names`.
    void giveShippedColumns(const QString &path, const QStringList &hidden,
                            const QHash<QString, QHash<QString, QString>> &names);
    void updateStandInNames();
    StandInNames standInNames() const;
    /// The name of the personal settings, or the default one.
    QString personalName() const;
    /// The name generated for a user who gave none (PersonalSettings::
    /// generatedName), kept on this computer until they give one.
    QString defaultName() const;
    void applyProject(const Project &project, bool openFirstGameIfNone);
    void updateWindowTitle();
    /// Says in the tooltips of their toolbar buttons which book, engine and database are in use.
    void updateResourceButtons();
    void updateProjectModified();

    void newDatabase();
    void openDatabase();
    bool openDatabaseFile(const QString &path);
    /// While playing online, the game stays on the board after another database is opened.
    bool keepOnlineGame();
    void openInitialDatabase(const QString &preferredPath);
    void saveDatabase();
    void saveDatabaseAs();
    void rebuildDatabasesMenu();

    // Opening books (Book menu, shown in the Opening Tree dock).
    /// Opens the book chosen last time; on first launch seeds and chooses the default one.
    void restoreBook();
    /// Opens a Polyglot book and remembers the choice; an empty path chooses no book.
    void chooseBook(const QString &path);
    void openBookFile();
    /// Book ▸ New Book…: a copy of the book we ship, under a name the user picks, chosen at once.
    void newBook();
    void rebuildBookMenu();
    /// The books of the Books folder and No Book, checkable, into `menu`.
    void fillBookChoices(QMenu *menu);
    void updateBookMoves();
    /// Chooses the database whose games name the openings; empty for none.
    /// `explicitly` when the user picked it in Options ▸ Opening Names: until
    /// then the names follow the interface language.
    void chooseOpeningNames(const QString &path, bool explicitly = false);
    /// Seeds the shipped names into Books/Opening Names and chooses the names:
    /// the user's choice, or the ones of the interface language.
    void restoreOpeningNames();
    /// Moves shipped names found among the databases of games (seeded there by
    /// older versions) into Books/Opening Names. Runs before the session is
    /// restored; the paths it moved are remembered for applyProject().
    void migrateOpeningNames();
    void rebuildOpeningNamesMenu();
    /// Gives the seeded Classic Games its fixed universal id.
    void adoptShippedLineages();
    /// Brings the databases we distribute up to this version, by lineage:
    /// adds the games they bring, creates the training sets once.
    void updateDistributedDatabases();
    /// The projects we distribute (`:/projects`), copied once into the
    /// Projects folder: one the user deleted is not brought back.
    void seedDistributedProjects();
    /// Makes sure the database at `path` is typed Opening Book; false if it cannot be opened.
    bool markAsOpeningBook(const QString &path);
    /// Reads the names again when their database changed since they were read.
    void reloadOpeningNamesIfChanged();
    /// Plays a move chosen outside the board, keeping a stored game unchanged.
    /// A move the user made (the board, the Opening Tree): one other than
    /// the next in the middle of a line asks first whether it is a variation
    /// or replaces the rest of the line.
    void playUserMove(const ChessMove &move);
    void playMove(const ChessMove &move);
    void updateDatabaseActions();
    // External sources of games (lichess.org, chess.com, …), synced in the background.
    void connectSource();
    void manageSources();
    /// A ChessBase source whose file is not on this computer: asks what to do.
    void reportUnavailableSource(const GameSource &source);
    void editDatabaseSettings();
    void editGraphicsSettings();
    void editFolderSettings();
    void editPersonalSettings();
    /// Paints every board with the style of the personal settings.
    void applyBoardTheme(const PersonalSettings &settings);
    /// The user's name for new games: the open database's "me", else the
    /// personal settings', else the name generated for this computer.
    QString myName() const;
    /// Puts myName() on the side at the bottom of the board.
    void nameMe(GameRecord &game) const;
    void applyGraphicsSettings(const GraphicsSettings &settings);
    /// The sound of a piece set down, unless Graphics Settings turned it off.
    void playMoveSound();
    // Options ▸ Sync Settings: the Pragma folder kept the same on several computers.
    void openSyncDialog();
    /// Manage Files of the Sync Settings dialog: the files on the server,
    /// deleted from every device.
    void openManageSyncFiles(QWidget *parent);
    /// Files deleted by hand from the Pragma folder (FolderSync::deletedByHand):
    /// deleted from every synced device, restored, or asked again next time.
    void askAboutFilesDeletedByHand(const QStringList &paths);
    // Options ▸ Connect Mobile App: the Android app copies the databases
    // (docs/phone-link.md) and sends back the games played on the phone.
    void createPhoneLink();
    void openConnectMobileDialog();
    /// A phone deleted databases this computer has: asks, one at a time,
    /// whether they go from every synced device too or stay.
    void askAboutPhoneDeletions();
    /// Games were appended to the open database (a source, a phone): show them.
    void showAddedGames();
    // Syncing everything, in order: the sources fill the database, the project
    // file is written, then the folder goes to the server. SyncPipeline owns
    // the order and the reporting, so new steps are one task away.
    /// Builds the pipeline for this moment and runs it; `then` runs when it ends.
    void syncNow(std::function<void()> then = {});
    void updateSyncActions();
    /// Uses the saved sync settings: connects to the server and syncs soon.
    void applySyncSettings();
    void setAnalysisEnabled(bool enabled);
    /// Engine ▸ Manage Engines…: edits the engines of this computer.
    void manageEngines();
    /// Makes @p id the engine for analysis, training and Explain, restarting
    /// the analysis on it if it was running.
    void selectEngine(const QString &id);
    void rebuildEngineChoiceMenu();
    void analyzeCurrentPosition();
    void editGameInfo();
    void updateGameHeader();
    void copyFen();
    void pasteFen();
    /// A new game with the moves on the clipboard (PGN, SAN or UCI).
    void pasteLine();
    /// The moves on the clipboard played from the position on the board, into
    /// the game: at its end, or as a variation.
    void pasteLineFromCurrentPosition();
    /// Edit ▸ Copy: puts `text` on the clipboard and confirms with `message`.
    void copyText(const QString &text, const QString &message);
    /// Puts the panels back where they start: what a new project opens with.
    void applyDefaultLayout();
    void showAbout();
    /// Help ▸ Pragma Chess Guide (F1).
    void showGuide();
    void showWelcome();
    void showLobby();
    /// The lobby's Play: the game `white` plays against `black` in room
    /// `roomId` on the board, after asking about a game on the board that
    /// is not saved, seen from the user's side, in Lobby Mode.
    void playLobbyGame(const QString &roomId, const QString &white, const QString &black);
    /// The lobby on the network, made the first time it is needed; nothing
    /// in a build without the network (no Phone Link libraries).
    LobbyService *lobbyService();
    /// The lobby key changed: the node goes, and comes back with the new one.
    void resetLobbyService();
    /// The ledger of this computer, kept between runs.
    static QString lobbyDirectory();
    /// The name the user sits with in the lobby.
    QString lobbyName() const;
    /// The lobby game on the board as the ledger has it now, if any.
    const LobbyRoom *linkedRoom();
    const LobbyGame *linkedGame();
    /// Lobby Mode's sends: the move after where the game stands, or the
    /// whole plan prepared on the board (`plan`).
    void sendLobby(bool plan);
    /// The lobby changed (the network, or the user): the board follows the
    /// game it holds, and the panel says where it stands.
    void lobbyChanged();
    /// The lobby game as it stands now on the board; the moves from ply
    /// `from` on are new, and the last one slides in.
    void showLobbyGameOnBoard(int from);
    /// The moves of the lobby game the board has not had yet, into its tree.
    void mergeLobbyMoves(const LobbyGame &game);
    /// What the Engine panel says in Lobby Mode, and which sends it allows.
    void updateLobbyPanel();
    /// A send clicked with nothing to send: the panel says what to do, until the board moves.
    void showLobbyHint(const QString &hint);
    /// The status bar's word on the lobby's network.
    void updateLobbyNetwork();
    /// A project's lobby game, when the board holds it: Lobby Mode as it was.
    void restoreLobbyLink(const Project &project);
    /// The board left the lobby game: Lobby Mode goes off.
    void leaveLobbyGame();
    /// Edit ▸ Drawers…, kept in the file of the personal settings.
    void manageDrawers();

    // Session persistence: the current project state (saved or not) and the
    // window geometry are stored per user (QSettings) shortly after they change
    // and restored on the next launch.
    void restoreSession();
    void saveSession();
    void scheduleSaveSession();

    std::unique_ptr<GameDatabase> m_database;
    GameSession *m_session;
    /// The project's chapters; its current game is the one in m_session.
    ChapterBook m_chapters;
    /// The project's name (Project Settings), empty for the file's.
    /// The project's name, in each language its texts are written in.
    LocalizedText m_projectName;
    /// The project's texts are in several languages: the one shown and
    /// written (m_chapters.language) is chosen in Project Settings.
    bool m_multilingual = false;
    /// The project is read-only (Project Settings): nothing in it changes,
    /// and it is not saved (projectEditable, writeProject).
    bool m_projectReadOnly = false;
    GameListModel *m_gameListModel;
    GameFilterProxyModel *m_gameListProxy;

    BoardWidget *m_board;
    /// Hosts the sidebar docks next to the board (no central widget).
    QMainWindow *m_sidebar;
    EvaluationBar *m_evaluationBar;
    GameHeaderWidget *m_gameHeader;
    CapturedPiecesWidget *m_capturedPieces;
    BoardSideColumn *m_boardSideColumn;
    BoardPanel *m_boardPanel = nullptr;
    EnginePanel *m_enginePanel;
    BookPanel *m_bookPanel;
    std::unique_ptr<PolyglotBook> m_book;
    std::unique_ptr<OpeningNames> m_openingNames;
    QString m_openingNamesPath;
    /// Modification time and size of the names database when it was read.
    QDateTime m_openingNamesModified;
    qint64 m_openingNamesSize = -1;
    QMenu *m_openingNamesMenu = nullptr;
    /// Databases moved by migrateOpeningNames(): old path → new path.
    QHash<QString, QString> m_movedDatabases;
    UciEngine *m_engine;
    Explainer *m_explainer;
    MoveTreeView *m_moveView;
    QTableView *m_gameView;
    DatabaseTreeWidget *m_databaseTree;
    /// Databases tree | games list, inside the Games dock.
    QSplitter *m_gamesSplitter;
    /// Source whose games the list shows, or 0; its game ids change during a sync.
    qint64 m_filterSource = 0;
    /// The part of the database the list shows, applied again when roles change.
    GameCategory m_category;
    QLabel *m_gameCountLabel;
    /// The lobby's network in the status bar: relays and peers, or none.
    QLabel *m_lobbyNetworkLabel;
    QLabel *m_syncLabel;
    SourceSync *m_sourceSync;
    /// Positions and lines of the open database, for Position and Variant.
    PositionIndexBuilder *m_positionIndex = nullptr;
    /// Coalesces rebuilds while games keep arriving.
    QTimer *m_positionIndexTimer = nullptr;
    FolderSync *m_folderSync;
    SyncPipeline *m_syncPipeline;
    PhoneLink *m_phoneLink = nullptr;
#ifdef PRAGMA_HAS_PHONE_LINK // The type is only complete in a build with Phone Link.
    std::unique_ptr<DatabaseFolderStore> m_phoneGameStore;
#endif
    QAction *m_connectMobileAction = nullptr;
    /// Set while the window waits for a sync before closing for good.
    bool m_closingAfterSync = false;
    /// Set when the app was stopped from outside: close, ask nothing.
    bool m_forcedQuit = false;
    RemoteStore *m_syncStore = nullptr;
    QTimer *m_syncTimer;
    QLabel *m_folderSyncLabel;
    /// The open database, while the sync replaces its file.
    QString m_reopenAfterSync;
    qint64 m_reopenGameId = -1;
    int m_reopenPly = 0;

    QDockWidget *m_movesDock;
    QDockWidget *m_gamesDock;
    QDockWidget *m_openingTreeDock;
    QToolBar *m_mainToolBar = nullptr;
    QDockWidget *m_engineDock;

    QString m_projectPath;
    QString m_savedProjectYaml;
    QString m_engineName;
    QString m_engineExecutable;
    /// Engines of this computer (user settings) and the one in use (its id;
    /// stored in the project, falling back to the bundled engine).
    EngineCatalog m_engines;
    QString m_engineId;
    QMenu *m_engineChoiceMenu = nullptr;
    /// Latest engine line (SAN) and explanation, for Edit ▸ Copy.
    QString m_engineLine;
    QString m_explanationText;
    /// The plans drawn over the end of the engine's line while the eye is held.
    QList<BoardArrow> m_peekArrows;
    /// The explanation on the board, as the local API reports it.
    MoveExplanation m_explanation;
    /// The development API (PRAGMA_DEV_API=1, set by make start).
    DesktopApi *m_api = nullptr;
    /// The mate the explanation is playing on the board, if any.
    QStringList m_explanationPlayback;
    /// Latest evaluation reported by the engine; its first move is the one it plays.
    EngineEvaluation m_lastEvaluation;
    /// The colour the user plays in training; the engine plays the other one.
    Side m_trainingSide = Side::White;
    /// The colour chosen with "Remember for this session" in New Training:
    /// never saved, a restarted client asks again.
    std::optional<NewTrainingDialog::Choice> m_rememberedTraining;
    /// New Game in online play mode: the choice remembered for the session (online or analysis).
    std::optional<NewGameChoiceDialog::Choice> m_rememberedNewGame;
    /// A training move is being searched, so the analysis must not restart.
    bool m_trainingThinking = false;
    /// The user just played a move on the board in training: their turn, even inside a game (updateTraining).
    bool m_trainingMovePlayed = false;
    /// The engine is searching the position before the move for Explain (analyzeCurrentPosition).
    bool m_explainingBefore = false;
    /// The evaluation of the position the user is to move from in training
    /// (the engine's search for its last move, or the analysis running while
    /// they think) and that position, as FEN: what the tutor judges their
    /// move against.
    EngineEvaluation m_trainingBaseline;
    QString m_trainingBaselineFen;
    /// Sources already reported as not found while the application runs.
    QSet<QString> m_unavailableSourcesReported;
    /// Deletions on a phone the user put off answering in this run (asked
    /// again at the next start), and whether a question is on screen.
    QSet<QString> m_postponedPhoneDeletions;
    bool m_askingPhoneDeletions = false;
    /// Files deleted by hand the user said to ask about later: not again in this run.
    QSet<QString> m_postponedHandDeletions;
    bool m_askingHandDeletions = false;
    /// What Explain wants the border of the board to say; see updateBoardBorder().
    BoardBorder m_explainBorder = BoardBorder::Plain;
    /// The tutor's alert is up: the engine's answer it holds back, the search
    /// it came from and the ply of the move in question.
    std::optional<ChessMove> m_tutorReply;
    EngineEvaluation m_tutorEvaluation;
    int m_tutorPly = -1;
    TrainingTutor::Alert m_tutorAlert = TrainingTutor::Alert::None;
    /// The next board update is the engine's move: show it slowly.
    bool m_animateNextBoard = false;
    bool m_moveSound = true;

    QAction *m_newProjectAction;
    QAction *m_openProjectAction;
    QAction *m_saveProjectAction;
    QAction *m_saveProjectAsAction;
    QMenu *m_switchChapterMenu = nullptr;
    QAction *m_syncAction;
    QAction *m_syncNowAction;
    QMenu *m_recentProjectsMenu;

    QAction *m_newDatabaseAction;
    QAction *m_openDatabaseAction;
    QAction *m_saveDatabaseAction;
    QAction *m_saveDatabaseAsAction;
    QAction *m_showDatabasesFolderAction;
    QAction *m_connectSourceAction;
    QAction *m_manageSourcesAction;
    QAction *m_databaseSettingsAction;
    QAction *m_quitAction;
    QAction *m_copyFenAction;
    QAction *m_pasteFenAction;
    QAction *m_firstMoveAction;
    QAction *m_previousMoveAction;
    QAction *m_nextMoveAction;
    QAction *m_lastMoveAction;
    QAction *m_defaultLayoutAction;
    QAction *m_flipBoardAction;
    QAction *m_coordinatesAction;
    QAction *m_newGameAction;
    QAction *m_newTrainingAction;
    QAction *m_playOnlineAction;
    QAction *m_lobbyAction;
    QAction *m_setUpPositionAction;
    QAction *m_quickOnlineAction;
    std::optional<LichessBoardClient::Seek> m_rememberedOnline; // "Remember for this session", never saved.
    /// Run once the online game resigned by leaveOnlineThen has ended and been saved.
    std::function<void()> m_afterOnlineGame;
    /// Following again a game left in progress, until its start comes back.
    bool m_resumingOnline = false;
    std::unique_ptr<LichessBoardClient> m_online; // Alive while looking for an opponent or playing.
    OnlineAccount m_onlineAccount;
    bool m_onlinePlay = false;
    std::optional<Side> m_onlineSide; // The user's colour, once the game is on.
    /// The same in the toolbar, which may skip the dialog.
    QAction *m_quickTrainingAction;
    /// The toolbar's book, engine and database: an icon each, dropping down
    /// the menu that chooses it; the tooltip names the one in use.
    QToolButton *m_bookButton = nullptr;
    QToolButton *m_engineButton = nullptr;
    QToolButton *m_databaseButton = nullptr;
    QAction *m_trainingModeAction;
    QAction *m_saveGameAction;
    QAction *m_saveGameElsewhereAction;
    QAction *m_explainAction;
    QAction *m_startEngineAction;
    QAction *m_analysisAction;   // Engine ▸ Analysis: the same switch, one name, a check mark.
    QAction *m_onlineModeAction; // Engine ▸ Online Play Mode: checked while playing online.
    QAction *m_lobbyModeAction; // Engine ▸ Lobby Mode: a lobby game on the board, the sends in the Engine panel.
    QAction *m_aboutAction;
    QAction *m_guideAction;
    HelpDialog *m_guideDialog = nullptr;
    QAction *m_welcomeAction;
    QPointer<WelcomeDialog> m_welcomeDialog;
    bool m_welcomeShown = false; ///< At startup, once: the first showEvent.
    /// Game ▸ Enter the Lobby…: one window, kept with its rooms while the application runs.
    LobbyDialog *m_lobbyDialog = nullptr;
    /// The lobby of Game ▸ Enter the Lobby…, on the network (LobbyNode).
    LobbyService *m_lobbyService = nullptr;
    /// Its ledger on the network: the relays and the peers (LobbyNetwork).
    LobbyNetwork *m_lobbyNetwork = nullptr;
    /// The lobby game on the board: its room, its players, the user's side,
    /// the board game's uid and how many of the game's moves the board was
    /// last given (more in the ledger: the opponent moved).
    struct LobbyLink {
        QString room;
        QString white;
        QString black;
        Side side = Side::White;
        QString uid;
        int plies = 0;
    };
    std::optional<LobbyLink> m_lobbyGame;
    /// Moves of the lobby game going into the board's tree: the board waits.
    bool m_mergingLobbyMoves = false;
    /// The board is being set to the lobby game: not the user leaving it.
    bool m_settingLobbyGame = false;
    /// What the Engine panel says in Lobby Mode (also for the development API).
    QString m_lobbyStatus;
    /// What a send with nothing to send asked for, and where the board stood then.
    QString m_lobbyHint;
    QPair<int, int> m_lobbyHintAt{-1, -1};
    /// Whether Send Move and Send Plan are offered (also for the development API).
    std::pair<bool, bool> m_lobbyCanSend{false, false};

    QTimer *m_saveTimer = nullptr;
    bool m_restoringSession = false;
    /// Maximized or full screen state to apply once the window is shown.
    Qt::WindowStates m_restoredWindowState;
    WorkspaceLayout m_layout;      // The last layout applied or captured: hidden panels keep their share here.
    bool m_layoutPending = false;  // applyLayout() has shares to set once the window is laid out.
    void applyLayoutShares();
    /// The user let go of a separator: the panels' shares are measured and kept.
    void separatorReleased();
    /// Source row of the open game in the database, or -1 (e.g. a pasted FEN).
    qint64 m_openGameIndex = -1;

    QMenu *m_viewMenu;
    QMenu *m_databasesMenu;
    QMenu *m_bookMenu;
    QMenu *m_switchBookMenu; // Book ▸ Switch Book: the books, No Book, the folder.
};
