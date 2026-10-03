#include "MainWindow.h"

#include "app/BookWeights.h"
#include "app/ClassicGames.h"
#include "app/GameIdentity.h"
#include "app/OpeningNames.h"
#include "app/DatabaseMerge.h"
#include "app/ShippedOpeningNames.h"
#include "dialogs/AboutDialog.h"
#include "dialogs/BoardSettingsDialog.h"
#include "dialogs/DatabaseSettingsDialog.h"
#include "app/PolyglotBook.h"
#include "app/PositionIndexBuilder.h"
#include "app/DatabaseOutline.h"
#include "dialogs/ConnectSourceWizard.h"
#include "dialogs/GameInfoDialog.h"
#include "dialogs/HelpDialog.h"
#include "dialogs/ManageEnginesDialog.h"
#include "dialogs/ManageSourcesDialog.h"
#include "dialogs/NewTrainingDialog.h"
#include "dialogs/SyncDialog.h"
#ifdef PRAGMA_HAS_PHONE_LINK
#include "app/phone/DatabaseFolderStore.h"
#include "app/phone/PhoneLink.h"
#include "dialogs/ConnectMobileDialog.h"
#endif
#include "app/Explainer.h"
#include "app/GameSession.h"
#include "app/GameVariations.h"
#include "app/MoveAnnotation.h"
#include "app/Pgn.h"
#include "app/Project.h"
#include "app/SqliteGameDatabase.h"
#include "app/UiLanguage.h"
#include "app/UciEngine.h"
#include "app/UserFolders.h"
#include "app/sources/ChessBaseFetch.h"
#include "app/sources/SourceCatalog.h"
#include "app/sources/SourceCredentials.h"
#include "app/sources/SourceSync.h"
#include "app/sync/FolderSync.h"
#include "app/sync/RemoteStore.h"
#include "app/sync/SyncPipeline.h"
#include "app/sync/SyncTasks.h"
#include "models/GameFilterProxyModel.h"
#include "models/GameListModel.h"
#include "platform/SymbolicIcons.h"
#include "platform/WindowChrome.h"
#include "widgets/BoardPanel.h"
#include "widgets/BookPanel.h"
#include "widgets/PaddedHeaderView.h"
#include "widgets/PaddedItemDelegate.h"
#include "widgets/BoardWidget.h"
#include "widgets/BoardSideColumn.h"
#include "widgets/CapturedPiecesWidget.h"
#include "widgets/PaddedStatusBar.h"
#include "widgets/CentralArea.h"
#include "widgets/DatabaseTreeWidget.h"
#include "widgets/EnginePanel.h"
#include "widgets/EvaluationBar.h"
#include "widgets/FigurineFont.h"
#include "widgets/GameHeaderWidget.h"
#include "widgets/GlyphMenuAction.h"
#include "widgets/MoveTreeView.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDataStream>
#include <QActionGroup>
#include <QDesktopServices>
#include <QDateTime>
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
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

/// Delay before maximizing a window restored maximized, once it is mapped.
constexpr int kRestoreWindowStateDelayMs = 250;

/// Depth of the search that picks the engine's move in training: deep
/// enough to play well, shallow enough to answer at once.
constexpr int kTrainingDepth = 12;
/// The tutor only judges a move against an evaluation at least this deep.
constexpr int kTutorMinDepth = 8;

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
    , m_board(new BoardWidget)
    , m_sidebar(new QMainWindow)
    , m_evaluationBar(new EvaluationBar)
    , m_gameHeader(new GameHeaderWidget)
    , m_capturedPieces(new CapturedPiecesWidget)
    , m_boardSideColumn(new BoardSideColumn)
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
    m_boardPanel = new BoardPanel(m_board, m_evaluationBar, m_gameHeader, m_capturedPieces, m_boardSideColumn,
                                  {m_firstMoveAction, m_previousMoveAction, m_explainAction,
                                   m_nextMoveAction, m_lastMoveAction, m_flipBoardAction});
    applyBoardSettings(BoardSettings::load());
    setCentralWidget(new CentralArea(m_boardPanel, m_sidebar));
    createDocks();
    createToolBar();
    createMenus();
    // The toolbar's book drops down the choice alone: the books and No Book.
    auto *bookChoices = new QMenu(this);
    connect(bookChoices, &QMenu::aboutToShow, this, [this, bookChoices] {
        bookChoices->clear();
        fillBookChoices(bookChoices);
    });
    m_bookButton->setMenu(bookChoices);
    m_engineButton->setMenu(m_engineChoiceMenu);
    m_databaseButton->setMenu(m_databasesMenu);
    updateResourceButtons();
    createStatusBar();
    updateSeparatorStyle();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateGameHeader);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateGameActions);
    connect(m_session, &GameSession::headerChanged, this, &MainWindow::updateGameHeader);
    connect(m_gameHeader, &GameHeaderWidget::activated, this, &MainWindow::editGameInfo);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::syncBoard);
    // Before the analysis: in training the engine's own search replaces it.
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateTraining);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::analyzeCurrentPosition);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateBookMoves);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::updateBookMoves);
    m_positionIndex = new PositionIndexBuilder(this);
    m_positionIndexTimer = new QTimer(this);
    m_positionIndexTimer->setSingleShot(true);
    m_positionIndexTimer->setInterval(1000);
    connect(m_positionIndexTimer, &QTimer::timeout, this, &MainWindow::rebuildPositionIndex);
    connect(m_positionIndex, &PositionIndexBuilder::indexChanged, this, &MainWindow::updateBoardFilters);
    connect(m_positionIndex, &PositionIndexBuilder::indexChanged, this, &MainWindow::updateBookDatabaseStats);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::updateBoardFilters);
    connect(m_engine, &UciEngine::searchFinished, this, &MainWindow::finishEngineMove);
    connect(m_session, &GameSession::gameChanged, this, &MainWindow::clearTutor);
    connect(m_engine, &UciEngine::evaluationChanged, this, [this](const EngineEvaluation &evaluation) {
        m_lastEvaluation = evaluation;
        // While the user thinks in training, the analysis of their position
        // is what the tutor will judge their move against.
        if (m_trainingModeAction->isChecked() && !m_trainingThinking && !m_tutorReply && !isEngineTurn()) {
            const QString fen = m_session->position().fen();
            if (m_trainingBaselineFen != fen || evaluation.depth >= m_trainingBaseline.depth) {
                m_trainingBaseline = evaluation;
                m_trainingBaselineFen = fen;
            }
        }
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
        // The border stops breathing and turns blue, but only for an answer.
        const bool explained = !explanation.summary.isEmpty() || !explanation.arrows.isEmpty();
        m_explainBorder = explained ? BoardBorder::Explained : BoardBorder::Plain;
        updateBoardBorder();
    });
    connect(m_engine, &UciEngine::nameChanged, m_enginePanel, &EnginePanel::setEngineName);
    connect(m_engine, &UciEngine::failed, this, [this](const QString &message) {
        m_startEngineAction->setChecked(false);
        m_enginePanel->setStatus(tr("The engine stopped: %1").arg(message));
    });
    connect(m_board, &BoardWidget::navigateRequested, this,
            [this](int steps) { m_session->goToPly(m_session->ply() + steps); });
    connect(m_board, &BoardWidget::moveRequested, this, &MainWindow::playBoardMove);
    connect(m_sourceSync, &SourceSync::gamesImported, this, &MainWindow::showAddedGames);
    connect(m_sourceSync, &SourceSync::sourcesChanged, m_databaseTree, &DatabaseTreeWidget::scheduleRefresh);
    connect(m_sourceSync, &SourceSync::sourceUnavailable, this, &MainWindow::reportUnavailableSource);

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

    // Duplicate databases are merged into one, and the merge reaches the other devices.
    m_folderSync->setDatabaseHooks(DatabaseMerge::hooks([](const QString &relative) {
        const QFileInfo file(QDir(UserFolders::pragmaDir()).filePath(relative));
        for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
            if (file == QFileInfo(QDir(UserFolders::openingNamesDir()).filePath(names.fileName)))
                return true;
        }
        return false;
    }));
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
    // An open database merged into another: the other one is opened instead.
    connect(m_folderSync, &FolderSync::localFileMerged, this, [this](const QString &from, const QString &into) {
        // The opening names follow their database too.
        if (!m_openingNamesPath.isEmpty() && QFileInfo(m_openingNamesPath) == QFileInfo(from))
            chooseOpeningNames(into);
        if (m_reopenAfterSync.isEmpty() || QFileInfo(m_reopenAfterSync) != QFileInfo(from))
            return;
        m_reopenAfterSync = into;
        m_reopenGameId = -1;
        m_reopenPly = 0;
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
    migrateOpeningNames(); // Before the session, which may have the moved database open.
    restoreSession();
    adoptShippedLineages();
    restoreOpeningNames(); // After the session, so the first launch still seeds Classic Games first.
    applySyncSettings();
    createPhoneLink();

    connect(m_session, &GameSession::gameChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_session, &GameSession::plyChanged, this, &MainWindow::scheduleSaveSession);
    connect(m_flipBoardAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_coordinatesAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_startEngineAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
    connect(m_trainingModeAction, &QAction::toggled, this, &MainWindow::scheduleSaveSession);
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

MainWindow::~MainWindow()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    delete m_phoneLink; // Before the game store it uses.
#endif
}

void MainWindow::showAddedGames()
{
    m_gameListModel->refreshAppended();
    if (m_filterSource != 0)
        showCategory({GameCategory::Kind::Source, QString(), m_filterSource}); // Its new games too.
    updateGameCount();
    m_databaseTree->scheduleRefresh();
    m_positionIndexTimer->start();
}

void MainWindow::createPhoneLink()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    m_phoneGameStore = std::make_unique<DatabaseFolderStore>();
    // Games for the open database go through it, on this thread, so its game
    // list stays right; other databases are opened on their own.
    m_phoneGameStore->setOpenDatabase([this] { return m_database.get(); },
                                      [this](int added, const QList<qint64> &updated) {
                                          for (const qint64 index : updated)
                                              m_gameListModel->refreshRow(int(index));
                                          if (added > 0)
                                              showAddedGames();
                                          else if (!updated.isEmpty())
                                              m_positionIndexTimer->start(); // Their moves may differ.
                                      });
    // A sync may be replacing the files: the phone sends its games next time.
    m_phoneGameStore->setBusy([this] { return m_syncPipeline->isRunning(); });
    const QString state = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                              .filePath(QStringLiteral("phone-link.json"));
    m_phoneLink = new PhoneLink(state, UserFolders::databasesDir(), this);
    m_phoneLink->setGameStore(m_phoneGameStore.get());
#endif
}

void MainWindow::openConnectMobileDialog()
{
#ifdef PRAGMA_HAS_PHONE_LINK
    if (!m_phoneLink)
        return;
    UserFolders::ensureDatabasesDir();
    ConnectMobileDialog dialog(m_phoneLink, this);
    dialog.exec();
#endif
}

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
    m_syncNowAction->setToolTip(tr("Sync Everything…"));
    connect(m_syncNowAction, &QAction::triggered, this, [this] { syncNow(); });

#ifdef PRAGMA_HAS_PHONE_LINK
    m_connectMobileAction = new QAction(tr("Connect &Mobile App…"), this);
    m_connectMobileAction->setToolTip(tr("Copy your databases to the Pragma Chess app on your phone"));
    connect(m_connectMobileAction, &QAction::triggered, this, &MainWindow::openConnectMobileDialog);
#endif
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

    m_databaseSettingsAction = new QAction(themeIcon("document-properties", QStyle::SP_FileDialogInfoView),
                                           tr("Database Se&ttings…"), this);
    m_databaseSettingsAction->setToolTip(tr("Type and description of the open database"));
    connect(m_databaseSettingsAction, &QAction::triggered, this, &MainWindow::editDatabaseSettings);

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
    connect(m_flipBoardAction, &QAction::toggled, m_boardSideColumn, &BoardSideColumn::setFlipped);

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
    connect(m_newTrainingAction, &QAction::triggered, this, [this] { newTraining(true); });
    // The toolbar's button skips the question once an answer was remembered.
    m_quickTrainingAction = new QAction(m_newTrainingAction->icon(), m_newTrainingAction->text(), this);
    m_quickTrainingAction->setToolTip(m_newTrainingAction->toolTip());
    connect(m_quickTrainingAction, &QAction::triggered, this, [this] { newTraining(false); });

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

    m_guideAction = new QAction(tr("Pragma Chess &Guide"), this);
    m_guideAction->setShortcut(QKeySequence::HelpContents);
    connect(m_guideAction, &QAction::triggered, this, &MainWindow::showGuide);
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
        copyText(moveText(m_session->ply()), tr("Move copied"));
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
    for (QDockWidget *dock : {m_movesDock, m_openingTreeDock, m_engineDock, m_gamesDock})
        m_viewMenu->addAction(dock->toggleViewAction());
    m_gamesDock->toggleViewAction()->setText(tr("Games &List")); // The panel is the tree and the list, last in the menu.
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

    m_bookMenu = menuBar()->addMenu(tr("&Book"));
    connect(m_bookMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildBookMenu);
    rebuildBookMenu(); // Keeps the menu non-empty, so it shows on every platform.

    QMenu *engine = menuBar()->addMenu(tr("E&ngine"));
    engine->addAction(m_startEngineAction);
    engine->addAction(m_explainAction);
    engine->addSeparator();
    m_engineChoiceMenu = engine->addMenu(tr("&Use Engine"));
    connect(m_engineChoiceMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildEngineChoiceMenu);
    rebuildEngineChoiceMenu();
    engine->addAction(tr("&Manage Engines…"), this, &MainWindow::manageEngines);
    engine->addSeparator();
    engine->addAction(m_trainingModeAction);

    QMenu *database = menuBar()->addMenu(tr("&Database"));
    database->addAction(m_newDatabaseAction);
    database->addAction(m_openDatabaseAction);
    m_databasesMenu = database->addMenu(themeIcon("folder", QStyle::SP_DirIcon), tr("&Databases"));
    connect(m_databasesMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildDatabasesMenu);
    database->addSeparator();
    database->addAction(m_connectSourceAction);
    database->addAction(m_manageSourcesAction);
    database->addSeparator();
    database->addAction(m_databaseSettingsAction);
    database->addSeparator();
    database->addAction(m_saveDatabaseAction);
    database->addAction(m_saveDatabaseAsAction);

    menuBar()->addMenu(tr("&Tools"))->setEnabled(false);

    QMenu *options = menuBar()->addMenu(tr("&Options"));
    if (m_connectMobileAction) {
        options->addAction(m_connectMobileAction);
        options->addSeparator();
    }
    options->addAction(tr("&Board Settings…"), this, &MainWindow::editBoardSettings);
    m_openingNamesMenu = options->addMenu(tr("Opening &Names"));
    m_openingNamesMenu->setToolTip(tr("The database whose games name the openings and variations"));
    connect(m_openingNamesMenu, &QMenu::aboutToShow, this, &MainWindow::rebuildOpeningNamesMenu);
    rebuildOpeningNamesMenu();
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
    help->addAction(m_guideAction);
    help->addSeparator();
    help->addAction(m_aboutAction);
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
    // Saving: the project for now (the floppy).
    toolBar->addAction(m_saveProjectAction);
    toolBar->addSeparator();
    toolBar->addAction(m_newGameAction);
    toolBar->addAction(m_quickTrainingAction);
    // Some air around each icon: bigger buttons to aim at, the same icons.
    constexpr int kButtonPadding = 4;
    // The book, the engine and the database in use: an icon each, which
    // drops down the list to choose another (the menus of the menu bar, set
    // in createMenus()); the tooltip says which one is in use.
    toolBar->addSeparator();
    const auto resourceButton = [toolBar](const char *icon) {
        auto *button = new QToolButton(toolBar);
        button->setIcon(themeIcon(icon, QStyle::SP_FileIcon));
        button->setPopupMode(QToolButton::InstantPopup);
        button->setFocusPolicy(Qt::NoFocus);
        toolBar->addWidget(button);
        return button;
    };
    m_bookButton = resourceButton("pragma-book");
    m_engineButton = resourceButton("pragma-engine");
    m_databaseButton = resourceButton("pragma-database");

    for (QAction *action : toolBar->actions()) {
        if (QWidget *button = toolBar->widgetForAction(action); button && !action->isSeparator())
            button->setMinimumSize(button->sizeHint() + 2 * QSize(kButtonPadding, kButtonPadding));
    }
}

void MainWindow::updateResourceButtons()
{
    if (!m_bookButton) // Called while the window is still being put together.
        return;
    const QString book = m_book ? QFileInfo(m_book->path()).completeBaseName() : tr("No Book");
    m_bookButton->setToolTip(tr("Opening book: %1").arg(book));
    const QString engine = m_engines.resolve(m_engineId, m_engineName).name;
    m_engineButton->setToolTip(tr("Engine: %1").arg(engine));
    const QString database = m_database ? m_database->name() : tr("No Database");
    m_databaseButton->setToolTip(tr("Database: %1").arg(database));
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
    m_moveView = new MoveTreeView(m_session);
    connect(m_moveView, &MoveTreeView::moveActivated, m_session, &GameSession::goToLine);
    m_moveView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_moveView, &QWidget::customContextMenuRequested, this, &MainWindow::showMoveListMenu);
    m_movesDock = addDock(m_sidebar, QStringLiteral("movesDock"), tr("Moves"), m_moveView, Qt::RightDockWidgetArea);

    m_enginePanel = new EnginePanel(m_startEngineAction);
    {
        QSettings settings;
        m_engines = EngineCatalog::load(settings);
    }
    m_enginePanel->setEngineName(m_engines.resolve(m_engineId, m_engineName).name);
    connect(m_enginePanel, &EnginePanel::takeBackRequested, this, &MainWindow::takeBackTutorMove);
    connect(m_enginePanel, &EnginePanel::explainRequested, this, [this] { m_explainAction->setChecked(true); });
    connect(m_enginePanel, &EnginePanel::ignoreRequested, this, &MainWindow::ignoreTutorAlert);
    m_engineDock = addDock(m_sidebar, QStringLiteral("engineDock"), tr("Engine"), m_enginePanel, Qt::RightDockWidgetArea);

    m_bookPanel = new BookPanel;
    connect(m_bookPanel, &BookPanel::moveActivated, this, &MainWindow::playMove);
    connect(m_bookPanel, &BookPanel::backActivated, m_session, &GameSession::goBack);
    connect(m_bookPanel, &BookPanel::repertoireToggled, this, [this](const ChessMove &move, bool inRepertoire) {
        QString error;
        if (!m_book || !m_book->setInRepertoire(m_session->position(), move, inRepertoire, &error))
            QMessageBox::warning(this, tr("Repertoire"), tr("Could not change the book: %1").arg(error));
        updateBookMoves();
    });
    connect(m_bookPanel, &BookPanel::weightAdjustRequested, this, [this](const ChessMove &move, int percent) {
        // The shares of the position's moves shift and are written into the book in use.
        if (!m_book)
            return;
        const ChessPosition &position = m_session->position();
        QList<PolyglotBook::Move> moves = m_book->moves(position);
        QList<int> weights;
        int index = -1;
        for (int i = 0; i < moves.size(); ++i) {
            weights << moves.at(i).weight;
            if (moves.at(i).move == move)
                index = i;
        }
        if (index < 0)
            return;
        const QList<int> changed = percent == 0 ? BookWeights::zeroed(weights, index)
                                                : BookWeights::adjusted(weights, index, percent);
        if (changed == weights)
            return;
        for (int i = 0; i < moves.size(); ++i)
            moves[i].weight = changed.at(i);
        QString error;
        if (!m_book->setWeights(position, moves, &error))
            QMessageBox::warning(this, tr("Opening Book"), tr("Could not change the book: %1").arg(error));
        updateBookMoves();
    });
    m_openingTreeDock = addDock(m_sidebar, QStringLiteral("openingTreeDock"), tr("Opening Tree"), m_bookPanel,
                                Qt::RightDockWidgetArea);
    // The panels speak for themselves: no title bars (the names stay in the View menu).
    for (QDockWidget *dock : {m_movesDock, m_openingTreeDock, m_engineDock})
        dock->setTitleBarWidget(new QWidget(dock));
    // The opening tree sits right of the moves, with a separator to resize both.
    m_sidebar->splitDockWidget(m_movesDock, m_openingTreeDock, Qt::Horizontal);

    m_gameView = new QTableView;
    m_gameView->setModel(m_gameListProxy);
    m_gameView->setFont(FigurineFont::apply(m_gameView->font())); // The Line column shows moves.
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
    m_gameView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_gameView->horizontalHeader(), &QWidget::customContextMenuRequested, this,
            &MainWindow::showGameColumnsMenu);

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
    setStatusBar(new PaddedStatusBar);

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
    // The filter too: it reads the headers as soon as the list is reset.
    m_gameListProxy->setDatabase(nullptr);
    m_gameListModel->setDatabase(nullptr);
    m_database = std::move(database);
    m_gameListModel->setDatabase(m_database.get());
    m_gameView->resizeColumnsToContents();
    applyGameColumns();
    updateResourceButtons();
    m_openGameIndex = -1;

    updateGameCount();
    updateDatabaseActions();
    updateGameActions();
    m_filterSource = 0;
    m_category = {};
    m_gameListProxy->setDatabase(m_database.get());
    m_databaseTree->setDatabase(m_database.get());
    m_sourceSync->setDatabase(m_database.get());
    m_positionIndex->clear(); // Its games are another database's.
    rebuildPositionIndex();
}

void MainWindow::rebuildPositionIndex()
{
    m_positionIndexTimer->stop();
    if (m_database)
        m_positionIndex->build(m_database->gameLines());
    else
        m_positionIndex->clear();
    updateBoardFilters();
}

void MainWindow::updateBoardFilters()
{
    if (const PositionIndex *index = m_positionIndex->index()) {
        m_databaseTree->setBoardCounts(index->countWithPosition(m_session->position()),
                                       index->countWithLine(m_session->initialPosition(), m_session->movesToHere()));
    } else {
        m_databaseTree->setBoardCounts(-1, -1);
    }
    if (m_category.kind == GameCategory::Kind::Position || m_category.kind == GameCategory::Kind::Variant)
        showCategory(m_category);
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

    QMenu menu(this);
    if (!player.isEmpty()) {
        QMenu *who = menu.addMenu(tr("Who Is This?"));
        who->setToolTip(player);
        const PlayerRole current = m_database->playerRoles().value(player);
        auto *roles = new QActionGroup(who);
        const std::pair<PlayerRole, QString> choices[] = {{PlayerRole::Me, tr("It's &Me")},
                                                          {PlayerRole::Friend, tr("A &Friend")},
                                                          {PlayerRole::Opponent, tr("An &Opponent")}};
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
        menu.addSeparator();
    }
    // A game is only ever deleted from the trash, and even then it stays in
    // the file until the database is optimized.
    const QString uid = header.uid;
    if (header.state == GameState::Trashed) {
        menu.addAction(tr("&Restore Game"), this, [this, uid] { setGameState(uid, GameState::Live); });
        menu.addAction(tr("&Delete Game…"), this, [this, uid] { deleteGame(uid); });
    } else {
        menu.addAction(tr("Move Game to &Trash"), this, [this, uid] { setGameState(uid, GameState::Trashed); });
    }
    menu.exec(m_gameView->viewport()->mapToGlobal(position));
}

void MainWindow::applyGameColumns()
{
    const QStringList hidden = m_database ? m_database->properties().hiddenColumns : QStringList();
    for (int column = 0; column < GameListModel::ColumnCount; ++column)
        m_gameView->setColumnHidden(column, hidden.contains(GameListModel::columnKey(column)));
}

void MainWindow::setGameColumnsHidden(const QStringList &hidden)
{
    if (!m_database)
        return;
    DatabaseProperties properties = m_database->properties();
    properties.hiddenColumns = hidden;
    QString error;
    if (!m_database->setProperties(properties, &error)) {
        QMessageBox::warning(this, tr("Columns"), tr("Could not save the columns in the database: %1").arg(error));
        return;
    }
    applyGameColumns();
}

void MainWindow::showGameColumnsMenu(const QPoint &position)
{
    if (!m_database)
        return;
    const QHeaderView *header = m_gameView->horizontalHeader();
    const int column = header->logicalIndexAt(position);
    const QStringList hidden = m_database->properties().hiddenColumns;

    QMenu menu(this);
    QAction *hide = menu.addAction(column < 0 ? tr("&Hide") : tr("&Hide “%1”").arg(GameListModel::columnName(column)),
                                   this, [this, hidden, column] {
        setGameColumnsHidden(hidden + QStringList{GameListModel::columnKey(column)});
    });
    // The list never loses its last column.
    hide->setEnabled(column >= 0 && header->hiddenSectionCount() < GameListModel::ColumnCount - 1);

    QMenu *show = menu.addMenu(tr("&Show"));
    for (int hiddenColumn = 0; hiddenColumn < GameListModel::ColumnCount; ++hiddenColumn) {
        const QString key = GameListModel::columnKey(hiddenColumn);
        if (!hidden.contains(key))
            continue;
        show->addAction(GameListModel::columnName(hiddenColumn), this, [this, hidden, key] {
            QStringList remaining = hidden;
            remaining.removeAll(key);
            setGameColumnsHidden(remaining);
        });
    }
    if (show->actions().size() > 1) {
        show->addSeparator();
        show->addAction(tr("&All Columns"), this, [this] { setGameColumnsHidden({}); });
    }
    show->setEnabled(!show->isEmpty());
    menu.exec(header->viewport()->mapToGlobal(position));
}

QString MainWindow::moveText(int ply) const
{
    if (ply < 1 || ply > m_session->plyCount())
        return {};
    const MoveRecord &move = m_session->moveAt(ply);
    return m_session->positionAt(ply - 1).lineText({move.uci}) + MoveAnnotation::pgnSuffix(move.nags);
}

void MainWindow::showMoveListMenu(const QPoint &position)
{
    const MoveTreeView::Place place = m_moveView->placeAt(position);
    if (!place.isValid())
        return;
    // A move of another line is brought on the board first: the menu acts on the line followed.
    if (place.path != m_session->path())
        m_session->goToLine(place.path, place.ply);
    const int ply = place.ply;

    QMenu menu(this);
    QMenu *copy = menu.addMenu(themeIcon("edit-copy", QStyle::SP_FileIcon), tr("&Copy"));
    copy->addAction(tr("Copy &Move"), this, [this, ply] { copyText(moveText(ply), tr("Move copied")); });
    copy->addAction(tr("Copy &Line up to Here"), this, [this, ply] {
        copyText(Pgn::moveText(m_session->game(), ply), tr("Line copied"));
    });

    // Every glyph with what it means; choosing the one the move has takes it off.
    QMenu *annotations = menu.addMenu(tr("&Annotations"));
    const QList<int> current = m_session->moveAt(ply).nags;
    MoveAnnotation::Kind kind = MoveAnnotation::Kind::Move;
    for (const MoveAnnotation::Glyph &glyph : MoveAnnotation::glyphs()) {
        if (glyph.kind != kind)
            annotations->addSeparator();
        kind = glyph.kind;
        auto *action = new GlyphMenuAction(glyph.symbol, MoveAnnotation::meaning(glyph.nag), annotations);
        action->setCheckable(true);
        action->setChecked(current.contains(glyph.nag));
        connect(action, &QAction::triggered, this, [this, ply, current, nag = glyph.nag] {
            annotateMove(ply, MoveAnnotation::toggled(current, nag));
        });
        annotations->addAction(action);
    }
    annotations->addSeparator();
    auto *none = new GlyphMenuAction(QString(), tr("No Annotation"), annotations);
    none->setEnabled(!current.isEmpty());
    connect(none, &QAction::triggered, this, [this, ply] { annotateMove(ply, {}); });
    annotations->addAction(none);

    menu.exec(m_moveView->viewport()->mapToGlobal(position));
}

void MainWindow::annotateMove(int ply, const QList<int> &nags)
{
    if (ply < 1 || ply > m_session->plyCount())
        return;
    const QList<int> before = m_session->moveAt(ply).nags;
    m_session->setAnnotations(ply, nags);
    QString error;
    if (!storeOpenGame(&error)) {
        m_session->setAnnotations(ply, before);
        QMessageBox::warning(this, tr("Annotations"), tr("Could not save the annotation: %1").arg(error));
    }
}

bool MainWindow::storeOpenGame(QString *error)
{
    if (m_openGameIndex < 0) {
        scheduleSaveSession(); // Not in the database yet: the project keeps it.
        return true;
    }
    // A stored game keeps its moves, variations and annotations in the
    // database at once, as its header does.
    GameRecord game = m_session->game();
    game.modified.clear(); // Changed now.
    if (!m_database || m_openGameIndex >= m_database->gameCount()) {
        *error = tr("The game is no longer in the database.");
        return false;
    }
    if (m_database->header(m_openGameIndex).plyCount > game.moves.size()) {
        *error = tr("Some moves of the stored game cannot be replayed."); // Never cut a stored game short.
        return false;
    }
    if (!m_database->replaceGame(m_openGameIndex, game, error))
        return false;
    m_gameListModel->refreshRow(int(m_openGameIndex));
    m_session->setHeader(m_database->header(m_openGameIndex));
    rebuildPositionIndex(); // The main line may have grown.
    return true;
}

qint64 MainWindow::gameIndexOf(const QString &uid) const
{
    for (qint64 index = 0; m_database && !uid.isEmpty() && index < m_database->gameCount(); ++index) {
        if (m_database->header(index).uid == uid)
            return index;
    }
    return -1;
}

void MainWindow::setGameState(const QString &uid, GameState state)
{
    const qint64 index = gameIndexOf(uid);
    if (index < 0)
        return;
    QString error;
    if (!m_database->setGameState(index, state, &error)) {
        QMessageBox::warning(this, tr("Trash"), tr("Could not move the game: %1").arg(error));
        return;
    }
    showCategory(m_category); // The game leaves the list it was in.
    m_databaseTree->refresh();
    m_positionIndexTimer->start(); // Only the games in the lists are searched.
    switch (state) {
    case GameState::Trashed:
        statusBar()->showMessage(tr("Game moved to the trash"), 3000);
        break;
    case GameState::Live:
        statusBar()->showMessage(tr("Game restored"), 3000);
        break;
    default:
        statusBar()->showMessage(tr("Game deleted"), 3000);
        break;
    }
}

void MainWindow::deleteGame(const QString &uid)
{
    const qint64 index = gameIndexOf(uid);
    if (index < 0)
        return;
    const GameRecord header = m_database->header(index);
    const auto answer = QMessageBox::question(
        this, tr("Delete Game"),
        tr("Delete “%1” from the trash?\n\nIt will not be listed anywhere any more. It stays in the file until "
           "the database is optimized (Database ▸ Database Settings…).")
            .arg(tr("%1 – %2").arg(header.white, header.black)),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
        setGameState(uid, GameState::Deleted);
}

void MainWindow::optimizeDatabase(QWidget *dialog)
{
    if (!m_database)
        return;
    const QString title = tr("Optimize Database");
    if (m_syncPipeline->isRunning()) {
        QMessageBox::information(dialog, title, tr("A sync is running: optimize the database when it has finished."));
        return;
    }
    const int deleted = int(m_database->countGames(GameState::Deleted));
    if (deleted > 0) {
        const auto answer = QMessageBox::question(
            dialog, title,
            tr("Optimize “%1”?\n\n%n deleted game(s) will be removed for good, here and on every device this "
               "database is synced with.", nullptr, deleted)
                .arg(m_database->name()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }

    const QString openUid = m_openGameIndex >= 0 && m_openGameIndex < m_database->gameCount()
        ? m_database->header(m_openGameIndex).uid : QString();
    const bool wasInDatabase = m_openGameIndex >= 0;
    const qint64 sizeBefore = QFileInfo(m_database->location()).size();
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QString error;
    const int removed = m_database->optimize(&error);
    QGuiApplication::restoreOverrideCursor();

    // The games have other indexes now: everything that holds one starts over.
    m_gameListModel->setDatabase(m_database.get());
    if (wasInDatabase)
        m_openGameIndex = gameIndexOf(openUid);
    showCategory(m_category);
    if (m_openGameIndex >= 0) {
        const QModelIndex proxyIndex = m_gameListProxy->mapFromSource(m_gameListModel->index(int(m_openGameIndex), 0));
        if (proxyIndex.isValid())
            m_gameView->selectRow(proxyIndex.row());
    }
    m_databaseTree->refresh();
    rebuildPositionIndex();
    updateGameActions();
    updateGameHeader();

    if (removed < 0) {
        QMessageBox::warning(dialog, title, tr("Could not optimize the database: %1").arg(error));
        return;
    }
    const QLocale locale;
    QMessageBox::information(
        dialog, title,
        tr("%n deleted game(s) removed.", nullptr, removed) + QLatin1Char(' ')
            + tr("The file went from %1 to %2.")
                  .arg(locale.formattedDataSize(sizeBefore),
                       locale.formattedDataSize(QFileInfo(m_database->location()).size())));
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
    const PieceCounts captured = m_session->position().capturedSince(m_session->initialPosition());
    m_capturedPieces->setCaptured(captured);
    m_boardSideColumn->setCaptured(captured);
    m_boardSideColumn->setSideToMove(m_session->position().sideToMove());
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
    // The trash is counted apart: its games are in no other list.
    const bool trash = m_gameListProxy->state() == GameState::Trashed;
    const qint64 total = m_database->countGames(m_gameListProxy->state());
    const qint64 shown = m_gameListProxy->rowCount();
    const QLocale locale;
    QString count = m_gameListProxy->isFiltered()
        ? tr("%1 of %2 games").arg(locale.toString(shown), locale.toString(total))
        : total == 1 ? tr("1 game") : tr("%1 games").arg(locale.toString(total));
    if (trash)
        count = tr("Trash: %1").arg(count);
    m_gameCountLabel->setText(tr("%1 — %2").arg(m_database->name(), count));
}

void MainWindow::showCategory(const GameCategory &category)
{
    using Kind = GameCategory::Kind;
    m_category = category;
    m_filterSource = category.kind == Kind::Source ? category.sourceId : 0;
    GameFilterProxyModel::Predicate predicate;
    GameState state = GameState::Live;
    const QString value = category.value;
    switch (category.kind) {
    case Kind::All:
        break;
    case Kind::Trash:
        state = GameState::Trashed;
        break;
    case Kind::TrashRecent:
    case Kind::TrashOld:
        state = GameState::Trashed;
        predicate = [recent = category.kind == Kind::TrashRecent,
                     now = QDateTime::currentDateTimeUtc()](const GameRecord &game) {
            return GameStates::isRecent(game.stateModified, now) == recent;
        };
        break;
    case Kind::Position:
    case Kind::Variant: {
        // No games until the index is ready; updateBoardFilters() comes back then.
        QSet<qint64> ids;
        if (const PositionIndex *index = m_positionIndex->index()) {
            ids = category.kind == Kind::Position
                ? index->gamesWithPosition(m_session->position())
                : index->gamesWithLine(m_session->initialPosition(), m_session->movesToHere());
        }
        predicate = [ids](const GameRecord &game) { return ids.contains(game.id); };
        break;
    }
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
    m_gameListProxy->setPredicate(predicate, state);
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
        // Every install's Classic Games is the same database (see GameIdentity).
        DatabaseProperties properties = database->properties();
        properties.id = GameIdentity::kClassicGamesLineage;
        database->setProperties(properties, nullptr);
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
    // A copy saved as another file is a new database: two files with one
    // universal id on the same computer would be one database to a sync.
    DatabaseProperties properties = database->properties();
    properties.id = GameIdentity::newLineageId();
    database->setProperties(properties, nullptr);
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
    updateResourceButtons();
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

void MainWindow::newBook()
{
    if (!UserFolders::ensureBooksDir()) {
        QMessageBox::warning(this, tr("New Book"), tr("Could not create the folder “%1”.").arg(UserFolders::booksDir()));
        return;
    }
    QString name = tr("New Book");
    while (true) {
        bool ok = false;
        name = QInputDialog::getText(this, tr("New Book"), tr("Book name:"), QLineEdit::Normal, name, &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
        if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')) || name.startsWith(QLatin1Char('.'))) {
            QMessageBox::warning(this, tr("New Book"), tr("“%1” is not a valid name.").arg(name));
            continue;
        }
        const QString path = QDir(UserFolders::booksDir())
                                 .filePath(name + QLatin1Char('.') + QLatin1String(UserFolders::bookSuffix));
        if (QFileInfo::exists(path)) {
            QMessageBox::warning(this, tr("New Book"), tr("A book named “%1” already exists.").arg(name));
            continue;
        }
        // A clone of the book we ship: a starting point the user then makes their own.
        if (!QFile::copy(QStringLiteral(":/books/pragma-openings.bin"), path)) {
            QMessageBox::warning(this, tr("New Book"), tr("Could not create the book “%1”.").arg(name));
            return;
        }
        // A file copied out of a resource is read-only.
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
        chooseBook(path);
        if (m_book)
            m_openingTreeDock->show();
        statusBar()->showMessage(tr("Created %1").arg(QDir::toNativeSeparators(path)), 5000);
        return;
    }
}

void MainWindow::rebuildBookMenu()
{
    m_bookMenu->clear();
    fillBookChoices(m_bookMenu);
    m_bookMenu->addSeparator();
    m_bookMenu->addAction(themeIcon("document-new", QStyle::SP_FileIcon), tr("N&ew Book…"), this, &MainWindow::newBook);
    m_bookMenu->addAction(tr("&Open Book…"), this, &MainWindow::openBookFile);
    m_bookMenu->addAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon), tr("Show Books &Folder"), this, [] {
        UserFolders::ensureBooksDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::booksDir()));
    });
}

void MainWindow::fillBookChoices(QMenu *menu)
{
    const QString current = m_book ? QFileInfo(m_book->path()).absoluteFilePath() : QString();
    auto *group = new QActionGroup(menu);

    const QDir folder(UserFolders::booksDir());
    QFileInfoList files = folder.entryInfoList({QStringLiteral("*.") + QLatin1String(UserFolders::bookSuffix)},
                                               QDir::Files, QDir::Name);
    // A book chosen outside the folder is listed too.
    if (!current.isEmpty() && std::none_of(files.cbegin(), files.cend(), [&](const QFileInfo &file) {
            return file.absoluteFilePath() == current;
        }))
        files.prepend(QFileInfo(current));
    for (const QFileInfo &file : std::as_const(files)) {
        QAction *action = menu->addAction(file.completeBaseName());
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
    QAction *none = menu->addAction(tr("&No Book"));
    none->setCheckable(true);
    none->setActionGroup(group);
    none->setChecked(current.isEmpty());
    connect(none, &QAction::triggered, this, [this] { chooseBook(QString()); });
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
    updateBookDatabaseStats();
}

void MainWindow::updateBookDatabaseStats()
{
    const PositionIndex *index = m_positionIndex->index();
    if (!m_database) {
        m_bookPanel->setDatabaseStats(BookPanel::DatabaseState::NoDatabase, {});
        return;
    }
    if (!index) {
        m_bookPanel->setDatabaseStats(BookPanel::DatabaseState::Indexing, {});
        return;
    }
    const ChessPosition &position = m_session->position();
    QList<PositionIndex::Stats> stats;
    for (const PolyglotBook::Move &move : m_book ? m_book->moves(position) : QList<PolyglotBook::Move>()) {
        ChessPosition next = position;
        next.play(move.move);
        stats << index->statsWithPosition(next);
    }
    m_bookPanel->setDatabaseStats(BookPanel::DatabaseState::Ready, stats);
}

void MainWindow::restoreOpeningNames()
{
    // The shipped names, one per language, ready made so that the first launch
    // is not spent building them; the TSVs they were built from stay as the fallback.
    if (UserFolders::ensureOpeningNamesDir()) {
        const QDir folder(UserFolders::openingNamesDir());
        for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
            const QString seed = folder.filePath(names.fileName);
            if (QFile::exists(seed))
                continue;
            if (QFile::copy(names.resource, seed)) {
                // A file copied out of a resource is read-only, and this one is a
                // database the user may edit.
                QFile::setPermissions(seed, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
                SqliteGameDatabase::adoptLineage(seed, names.lineage);
                continue;
            }
            QFile tsv(names.tsv);
            QString error;
            std::unique_ptr<SqliteGameDatabase> built;
            if (tsv.open(QIODevice::ReadOnly))
                built = SqliteGameDatabase::create(seed, OpeningNames::gamesFromTsv(QString::fromUtf8(tsv.readAll())), &error);
            if (built) {
                DatabaseProperties properties = built->properties();
                properties.id = names.lineage;
                properties.type = DatabaseType::OpeningBook;
                names.applyNames(properties);
                built->setProperties(properties, nullptr);
            } else {
                statusBar()->showMessage(tr("Could not create %1: %2").arg(QDir::toNativeSeparators(seed), error));
            }
        }
    }

    QSettings settings;
    if (settings.value(QStringLiteral("book/openingNamesExplicit")).toBool()) {
        const QString path = settings.value(QStringLiteral("book/openingNames")).toString();
        chooseOpeningNames(path.isEmpty() || (QFile::exists(path) && markAsOpeningBook(path)) ? path : QString());
        return;
    }
    // Not chosen by the user: the names of the interface language.
    const ShippedOpeningNames::Names names = ShippedOpeningNames::forLanguage(UiLanguage::effective());
    const QString path = QDir(UserFolders::openingNamesDir()).filePath(names.fileName);
    chooseOpeningNames(QFile::exists(path) && markAsOpeningBook(path) ? path : QString());
}

void MainWindow::migrateOpeningNames()
{
    QSettings settings;
    const auto follow = [this, &settings](const QString &from, const QString &to) {
        m_movedDatabases.insert(from, to);
        if (settings.value(QStringLiteral("book/openingNames")).toString() == from)
            settings.setValue(QStringLiteral("book/openingNames"), to);
    };
    // Moves a shipped names database to its place in the Opening Names folder
    // (English.pdb, Italian.pdb). When that place already holds every game of
    // this copy, none older, the copy adds nothing and is removed instead of
    // piling up (the folder sync never deletes, so a server brings an old path
    // back on every sync); any other copy is kept under Old/. Nothing is ever
    // overwritten.
    const auto relocate = [&](const QString &source, const ShippedOpeningNames::Names &shipped) {
        const QString place = QDir(UserFolders::openingNamesDir()).filePath(shipped.fileName);
        if (QFile::exists(place) && SqliteGameDatabase::readProperties(place).id == shipped.lineage
            && ShippedOpeningNames::addsNothing(SqliteGameDatabase::readRevisions(place),
                                                SqliteGameDatabase::readRevisions(source))) {
            if (QFile::remove(source))
                follow(source, place);
            return;
        }
        const QString target = ShippedOpeningNames::moveTarget(UserFolders::openingNamesDir(), shipped,
                                                               [](const QString &path) { return QFile::exists(path); });
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::rename(source, target)) {
            statusBar()->showMessage(tr("Could not move %1 to %2").arg(QDir::toNativeSeparators(source),
                                                                          QDir::toNativeSeparators(target)), 8000);
            return;
        }
        SqliteGameDatabase::adoptLineage(target, shipped.lineage);
        markAsOpeningBook(target);
        follow(source, target);
    };

    if (!UserFolders::ensureOpeningNamesDir())
        return;
    // Shipped names seeded among the databases of games by older versions.
    const QString pattern = QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix);
    const QFileInfoList databases = QDir(UserFolders::databasesDir()).entryInfoList({pattern}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : databases) {
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(file.absoluteFilePath());
        if (!ShippedOpeningNames::shouldMove(file.fileName(), properties))
            continue;
        const ShippedOpeningNames::Names *names = ShippedOpeningNames::byLineage(properties.id);
        relocate(file.absoluteFilePath(), names ? *names : ShippedOpeningNames::all().first());
    }
    // Shipped names in the folder under the file names older versions gave them.
    const QFileInfoList kept = QDir(UserFolders::openingNamesDir()).entryInfoList({pattern}, QDir::Files, QDir::Name);
    for (const QFileInfo &file : kept) {
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(file.absoluteFilePath());
        if (!ShippedOpeningNames::renamedFileName(file.fileName(), properties).isEmpty())
            relocate(file.absoluteFilePath(), *ShippedOpeningNames::byLineage(properties.id));
    }
    const QDir folder(UserFolders::openingNamesDir());
    for (const ShippedOpeningNames::Names &names : ShippedOpeningNames::all()) {
        const QString place = folder.filePath(names.fileName);
        const DatabaseProperties properties = SqliteGameDatabase::readProperties(place);
        if (properties.id != names.lineage)
            continue;
        // Shipped names from before they had a display name get theirs.
        QString error;
        if (properties.name.isEmpty()) {
            if (const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(place, &error)) {
                DatabaseProperties updated = database->properties();
                names.applyNames(updated);
                database->setProperties(updated, &error);
            }
        }
        // A project (or the names setting) on another copy that adds nothing to
        // the shipped one opens the shipped one; the copy itself is left alone.
        const QHash<QString, QString> revisions = SqliteGameDatabase::readRevisions(place);
        const QFileInfoList copies = folder.entryInfoList({pattern}, QDir::Files, QDir::Name);
        for (const QFileInfo &copy : copies) {
            const QString path = copy.absoluteFilePath();
            if (path != QFileInfo(place).absoluteFilePath()
                && SqliteGameDatabase::readProperties(path).id == names.lineage
                && ShippedOpeningNames::addsNothing(revisions, SqliteGameDatabase::readRevisions(path)))
                follow(path, place);
        }
    }
}

void MainWindow::rebuildOpeningNamesMenu()
{
    m_openingNamesMenu->clear();
    auto *group = new QActionGroup(m_openingNamesMenu);
    const QString current = m_openingNamesPath.isEmpty() ? QString() : QFileInfo(m_openingNamesPath).absoluteFilePath();
    const QString pattern = QStringLiteral("*.") + QLatin1String(UserFolders::databaseSuffix);
    // The database's own name in the interface language, not its file name;
    // copies that would read the same say which file they are.
    struct Entry {
        QFileInfo file;
        QString name;
    };
    const auto opening = [&pattern](const QString &folder) {
        QList<Entry> entries;
        for (const QFileInfo &file : QDir(folder).entryInfoList({pattern}, QDir::Files, QDir::Name)) {
            const DatabaseProperties properties = SqliteGameDatabase::readProperties(file.absoluteFilePath());
            if (properties.type == DatabaseType::OpeningBook)
                entries.append({file, properties.displayName(UiLanguage::effective(), file.completeBaseName())});
        }
        return entries;
    };
    const QList<Entry> kept = opening(UserFolders::openingNamesDir());
    const QList<Entry> databases = opening(UserFolders::databasesDir());
    QHash<QString, int> seen;
    for (const QList<Entry> *list : {&kept, &databases}) {
        for (const Entry &entry : *list)
            ++seen[entry.name];
    }
    const auto addEntries = [&](const QList<Entry> &entries) {
        for (const Entry &entry : entries) {
            const QFileInfo &file = entry.file;
            const QString label = seen.value(entry.name) > 1 && entry.name != file.completeBaseName()
                ? tr("%1 (%2)").arg(entry.name, file.completeBaseName())
                : entry.name;
            QAction *action = m_openingNamesMenu->addAction(label);
            action->setCheckable(true);
            action->setActionGroup(group);
            action->setToolTip(QDir::toNativeSeparators(file.absoluteFilePath()));
            const QString path = file.absoluteFilePath();
            action->setChecked(path == current);
            connect(action, &QAction::triggered, this, [this, path] { chooseOpeningNames(path, true); });
        }
        if (!entries.isEmpty())
            m_openingNamesMenu->addSeparator();
    };
    // The shipped names and any others kept with them, then opening books among the databases.
    addEntries(kept);
    addEntries(databases);
    QAction *none = m_openingNamesMenu->addAction(tr("No Names"));
    none->setCheckable(true);
    none->setActionGroup(group);
    none->setChecked(current.isEmpty());
    connect(none, &QAction::triggered, this, [this] { chooseOpeningNames(QString(), true); });
    m_openingNamesMenu->addSeparator();
    m_openingNamesMenu->addAction(themeIcon("folder-open", QStyle::SP_DirOpenIcon), tr("Show Opening Names &Folder"),
                                  this, [] {
                                      UserFolders::ensureOpeningNamesDir();
                                      QDesktopServices::openUrl(QUrl::fromLocalFile(UserFolders::openingNamesDir()));
                                  });
}

void MainWindow::adoptShippedLineages()
{
    // The databases we ship have fixed universal ids, so every install and
    // update of them is the same database when devices sync. Copies seeded
    // before ids existed get theirs here (a file that already has one keeps it).
    // The shipped opening names get theirs in migrateOpeningNames() and when seeded.
    const QString path = QDir(UserFolders::databasesDir())
                             .filePath(tr("Classic Games") + QLatin1Char('.') + QLatin1String(UserFolders::databaseSuffix));
    if (QFile::exists(path))
        SqliteGameDatabase::adoptLineage(path, GameIdentity::kClassicGamesLineage);
}

bool MainWindow::markAsOpeningBook(const QString &path)
{
    // Databases chosen for names before they had a type (and the seed, built
    // from the shipped file) are opening books.
    if (SqliteGameDatabase::readProperties(path).type == DatabaseType::OpeningBook)
        return true;
    QString error;
    const std::unique_ptr<SqliteGameDatabase> database = SqliteGameDatabase::open(path, &error);
    if (!database)
        return false;
    DatabaseProperties properties = database->properties();
    properties.type = DatabaseType::OpeningBook;
    return database->setProperties(properties, &error);
}

void MainWindow::chooseOpeningNames(const QString &path, bool explicitly)
{
    QSettings settings;
    settings.setValue(QStringLiteral("book/openingNames"), path);
    if (explicitly)
        settings.setValue(QStringLiteral("book/openingNamesExplicit"), true);
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
        if (database->header(index).state != GameState::Live)
            continue; // A line in the trash names nothing.
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
    m_databaseSettingsAction->setEnabled(hasDatabase && !m_database->location().isEmpty());
}

void MainWindow::editBoardSettings()
{
    BoardSettingsDialog dialog(BoardSettings::load(), m_coordinatesAction->isChecked(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const BoardSettings settings = dialog.settings();
    settings.save();
    applyBoardSettings(settings);
    m_coordinatesAction->setChecked(dialog.showCoordinates());
}

void MainWindow::applyBoardSettings(const BoardSettings &settings)
{
    m_boardPanel->setCapturedPiecesBelow(settings.capturedPieces == CapturedPiecesPlacement::BelowBoard);
    m_boardSideColumn->setShowTurn(settings.showTurn);
}

void MainWindow::editDatabaseSettings()
{
    if (!m_database)
        return;
    DatabaseSettingsDialog dialog(m_database->name(), m_database->properties(), this);
    connect(&dialog, &DatabaseSettingsDialog::optimizeRequested, this, [this, &dialog] { optimizeDatabase(&dialog); });
    if (dialog.exec() != QDialog::Accepted || dialog.properties() == m_database->properties())
        return;
    QString error;
    if (!m_database->setProperties(dialog.properties(), &error)) {
        QMessageBox::warning(this, tr("Database Settings"), tr("Could not save the settings: %1").arg(error));
        return;
    }
    // Only opening books name openings: a database that is no longer one stops doing it.
    if (dialog.properties().type != DatabaseType::OpeningBook && !m_openingNamesPath.isEmpty()
        && QFileInfo(m_openingNamesPath) == QFileInfo(m_database->location()))
        chooseOpeningNames(QString());
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
    // Duplicates merged before the folder goes to the server (the folder sync
    // does it too, for the syncs it starts by itself).
    m_syncPipeline->addTask(new SyncStepTask(tr("Duplicates"), [this] {
        m_folderSync->mergeDuplicates();
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

void MainWindow::reportUnavailableSource(const GameSource &source)
{
    // Once per source while the application runs: the sync comes round every
    // twenty minutes, and the answer was given.
    if (m_unavailableSourcesReported.contains(source.uuid))
        return;
    m_unavailableSourcesReported.insert(source.uuid);
    QMessageBox box(QMessageBox::Warning, tr("Source Not Found"),
                    tr("The source “%1” cannot be found: the file\n%2\nis not on this computer.")
                        .arg(SourceCatalog::displayName(source), QDir::toNativeSeparators(ChessBaseFetch::path(source))),
                    QMessageBox::NoButton, this);
    box.setInformativeText(tr("To fix it, open Database ▸ Manage Sources… and choose the file again, or remove "
                              "the source."));
    QPushButton *ignore = box.addButton(tr("Ignore"), QMessageBox::RejectRole);
    QPushButton *ignoreHere = box.addButton(tr("Ignore on This Computer"), QMessageBox::AcceptRole);
    ignoreHere->setToolTip(tr("This computer stops looking for the file; the source stays in the database for "
                              "the computers that have it"));
    box.setDefaultButton(ignore);
    box.exec();
    if (box.clickedButton() == ignoreHere) {
        SourceCredentials::setIgnoredHere(source.uuid, true);
        m_databaseTree->scheduleRefresh();
    }
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
        const EngineProfile &profile = m_engines.resolve(m_engineId, m_engineName);
        const QString executable = EngineCatalog::executableFor(profile);
        m_engine->clearOptions();
        if (profile.threads > 0)
            m_engine->setOption(QStringLiteral("Threads"), QString::number(profile.threads));
        if (profile.hashMb > 0)
            m_engine->setOption(QStringLiteral("Hash"), QString::number(profile.hashMb));
        if (executable.isEmpty() || !m_engine->start(executable)) {
            m_enginePanel->setStatus(tr("The engine “%1” was not found. Choose another one or set its "
                                        "executable in Engine ▸ Manage Engines….").arg(profile.name));
            // Defer so the action's toggle finishes before being reverted.
            QMetaObject::invokeMethod(m_startEngineAction, [this] { m_startEngineAction->setChecked(false); },
                                      Qt::QueuedConnection);
            return;
        }
        m_engineId = profile.id;
        m_engineName = profile.name;
        m_engineExecutable = executable;
    }
    m_enginePanel->setStatus(tr("Analyzing…"));
    analyzeCurrentPosition();
}

void MainWindow::manageEngines()
{
    const QString current = m_engines.resolve(m_engineId, m_engineName).id;
    ManageEnginesDialog dialog(m_engines, current, UserFolders::pragmaDir(), this);
    const auto apply = [this, &dialog] {
        m_engines = dialog.catalog();
        {
            QSettings settings;
            m_engines.save(settings);
        }
        // Restart on the engine in use as edited (its executable or parameters
        // may have changed), or on the one chosen with "Use This Engine". The
        // engine belongs to the project: selectEngine() has it saved.
        m_engineId.clear();
        selectEngine(dialog.activeId());
    };
    // "Use This Engine" switches at once, without waiting for OK.
    connect(&dialog, &ManageEnginesDialog::useEngineRequested, this, apply);
    if (dialog.exec() == QDialog::Accepted)
        apply();
}

void MainWindow::selectEngine(const QString &id)
{
    const EngineProfile &profile = m_engines.resolve(id);
    if (profile.id == m_engineId)
        return;
    m_engineId = profile.id;
    m_engineName = profile.name;
    m_enginePanel->setEngineName(profile.name);
    updateResourceButtons();
    const bool analyzing = m_startEngineAction->isChecked();
    m_trainingThinking = false; // A search in progress dies with the old engine.
    m_startEngineAction->setChecked(false);
    m_engine->shutdown();
    if (analyzing)
        m_startEngineAction->setChecked(true);
    scheduleSaveSession();
}

void MainWindow::rebuildEngineChoiceMenu()
{
    m_engineChoiceMenu->clear();
    auto *group = new QActionGroup(m_engineChoiceMenu);
    const QString current = m_engines.resolve(m_engineId, m_engineName).id;
    for (const EngineProfile &profile : m_engines.engines()) {
        QAction *action = m_engineChoiceMenu->addAction(profile.name);
        action->setCheckable(true);
        action->setChecked(profile.id == current);
        action->setActionGroup(group);
        const QString id = profile.id;
        connect(action, &QAction::triggered, this, [this, id] { selectEngine(id); });
    }
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
        m_explainBorder = BoardBorder::Thinking; // Until the engine answers.
        updateBoardBorder();
        m_explainer->setEnabled(true, m_engineExecutable);
        return;
    }
    m_explainer->setEnabled(false);
    m_explainBorder = BoardBorder::Plain;
    updateBoardBorder();
    m_board->stopSequence();
    m_board->setExplanation({}, {});
    m_enginePanel->setExplanation(QString());
    m_explanationText.clear();
}

void MainWindow::updateBoardBorder()
{
    // Explain speaks first, while it is on; under it the tutor's alert keeps
    // the border red until the user has chosen what to do with the move.
    if (m_explainBorder != BoardBorder::Plain)
        m_board->setBorder(m_explainBorder);
    else
        m_board->setBorder(m_tutorReply ? BoardBorder::Alert : BoardBorder::Plain);
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

void MainWindow::newTraining(bool alwaysAsk)
{
    using Choice = NewTrainingDialog::Choice;
    Choice choice = m_rememberedTraining.value_or(m_trainingSide == Side::White ? Choice::White : Choice::Black);
    if (alwaysAsk || !m_rememberedTraining) {
        NewTrainingDialog dialog(choice, m_rememberedTraining.has_value(), this);
        if (dialog.exec() != QDialog::Accepted)
            return;
        choice = dialog.choice();
        // Unticking it forgets the choice: the toolbar asks again.
        m_rememberedTraining = dialog.remember() ? std::optional<Choice>(choice) : std::nullopt;
    }
    m_trainingSide = NewTrainingDialog::sideFor(choice);
    const GameRecord game = trainingHeader();
    const QString engineName = m_trainingSide == Side::White ? game.black : game.white;

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

GameRecord MainWindow::trainingHeader() const
{
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
    return game;
}

void MainWindow::setTrainingMode(bool enabled)
{
    if (!enabled) {
        m_trainingThinking = false;
        m_enginePanel->setLineHidden(false);
        clearTutor();
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
    // The tutor's alert is about one move: leaving it takes the alert away.
    if (m_tutorReply && m_session->ply() != m_tutorPly)
        clearTutor();
    // The best line is the user's move: it may only be shown once they played.
    m_enginePanel->setLineHidden(training && !isEngineTurn());
    if (!training || m_tutorReply) // With the alert up the engine waits for the user's choice.
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
    for (int i = 1; i <= m_session->ply(); ++i)
        moves << m_session->moveAt(i).uci; // The line on the board, variations included.
    m_trainingThinking = true;
    m_lastEvaluation = {};
    m_enginePanel->setStatus(tr("Thinking…"));
    // The search is still run while the book answers: the tutor judges the
    // user's move against it.
    m_engine->analyze(game.startFen, moves, m_session->position().sideToMove(), {kTrainingDepth, 0, false});
}

std::optional<ChessMove> MainWindow::bookReply() const
{
    if (!m_book)
        return std::nullopt;
    const QList<PolyglotBook::Move> moves = m_book->moves(m_session->position());
    const quint32 total = PolyglotBook::totalWeight(moves);
    if (total == 0)
        return std::nullopt;
    const int index = PolyglotBook::pick(moves, QRandomGenerator::global()->bounded(total));
    return index < 0 ? std::nullopt : std::optional<ChessMove>(moves.at(index).move);
}

void MainWindow::finishEngineMove()
{
    if (!m_trainingThinking)
        return;
    m_trainingThinking = false;
    // In the opening the engine plays the book, each move as often as its
    // weight says — that is what the weights of the Opening Tree are for;
    // out of the book, its own best move.
    std::optional<ChessMove> move = bookReply();
    if (!move && !m_lastEvaluation.pv.isEmpty())
        move = m_session->position().moveFromUci(m_lastEvaluation.pv.constFirst());
    if (!move) {
        m_enginePanel->setStatus(tr("The engine found no move to play."));
        return;
    }
    // The tutor: a jump from the evaluation the user moved from to the one
    // of this search says their move was an error, with no analysis of its own.
    const EngineEvaluation evaluation = m_lastEvaluation;
    const std::optional<ChessMove> played = m_session->lastMove();
    if (played && m_trainingBaseline.depth >= kTutorMinDepth
        && m_trainingBaselineFen == m_session->positionAt(m_session->ply() - 1).fen()) {
        const TrainingTutor::Alert alert = TrainingTutor::judge(m_trainingBaseline, evaluation, m_trainingSide, *played);
        if (alert != TrainingTutor::Alert::None) {
            holdEngineReply(*move, evaluation, alert);
            return;
        }
    }
    playEngineReply(*move, evaluation);
}

void MainWindow::playEngineReply(const ChessMove &move, const EngineEvaluation &evaluation)
{
    // What the engine expects after its move: the user's next one is judged
    // against it, unless the analysis goes deeper while they think.
    m_trainingBaseline = evaluation;
    m_trainingBaseline.pv = evaluation.pv.mid(1);
    m_trainingBaseline.depth = qMax(0, evaluation.depth - 1);
    if (evaluation.pv.isEmpty() || evaluation.pv.constFirst() != move.uci())
        m_trainingBaseline.depth = 0; // A book move the search did not look at: the analysis will tell.
    ChessPosition next = m_session->position();
    next.play(move);
    m_trainingBaselineFen = next.fen();
    m_animateNextBoard = true; // syncBoard() slides it across the board.
    playMove(move);
}

void MainWindow::holdEngineReply(const ChessMove &reply, const EngineEvaluation &evaluation, TrainingTutor::Alert alert)
{
    m_tutorReply = reply;
    m_tutorEvaluation = evaluation;
    m_tutorPly = m_session->ply();
    const QString move = m_session->positionAt(m_tutorPly - 1)
                             .lineText({m_session->moveAt(m_tutorPly).uci}, 1, SanStyle::Figurines);
    const QString before = m_trainingBaseline.text();
    const QString after = evaluation.text();
    QString message;
    switch (alert) {
    case TrainingTutor::Alert::Blunder: message = tr("Blunder: %1 (%2 → %3).").arg(move, before, after); break;
    case TrainingTutor::Alert::Mistake: message = tr("Mistake: %1 (%2 → %3).").arg(move, before, after); break;
    case TrainingTutor::Alert::Inaccuracy: message = tr("Inaccuracy: %1 (%2 → %3).").arg(move, before, after); break;
    case TrainingTutor::Alert::MissedChance:
        message = tr("Missed chance: %1 lets your advantage go (%2 → %3).").arg(move, before, after);
        break;
    case TrainingTutor::Alert::None: return;
    }
    m_enginePanel->setTutorAlert(message + QLatin1Char(' ') + tr("The engine has not answered yet."));
    m_engineDock->show();
    updateBoardBorder(); // Red: the game stopped on this move.
}

void MainWindow::clearTutor()
{
    m_tutorReply.reset();
    m_enginePanel->setTutorAlert(QString());
    updateBoardBorder();
}

void MainWindow::takeBackTutorMove()
{
    if (!m_tutorReply)
        return;
    // Back where the move was played from: the evaluation it was judged
    // against is still that position's, so the next try is judged too.
    clearTutor();
    m_session->goBack();
}

void MainWindow::ignoreTutorAlert()
{
    if (!m_tutorReply)
        return;
    const ChessMove reply = *m_tutorReply;
    const EngineEvaluation evaluation = m_tutorEvaluation;
    clearTutor();
    playEngineReply(reply, evaluation);
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
    // The next move of the line only steps forward; anything else adds to the
    // game — at its end, or as a variation — and a stored game is saved at once.
    const bool adds = !m_session->isNextMove(move);
    if (!m_session->playMove(move) || !adds)
        return;
    QString error;
    if (!storeOpenGame(&error))
        statusBar()->showMessage(tr("The move could not be saved in the database: %1").arg(error), 8000);
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
    m_databaseTree->scheduleRefresh();
    rebuildPositionIndex();
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
    AboutDialog dialog(QString::fromLatin1(APP_VERSION), this);
    dialog.exec();
}

void MainWindow::showGuide()
{
    // One window, kept open beside the application: a guide is read while trying.
    if (!m_guideDialog)
        m_guideDialog = new HelpDialog(UiLanguage::effective(), this);
    m_guideDialog->show();
    m_guideDialog->raise();
    m_guideDialog->activateWindow();
}

void MainWindow::restoreSession()
{
    m_restoringSession = true;
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry)) {
        resize(1280, 820);
    } else {
        // The saved size is the whole window, frame included (WindowChrome),
        // and the screen may be smaller than the one it was saved on.
        WindowChrome::markFramed(this);
        if (const QScreen *screen = this->screen())
            resize(size().boundedTo(screen->availableGeometry().size()));
    }
    // A maximized state set before the window is mapped does not survive
    // mapping (Qt's xcb plugin reads back the state the window manager has not
    // applied yet): start normal and maximize once shown (showEvent).
    m_restoredWindowState = windowState() & (Qt::WindowMaximized | Qt::WindowFullScreen);
    if (m_restoredWindowState != Qt::WindowNoState)
        setWindowState(windowState() & ~m_restoredWindowState);

    QHeaderView *gameHeader = m_gameView->horizontalHeader();
    if (gameHeader->restoreState(settings.value(QStringLiteral("games/header")).toByteArray()))
        m_gameView->sortByColumn(gameHeader->sortIndicatorSection(), gameHeader->sortIndicatorOrder());
    applyGameColumns(); // Which columns are shown is the database's, not the session's.

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
        for (const MoveRecord &move : m_session->game().moves) {
            project.moves << move.uci;
            if (!move.nags.isEmpty())
                project.annotations << QStringLiteral("%1:%2").arg(project.moves.size())
                                           .arg(MoveAnnotation::storedSuffix(move.nags));
        }
        project.variations = GameVariations::toText(m_session->game().variations);
    }
    project.boardFlipped = m_flipBoardAction->isChecked();
    project.showCoordinates = m_coordinatesAction->isChecked();
    project.engineId = m_engineId;
    project.engineName = m_engineName;
    project.engineAnalyzing = m_startEngineAction->isChecked();
    project.training = m_trainingModeAction->isChecked();
    project.trainingSide = m_trainingSide;
    project.layout = saveLayout();
    return project;
}

void MainWindow::applyProject(const Project &project, bool openFirstGameIfNone)
{
    const bool wasRestoring = m_restoringSession;
    m_restoringSession = true;
    // Off while the game changes, or the engine would answer in the one being opened.
    m_trainingModeAction->setChecked(false);

    if (!project.layout.isEmpty())
        restoreLayout(project.layout);
    m_flipBoardAction->setChecked(project.boardFlipped);
    m_coordinatesAction->setChecked(project.showCoordinates);
    m_engineId = m_engines.resolve(project.engineId, project.engineName).id;
    m_engineName = m_engines.resolve(m_engineId).name;
    m_enginePanel->setEngineName(m_engineName);
    updateResourceButtons();

    // A database moved since the project was saved (migrateOpeningNames) is opened where it is now.
    const QString databasePath = m_movedDatabases.value(project.databasePath, project.databasePath);
    openInitialDatabase(databasePath);

    bool opened = false;
    // A project without a database path refers to whichever default database was opened.
    const bool sameDatabase = m_database
        && (databasePath.isEmpty() || m_database->location() == databasePath);
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
            game.moves << MoveRecord{QString(), uci, {}}; // SAN is filled in when the game is opened.
        for (const QString &annotation : project.annotations) {
            const int ply = annotation.section(QLatin1Char(':'), 0, 0).toInt();
            if (ply >= 1 && ply <= game.moves.size())
                MoveAnnotation::split(annotation.section(QLatin1Char(':'), 1), &game.moves[ply - 1].nags);
        }
        game.variations = GameVariations::fromText(project.variations);
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
    // A project closed while training goes on training: the engine answers
    // at once if the move is its own.
    if (project.training) {
        m_trainingSide = project.trainingSide;
        // An unsaved game comes back as its moves only: say again who plays.
        if (m_openGameIndex < 0)
            m_session->setHeader(trainingHeader());
        m_trainingModeAction->setChecked(true);
    }

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
    // Only the project, with the asterisk of unsaved changes, and the
    // application: "Untitled* - Pragma Chess". The database and the game are
    // on show in the window itself. Written in full, with a plain hyphen: Qt
    // would add the name by itself after a long dash.
    const QString name = m_projectPath.isEmpty() ? tr("Untitled") : QFileInfo(m_projectPath).completeBaseName();
    setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, QGuiApplication::applicationDisplayName()));
}

void MainWindow::updateProjectModified()
{
    const bool modified = !m_projectPath.isEmpty() && captureProject().toYaml() != m_savedProjectYaml;
    setWindowModified(m_projectPath.isEmpty() || modified); // A project never saved has the asterisk too.
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
