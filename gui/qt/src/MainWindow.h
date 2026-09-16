#pragma once

#include <QMainWindow>

#include <memory>

struct Project;

class BoardWidget;
class DatabaseTreeWidget;
struct GameCategory;
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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// Opens a .pch project file, e.g. one passed on the command line.
    bool openProjectFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class Workspace { Analysis, Database, OpeningPreparation };

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

    // Entering games move by move.
    void newGame();
    void saveGameToDatabase();
    /// Plays the move the user made on the board, asking for the promotion piece if needed.
    void playBoardMove(int from, int to, const QPoint &globalPosition);
    void updateGameActions();

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
    void updateDatabaseActions();
    // External sources of games (lichess.org, chess.com, …), synced in the background.
    void connectSource();
    void manageSources();
    // File ▸ Sync: the Pragma folder kept the same on several computers.
    void openSyncDialog();
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
    void applyWorkspace(Workspace workspace);
    void saveWorkspaceAs();
    void rebuildWorkspaceMenu();
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
    UciEngine *m_engine;
    Explainer *m_explainer;
    QTableView *m_moveView;
    QTableView *m_gameView;
    DatabaseTreeWidget *m_databaseTree;
    /// Databases tree | games list, inside the Games dock.
    QSplitter *m_gamesSplitter;
    /// Source whose games the list shows, or 0; its game ids change during a sync.
    qint64 m_filterSource = 0;
    QLabel *m_gameCountLabel;
    QLabel *m_syncLabel;
    SourceSync *m_sourceSync;
    FolderSync *m_folderSync;
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
    QDockWidget *m_engineDock;

    QString m_projectPath;
    QString m_savedProjectYaml;
    QString m_engineName;
    QString m_engineExecutable;
    /// Latest engine line (SAN) and explanation, for Edit ▸ Copy.
    QString m_engineLine;
    QString m_explanationText;

    QAction *m_newProjectAction;
    QAction *m_openProjectAction;
    QAction *m_saveProjectAction;
    QAction *m_saveProjectAsAction;
    QAction *m_syncAction;
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
    QAction *m_flipBoardAction;
    QAction *m_coordinatesAction;
    QAction *m_newGameAction;
    QAction *m_saveGameAction;
    QAction *m_explainAction;
    QAction *m_startEngineAction;
    QAction *m_aboutAction;
    QAction *m_aboutQtAction;

    QTimer *m_saveTimer = nullptr;
    bool m_restoringSession = false;
    /// Source row of the open game in the database, or -1 (e.g. a pasted FEN).
    qint64 m_openGameIndex = -1;

    QMenu *m_viewMenu;
    QMenu *m_workspaceMenu;
    QMenu *m_databasesMenu;
};
