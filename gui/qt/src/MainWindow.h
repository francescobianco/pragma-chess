#pragma once

#include "app/EngineEvaluation.h"
#include "app/PlayerRole.h"
#include "widgets/DatabaseTreeWidget.h"

#include <QDateTime>
#include <QMainWindow>

#include <functional>
#include <memory>

struct Project;

class BoardWidget;
class GameFilterProxyModel;
class QSplitter;
class CapturedPiecesWidget;
class EnginePanel;
class EvaluationBar;
class Explainer;
class GameHeaderWidget;
class UciEngine;
class GameDatabase;
class GameListModel;
class GameSession;
class MoveListModel;
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
class QTimer;
class SourceSync;
class FolderSync;
class RemoteStore;
class SyncPipeline;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Opens a .pch project file, e.g. one passed on the command line.
    bool openProjectFile(const QString &path);

    /// Closes without asking anything: the app was stopped from outside
    /// (SIGTERM from `make start`, a session logout), not by the user.
    void quitWithoutAsking();

protected:
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
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

    /// Layout of the main window and of the sidebar, as one blob.
    QByteArray saveLayout() const;
    void restoreLayout(const QByteArray &layout);

    void setDatabase(std::unique_ptr<GameDatabase> database);
    void openGame(const QModelIndex &proxyIndex);
    void syncBoard();
    void updateNavigationActions();
    void updateGameCount();
    /// Shows the games of a part of the database chosen in the tree.
    void showCategory(const GameCategory &category);
    /// "Who Is This?" on a player of the games list.
    void showGameListMenu(const QPoint &position);
    void setPlayerRole(const QString &player, PlayerRole role);
    /// Turns the board so the user plays from the bottom, when the database knows who they are.
    void orientBoardForMe(const GameRecord &game);

    // Entering games move by move.
    void newGame();
    /// Opens `game` as the game being entered, unlinked from the database.
    void startGame(const GameRecord &game);
    void saveGameToDatabase();
    /// Plays the move the user made on the board, asking for the promotion piece if needed.
    void playBoardMove(int from, int to, const QPoint &globalPosition);
    void updateGameActions();

    // Training: the user plays a colour and the engine answers with the other.
    // There is no session, only the "Training Mode" flag of the Engine menu:
    // "New Training" is a new game with the flag on, a plain new game turns it off.
    /// Asks for the colour and starts a game against the engine.
    void newTraining();
    void setTrainingMode(bool enabled);
    /// Whether the engine, not the user, owns the side to move.
    bool isEngineTurn() const;
    /// Hides the engine's line while the user thinks and lets the engine answer.
    void updateTraining();
    void playEngineMove();
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
    bool saveProjectAs();
    /// Asks to save a modified project. Returns false if the user cancels.
    bool maybeSaveProject();
    void addRecentProject(const QString &path);
    void rebuildRecentProjectsMenu();
    Project captureProject() const;
    void applyProject(const Project &project, bool openFirstGameIfNone);
    void updateWindowTitle();
    void updateProjectModified();

    void newDatabase();
    void openDatabase();
    bool openDatabaseFile(const QString &path);
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
    void rebuildBookMenu();
    void updateBookMoves();
    /// Chooses the database whose games name the openings; empty for none.
    void chooseOpeningNames(const QString &path);
    /// On first launch, seeds and chooses the Opening Names database.
    void restoreOpeningNames();
    /// Reads the names again when their database changed since they were read.
    void reloadOpeningNamesIfChanged();
    /// Plays a move chosen outside the board, keeping a stored game unchanged.
    void playMove(const ChessMove &move);
    void updateDatabaseActions();
    // External sources of games (lichess.org, chess.com, …), synced in the background.
    void connectSource();
    void manageSources();
    // File ▸ Sync: the Pragma folder kept the same on several computers.
    void openSyncDialog();
    // Syncing everything, in order: the sources fill the database, the project
    // file is written, then the folder goes to the server. SyncPipeline owns
    // the order and the reporting, so new steps are one task away.
    /// Builds the pipeline for this moment and runs it; `then` runs when it ends.
    void syncNow(std::function<void()> then = {});
    void updateSyncActions();
    /// Uses the saved sync settings: connects to the server and syncs soon.
    void applySyncSettings();
    void setAnalysisEnabled(bool enabled);
    void analyzeCurrentPosition();
    void editGameInfo();
    void updateGameHeader();
    void copyFen();
    void pasteFen();
    /// Edit ▸ Copy: puts `text` on the clipboard and confirms with `message`.
    void copyText(const QString &text, const QString &message);
    /// Puts the panels back where they start: what a new project opens with.
    void applyDefaultLayout();
    void showAbout();

    // Session persistence: the current project state (saved or not) and the
    // window geometry are stored per user (QSettings) shortly after they change
    // and restored on the next launch.
    void restoreSession();
    void saveSession();
    void scheduleSaveSession();

    std::unique_ptr<GameDatabase> m_database;
    GameSession *m_session;
    GameListModel *m_gameListModel;
    GameFilterProxyModel *m_gameListProxy;
    MoveListModel *m_moveListModel;

    BoardWidget *m_board;
    /// Hosts the sidebar docks next to the board (no central widget).
    QMainWindow *m_sidebar;
    EvaluationBar *m_evaluationBar;
    GameHeaderWidget *m_gameHeader;
    CapturedPiecesWidget *m_capturedPieces;
    EnginePanel *m_enginePanel;
    BookPanel *m_bookPanel;
    std::unique_ptr<PolyglotBook> m_book;
    std::unique_ptr<OpeningNames> m_openingNames;
    QString m_openingNamesPath;
    /// Modification time and size of the names database when it was read.
    QDateTime m_openingNamesModified;
    qint64 m_openingNamesSize = -1;
    UciEngine *m_engine;
    Explainer *m_explainer;
    QTableView *m_moveView;
    QTableView *m_gameView;
    DatabaseTreeWidget *m_databaseTree;
    /// Databases tree | games list, inside the Games dock.
    QSplitter *m_gamesSplitter;
    /// Source whose games the list shows, or 0; its game ids change during a sync.
    qint64 m_filterSource = 0;
    /// The part of the database the list shows, applied again when roles change.
    GameCategory m_category;
    QLabel *m_gameCountLabel;
    QLabel *m_syncLabel;
    SourceSync *m_sourceSync;
    FolderSync *m_folderSync;
    SyncPipeline *m_syncPipeline;
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
    /// Latest engine line (SAN) and explanation, for Edit ▸ Copy.
    QString m_engineLine;
    QString m_explanationText;
    /// Latest evaluation reported by the engine; its first move is the one it plays.
    EngineEvaluation m_lastEvaluation;
    /// The colour the user plays in training; the engine plays the other one.
    Side m_trainingSide = Side::White;
    /// A training move is being searched, so the analysis must not restart.
    bool m_trainingThinking = false;
    /// The next board update is the engine's move: show it slowly.
    bool m_animateNextBoard = false;

    QAction *m_newProjectAction;
    QAction *m_openProjectAction;
    QAction *m_saveProjectAction;
    QAction *m_saveProjectAsAction;
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
    QAction *m_trainingModeAction;
    QAction *m_saveGameAction;
    QAction *m_explainAction;
    QAction *m_startEngineAction;
    QAction *m_aboutAction;
    QAction *m_aboutQtAction;

    QTimer *m_saveTimer = nullptr;
    bool m_restoringSession = false;
    /// Maximized or full screen state to apply once the window is shown.
    Qt::WindowStates m_restoredWindowState;
    /// Source row of the open game in the database, or -1 (e.g. a pasted FEN).
    qint64 m_openGameIndex = -1;

    QMenu *m_viewMenu;
    QMenu *m_databasesMenu;
    QMenu *m_bookMenu;
};
