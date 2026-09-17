#include "MainWindow.h"

#include "app/ClassicGames.h"
#include "app/OpeningNames.h"
#include "app/PolyglotBook.h"
#include "app/DatabaseOutline.h"
#include "dialogs/ConnectSourceWizard.h"
#include "dialogs/GameInfoDialog.h"
#include "dialogs/ManageSourcesDialog.h"
#include "dialogs/NewTrainingDialog.h"
#include "dialogs/SyncDialog.h"
#include "app/Explainer.h"
#include "app/GameSession.h"
#include "app/Pgn.h"
#include "app/Project.h"
#include "app/SqliteGameDatabase.h"
#include "app/UiLanguage.h"
#include "app/UciEngine.h"
#include "app/UserFolders.h"
#include "app/sources/SourceCatalog.h"
#include "app/sources/SourceSync.h"
#include "app/sync/FolderSync.h"
#include "app/sync/RemoteStore.h"
#include "app/sync/SyncPipeline.h"
#include "app/sync/SyncTasks.h"
#include "models/GameFilterProxyModel.h"
#include "models/GameListModel.h"
#include "models/MoveListModel.h"
#include "platform/SymbolicIcons.h"
#include "widgets/BoardPanel.h"
#include "widgets/BookPanel.h"
#include "widgets/PaddedHeaderView.h"
#include "widgets/PaddedItemDelegate.h"
#include "widgets/BoardWidget.h"
#include "widgets/CapturedPiecesWidget.h"
#include "widgets/CentralArea.h"
#include "widgets/DatabaseTreeWidget.h"
#include "widgets/EnginePanel.h"
#include "widgets/EvaluationBar.h"
#include "widgets/GameHeaderWidget.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDataStream>
#include <QActionGroup>
#include <QDesktopServices>
#include <QDir>
#include <QCloseEvent>
#include <QDate>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QDockWidget>
#include <QFileInfo>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

/// Delay before maximizing a window restored maximized, once it is mapped.
constexpr int kRestoreWindowStateDelayMs = 250;

/// Depth of the search that picks the engine's move in training: deep
/// enough to play well, shallow enough to answer at once.
constexpr int kTrainingDepth = 12;

/// How long the engine's move takes to cross the board, so that the user
/// cannot miss what just happened.
constexpr int kEngineMoveMs = 1500;

/// How long closing waits for a sync before giving up on the server.
constexpr int kCloseSyncTimeoutMs = 30000;

QIcon themeIcon(const char *name, QStyle::StandardPixmap)
{
    // Flat monochrome icons that follow the theme's text color.
    return SymbolicIcons::icon(QString::fromLatin1(name));
}

/// A position to show, with its last move and the king in check or mated marked.
BoardFrame frameFor(const ChessPosition &position, int lastMoveFrom, int lastMoveTo)
{
    BoardFrame frame{position.boardState(), lastMoveFrom, lastMoveTo};
    if (position.inCheck()) {
        frame.markedKing = position.kingSquare(position.sideToMove());
        frame.kingMark = position.legalMoves().isEmpty() ? KingMark::Mate : KingMark::Check;
    }
    return frame;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_session(new GameSession(this))
    , m_gameListModel(new GameListModel(this))
    , m_gameListProxy(new GameFilterProxyModel(this))
    , m_moveListModel(new MoveListModel(m_session, this))
    , m_board(new BoardWidget)
    , m_sidebar(new QMainWindow)
    , m_evaluationBar(new EvaluationBar)
    , m_gameHeader(new GameHeaderWidget)
    , m_capturedPieces(new CapturedPiecesWidget)
    , m_engine(new UciEngine(this))
    , m_explainer(new Explainer(this))
    , m_sourceSync(new SourceSync(this))
    , m_folderSync(new FolderSync(UserFolders::pragmaDir(), SyncSettings::statePath(), SyncSettings::deviceName(), this))
    , m_syncPipeline(new SyncPipeline(this))
    , m_syncTimer(new QTimer(this))
{
    setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);
    m_sidebar->setWindowFlags(Qt::Widget);
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    m_sidebar->setDockOptions(AnimatedDocks | AllowTabbedDocks | AllowNestedDocks);

    m_gameListProxy->setSourceModel(m_gameListModel);
    m_gameListProxy->setSortRole(Qt::DisplayRole);

    createActions();
    auto *boardPanel = new BoardPanel(m_board, m_evaluationBar, m_gameHeader, m_capturedPieces,
                                      {m_firstMoveAction, m_previousMoveAction, m_explainAction,
                                       m_nextMoveAction, m_lastMoveAction, m_flipBoardAction});
    setCentralWidget(new CentralArea(boardPanel, m_sidebar));
    createDocks();
    createToolBar();
    createMenus();
    createStatusBar();
    updateSeparatorStyle();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateWindowTitle);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateGameHeader);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateGameActions);
    connect(m_session, &GameSession::headerChanged, this, &MainWindow::updateWindowTitle);
    connect(m_session, &GameSession::headerChanged, this, &MainWindow::updateGameHeader);
    connect(m_gameHeader, &GameHeaderWidget::activated, this, &MainWindow::editGameInfo);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::syncBoard);
    // Before the analysis: in training the engine's own search replaces it.
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateTraining);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::analyzeCurrentPosition);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateBookMoves);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateBookMoves);
    connect(m_engine, &UciEngine::searchFinished, this, &MainWindow::finishEngineMove);
    connect(m_engine, &UciEngine::evaluationChanged, this, [this](const EngineEvaluation &evaluation) {
        m_lastEvaluation = evaluation;
        m_evaluationBar->setEvaluation(evaluation);
        m_explainer->setLiveEvaluation(evaluation);
        m_engineLine = m_session->position().lineText(evaluation.pv);
        m_enginePanel->setEvaluation(evaluation, m_session->position().lineText(evaluation.pv, 12, SanStyle::Figurines));
    });
    connect(m_explainer, &Explainer::explanationChanged, this, [this](const MoveExplanation &explanation) {
        m_board->setExplanation(explanation.arrows, explanation.lostPieces);
        // A forced mate is shown by playing it; the board returns when Explain is turned off.
        QList<BoardFrame> frames;
        ChessPosition position = m_session->position();
        for (const QString &uci : explanation.playback) {
            const std::optional<ChessMove> move = position.moveFromUci(uci);
            if (!move)
                break;
            position.play(*move);
            frames << frameFor(position, move->from, move->to);
        }
        if (frames.isEmpty())
            m_board->stopSequence();
        else
            m_board->playSequence(frames);
        m_enginePanel->setExplanation(explanation.summary);
        m_explanationText = explanation.summary;
    });
    connect(m_engine, &UciEngine::nameChanged, m_enginePanel, &EnginePanel::setEngineName);
    connect(m_engine, &UciEngine::failed, this, [this](const QString &message) {
        m_startEngineAction->setChecked(false);
        m_enginePanel->setStatus(tr("The engine stopped: %1").arg(message));
    });
    connect(m_board, &BoardWidget::navigateRequested, this,
            [this](int steps) { m_session->goToPly(m_session->ply() + steps); });
    connect(m_board, &BoardWidget::moveRequested, this, &MainWindow::playBoardMove);
    connect(m_sourceSync, &SourceSync::gamesImported, this, [this] {
        m_gameListModel->refreshAppended();
        if (m_filterSource != 0)
            showCategory({GameCategory::Kind::Source, QString(), m_filterSource}); // Its new games too.
        updateGameCount();
        m_databaseTree->scheduleRefresh();
    });
    connect(m_sourceSync, &SourceSync::sourcesChanged, m_databaseTree, &DatabaseTreeWidget::scheduleRefresh);

    // The whole sync, in order, from the toolbar button or before closing.
    connect(m_syncPipeline, &SyncPipeline::started, this, [this] {
        updateSyncActions();
        m_folderSyncLabel->setToolTip(QString());
        m_folderSyncLabel->show();
    });
    connect(m_syncPipeline, &SyncPipeline::taskStarted, this,
            [this](const QString &name) { m_folderSyncLabel->setText(tr("Syncing %1…").arg(name)); });
    connect(m_syncPipeline, &SyncPipeline::progress, m_folderSyncLabel, &QLabel::setText);
    connect(m_syncPipeline, &SyncPipeline::finished, this, [this](const QStringList &errors) {
        updateSyncActions();
        if (errors.isEmpty()) {
            m_folderSyncLabel->hide();
            statusBar()->showMessage(tr("Everything is in sync"), 5000);
            return;
        }
        m_folderSyncLabel->setText(tr("Sync failed"));
        m_folderSyncLabel->setToolTip(errors.join(QLatin1Char('\n')));
    });

    // Folder sync with a server: every few minutes, and shortly after starting.
    m_syncTimer->setInterval(5 * 60 * 1000);
    connect(m_syncTimer, &QTimer::timeout, m_folderSync, &FolderSync::sync);
    connect(m_folderSync, &FolderSync::started, this, [this] {
        m_folderSyncLabel->setText(tr("Syncing…"));
        m_folderSyncLabel->setToolTip(QString());
        m_folderSyncLabel->show();
    });
    connect(m_folderSync, &FolderSync::progress, m_folderSyncLabel, &QLabel::setText);
    connect(m_folderSync, &FolderSync::finished, this, [this](const QString &error, int changes) {
        if (!error.isEmpty()) {
            m_folderSyncLabel->setText(tr("Sync failed"));
            m_folderSyncLabel->setToolTip(error);
            return;
        }
        m_folderSyncLabel->hide();
        if (changes > 0)
            statusBar()->showMessage(tr("Synced %n file(s) with the server", nullptr, changes), 5000);
    });
    // A database the sync replaces is closed first and opened again after.
    connect(m_folderSync, &FolderSync::localFileAboutToChange, this, [this](const QString &path) {
        if (!m_database || QFileInfo(m_database->location()) != QFileInfo(path))
            return;
        m_reopenAfterSync = m_database->location();
        m_reopenGameId = m_openGameIndex >= 0 ? m_database->header(m_openGameIndex).id : -1;
        m_reopenPly = m_session->ply();
        setDatabase(nullptr);
    });
    connect(m_folderSync, &FolderSync::localFileChanged, this, [this](const QString &path) {
        if (m_reopenAfterSync.isEmpty() || QFileInfo(m_reopenAfterSync) != QFileInfo(path))
            return;
        const QString reopen = m_reopenAfterSync;
        m_reopenAfterSync.clear();
        if (!QFileInfo::exists(reopen)) {
            openInitialDatabase(QString());
            return;
        }
        Project project = captureProject();
        project.databasePath = reopen;
        project.gameId = m_reopenGameId;
        project.ply = m_reopenPly;
        applyProject(project, false);
    });
    connect(m_sourceSync, &SourceSync::activityChanged, this, [this](const QString &text) {
        m_syncLabel->setText(text);
        m_syncLabel->setVisible(!text.isEmpty());
    });

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(400);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveSession);
    // quit() (e.g. on SIGTERM) does not deliver closeEvent, so save here as well.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveSession);

    applyDefaultLayout();
    restoreBook();
    restoreSession();
    restoreOpeningNames(); // After the session, so the first launch still seeds Classic Games first.
    applySyncSettings();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_flipBoardAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_coordinatesAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_startEngineAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    const QHeaderView *gameHeader = m_gameView->horizontalHeader();
    connect(gameHeader, &QHeaderView::sectionMoved, this, &MainWindow::scheduleSaveSession);
    connect(gameHeader, &QHeaderView::sectionResized, this, &MainWindow::scheduleSaveSession);
    connect(gameHeader, &QHeaderView::sortIndicatorChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_gamesSplitter, &QSplitter::splitterMoved, this, &MainWindow::scheduleSaveSession);
    for (QDockWidget *dock : findChildren<QDockWidget *>()) {
        connect(dock, &QDockWidget::visibilityChanged, this, &MainWindow::scheduleSaveSession);
        connect(dock, &QDockWidget::topLevelChanged, this, &MainWindow::scheduleSaveSession);
        connect(dock, &QDockWidget::dockLocationChanged, this, &MainWindow::scheduleSaveSession);
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    m_newProjectAction = new QAction(themeIcon("document-new", QStyle::SP_FileIcon), tr("&New Project"), this);
    m_newProjectAction->setShortcut(QKeySequence::New);
    connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::newProject);

    m_openProjectAction = new QAction(themeIcon("document-open", QStyle::SP_DialogOpenButton),
                                      tr("&Open Project…"), this);
    m_openProjectAction->setShortcut(QKeySequence::Open);
    connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::openProject);

    m_saveProjectAction = new QAction(themeIcon("document-save", QStyle::SP_DialogSaveButton),
                                      tr("&Save Project"), this);
    m_saveProjectAction->setShortcut(QKeySequence::Save);
    connect(m_saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);

    m_saveProjectAsAction = new QAction(themeIcon("document-save-as", QStyle::SP_DialogSaveButton),
                                        tr("Save Project &As…"), this);
    m_saveProjectAsAction->setShortcut(QKeySequence::SaveAs);
    connect(m_saveProjectAsAction, &QAction::triggered, this, &MainWindow::saveProjectAs);

    m_syncNowAction = new QAction(themeIcon("view-refresh", QStyle::SP_BrowserReload), tr("S&ync Now"), this);
    m_syncNowAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    m_syncNowAction->setToolTip(tr("Sync the connected sources, save the project and send the folder to the server"));
    connect(m_syncNowAction, &QAction::triggered, this, [this] { syncNow(); });

    m_syncAction = new QAction(tr("S&ync…"), this);
    m_syncAction->setToolTip(tr("Keep databases and projects the same on several computers through a server"));
    connect(m_syncAction, &QAction::triggered, this, &MainWindow::openSyncDialog);

    m_newDatabaseAction = new QAction(themeIcon("document-new", QStyle::SP_FileIcon),
                                      tr("&New Database…"), this);
    connect(m_newDatabaseAction, &QAction::triggered, this, &MainWindow::newDatabase);

    m_openDatabaseAction = new QAction(themeIcon("document-open", QStyle::SP_DialogOpenButton),
                                       tr("&Open Database…"), this);
    connect(m_openDatabaseAction, &QAction::triggered, this, &MainWindow::openDatabase);

    m_saveDatabaseAction = new QAction(themeIcon("document-save", QStyle::SP_DialogSaveButton),
                                       tr("&Save Database"), this);
    connect(m_saveDatabaseAction, &QAction::triggered, this, &MainWindow::saveDatabase);

    m_saveDatabaseAsAction = new QAction(themeIcon("document-save-as", QStyle::SP_DialogSaveButton),
                                         tr("Save Database &As…"), this);
    connect(m_saveDatabaseAsAction, &QAction::triggered, this, &MainWindow::saveDatabaseAs);

    m_showDatabasesFolderAction = new QAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon),
                                              tr("Show Databases &Folder"), this);
    connect(m_showDatabasesFolderAction, &QAction::triggered, this, [] {
        UserFolders::ensureDatabasesDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::databasesDir()));
    });

    m_connectSourceAction = new QAction(tr("Connect &Source…"), this);
    m_connectSourceAction->setToolTip(tr("Import and keep in sync the games of a lichess.org or chess.com account"));
    connect(m_connectSourceAction, &QAction::triggered, this, &MainWindow::connectSource);

    m_manageSourcesAction = new QAction(tr("&Manage Sources…"), this);
    connect(m_manageSourcesAction, &QAction::triggered, this, &MainWindow::manageSources);

    m_quitAction = new QAction(themeIcon("application-exit", QStyle::SP_DialogCloseButton),
                               tr("&Quit"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);
    m_quitAction->setMenuRole(QAction::QuitRole);
    connect(m_quitAction, &QAction::triggered, this, &QWidget::close);

    m_copyFenAction = new QAction(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("Position (&FEN)"), this);
    m_copyFenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    connect(m_copyFenAction, &QAction::triggered, this, &MainWindow::copyFen);

    m_pasteFenAction = new QAction(themeIcon("edit-paste", QStyle::SP_FileIcon), tr("&Paste FEN"), this);
    m_pasteFenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
    connect(m_pasteFenAction, &QAction::triggered, this, &MainWindow::pasteFen);

    m_firstMoveAction = new QAction(themeIcon("go-first", QStyle::SP_MediaSkipBackward),
                                    tr("&First Move"), this);
    m_firstMoveAction->setShortcut(Qt::Key_Home);
    connect(m_firstMoveAction, &QAction::triggered, m_session, &GameSession::goToStart);

    m_previousMoveAction = new QAction(themeIcon("go-previous", QStyle::SP_MediaSeekBackward),
                                       tr("&Previous Move"), this);
    m_previousMoveAction->setShortcut(Qt::Key_Left);
    connect(m_previousMoveAction, &QAction::triggered, m_session, &GameSession::goBack);

    m_nextMoveAction = new QAction(themeIcon("go-next", QStyle::SP_MediaSeekForward),
                                   tr("&Next Move"), this);
    m_nextMoveAction->setShortcut(Qt::Key_Right);
    connect(m_nextMoveAction, &QAction::triggered, m_session, &GameSession::goForward);

    m_lastMoveAction = new QAction(themeIcon("go-last", QStyle::SP_MediaSkipForward),
                                   tr("&Last Move"), this);
    m_lastMoveAction->setShortcut(Qt::Key_End);
    connect(m_lastMoveAction, &QAction::triggered, m_session, &GameSession::goToEnd);

    m_flipBoardAction = new QAction(themeIcon("object-flip-vertical", QStyle::SP_BrowserReload),
                                    tr("&Flip Board"), this);
    m_flipBoardAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_flipBoardAction->setCheckable(true);
    connect(m_flipBoardAction, &QAction::toggled, m_board, &BoardWidget::setFlipped);
    connect(m_flipBoardAction, &QAction::toggled, m_evaluationBar, &EvaluationBar::setFlipped);
    connect(m_flipBoardAction, &QAction::toggled, m_capturedPieces, &CapturedPiecesWidget::setFlipped);

    m_defaultLayoutAction = new QAction(tr("&Reset Panel Layout"), this);
    m_defaultLayoutAction->setToolTip(tr("Put the panels back where a new project starts"));
    connect(m_defaultLayoutAction, &QAction::triggered, this, &MainWindow::applyDefaultLayout);

    m_coordinatesAction = new QAction(tr("Show &Coordinates"), this);
    m_coordinatesAction->setCheckable(true);
    m_coordinatesAction->setChecked(true);
    connect(m_coordinatesAction, &QAction::toggled, m_board, &BoardWidget::setShowCoordinates);

    m_newGameAction = new QAction(themeIcon("pragma-new-game", QStyle::SP_FileIcon), tr("&New Game"), this);
    m_newGameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    m_newGameAction->setToolTip(tr("Start a game to enter move by move"));
    connect(m_newGameAction, &QAction::triggered, this, &MainWindow::newGame);

    m_newTrainingAction = new QAction(themeIcon("pragma-training", QStyle::SP_MediaPlay), tr("New &Training…"), this);
    m_newTrainingAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    m_newTrainingAction->setToolTip(tr("Start a game against the engine, choosing your colour"));
    connect(m_newTrainingAction, &QAction::triggered, this, &MainWindow::newTraining);

    m_trainingModeAction = new QAction(tr("&Training Mode"), this);
    m_trainingModeAction->setCheckable(true);
    m_trainingModeAction->setToolTip(tr("The engine answers as the other colour and hides its line while you think"));
    connect(m_trainingModeAction, &QAction::toggled, this, &MainWindow::setTrainingMode);

    m_saveGameAction = new QAction(themeIcon("document-save", QStyle::SP_DialogSaveButton),
                                   tr("Save Game to &Database"), this);
    connect(m_saveGameAction, &QAction::triggered, this, &MainWindow::saveGameToDatabase);

    m_explainAction = new QAction(themeIcon("pragma-explain", QStyle::SP_MessageBoxQuestion), tr("E&xplain"), this);
    m_explainAction->setShortcut(Qt::Key_E);
    m_explainAction->setCheckable(true);
    m_explainAction->setToolTip(tr("Explain the evaluation: show on the board what the last move allows or wins (E)"));
    connect(m_explainAction, &QAction::toggled, this, &MainWindow::setExplainEnabled);

    m_startEngineAction = new QAction(themeIcon("media-playback-start", QStyle::SP_MediaPlay),
                                      tr("&Analyze"), this);
    m_startEngineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    m_startEngineAction->setCheckable(true);
    m_startEngineAction->setToolTip(tr("Analyze the position with the engine"));
    connect(m_startEngineAction, &QAction::toggled, this, &MainWindow::setAnalysisEnabled);

    m_aboutAction = new QAction(themeIcon("help-about", QStyle::SP_MessageBoxInformation),
                                tr("&About Pragma Chess"), this);
    m_aboutAction->setMenuRole(QAction::AboutRole);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    m_aboutQtAction = new QAction(tr("About &Qt"), this);
    m_aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(m_aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(m_newProjectAction);
    file->addAction(m_openProjectAction);
    m_recentProjectsMenu = file->addMenu(tr("Open &Recent"));
    connect(m_recentProjectsMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildRecentProjectsMenu);
    file->addSeparator();
    file->addAction(m_saveProjectAction);
    file->addAction(m_saveProjectAsAction);
    file->addSeparator();
    file->addAction(m_syncAction);
    file->addSeparator();
    file->addAction(m_quitAction);

    QMenu *edit = menuBar()->addMenu(tr("&Edit"));
    QMenu *copy = edit->addMenu(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("&Copy"));
    QAction *copyMovesToHere = copy->addAction(tr("&Moves up to Current Position"), this, [this] {
        copyText(Pgn::moveText(m_session->game(), m_session->ply()), tr("Moves up to the current position copied"));
    });
    copyMovesToHere->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
    QAction *copyAllMoves = copy->addAction(tr("&All Moves"), this, [this] {
        copyText(Pgn::moveText(m_session->game()), tr("Moves copied"));
    });
    QAction *copyCurrentMove = copy->addAction(tr("Current &Move"), this, [this] {
        const int ply = m_session->ply();
        if (ply > 0)
            copyText(m_session->positionAt(ply - 1).lineText({m_session->game().moves.at(ply - 1).uci}),
                     tr("Move copied"));
    });
    copy->addSeparator();
    QAction *copyPgn = copy->addAction(tr("Game as &PGN"), this, [this] {
        copyText(Pgn::game(m_session->game()), tr("Game copied as PGN"));
    });
    QAction *copyPgnToHere = copy->addAction(tr("Game up to Current Position as P&GN"), this, [this] {
        copyText(Pgn::game(m_session->game(), m_session->ply()), tr("Game up to the current position copied as PGN"));
    });
    copy->addSeparator();
    copy->addAction(m_copyFenAction);
    copy->addSeparator();
    QAction *copyEngineLine = copy->addAction(tr("&Engine Line"), this, [this] {
        copyText(m_engineLine, tr("Engine line copied"));
    });
    QAction *copyExplanation = copy->addAction(tr("E&xplanation"), this, [this] {
        copyText(m_explanationText, tr("Explanation copied"));
    });
    const auto updateCopyActions = [=, this] {
        const bool hasMoves = m_session->plyCount() > 0;
        copyMovesToHere->setEnabled(m_session->ply() > 0);
        copyCurrentMove->setEnabled(m_session->ply() > 0);
        copyAllMoves->setEnabled(hasMoves);
        copyPgnToHere->setEnabled(m_session->ply() > 0);
        copyPgn->setEnabled(true);
        copyEngineLine->setEnabled(m_startEngineAction->isChecked() && !m_engineLine.isEmpty());
        copyExplanation->setEnabled(m_explainAction->isChecked() && !m_explanationText.isEmpty());
    };
    connect(copy, &QMenu::aboutToShow, this, updateCopyActions);
    connect(m_session, &GameSession::plyChanged, this, updateCopyActions); // Keeps shortcuts in step.
    connect(m_session, &GameSession::gameChanged, this, updateCopyActions);
    updateCopyActions();
    edit->addSeparator();
    edit->addAction(m_pasteFenAction);

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->addAction(m_flipBoardAction);
    m_viewMenu->addAction(m_coordinatesAction);
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_mainToolBar->toggleViewAction());
    for (QDockWidget *dock : {m_gamesDock, m_movesDock, m_openingTreeDock, m_engineDock})
        m_viewMenu->addAction(dock->toggleViewAction());
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_defaultLayoutAction);

    QMenu *game = menuBar()->addMenu(tr("&Game"));
    game->addAction(m_newGameAction);
    game->addAction(m_newTrainingAction);
    game->addAction(m_saveGameAction);
    game->addSeparator();
    game->addAction(m_firstMoveAction);
    game->addAction(m_previousMoveAction);
    game->addAction(m_nextMoveAction);
    game->addAction(m_lastMoveAction);
    game->addSeparator();
    game->addAction(m_explainAction);

    QMenu *database = menuBar()->addMenu(tr("&Database"));
    database->addAction(m_newDatabaseAction);
    database->addAction(m_openDatabaseAction);
    m_databasesMenu = database->addMenu(themeIcon("folder", QStyle::SP_DirIcon), tr("&Databases"));
    connect(m_databasesMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildDatabasesMenu);
    database->addSeparator();
    database->addAction(m_connectSourceAction);
    database->addAction(m_manageSourcesAction);
    database->addSeparator();
    database->addAction(m_saveDatabaseAction);
    database->addAction(m_saveDatabaseAsAction);

    QMenu *engine = menuBar()->addMenu(tr("E&ngine"));
    engine->addAction(m_startEngineAction);
    engine->addAction(m_explainAction);
    engine->addSeparator();
    engine->addAction(m_trainingModeAction);

    m_bookMenu = menuBar()->addMenu(tr("&Book"));
    connect(m_bookMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildBookMenu);
    rebuildBookMenu(); // Keeps the menu non-empty, so it shows on every platform.

    menuBar()->addMenu(tr("&Tools"))->setEnabled(false);

    QMenu *options = menuBar()->addMenu(tr("&Options"));
    QMenu *language = options->addMenu(tr("&Language"));
    auto *languages = new QActionGroup(language);
    for (const UiLanguage::Language &entry : UiLanguage::available()) {
        QAction *action = language->addAction(entry.name);
        action->setCheckable(true);
        action->setActionGroup(languages);
        action->setChecked(entry.code == UiLanguage::chosen());
        const QString code = entry.code;
        connect(action, &QAction::triggered, this, [this, code] {
            if (code == UiLanguage::chosen())
                return;
            UiLanguage::setChosen(code);
            QMessageBox::information(this, tr("Language"),
                                     tr("The new language is used the next time Pragma Chess starts."));
        });
    }

    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(m_aboutAction);
    help->addAction(m_aboutQtAction);
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar(tr("Main Toolbar"));
    m_mainToolBar = toolBar;
    toolBar->toggleViewAction()->setText(tr("&Toolbar"));
    toolBar->setObjectName(QStringLiteral("mainToolBar"));
    toolBar->setMovable(false);
    // Syncing everything is the one button that stands on its own.
    toolBar->addAction(m_syncNowAction);
    toolBar->addSeparator();
    toolBar->addAction(m_newGameAction);
    toolBar->addAction(m_newTrainingAction);
}

QDockWidget *MainWindow::addDock(QMainWindow *host, const QString &objectName, const QString &title,
                                 QWidget *widget, Qt::DockWidgetArea area)
{
    auto *dock = new QDockWidget(title, host);
    dock->setObjectName(objectName);
    // Closable, or Qt would disable the toggleViewAction the View menu shows;
    // the empty title bar set below keeps the close and float buttons hidden.
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    dock->setWidget(widget);
    host->addDockWidget(area, dock);
    return dock;
}

QByteArray MainWindow::saveLayout() const
{
    QByteArray layout;
    QDataStream stream(&layout, QIODevice::WriteOnly);
    stream << QByteArray("pragma-layout-4") << saveState() << m_sidebar->saveState()
           << m_gamesSplitter->saveState();
    return layout;
}

void MainWindow::restoreLayout(const QByteArray &layout)
{
    QDataStream stream(layout);
    QByteArray magic;
    QByteArray windowState;
    QByteArray sidebarState;
    QByteArray splitterState;
    stream >> magic;
    if (magic != "pragma-layout-4" && magic != "pragma-layout-3" && magic != "pragma-layout-2") {
        // Layouts from before the sidebar existed only hold the window state.
        restoreState(layout);
        return;
    }
    stream >> windowState >> sidebarState;
    restoreState(windowState);
    // Version 2 stacked the opening tree under the moves: keep the default sidebar.
    if (magic == "pragma-layout-2")
        return;
    m_sidebar->restoreState(sidebarState);
    // Version 4 added the tree | games list ruler inside the Games panel.
    if (magic == "pragma-layout-4") {
        stream >> splitterState;
        m_gamesSplitter->restoreState(splitterState);
    }
}

void MainWindow::createDocks()
{
    m_moveView = new QTableView;
    m_moveView->setModel(m_moveListModel);
    m_moveView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_moveView->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_moveView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_moveView->setShowGrid(false);
    m_moveView->setFocusPolicy(Qt::NoFocus);
    m_moveView->setItemDelegate(new PaddedItemDelegate(CellPadding::vertical, CellPadding::horizontal, m_moveView));
    for (Qt::Orientation orientation : {Qt::Horizontal, Qt::Vertical}) {
        auto *header = new PaddedHeaderView(orientation, CellPadding::vertical, CellPadding::horizontal, m_moveView);
        // As a table's own headers: the current move's number and column stand out.
        header->setSectionsClickable(true);
        header->setHighlightSections(true);
        if (orientation == Qt::Horizontal)
            m_moveView->setHorizontalHeader(header);
        else
            m_moveView->setVerticalHeader(header);
    }
    m_moveView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_moveView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(m_moveView, &QTableView::clicked, this, [this](const QModelIndex &index) {
        const int ply = m_moveListModel->plyForIndex(index);
        if (ply > 0)
            m_session->goToPly(ply);
    });
    m_movesDock = addDock(m_sidebar, QStringLiteral("movesDock"), tr("Moves"), m_moveView, Qt::RightDockWidgetArea);

    m_enginePanel = new EnginePanel(m_startEngineAction);
    m_enginePanel->setEngineName(tr("Stockfish"));
    m_engineDock = addDock(m_sidebar, QStringLiteral("engineDock"), tr("Engine"), m_enginePanel, Qt::RightDockWidgetArea);

    m_bookPanel = new BookPanel;
    connect(m_bookPanel, &BookPanel::moveActivated, this, &MainWindow::playMove);
    connect(m_bookPanel, &BookPanel::backActivated, m_session, &GameSession::goBack);
    m_openingTreeDock = addDock(m_sidebar, QStringLiteral("openingTreeDock"), tr("Opening Tree"), m_bookPanel,
                                Qt::RightDockWidgetArea);
    // The panels speak for themselves: no title bars (the names stay in the View menu).
    for (QDockWidget *dock : {m_movesDock, m_openingTreeDock, m_engineDock})
        dock->setTitleBarWidget(new QWidget(dock));
    // The opening tree sits right of the moves, with a separator to resize both.
    m_sidebar->splitDockWidget(m_movesDock, m_openingTreeDock, Qt::Horizontal);

    m_gameView = new QTableView;
    m_gameView->setModel(m_gameListProxy);
    m_gameView->setSortingEnabled(true);
    m_gameView->sortByColumn(GameListModel::Number, Qt::AscendingOrder);
    m_gameView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_gameView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_gameView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gameView->setAlternatingRowColors(true);
    m_gameView->setShowGrid(false);
    m_gameView->setWordWrap(false);
    m_gameView->verticalHeader()->hide();
    m_gameView->horizontalHeader()->setStretchLastSection(true);
    m_gameView->horizontalHeader()->setSectionsMovable(true);
    connect(m_gameView, &QTableView::activated, this, &MainWindow::openGame);
    m_gameView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gameView, &QWidget::customContextMenuRequested, this, &MainWindow::showGameListMenu);

    m_databaseTree = new DatabaseTreeWidget;
    connect(m_databaseTree, &DatabaseTreeWidget::categorySelected, this, &MainWindow::showCategory);
    connect(m_databaseTree, &DatabaseTreeWidget::connectSourceRequested, this, &MainWindow::connectSource);
    connect(m_databaseTree, &DatabaseTreeWidget::manageSourcesRequested, this, &MainWindow::manageSources);
    connect(m_databaseTree, &DatabaseTreeWidget::syncSourceRequested, this,
            [this](qint64 id) { m_sourceSync->syncSource(id); });

    // A vertical ruler between the tree and the list resizes them.
    m_gamesSplitter = new QSplitter(Qt::Horizontal);
    m_gamesSplitter->setObjectName(QStringLiteral("gamesSplitter"));
    m_gamesSplitter->setChildrenCollapsible(false);
    m_gamesSplitter->addWidget(m_databaseTree);
    m_gamesSplitter->addWidget(m_gameView);
    m_gamesSplitter->setStretchFactor(0, 0);
    m_gamesSplitter->setStretchFactor(1, 1);
    m_gamesSplitter->setSizes({220, 800});
    m_gamesDock = addDock(this, QStringLiteral("gamesDock"), tr("Games"), m_gamesSplitter, Qt::BottomDockWidgetArea);
    // The tree and the list speak for themselves: no title bar ("Games" stays in the View menu).
    m_gamesDock->setTitleBarWidget(new QWidget(m_gamesDock));
}

void MainWindow::createStatusBar()
{
    m_folderSyncLabel = new QLabel;
    m_folderSyncLabel->hide();
    statusBar()->addPermanentWidget(m_folderSyncLabel);

    m_syncLabel = new QLabel;
    m_syncLabel->hide();
    statusBar()->addPermanentWidget(m_syncLabel);

    m_gameCountLabel = new QLabel;
    statusBar()->addPermanentWidget(m_gameCountLabel);
}

void MainWindow::setDatabase(std::unique_ptr<GameDatabase> database)
{
    m_sourceSync->setDatabase(nullptr); // Before the old database goes away.
    m_gameListModel->setDatabase(nullptr);
    m_database = std::move(database);
    m_gameListModel->setDatabase(m_database.get());
    m_gameView->resizeColumnsToContents();
    m_openGameIndex = -1;

    updateGameCount();
    updateDatabaseActions();
    updateGameActions();
    m_filterSource = 0;
    m_category = {};
    m_gameListProxy->setDatabase(m_database.get());
    m_databaseTree->setDatabase(m_database.get());
    m_sourceSync->setDatabase(m_database.get());
}

void MainWindow::openGame(const QModelIndex &proxyIndex)
{
    const QModelIndex source = m_gameListProxy->mapToSource(proxyIndex);
    if (!m_database || !source.isValid())
        return;
    if (std::optional<GameRecord> game = m_database->loadGame(source.row())) {
        m_gameView->selectRow(proxyIndex.row());
        m_openGameIndex = source.row();
        orientBoardForMe(*game);
        m_session->setGame(*game);
    }
}

void MainWindow::orientBoardForMe(const GameRecord &game)
{
    if (!m_database)
        return;
    if (const std::optional<Side> side = mySide(game, m_database->playerRoles()))
        m_flipBoardAction->setChecked(*side == Side::Black);
}

void MainWindow::showGameListMenu(const QPoint &position)
{
    const QModelIndex index = m_gameView->indexAt(position);
    if (!m_database || !index.isValid())
        return;
    const QModelIndex source = m_gameListProxy->mapToSource(index);
    const GameRecord header = m_database->header(source.row());
    QString player;
    if (source.column() == GameListModel::White || source.column() == GameListModel::WhiteElo)
        player = header.white;
    else if (source.column() == GameListModel::Black || source.column() == GameListModel::BlackElo)
        player = header.black;
    if (player.isEmpty())
        return; // Only players have a menu, for now.

    QMenu menu(this);
    QMenu *who = menu.addMenu(tr("Who Is This?"));
    who->setToolTip(player);
    const PlayerRole current = m_database->playerRoles().value(player);
    auto *roles = new QActionGroup(who);
    const std::pair<PlayerRole, QString> choices[] = {
        {PlayerRole::Me, tr("It's &Me")}, {PlayerRole::Friend, tr("A &Friend")}, {PlayerRole::Opponent, tr("An &Opponent")}};
    for (const auto &[role, text] : choices) {
        QAction *action = who->addAction(text);
        action->setCheckable(true);
        action->setChecked(role == current);
        action->setActionGroup(roles);
        connect(action, &QAction::triggered, this, [this, player, role] { setPlayerRole(player, role); });
    }
    who->addSeparator();
    QAction *forget = who->addAction(tr("&Nobody in Particular"), this,
                                     [this, player] { setPlayerRole(player, PlayerRole::None); });
    forget->setEnabled(current != PlayerRole::None);
    menu.exec(m_gameView->viewport()->mapToGlobal(position));
}

void MainWindow::setPlayerRole(const QString &player, PlayerRole role)
{
    if (!m_database)
        return;
    QString error;
    if (!m_database->setPlayerRole(player, role, &error)) {
        QMessageBox::warning(this, tr("Who Is This?"), tr("Could not save who %1 is: %2").arg(player, error));
        return;
    }
    m_databaseTree->refresh();
    showCategory(m_category); // A filter on roles now shows other games.
    if (role == PlayerRole::Me)
        orientBoardForMe(m_session->game());
}

void MainWindow::syncBoard()
{
    const BoardFrame frame = frameFor(m_session->position(), m_session->lastMoveFrom(), m_session->lastMoveTo());
    if (m_animateNextBoard) {
        m_animateNextBoard = false;
        m_board->setBoardAnimated(frame, kEngineMoveMs);
    } else {
        m_board->setBoard(frame);
    }
    m_capturedPieces->setCaptured(m_session->position().capturedSince(m_session->initialPosition()));
    QMultiHash<int, int> legalMoves;
    // In training the user only moves their own colour; the engine answers by itself.
    if (!isEngineTurn()) {
        for (const ChessMove &move : m_session->position().legalMoves())
            legalMoves.insert(move.from, move.to);
    }
    m_board->setLegalMoves(legalMoves);
    m_engineLine.clear();
    // An explanation belongs to one move: moving on turns it off until asked again.
    m_explainAction->setChecked(false);
    updateExplainer();

    const QModelIndex current = m_moveListModel->indexForPly(m_session->ply());
    if (current.isValid()) {
        m_moveView->setCurrentIndex(current);
        m_moveView->scrollTo(current);
    } else {
        m_moveView->clearSelection();
        m_moveView->scrollToTop();
    }
    updateNavigationActions();
}

void MainWindow::updateNavigationActions()
{
    const bool atStart = m_session->ply() == 0;
    const bool atEnd = m_session->ply() == m_session->plyCount();
    m_firstMoveAction->setEnabled(!atStart);
    m_previousMoveAction->setEnabled(!atStart);
    m_nextMoveAction->setEnabled(!atEnd);
    m_lastMoveAction->setEnabled(!atEnd);
}

void MainWindow::updateGameCount()
{
    if (!m_database) {
        m_gameCountLabel->setText(tr("No database"));
        return;
    }
    const qint64 total = m_database->gameCount();
    const qint64 shown = m_gameListProxy->rowCount();
    const QLocale locale;
    const QString count = m_gameListProxy->isFiltered()
        ? tr("%1 of %2 games").arg(locale.toString(shown), locale.toString(total))
        : total == 1 ? tr("1 game") : tr("%1 games").arg(locale.toString(total));
    m_gameCountLabel->setText(tr("%1 — %2").arg(m_database->name(), count));
}

void MainWindow::showCategory(const GameCategory &category)
{
    using Kind = GameCategory::Kind;
    m_category = category;
    m_filterSource = category.kind == Kind::Source ? category.sourceId : 0;
    GameFilterProxyModel::Predicate predicate;
    const QString value = category.value;
    switch (category.kind) {
    case Kind::All:
        break;
    case Kind::Role:
        if (m_database) {
            predicate = [roles = m_database->playerRoles(), role = playerRoleFromKey(value)](const GameRecord &game) {
                return DatabaseOutline::hasRole(game, roles, role);
            };
        }
        break;
    case Kind::Player:
        predicate = [value](const GameRecord &game) { return game.white == value || game.black == value; };
        break;
    case Kind::EcoLetter:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::ecoCode(game.eco).startsWith(value); };
        break;
    case Kind::Eco:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::ecoCode(game.eco) == value; };
        break;
    case Kind::Event:
        predicate = [value](const GameRecord &game) { return DatabaseOutline::eventName(game.event) == value; };
        break;
    case Kind::Year:
        predicate = [year = value.toInt()](const GameRecord &game) { return DatabaseOutline::year(game.date) == year; };
        break;
    case Kind::Source:
        if (m_database) {
            predicate = [ids = m_database->sourceGameIds(category.sourceId)](const GameRecord &game) {
                return ids.contains(game.id);
            };
        }
        break;
    }
    m_gameListProxy->setPredicate(predicate);
    updateGameCount();
}

void MainWindow::newDatabase()
{
    if (!UserFolders::ensureDatabasesDir()) {
        QMessageBox::warning(this, tr("New Database"),
                             tr("Could not create the folder “%1”.").arg(UserFolders::databasesDir()));
        return;
    }

    QString name = tr("New Database");
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, tr("New Database"), tr("Database name:"),
                                     QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.startsWith(QLatin1Char('.'))) {
            QMessageBox::warning(this, tr("New Database"), tr("“%1” is not a valid name.").arg(name));
            continue;
        }
        const QString path = QDir(UserFolders::databasesDir())
                                 .filePath(name + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
        if (QFileInfo::exists(path)) {
            QMessageBox::warning(this, tr("New Database"),
                                 tr("A database named “%1” already exists.").arg(name));
            continue;
        }

        QString error;
        std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::create(path, {}, &error);
        if (!database) {
            QMessageBox::warning(this, tr("New Database"),
                                 tr("Could not create the database: %1").arg(error));
            return;
        }
        setDatabase(std::move(database));
        m_session->setGame(GameRecord());
        statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(path)), 5000);
        return;
    }
}

void MainWindow::openDatabase()
{
    UserFolders::ensureDatabasesDir();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Database"), UserFolders::databasesDir(),
        tr("Pragma Chess databases (*.pdb);;All files (*)"));
    if (!path.isEmpty())
        openDatabaseFile(path);
}

bool MainWindow::openDatabaseFile(const QString &path)
{
    if (m_database && m_database->location() == QFileInfo(path).absoluteFilePath())
        return true;

    QString error;
    std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
    if (!database) {
        QMessageBox::warning(this, tr("Open Database"),
                             tr("Could not open the database: %1").arg(error));
        return false;
    }
    setDatabase(std::move(database));
    if (m_gameListProxy->rowCount() > 0)
        openGame(m_gameListProxy->index(0, 0));
    else
        m_session->setGame(GameRecord());
    return true;
}

void MainWindow::openInitialDatabase(const QString &preferredPath)
{
    if (m_database && !preferredPath.isEmpty() && m_database->location() == preferredPath)
        return;

    QString error;
    if (!preferredPath.isEmpty() && QFileInfo::exists(preferredPath)) {
        if (auto database = SqliteGameDatabase::open(preferredPath, &error)) {
            setDatabase(std::move(database));
            return;
        }
    }

    if (!UserFolders::ensureDatabasesDir()) {
        statusBar()->showMessage(tr("Could not create the folder %1")
                                     .arg(QDir::toNativeSeparators(UserFolders::databasesDir())));
        setDatabase(nullptr);
        return;
    }

    const QDir folder(UserFolders::databasesDir());
    const QStringList existing = folder.entryList(
        {QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)}, QDir::Files, QDir::Name);
    for (const QString &file : existing) {
        if (auto database = SqliteGameDatabase::open(folder.filePath(file), &error)) {
            setDatabase(std::move(database));
            return;
        }
    }

    // First run: seed the user's databases folder with a few classic games.
    const QString seed = folder.filePath(tr("Classic Games") + QLatin1Char('.')
                                         + QLatin1String(UserFolders::databaseSuffix));
    if (auto database = SqliteGameDatabase::create(seed, classicGames(), &error)) {
        setDatabase(std::move(database));
        return;
    }
    statusBar()->showMessage(tr("Could not create %1: %2").arg(QDir::toNativeSeparators(seed), error));
    setDatabase(nullptr);
}

void MainWindow::saveDatabase()
{
    if (!m_database)
        return;
    if (m_database->location().isEmpty()) {
        saveDatabaseAs();
        return;
    }
    // Changes are written to the .pdb file as they happen; nothing pending.
    statusBar()->showMessage(tr("%1 is up to date").arg(m_database->name()), 3000);
}

void MainWindow::saveDatabaseAs()
{
    if (!m_database)
        return;
    UserFolders::ensureDatabasesDir();
    const QString suffix = QLatin1String(UserFolders::databaseSuffix);
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Database As"),
        QDir(UserFolders::databasesDir()).filePath(m_database->name() + QLatin1Char('.') + suffix),
        tr("Pragma Chess databases (*.pdb)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        path += QLatin1Char('.') + suffix;

    QString error;
    if (!m_database->saveCopy(path, &error)) {
        QMessageBox::warning(this, tr("Save Database As"),
                             tr("Could not save the database: %1").arg(error));
        return;
    }

    // Continue working on the new file, at the same game and move.
    const qint64 gameIndex = m_openGameIndex;
    const GameRecord game = m_session->game();
    const int ply = m_session->ply();
    QString openError;
    std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &openError);
    if (!database) {
        QMessageBox::warning(this, tr("Save Database As"),
                             tr("The database was saved but could not be opened: %1").arg(openError));
        return;
    }
    setDatabase(std::move(database));
    if (gameIndex >= 0 && gameIndex < m_database->gameCount()) {
        const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(gameIndex), 0));
        m_openGameIndex = gameIndex;
        if (proxyIndex.isValid())
            m_gameView->selectRow(proxyIndex.row());
    }
    m_session->setGame(game);
    m_session->goToPly(ply);
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(path)), 5000);
}

void MainWindow::rebuildDatabasesMenu()
{
    m_databasesMenu->clear();

    const QDir folder(UserFolders::databasesDir());
    const QFileInfoList files = folder.entryInfoList(
        {QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : files) {
        QAction *action = m_databasesMenu->addAction(file.completeBaseName());
        action->setCheckable(true);
        action->setChecked(m_database && m_database->location() == file.absoluteFilePath());
        const QString path = file.absoluteFilePath();
        connect(action, &QAction::triggered, this, [this, path] { openDatabaseFile(path); });
    }
    if (files.isEmpty())
        m_databasesMenu->addAction(tr("No databases"))->setEnabled(false);

    m_databasesMenu->addSeparator();
    m_databasesMenu->addAction(m_showDatabasesFolderAction);
}

void MainWindow::restoreBook()
{
    QSettings settings;
    const QString key = QStringLiteral("book/path");
    if (!settings.contains(key)) {
        // First launch: seed the Books folder with the book built from named openings.
        const QString seed = QDir(UserFolders::booksDir())
                                 .filePath(QStringLiteral("Pragma Openings.") + QLatin1String(UserFolders::bookSuffix));
        if (!QFile::exists(seed) && UserFolders::ensureBooksDir()) {
            QFile::copy(QStringLiteral(":/books/pragma-openings.bin"), seed);
            QFile::setPermissions(seed, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        }
        chooseBook(QFile::exists(seed) ? seed : QString());
        return;
    }
    const QString path = settings.value(key).toString();
    if (path.isEmpty() || !QFile::exists(path)) {
        chooseBook(QString());
        return;
    }
    chooseBook(path);
}

void MainWindow::chooseBook(const QString &path)
{
    if (path.isEmpty()) {
        m_book.reset();
    } else {
        auto book = std::make_unique<PolyglotBook>();
        QString error;
        if (!book->open(path, &error)) {
            QMessageBox::warning(this, tr("Open Book"), tr("Could not open the book %1: %2")
                                                            .arg(QDir::toNativeSeparators(path), error));
            return;
        }
        m_book = std::move(book);
    }
    QSettings().setValue(QStringLiteral("book/path"), path);
    const QString bookName = path.isEmpty() ? QString() : QFileInfo(path).completeBaseName();
    m_bookPanel->setBookName(bookName);
    m_enginePanel->setBookName(bookName);
    updateBookMoves();
}

void MainWindow::openBookFile()
{
    UserFolders::ensureBooksDir();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open Book"), UserFolders::booksDir(),
                                                      tr("Polyglot opening books (*.bin);;All files (*)"));
    if (path.isEmpty())
        return;
    chooseBook(path);
    if (m_book)
        m_openingTreeDock->show();
}

void MainWindow::rebuildBookMenu()
{
    m_bookMenu->clear();
    const QString current = m_book ? QFileInfo(m_book->path()).absoluteFilePath() : QString();
    auto *group = new QActionGroup(m_bookMenu);

    const QDir folder(UserFolders::booksDir());
    QFileInfoList files = folder.entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::bookSuffix)},
                                               QDir::Files, QDir::Name);
    // A book chosen outside the folder is listed too.
    if (!current.isEmpty() && std::none_of(files.cbegin(), files.cend(), [&](const QFileInfo &file) {
            return file.absoluteFilePath() == current;
        }))
        files.prepend(QFileInfo(current));
    for (const QFileInfo &file : std::as_const(files)) {
        QAction *action = m_bookMenu->addAction(file.completeBaseName());
        action->setCheckable(true);
        action->setActionGroup(group);
        action->setToolTip(QDir::toNativeSeparators(file.absoluteFilePath()));
        action->setChecked(file.absoluteFilePath() == current);
        const QString path = file.absoluteFilePath();
        connect(action, &QAction::triggered, this, [this, path] {
            chooseBook(path);
            if (m_book)
                m_openingTreeDock->show();
        });
    }
    QAction *none = m_bookMenu->addAction(tr("&No Book"));
    none->setCheckable(true);
    none->setActionGroup(group);
    none->setChecked(current.isEmpty());
    connect(none, &QAction::triggered, this, [this] { chooseBook(QString()); });

    m_bookMenu->addSeparator();
    QMenu *names = m_bookMenu->addMenu(tr("Opening &Names"));
    names->setToolTip(tr("The database whose games name the openings and variations"));
    auto *namesGroup = new QActionGroup(names);
    const QDir databases(UserFolders::databasesDir());
    const QFileInfoList databaseFiles = databases.entryInfoList(
        {QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix)}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : databaseFiles) {
        QAction *action = names->addAction(file.completeBaseName());
        action->setCheckable(true);
        action->setActionGroup(namesGroup);
        const QString path = file.absoluteFilePath();
        action->setChecked(QFileInfo(m_openingNamesPath).absoluteFilePath() == path);
        connect(action, &QAction::triggered, this, [this, path] { chooseOpeningNames(path); });
    }
    QAction *noNames = names->addAction(tr("No Names"));
    noNames->setCheckable(true);
    noNames->setActionGroup(namesGroup);
    noNames->setChecked(m_openingNamesPath.isEmpty());
    connect(noNames, &QAction::triggered, this, [this] { chooseOpeningNames(QString()); });

    m_bookMenu->addSeparator();
    m_bookMenu->addAction(tr("&Open Book…"), this, &MainWindow::openBookFile);
    m_bookMenu->addAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon), tr("Show Books &Folder"), this, [] {
        UserFolders::ensureBooksDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::booksDir()));
    });
}

void MainWindow::updateBookMoves()
{
    reloadOpeningNamesIfChanged();
    const ChessPosition &position = m_session->position();
    const QList<PolyglotBook::Move> moves = m_book ? m_book->moves(position) : QList<PolyglotBook::Move>();
    QList<OpeningNames::Name> names;
    OpeningNames::Name opening;
    if (m_openingNames) {
        for (const PolyglotBook::Move &move : moves) {
            ChessPosition next = position;
            next.play(move.move);
            names << m_openingNames->name(next);
        }
        // The game is still in the last opening it passed through.
        for (int ply = m_session->ply(); ply >= 0 && opening.isEmpty(); --ply)
            opening = m_openingNames->name(m_session->positionAt(ply));
    }
    m_enginePanel->setOpening(opening);
    QString lastMove;
    if (const std::optional<ChessMove> move = m_session->lastMove()) {
        const ChessPosition &before = m_session->positionAt(m_session->ply() - 1);
        lastMove = before.moveNumberText() + figurineSan(before.san(*move));
    }
    m_bookPanel->setMoves(position, moves, names, lastMove);
}

void MainWindow::restoreOpeningNames()
{
    QSettings settings;
    const QString key = QStringLiteral("book/openingNames");
    if (settings.contains(key)) {
        const QString path = settings.value(key).toString();
        chooseOpeningNames(QFile::exists(path) ? path : QString());
        return;
    }
    // First launch: the named openings of lichess-org/chess-openings, as a database.
    const QString seed = QDir(UserFolders::databasesDir())
                             .filePath(tr("Opening Names") + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
    if (!QFile::exists(seed) && UserFolders::ensureDatabasesDir()) {
        QFile tsv(QStringLiteral(":/openings/lichess-openings.tsv"));
        QString error;
        if (!tsv.open(QIODevice::ReadOnly)
            || !SqliteGameDatabase::create(seed, OpeningNames::gamesFromTsv(QString::fromUtf8(tsv.readAll())), &error))
            statusBar()->showMessage(tr("Could not create %1: %2").arg(QDir::toNativeSeparators(seed), error));
    }
    chooseOpeningNames(QFile::exists(seed) ? seed : QString());
}

void MainWindow::chooseOpeningNames(const QString &path)
{
    QSettings().setValue(QStringLiteral("book/openingNames"), path);
    m_openingNamesPath = path;
    m_openingNames.reset();
    m_openingNamesSize = -1;
    updateBookMoves(); // Reads the names.
}

void MainWindow::reloadOpeningNamesIfChanged()
{
    if (m_openingNamesPath.isEmpty())
        return;
    const QFileInfo file(m_openingNamesPath);
    if (m_openingNames && file.lastModified() == m_openingNamesModified && file.size() == m_openingNamesSize)
        return;
    m_openingNamesModified = file.lastModified();
    m_openingNamesSize = file.size();

    QString error;
    const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(m_openingNamesPath, &error);
    if (!database) {
        m_openingNames = std::make_unique<OpeningNames>();
        statusBar()->showMessage(tr("Could not read the opening names in %1: %2")
                                     .arg(QDir::toNativeSeparators(m_openingNamesPath), error), 8000);
        return;
    }
    QList<GameRecord> games;
    games.reserve(database->gameCount());
    for (qint64 index = 0; index < database->gameCount(); ++index) {
        if (std::optional<GameRecord> game = database->loadGame(index))
            games << *game;
    }
    m_openingNames = std::make_unique<OpeningNames>(games);
}

void MainWindow::updateDatabaseActions()
{
    const bool hasDatabase = m_database != nullptr;
    m_saveDatabaseAction->setEnabled(hasDatabase && (m_database->isModified() || m_database->location().isEmpty()));
    m_saveDatabaseAsAction->setEnabled(hasDatabase);
    m_connectSourceAction->setEnabled(hasDatabase);
    m_manageSourcesAction->setEnabled(hasDatabase);
}

void MainWindow::connectSource()
{
    if (!m_database)
        return;
    ConnectSourceWizard wizard(m_database->name(), this);
    if (wizard.exec() != QDialog::Accepted)
        return;
    GameSource source = wizard.source();
    QString error;
    if (!m_database->addSource(source, &error)) {
        QMessageBox::warning(this, tr("Connect Source"), tr("Could not connect the source: %1").arg(error));
        return;
    }
    m_sourceSync->syncSource(source.id);
    m_databaseTree->refresh();
    statusBar()->showMessage(tr("Connected %1 to %2").arg(SourceCatalog::displayName(source), m_database->name()), 5000);
}

void MainWindow::openSyncDialog()
{
    SyncDialog dialog(SyncSettings::load(), m_folderSync, this);
    connect(&dialog, &SyncDialog::syncRequested, this, [this](const SyncSettings &settings) {
        settings.save();
        applySyncSettings();
        syncNow(); // The whole thing, in order, as the toolbar button does.
    });
    if (dialog.exec() != QDialog::Accepted)
        return;
    dialog.settings().save();
    applySyncSettings();
}

void MainWindow::applySyncSettings()
{
    const SyncSettings settings = SyncSettings::load();
    RemoteStore *store = settings.createStore(this);
    m_folderSync->setStore(store);
    if (m_syncStore)
        m_syncStore->deleteLater();
    m_syncStore = store;
    m_folderSyncLabel->hide();
    if (!store) {
        m_syncTimer->stop();
        return;
    }
    m_syncTimer->start();
    QTimer::singleShot(1500, m_folderSync, &FolderSync::sync);
}

void MainWindow::syncNow(std::function<void()> then)
{
    if (m_syncPipeline->isRunning()) {
        if (then)
            connect(m_syncPipeline, &SyncPipeline::finished, this, [then](const QStringList &) { then(); },
                    Qt::SingleShotConnection);
        return;
    }

    m_syncPipeline->clear();
    // The order is the point: games arrive first, the project is written with
    // them, and only a folder holding both goes to the server.
    m_syncPipeline->addTask(new SourceSyncTask(m_sourceSync));
    if (!m_projectPath.isEmpty() && isWindowModified()) {
        m_syncPipeline->addTask(new SyncStepTask(tr("Project"), [this] {
            const Project project = captureProject();
            QString error;
            if (!project.saveToFile(m_projectPath, &error))
                return error;
            m_savedProjectYaml = project.toYaml();
            addRecentProject(m_projectPath);
            updateProjectModified();
            return QString();
        }));
    }
    // The session is what a sync without a project file has to offer.
    m_syncPipeline->addTask(new SyncStepTask(tr("Session"), [this] {
        saveSession();
        return QString();
    }));
    m_syncPipeline->addTask(new FolderSyncTask(m_folderSync));

    if (then)
        connect(m_syncPipeline, &SyncPipeline::finished, this, [then](const QStringList &) { then(); },
                Qt::SingleShotConnection);
    m_syncPipeline->run();
}

void MainWindow::updateSyncActions()
{
    m_syncNowAction->setEnabled(!m_syncPipeline->isRunning());
}

void MainWindow::manageSources()
{
    if (!m_database)
        return;
    ManageSourcesDialog dialog(m_database.get(), m_sourceSync, this);
    connect(&dialog, &ManageSourcesDialog::connectRequested, this, &MainWindow::connectSource);
    dialog.exec();
    m_databaseTree->refresh();
}

void MainWindow::setAnalysisEnabled(bool enabled)
{
    m_startEngineAction->setText(enabled ? tr("Stop &Analysis") : tr("&Analyze"));
    m_startEngineAction->setIcon(enabled ? themeIcon("media-playback-stop", QStyle::SP_MediaStop)
                                         : themeIcon("media-playback-start", QStyle::SP_MediaPlay));

    if (!enabled) {
        m_explainAction->setChecked(false); // Explaining needs the analysis.
        m_engine->stopAnalysis();
        m_evaluationBar->setEvaluation(std::nullopt);
        m_enginePanel->setEvaluation(std::nullopt);
        m_enginePanel->setStatus(QString());
        return;
    }

    if (!m_engine->isRunning()) {
        const QString command = m_engineName.isEmpty() ? QStringLiteral("stockfish") : m_engineName;
        const QString executable = UciEngine::findExecutable(command);
        if (executable.isEmpty() || !m_engine->start(executable)) {
            m_enginePanel->setStatus(tr("The UCI engine “%1” was not found. Install Stockfish "
                                        "or choose another engine.").arg(command));
            // Defer so the action's toggle finishes before being reverted.
            QMetaObject::invokeMethod(m_startEngineAction, [this] { m_startEngineAction->setChecked(false); },
                                      Qt::QueuedConnection);
            return;
        }
        m_engineName = command;
        m_engineExecutable = executable;
    }
    m_enginePanel->setStatus(tr("Analyzing…"));
    analyzeCurrentPosition();
}

void MainWindow::analyzeCurrentPosition()
{
    if (!m_startEngineAction->isChecked() || !m_engine->isRunning())
        return;
    if (m_trainingThinking) // The engine is searching the move it will play.
        return;
    const GameRecord &game = m_session->game();
    QStringList moves;
    moves.reserve(m_session->ply());
    for (int i = 0; i < m_session->ply(); ++i)
        moves << game.moves.at(i).uci;
    m_engine->analyze(game.startFen, moves, m_session->position().sideToMove());
}

void MainWindow::setExplainEnabled(bool enabled)
{
    if (enabled) {
        // Explaining starts the engine if it is off.
        m_startEngineAction->setChecked(true);
        if (!m_engine->isRunning()) {
            QMetaObject::invokeMethod(m_explainAction, [this] { m_explainAction->setChecked(false); },
                                      Qt::QueuedConnection);
            return;
        }
        m_engineDock->show();
        m_explainer->setEnabled(true, m_engineExecutable);
        return;
    }
    m_explainer->setEnabled(false);
    m_board->stopSequence();
    m_board->setExplanation({}, {});
    m_enginePanel->setExplanation(QString());
    m_explanationText.clear();
}

void MainWindow::updateExplainer()
{
    const int ply = m_session->ply();
    m_explainer->setPosition(m_session->position(),
                             ply > 0 ? std::optional<ChessPosition>(m_session->positionAt(ply - 1)) : std::nullopt,
                             m_session->lastMove());
}

void MainWindow::newGame()
{
    m_trainingModeAction->setChecked(false); // A plain new game is not a training one.
    GameRecord game;
    game.result = QStringLiteral("*");
    game.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    startGame(game);
    statusBar()->showMessage(tr("New game: enter the moves on the board"), 5000);
}

void MainWindow::startGame(const GameRecord &game)
{
    m_gameView->clearSelection();
    m_openGameIndex = -1;
    m_session->setGame(game);
}

void MainWindow::newTraining()
{
    NewTrainingDialog dialog(m_trainingSide, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_trainingSide = dialog.side();

    // Who the user is, as the database already knows them from "Who Is This?".
    QString me = tr("Me");
    if (m_database) {
        const PlayerRoles roles = m_database->playerRoles();
        for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
            if (it.value() == PlayerRole::Me) {
                me = it.key();
                break;
            }
        }
    }
    const QString engineName = m_engine->name().isEmpty()
        ? (m_engineName.isEmpty() ? tr("Engine") : m_engineName)
        : m_engine->name();

    GameRecord game;
    game.event = tr("Training");
    game.result = QStringLiteral("*");
    game.date = QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd"));
    game.white = m_trainingSide == Side::White ? me : engineName;
    game.black = m_trainingSide == Side::White ? engineName : me;

    m_flipBoardAction->setChecked(m_trainingSide == Side::Black); // Play from the bottom.
    m_startEngineAction->setChecked(true); // The score stays visible throughout.
    startGame(game);
    m_trainingModeAction->setChecked(true);
    updateTraining();                      // setChecked() is silent when the flag was already on.
    statusBar()->showMessage(m_trainingSide == Side::White
                                 ? tr("Training: you play White against %1").arg(engineName)
                                 : tr("Training: you play Black against %1").arg(engineName),
                             5000);
}

void MainWindow::setTrainingMode(bool enabled)
{
    if (!enabled) {
        m_trainingThinking = false;
        m_enginePanel->setLineHidden(false);
    }
    syncBoard(); // The user may not move for the engine.
    updateTraining();
}

bool MainWindow::isEngineTurn() const
{
    return m_trainingModeAction->isChecked() && m_session->position().sideToMove() != m_trainingSide;
}

void MainWindow::updateTraining()
{
    const bool training = m_trainingModeAction->isChecked();
    // The best line is the user's move: it may only be shown once they played.
    m_enginePanel->setLineHidden(training && !isEngineTurn());
    if (!training)
        return;
    // Looking back at an earlier move is not a turn to answer.
    if (m_session->ply() != m_session->plyCount())
        return;
    const ChessPosition &position = m_session->position();
    if (position.isCheckmate() || position.isStalemate()) {
        recordTrainingResult();
        return;
    }
    if (isEngineTurn())
        playEngineMove();
}

void MainWindow::playEngineMove()
{
    if (m_trainingThinking)
        return;
    if (!m_engine->isRunning()) {
        m_startEngineAction->setChecked(true); // Starts the engine, or reports why it cannot.
        if (!m_engine->isRunning())
            return;
    }
    const GameRecord &game = m_session->game();
    QStringList moves;
    moves.reserve(m_session->ply());
    for (int i = 0; i < m_session->ply(); ++i)
        moves << game.moves.at(i).uci;
    m_trainingThinking = true;
    m_lastEvaluation = {};
    m_enginePanel->setStatus(tr("Thinking…"));
    m_engine->analyze(game.startFen, moves, m_session->position().sideToMove(), {kTrainingDepth, 0, false});
}

void MainWindow::finishEngineMove()
{
    if (!m_trainingThinking)
        return;
    m_trainingThinking = false;
    const std::optional<ChessMove> move = m_lastEvaluation.pv.isEmpty()
        ? std::nullopt
        : m_session->position().moveFromUci(m_lastEvaluation.pv.constFirst());
    if (!move) {
        m_enginePanel->setStatus(tr("The engine found no move to play."));
        return;
    }
    m_animateNextBoard = true; // syncBoard() slides it across the board.
    playMove(*move);
}

void MainWindow::recordTrainingResult()
{
    const ChessPosition &position = m_session->position();
    GameRecord game = m_session->game();
    QString message;
    if (position.isStalemate()) {
        game.result = QStringLiteral("1/2-1/2");
        message = tr("Stalemate: the training game is a draw.");
    } else {
        const bool whiteWon = position.sideToMove() == Side::Black;
        game.result = whiteWon ? QStringLiteral("1-0") : QStringLiteral("0-1");
        const bool userWon = (whiteWon ? Side::White : Side::Black) == m_trainingSide;
        message = userWon ? tr("Checkmate: you won the training game.")
                          : tr("Checkmate: the engine won the training game.");
    }
    m_session->setHeader(game);
    saveGameToDatabase(); // Does nothing once the game is in the database.
    statusBar()->showMessage(message, 8000);
}

void MainWindow::playBoardMove(int from, int to, const QPoint &globalPosition)
{
    QList<ChessMove> candidates;
    for (const ChessMove &move : m_session->position().legalMoves()) {
        if (move.from == from && move.to == to)
            candidates << move;
    }
    if (candidates.isEmpty())
        return;

    ChessMove move = candidates.first();
    if (candidates.size() > 1) {
        QMenu menu(this);
        menu.setAccessibleName(tr("Promote to"));
        const std::pair<PieceType, QString> pieces[] = {{PieceType::Queen, tr("Queen")},
                                                        {PieceType::Rook, tr("Rook")},
                                                        {PieceType::Bishop, tr("Bishop")},
                                                        {PieceType::Knight, tr("Knight")}};
        for (const auto &[type, name] : pieces)
            menu.addAction(name)->setData(int(type));
        const QAction *chosen = menu.exec(globalPosition);
        if (!chosen)
            return;
        move.promotion = PieceType(chosen->data().toInt());
    }

    playMove(move);
}

void MainWindow::playMove(const ChessMove &move)
{
    if (!m_session->isNextMove(move) && m_openGameIndex >= 0) {
        // Never rewrite a stored game: the new line becomes a game of its own.
        m_openGameIndex = -1;
        m_gameView->clearSelection();
        statusBar()->showMessage(tr("The game in the database is unchanged. Use Game ▸ Save Game to "
                                    "Database to keep this line."), 8000);
    }
    m_session->playMove(move);
}

void MainWindow::saveGameToDatabase()
{
    if (!m_database || m_openGameIndex >= 0)
        return;
    QString error;
    const qint64 index = m_database->addGame(m_session->game(), &error);
    if (index < 0) {
        QMessageBox::warning(this, tr("Save Game"), tr("Could not save the game: %1").arg(error));
        return;
    }

    m_gameListModel->setDatabase(m_database.get());
    m_openGameIndex = index;
    const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0));
    if (proxyIndex.isValid()) {
        m_gameView->selectRow(proxyIndex.row());
        m_gameView->scrollTo(proxyIndex);
    }
    m_session->setHeader(m_database->header(index));
    updateGameCount();
    updateGameActions();
    scheduleSaveSession();
    statusBar()->showMessage(tr("Game saved to %1").arg(m_database->name()), 3000);
}

void MainWindow::updateGameActions()
{
    m_saveGameAction->setEnabled(m_database && m_openGameIndex < 0 && m_session->plyCount() > 0);
}

void MainWindow::updateGameHeader()
{
    // Games not in the database (new games, pasted positions) are edited in memory.
    const bool editable = m_openGameIndex < 0 || (m_database && m_openGameIndex < m_database->gameCount());
    m_gameHeader->setGame(m_session->game(), editable);
}

void MainWindow::editGameInfo()
{
    const bool inDatabase = m_openGameIndex >= 0;
    if (inDatabase && (!m_database || m_openGameIndex >= m_database->gameCount()))
        return;

    GameInfoDialog dialog(m_session->game(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const GameRecord edited = dialog.game();
    if (!inDatabase) {
        m_session->setHeader(edited);
        return;
    }
    QString error;
    if (!m_database->updateHeader(m_openGameIndex, edited, &error)) {
        QMessageBox::warning(this, tr("Game Information"),
                             tr("Could not save the game information: %1").arg(error));
        return;
    }
    m_gameListModel->refreshRow(int(m_openGameIndex));
    m_session->setHeader(m_database->header(m_openGameIndex));
}

void MainWindow::copyFen()
{
    QGuiApplication::clipboard()->setText(m_session->position().fen());
    statusBar()->showMessage(tr("FEN copied to clipboard"), 3000);
}

void MainWindow::copyText(const QString &text, const QString &message)
{
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text);
    statusBar()->showMessage(message, 3000);
}

void MainWindow::pasteFen()
{
    const QString text = QGuiApplication::clipboard()->text().trimmed();
    if (!ChessPosition::fromFen(text)) {
        statusBar()->showMessage(tr("The clipboard does not contain a valid FEN"), 5000);
        return;
    }
    GameRecord game;
    game.startFen = text;
    m_gameView->clearSelection();
    m_openGameIndex = -1;
    m_session->setGame(game);
}

void MainWindow::applyDefaultLayout()
{
    const QList<QDockWidget *> right{m_movesDock, m_openingTreeDock, m_engineDock};
    for (QDockWidget *dock : {m_movesDock, m_gamesDock, m_openingTreeDock, m_engineDock})
        dock->setFloating(false);
    addDockWidget(Qt::BottomDockWidgetArea, m_gamesDock);
    for (QDockWidget *dock : right)
        m_sidebar->addDockWidget(Qt::RightDockWidgetArea, dock);
    m_sidebar->splitDockWidget(m_movesDock, m_openingTreeDock, Qt::Horizontal);

    m_openingTreeDock->hide();
    m_gamesDock->show();
    m_movesDock->show();
    m_engineDock->show();
    m_sidebar->resizeDocks({m_movesDock, m_engineDock}, {3, 1}, Qt::Vertical);
    resizeDocks({m_gamesDock}, {220}, Qt::Vertical);
    m_gamesSplitter->setSizes({220, 800});
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About Pragma Chess"),
        tr("<h3>Pragma Chess %1</h3>"
           "<p>An open source chess database engine with a native desktop client.</p>"
           "<p>Licensed under the MIT License.</p>").arg(QString::fromLatin1(APP_VERSION)));
}

void MainWindow::restoreSession()
{
    m_restoringSession = true;
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry))
        resize(1280, 820);
    // A maximized state set before the window is mapped does not survive
    // mapping (Qt's xcb plugin reads back the state the window manager has not
    // applied yet): start normal and maximize once shown (showEvent).
    m_restoredWindowState = windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen);
    if (m_restoredWindowState != Qt::WindowNoState)
        setWindowState(windowState() & ~m_restoredWindowState);

    QHeaderView *gameHeader = m_gameView->horizontalHeader();
    if (gameHeader->restoreState(settings.value(QStringLiteral("games/header")).toByteArray()))
        m_gameView->sortByColumn(gameHeader->sortIndicatorSection(), gameHeader->sortIndicatorOrder());

    const QString yaml = settings.value(QStringLiteral("session/project")).toString();
    std::optional<Project> project = yaml.isEmpty() ? std::nullopt : Project::fromYaml(yaml, QDir(), nullptr);
    if (project) {
        applyProject(*project, false);
    } else {
        Project first; // First launch: default layout, first game of the default database.
        first.layout = saveLayout();
        applyProject(first, true);
    }

    // Reattach to the project file the session belonged to, if it still exists.
    m_projectPath = settings.value(QStringLiteral("session/projectPath")).toString();
    if (!m_projectPath.isEmpty()) {
        if (std::optional<Project> saved = Project::loadFromFile(m_projectPath, nullptr))
            m_savedProjectYaml = saved->toYaml();
        else
            m_projectPath.clear();
    }

    m_restoringSession = false;
    updateWindowTitle();
    updateProjectModified();
}

void MainWindow::saveSession()
{
    if (m_restoringSession)
        return;
    m_saveTimer->stop();

    QSettings settings;
    // Once closed, the window manager may have dropped the maximized state:
    // keep the geometry saved while the window was still shown.
    if (isVisible())
        settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("games/header"), m_gameView->horizontalHeader()->saveState());
    settings.setValue(QStringLiteral("session/project"), captureProject().toYaml());
    settings.setValue(QStringLiteral("session/projectPath"), m_projectPath);
    // Superseded by session/project.
    // "workspaces" and "games/splitter" belong to the project (.pch) now.
    for (const char *key : {"window/state", "board", "games/search", "session/databasePath",
                            "session/gameIndex", "session/startFen", "session/ply",
                            "games/splitter", "workspaces"})
        settings.remove(QString::fromLatin1(key));

    updateProjectModified();
}

Project MainWindow::captureProject() const
{
    Project project;
    if (m_database) {
        project.databasePath = m_database->location();
        if (m_openGameIndex >= 0 && m_openGameIndex < m_database->gameCount())
            project.gameId = m_database->header(m_openGameIndex).id;
    }
    project.ply = m_session->ply();
    if (project.gameId < 0) {
        project.startFen = m_session->game().startFen;
        for (const MoveRecord &move : m_session->game().moves)
            project.moves << move.uci;
    }
    project.boardFlipped = m_flipBoardAction->isChecked();
    project.showCoordinates = m_coordinatesAction->isChecked();
    project.engineName = m_engineName;
    project.engineAnalyzing = m_startEngineAction->isChecked();
    project.layout = saveLayout();
    return project;
}

void MainWindow::applyProject(const Project &project, bool openFirstGameIfNone)
{
    const bool wasRestoring = m_restoringSession;
    m_restoringSession = true;

    if (!project.layout.isEmpty())
        restoreLayout(project.layout);
    m_flipBoardAction->setChecked(project.boardFlipped);
    m_coordinatesAction->setChecked(project.showCoordinates);
    m_engineName = project.engineName;

    openInitialDatabase(project.databasePath);

    bool opened = false;
    // A project without a database path refers to whichever default database was opened.
    const bool sameDatabase = m_database
        && (project.databasePath.isEmpty() || m_database->location() == project.databasePath);
    if (sameDatabase && project.gameId >= 0) {
        for (qint64 index = 0; index < m_database->gameCount() && !opened; ++index) {
            if (m_database->header(index).id != project.gameId)
                continue;
            const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(index), 0));
            if (proxyIndex.isValid()) {
                openGame(proxyIndex);
            } else if (std::optional<GameRecord> game = m_database->loadGame(index)) {
                // Not shown in the games list: open it anyway.
                m_gameView->clearSelection();
                m_openGameIndex = index;
                m_session->setGame(*game);
            }
            opened = true;
        }
    }
    const bool unsavedGame = project.startFen.isEmpty() ? !project.moves.isEmpty()
                                                        : ChessPosition::fromFen(project.startFen).has_value();
    if (!opened && project.gameId < 0 && unsavedGame) {
        GameRecord game;
        game.startFen = project.startFen;
        game.result = QStringLiteral("*");
        for (const QString &uci : project.moves)
            game.moves << MoveRecord{QString(), uci}; // SAN is filled in when the game is opened.
        m_gameView->clearSelection();
        m_openGameIndex = -1;
        m_session->setGame(game);
        opened = true;
    }
    if (!opened) {
        m_gameView->clearSelection();
        m_openGameIndex = -1;
        if (openFirstGameIfNone && m_gameListProxy->rowCount() > 0)
            openGame(m_gameListProxy->index(0, 0));
        else
            m_session->setGame(GameRecord());
    }
    m_session->goToPly(project.ply);
    m_startEngineAction->setChecked(project.engineAnalyzing);

    m_restoringSession = wasRestoring;
}

void MainWindow::newProject()
{
    if (!maybeSaveProject())
        return;

    Project project; // Same database, starting position, default layout.
    if (m_database)
        project.databasePath = m_database->location();
    applyDefaultLayout();
    project.layout = saveLayout();
    applyProject(project, false);

    m_projectPath.clear();
    m_savedProjectYaml.clear();
    updateWindowTitle();
    updateProjectModified();
    scheduleSaveSession();
}

void MainWindow::openProject()
{
    if (!maybeSaveProject())
        return;
    UserFolders::ensureProjectsDir();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Project"), UserFolders::projectsDir(),
        tr("Pragma Chess projects (*.pch);;All files (*)"));
    if (!path.isEmpty())
        openProjectFile(path);
}

bool MainWindow::openProjectFile(const QString &path)
{
    QString error;
    std::optional<Project> project = Project::loadFromFile(path, &error);
    if (!project) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("Could not open “%1”: %2").arg(QFileInfo(path).fileName(), error));
        return false;
    }

    applyProject(*project, false);
    if (!project->databasePath.isEmpty() && (!m_database || m_database->location() != project->databasePath)) {
        QMessageBox::warning(this, tr("Open Project"),
                             tr("The database “%1” used by this project could not be opened.")
                                 .arg(QDir::toNativeSeparators(project->databasePath)));
    }

    m_projectPath = QFileInfo(path).absoluteFilePath();
    // Compare against what was actually applied, so the project doesn't show
    // as modified just because the layout was normalized on restore.
    m_savedProjectYaml = captureProject().toYaml();
    addRecentProject(m_projectPath);
    updateWindowTitle();
    updateProjectModified();
    scheduleSaveSession();
    return true;
}

bool MainWindow::saveProject()
{
    if (m_projectPath.isEmpty())
        return saveProjectAs();

    const Project project = captureProject();
    QString error;
    if (!project.saveToFile(m_projectPath, &error)) {
        QMessageBox::warning(this, tr("Save Project"),
                             tr("Could not save “%1”: %2").arg(QFileInfo(m_projectPath).fileName(), error));
        return false;
    }
    m_savedProjectYaml = project.toYaml();
    addRecentProject(m_projectPath);
    updateProjectModified();
    scheduleSaveSession();
    statusBar()->showMessage(tr("Saved %1").arg(QDir::toNativeSeparators(m_projectPath)), 3000);
    return true;
}

bool MainWindow::saveProjectAs()
{
    UserFolders::ensureProjectsDir();
    const QString suffix = QLatin1String(Project::fileSuffix);
    const QString name = m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).completeBaseName();
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Project As"),
        QDir(UserFolders::projectsDir()).filePath(name + QLatin1Char('.') + suffix),
        tr("Pragma Chess projects (*.pch)"));
    if (path.isEmpty())
        return false;
    if (QFileInfo(path).suffix().compare(suffix, Qt::CaseInsensitive) != 0)
        path += QLatin1Char('.') + suffix;

    const QString previous = m_projectPath;
    m_projectPath = QFileInfo(path).absoluteFilePath();
    if (!saveProject()) {
        m_projectPath = previous;
        return false;
    }
    updateWindowTitle();
    return true;
}

bool MainWindow::maybeSaveProject()
{
    if (m_projectPath.isEmpty() || !isWindowModified())
        return true;
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Save Project"),
        tr("Save changes to “%1” before closing it?").arg(QFileInfo(m_projectPath).completeBaseName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save)
        return saveProject();
    return answer == QMessageBox::Discard;
}

void MainWindow::addRecentProject(const QString &path)
{
    QSettings settings;
    QStringList recent = settings.value(QStringLiteral("recentProjects")).toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10)
        recent.removeLast();
    settings.setValue(QStringLiteral("recentProjects"), recent);
}

void MainWindow::rebuildRecentProjectsMenu()
{
    m_recentProjectsMenu->clear();
    QSettings settings;
    const QStringList recent = settings.value(QStringLiteral("recentProjects")).toStringList();
    for (const QString &path : recent) {
        if (!QFileInfo::exists(path))
            continue;
        QAction *action = m_recentProjectsMenu->addAction(QFileInfo(path).completeBaseName());
        action->setToolTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] {
            if (path != m_projectPath && maybeSaveProject())
                openProjectFile(path);
        });
    }
    if (m_recentProjectsMenu->isEmpty()) {
        m_recentProjectsMenu->addAction(tr("No Recent Projects"))->setEnabled(false);
        return;
    }
    m_recentProjectsMenu->addSeparator();
    m_recentProjectsMenu->addAction(tr("Clear Recent Projects"), this, [] {
        QSettings().remove(QStringLiteral("recentProjects"));
    });
}

void MainWindow::updateWindowTitle()
{
    const GameRecord &game = m_session->game();
    const QString gameTitle = game.white.isEmpty() && game.black.isEmpty()
        ? tr("Position")
        : tr("%1 – %2").arg(game.white, game.black);
    if (m_projectPath.isEmpty())
        setWindowTitle(gameTitle);
    else
        setWindowTitle(tr("%1[*] — %2").arg(QFileInfo(m_projectPath).completeBaseName(), gameTitle));
}

void MainWindow::updateProjectModified()
{
    const bool modified = !m_projectPath.isEmpty() && captureProject().toYaml() != m_savedProjectYaml;
    setWindowModified(modified);
    m_saveProjectAction->setEnabled(m_projectPath.isEmpty() || modified);
}

void MainWindow::scheduleSaveSession()
{
    if (m_saveTimer && !m_restoringSession)
        m_saveTimer->start();
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    scheduleSaveSession();
}

void MainWindow::updateSeparatorStyle()
{
    // A solid line along the whole separator, highlighted while hovered.
    QColor line = palette().color(QPalette::WindowText);
    line.setAlphaF(0.3);
    const QColor hover = palette().color(QPalette::Highlight);
    const auto rgba = [](const QColor &color) {
        return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
    };
    const QString style = QStringLiteral("QMainWindow::separator { background: %1; width: 3px; height: 3px; }"
                                         "QMainWindow::separator:hover { background: %2; }")
                              .arg(rgba(line), rgba(hover));
    for (QMainWindow *window : {static_cast<QMainWindow *>(this), m_sidebar}) {
        if (window->styleSheet() != style)
            window->setStyleSheet(style);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        updateSeparatorStyle();
    else if (event->type() == QEvent::WindowStateChange)
        scheduleSaveSession();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_restoredWindowState == Qt::WindowNoState)
        return;
    const Qt::WindowStates state = std::exchange(m_restoredWindowState, Qt::WindowNoState);
    // Once the window manager has mapped the window, so it keeps the state.
    QTimer::singleShot(kRestoreWindowStateDelayMs, this, [this, state] {
        if ((windowState() & state) != state)
            setWindowState(windowState() | state);
    });
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    scheduleSaveSession();
}

void MainWindow::quitWithoutAsking()
{
    m_forcedQuit = true;
    close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_forcedQuit || m_closingAfterSync) { // Nothing left to ask.
        saveSession();
        event->accept();
        return;
    }

    SyncSettings settings = SyncSettings::load();
    if (!m_projectPath.isEmpty() && isWindowModified()) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Quit Pragma Chess"));
        box.setIcon(QMessageBox::Question);
        box.setText(tr("Save changes to “%1” before quitting?")
                        .arg(QFileInfo(m_projectPath).completeBaseName()));
        box.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Save);
        // Remembered, so the choice only has to be made once.
        auto *syncFirst = new QCheckBox(tr("Sync before closing"));
        syncFirst->setChecked(settings.syncBeforeClosing);
        box.setCheckBox(syncFirst);

        const int answer = box.exec();
        if (syncFirst->isChecked() != settings.syncBeforeClosing) {
            settings.syncBeforeClosing = syncFirst->isChecked();
            settings.save();
        }
        if (answer == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        if (answer == QMessageBox::Save && !saveProject()) {
            event->ignore();
            return;
        }
    }

    if (!settings.syncBeforeClosing) {
        saveSession();
        event->accept();
        return;
    }

    // Close once everything is synced, and anyway if the server keeps us waiting.
    event->ignore();
    m_closingAfterSync = true;
    statusBar()->showMessage(tr("Syncing before closing…"));
    QTimer::singleShot(kCloseSyncTimeoutMs, this, [this] {
        if (m_closingAfterSync)
            close();
    });
    syncNow([this] { close(); });
}
